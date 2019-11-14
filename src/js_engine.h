#pragma once

#include <cassert>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <memory>
#include <type_traits>
#include <utility>
#include <iostream> // TODO: Debug only

#include "ChakraCore.h"

#include "util.h"
#include "url.h"

using url::URLInfo;

namespace js {

  using Var = JsValueRef;

  struct VarRef {
    Var _ref;

    VarRef() : _ref {nullptr} {}

    explicit VarRef(void* ref) : _ref {ref} {
      if (_ref) {
        JsAddRef(_ref, nullptr);
      }
    }

    VarRef(const VarRef& other) = delete;
    VarRef& operator=(const VarRef& other) = delete;

    VarRef(VarRef&& other) : _ref {other._ref} {
      other._ref = nullptr;
    }

    VarRef& operator=(VarRef&& other) {
      if (this != &other) {
        _ref = other._ref;
        other._ref = nullptr;
      }
      return *this;
    }

    ~VarRef() {
      if (_ref) {
        JsRelease(_ref, nullptr);
      }
    }

    explicit operator bool() const {
      return _ref != nullptr;
    }

    const Var var() const { return _ref; }
    Var var() { return _ref; }

    void clear() {
      if (_ref) {
        JsRelease(_ref, nullptr);
        _ref = nullptr;
      }
    }

  };

  struct EngineError {
    JsErrorCode code;
  };

  struct ScriptError {
    JsErrorCode code;
  };

  enum class ModuleState {
    loading,
    parsing,
    initializing,
    complete,
    error,
  };

  struct ModuleInfo {
    ModuleState state = ModuleState::loading;
    URLInfo url;
    VarRef source;
  };

  enum class JobType {
    call,
    parse_module,
    evaluate_module,
    add_unhandled_rejection,
    remove_unhandled_rejection,
  };

  struct Job {
    JobType _type;
    VarRef _func;
    std::vector<Var> _args;

    Job(JobType type, Var func) : _type {type}, _func {func} {}

    Job(JobType type, Var func, std::vector<Var>&& args) :
      _type {type},
      _func {func},
      _args {args}
    {
      for (Var arg : args) {
        JsAddRef(arg, nullptr);
      }
    }

    Job(const Job& other) = delete;
    Job& operator=(const Job& other) = delete;

    Job(Job&& other) = default;
    Job& operator=(Job&& other) = default;

    ~Job() {
      for (Var arg : _args) {
        JsRelease(arg, nullptr);
      }
    }

    const JobType type() const { return _type; }
    const Var func() const { return _func.var(); }
    const std::vector<Var>& args() const { return _args; }
  };

  struct JobQueue {
    std::list<Job> _queue;

    bool empty() const {
      return _queue.empty();
    }

    void enqueue(Job&& job) {
      _queue.push_back(std::move(job));
    }

    Job dequeue() {
      Job job = std::move(_queue.front());
      _queue.pop_front();
      return job;
    }
  };

  struct RealmInfo {
    JsSourceContext next_script_id = 0;
    VarRef module_load_callback;
    std::map<std::string, VarRef> module_map;
    std::map<JsModuleRecord, ModuleInfo> module_info;
    std::map<JsSourceContext, URLInfo> script_urls;
    std::shared_ptr<JobQueue> job_queue;
  };

  // Forward
  struct RealmAPI;

  struct Realm {
    JsContextRef _context;
    RealmInfo _info;

    Realm(JsContextRef context, std::shared_ptr<JobQueue>& job_queue);

    Realm(const Realm& other) = delete;
    Realm& operator=(const Realm& other) = delete;

    Realm(Realm&& other) :
      _context {other._context},
      _info {std::move(other._info)}
    {
      JsSetContextData(_context, this);
      other._context = nullptr;
    }

    Realm& operator=(Realm&& other) {
      if (this != &other) {
        _info = std::move(other._info);
        _context = other._context;

        JsSetContextData(_context, this);
        other._context = nullptr;
      }
      return *this;
    }

    ~Realm() {
      if (_context != nullptr) {
        JsSetContextData(_context, nullptr);
      }
    }

    RealmInfo& info() {
      return _info;
    }

    const RealmInfo& info() const {
      return _info;
    }

    template<typename F>
    Var enter(F fn) {
      JsContextRef current = nullptr;
      JsGetCurrentContext(&current);
      JsSetCurrentContext(_context);
      auto cleanup = on_scope_exit([=]() {
        JsSetCurrentContext(current);
      });
      RealmAPI api {*this};
      return fn(api);
    }

    static Realm* from_context_ref(JsContextRef context) {
      if (!context) {
        return nullptr;
      }
      Realm* realm = nullptr;
      JsGetContextData(context, reinterpret_cast<void**>(&realm));
      return realm;
    }

    static Realm* current() {
      JsContextRef context;
      JsGetCurrentContext(&context);
      return Realm::from_context_ref(context);
    }

    static Realm* from_object(Var object) {
      JsContextRef context = nullptr;
      JsGetContextOfObject(object, &context);
      return Realm::from_context_ref(context);
    }

    template<typename F>
    static Var enter_current(F fn) {
      RealmAPI api {*Realm::current()};
      return fn(api);
    }

    template<typename F>
    static Var native_call(F fn) {
      RealmAPI api {*Realm::current()};
      try {
        return fn(api);
      } catch (const ScriptError&) {
        return api.undefined();
      }
      // TODO: Crash if other kind of error is thrown?
    }

    static void CHAKRA_CALLBACK enqueue_promise_callback(
      Var func,
      void* state)
    {
      enter_current([=](auto api) {
        api.enqueue_job(Job {JobType::call, func});
        return nullptr;
      });
    }

    static void CHAKRA_CALLBACK rejection_tracker_callback(
      Var promise,
      Var reason,
      bool handled,
      void* state)
    {
      enter_current([=](auto api) {
        JobType job_type = handled
          ? JobType::remove_unhandled_rejection
          : JobType::add_unhandled_rejection;
        api.enqueue_job(Job {job_type, promise, {reason}});
        return nullptr;
      });
    }

    static JsErrorCode CHAKRA_CALLBACK import_fetch_callback(
      JsModuleRecord importer,
      Var specifier,
      JsModuleRecord* module)
    {
      *module = enter_current([=](auto api) {
        return api.resolve_module(importer, specifier);
      });
      return JsNoError;
    }

    static JsErrorCode CHAKRA_CALLBACK dynamic_import_fetch_callback(
      JsSourceContext script_id,
      Var specifier,
      JsModuleRecord* module)
    {
      *module = enter_current([=](auto api) {
        return api.resolve_module_from_script(script_id, specifier);
      });
      return JsNoError;
    }

    static JsErrorCode CHAKRA_CALLBACK module_ready_callback(
      JsModuleRecord module,
      Var exception)
    {
      enter_current([=](auto api) {
        api.enqueue_job(Job {
          JobType::evaluate_module,
          api.undefined(),
          {module}
        });
        return nullptr;
      });
      return JsNoError;
    }

    static JsErrorCode CHAKRA_CALLBACK initialize_import_meta_callback(
      JsModuleRecord module,
      Var meta_object)
    {
      enter_current([=](auto api) {
        api.initialize_import_meta(module, meta_object);
        return nullptr;
      });
      return JsNoError;
    }

    static Var CHAKRA_CALLBACK set_module_source_callback(
      Var callee,
      bool construct,
      Var* args,
      unsigned short arg_count,
      Var data)
    {
      return native_call([=](auto api) {
        if (arg_count < 2) {
          // Throw: not enough arguments
        }
        api.set_module_source(data, args[1], arg_count > 2 ? args[2] : nullptr);
        return api.undefined();
      });
    }

  };

  struct RealmAPI {
    Realm& _realm;

    explicit RealmAPI(Realm& realm) : _realm {realm} {}

    void throw_on_error(JsErrorCode code) {
      if (code != JsNoError) {
        bool has_exception;
			  JsHasException(&has_exception);
        if (has_exception) {
          throw ScriptError {code};
        }
        throw EngineError {code};
      }
    }

    void throw_exception(Var exception) {
      JsSetException(exception);
      throw ScriptError {};
    }

    bool has_exception() {
      bool has_exception;
      JsHasException(&has_exception);
      return has_exception;
    }

    Var pop_exception() {
      if (has_exception()) {
        Var error;
        JsGetAndClearException(&error);
        return error;
      }
      return undefined();
    }

    Var pop_exception_info() {
      if (has_exception()) {
        Var error_info;
        JsGetAndClearExceptionWithMetadata(&error_info);
        return error_info;
      }
      return undefined();
    }

    Var eval(Var source, const std::string& url = "") {
      auto id = _realm.info().next_script_id++;
      _realm.info().script_urls[id] = URLInfo::parse(url);
      Var result;
      throw_on_error(
        JsRun(source, id, create_string(url), JsParseScriptAttributeNone, &result)
      );
      return result;
    }

    Var call_function(Var fn, const std::vector<Var>& args = {}) {
      Var result;
      if (args.empty()) {
        Var arg = undefined();
        JsCallFunction(fn, &arg, 1, &result);
      } else {
        Var* args_ptr = const_cast<Var*>(args.data());
        auto count = static_cast<unsigned short>(args.size());
        JsCallFunction(fn, args_ptr, count, &result);
      }
      return result;
    }

    Var create_object() {
      Var result;
      JsCreateObject(&result);
      return result;
    }

    Var create_array(unsigned length = 0) {
      Var result;
      JsCreateArray(length, &result);
      return result;
    }

    Var create_number(int value) {
      Var result;
      JsIntToNumber(value, &result);
      return result;
    }

    JsPropertyIdRef create_property_id(const std::string& name) {
      JsPropertyIdRef id;
      JsCreatePropertyId(name.c_str(), name.length(), &id);
      return id;
    }

    Var create_string(const char* buffer) {
      return create_string(buffer, std::char_traits<char>::length(buffer));
    }

    Var create_string(const char* buffer, size_t length) {
      Var value;
      JsCreateString(buffer, length, &value);
      return value;
    }

    Var create_string(const std::string& name) {
      return create_string(name.data(), name.length());
    }

    Var empty_string() {
      return create_string("", 0);
    }

    Var create_function(
      const std::string& name,
      JsNativeFunction native_func,
      Var hidden = nullptr)
    {
      Var func;
      JsCreateNamedFunction(create_string(name), native_func, hidden, &func);
      return func;
    }

    Var get_property(Var object, const std::string& name) {
      Var result;
      JsGetProperty(object, create_property_id(name), &result);
      return result;
    }

    void set_property(Var object, const std::string& name, Var value) {
      JsSetProperty(object, create_property_id(name), value, true);
    }

    void set_indexed_property(Var object, Var index, Var value) {
      JsSetIndexedProperty(object, index, value);
    }

    void set_indexed_property(Var object, int index, Var value) {
      return set_indexed_property(object, create_number(index), value);
    }

    Var undefined() {
      Var value;
      JsGetUndefinedValue(&value);
      return value;
    }

    Var global_object() {
      Var global;
      JsGetGlobalObject(&global);
      return global;
    }

    bool is_null_or_undefined(Var value) {
      bool equal;
      JsEquals(value, undefined(), &equal);
      return equal;
    }

    Var to_string(Var value) {
      Var result;
      JsConvertValueToString(value, &result);
      return result;
    }

    template<typename I = int>
    I to_integer(Var value) {
      int i;
      JsNumberToInt(value, &i);
      return static_cast<I>(i);
    }

    std::string utf8_string(Var value) {
      Var string_value = to_string(value);
      size_t length;
      JsCopyString(string_value, nullptr, 0, &length);
      // TODO: Can we provide a buffer without initializing?
      // TODO: [CC] Too many conversions between CC's internal string data and UTF8
      std::string buffer;
      buffer.resize(length);
      JsCopyString(string_value, buffer.data(), length, nullptr);
      return buffer;
    }

    void enqueue_job(Job& job) {
      _realm.info().job_queue->enqueue(std::move(job));
    }

    // ## MODULES

    ModuleInfo* find_module_info(Var module) {
      auto& module_info = _realm.info().module_info;
      if (auto p = module_info.find(module); p != module_info.end()) {
        return &p->second;
      }
      return nullptr;
    }

    Var find_module_record(const std::string& url) {
      auto& module_map = _realm.info().module_map;
      if (auto p = module_map.find(url); p != module_map.end()) {
        return p->second.var();
      } else {
        return nullptr;
      }
    }

    Var find_module_record(Var url_string) {
      return find_module_record(utf8_string(url_string));
    }

    JsModuleRecord resolve_module_specifier(
      Var specifier,
      const URLInfo* base_url,
      JsModuleRecord importer = nullptr)
    {
      // TODO: Handle failure to parse as URL. We can assume that base_url
      // is a valid absolute URL. What are the conditions under which it
      // would fail? Do we need to create a module record and set the
      // module's exception?

      // Resolve the specifier
      auto url_info = URLInfo::parse(utf8_string(specifier), base_url);
      auto url = URLInfo::stringify(url_info);
      Var url_string = create_string(url);

      // Return the module if it already exists
      if (Var module = find_module_record(url)) {
        return module;
      }

      // Create a new module record
      JsModuleRecord module;
      JsInitializeModuleRecord(importer, url_string, &module);
      JsSetModuleHostInfo(module, JsModuleHostInfo_Url, url_string);
      _realm.info().module_map[url] = VarRef {module};
      _realm.info().module_info[module].url = std::move(url_info);

      // Create a finisher callback
      Var fn = create_function(
        "setModuleSource",
        Realm::set_module_source_callback,
        module);

      // Enqueue a job to call the module load callback
      enqueue_job(Job {
        JobType::call,
        get_module_load_callback(),
        {undefined(), url_string, fn}
      });

      return module;
    }

    JsModuleRecord resolve_module(
      JsModuleRecord importer,
      Var specifier)
    {
      URLInfo* base_url = nullptr;
      if (auto* info = find_module_info(importer)) {
        base_url = &info->url;
      }
      return resolve_module_specifier(specifier, base_url, importer);
    }

    JsModuleRecord resolve_module_from_script(
      JsSourceContext script_id,
      Var specifier)
    {
      const URLInfo* base_url = nullptr;
      auto& script_urls = _realm._info.script_urls;
      if (auto p = script_urls.find(script_id); p != script_urls.end()) {
        base_url = &p->second;
      }
      return resolve_module_specifier(specifier, base_url);
    }

    void set_module_load_callback(Var callback) {
      _realm.info().module_load_callback = VarRef {callback};
    }

    Var get_module_load_callback() {
      return _realm.info().module_load_callback.var();
    }

    void set_module_source(Var module, Var error, Var source) {
      ModuleInfo* info = find_module_info(module);
      assert(info);
      if (info->state != ModuleState::loading) {
        // TODO: Throw (module not loading)
      }

      if (is_null_or_undefined(error)) {
        if (!source) {
          source = empty_string();
        }

        info->state = ModuleState::parsing;
        info->source = VarRef {source};

        enqueue_job(Job {
          JobType::parse_module,
          undefined(),
          {module}
        });
      } else {
        info->state = ModuleState::error;
        JsSetModuleHostInfo(module, JsModuleHostInfo_Exception, error);
      }
    }

    void parse_module(Var module) {
      ModuleInfo* info = find_module_info(module);
      assert(info);
      if (info->state != ModuleState::parsing) {
        // TODO: throw error
      }

      auto source = utf8_string(info->source.var());
      info->source.clear();

      Var err;

      JsParseModuleSource(
        module,
        _realm.info().next_script_id++,
        reinterpret_cast<uint8_t*>(source.data()),
        static_cast<unsigned>(source.length()),
        JsParseModuleSourceFlags_DataIsUTF8,
        &err);

      if (err) {
        info->state = ModuleState::error;
      } else {
        info->state = ModuleState::initializing;
      }
    }

    void evaluate_module(Var module) {
      ModuleInfo* info = find_module_info(module);
      assert(info);
      if (info->state != ModuleState::initializing) {
        // TODO: throw error
      }

      info->state = ModuleState::error;
      JsModuleEvaluation(module, nullptr);

      // TODO: [CC] This error handling dance is a little weird. Why can't
      // JsModuleEvaluation just set the exception for us?

      bool errored = has_exception();
      if (errored) {
        throw ScriptError {};
      }

      Var error;
      JsGetModuleHostInfo(module, JsModuleHostInfo_Exception, &error);
      if (error) {
        throw_exception(error);
      }

      info->state = ModuleState::complete;
    }

    void initialize_import_meta(Var module, Var meta_object) {
      Var url_string;
      JsGetModuleHostInfo(module, JsModuleHostInfo_Url, &url_string);
      set_property(meta_object, "url", url_string);
    }

  };

  struct Engine {
    JsRuntimeHandle _runtime;
    std::shared_ptr<JobQueue> _job_queue;

    Engine() {
      if (JsCreateRuntime(JsRuntimeAttributeNone, nullptr, &_runtime) != JsNoError) {
        throw EngineError {};
      }
      _job_queue = std::make_shared<JobQueue>();
    }

    Engine(const Engine& other) = delete;
    Engine& operator=(const Engine& other) = delete;

    Engine(Engine&& other) {
      _runtime = other._runtime;
      _job_queue = other._job_queue;
      other._runtime = JS_INVALID_RUNTIME_HANDLE;
    }

    Engine& operator=(Engine&& other) {
      if (this != &other) {
        _runtime = other._runtime;
        _job_queue = std::move(other._job_queue);
        other._runtime = JS_INVALID_RUNTIME_HANDLE;
      }
      return *this;
    }

    ~Engine() {
      if (_runtime != JS_INVALID_RUNTIME_HANDLE) {
        // TODO: What if there are still live Realms?
        JsSetCurrentContext(nullptr);
        JsDisposeRuntime(_runtime);
        _runtime = JS_INVALID_RUNTIME_HANDLE;
      }
    }

    Realm create_realm() {
      JsContextRef context;
      if (JsCreateContext(_runtime, &context) != JsNoError) {
        throw EngineError {};
      }
      return Realm {context, _job_queue};
    }

    void flush_job_queue();

  };

  Engine create_engine();

  Realm* current_realm();

  template<typename F>
  Var native_call(F fn) {
    return Realm::native_call(fn);
  }

}
