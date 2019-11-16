#pragma once

#include <map>
#include <list>
#include <vector>
#include <memory>

#include "ChakraCore.h"

#include "common.h"
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

  struct ScriptError {};

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

  enum class JobKind {
    call,
    parse_module,
    evaluate_module,
    add_unhandled_rejection,
    remove_unhandled_rejection,
  };

  struct Job {
    JobKind _kind;
    VarRef _func;
    std::vector<Var> _args;

    Job(JobKind kind, Var func) : _kind {kind}, _func {func} {}

    Job(JobKind kind, Var func, std::vector<Var>&& args) :
      _kind {kind},
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

    const JobKind kind() const { return _kind; }
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
  template<typename T>
  Var CHAKRA_CALLBACK native_func_callback(
    Var callee,
    bool construct,
    Var* args,
    unsigned short arg_count,
    void* data);

  inline void _checked(JsErrorCode code) {
    if (code != JsNoError) {
      bool has_exception;
      JsHasException(&has_exception);
      if (has_exception) {
        throw ScriptError {};
      }
      throw EngineError {code};
    }
  }

  struct RealmAPI {
    RealmInfo& _realm_info;

    explicit RealmAPI(RealmInfo& realm_info) : _realm_info {realm_info} {}

    void throw_exception(Var error) {
      set_exception(error);
      throw ScriptError {};
    }

    void set_exception(Var error) {
      JsSetException(error);
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
      auto id = _realm_info.next_script_id++;
      _realm_info.script_urls[id] = URLInfo::parse(url);
      Var result;
      _checked(JsRun(source, id, create_string(url), JsParseScriptAttributeNone, &result));
      return result;
    }

    Var call_function(Var fn, const std::vector<Var>& args = {}) {
      Var result;
      if (args.empty()) {
        Var arg = undefined();
        _checked(JsCallFunction(fn, &arg, 1, &result));
      } else {
        Var* args_ptr = const_cast<Var*>(args.data());
        auto count = static_cast<unsigned short>(args.size());
        _checked(JsCallFunction(fn, args_ptr, count, &result));
      }
      return result;
    }

    Var create_object() {
      Var result;
      JsCreateObject(&result);
      return result;
    }

    Var construct(Var fn, const std::vector<Var>& args = {}) {
      Var result;
      if (args.empty()) {
        Var arg = undefined();
        _checked(JsConstructObject(fn, &arg, 1, &result));
      } else {
        Var* args_ptr = const_cast<Var*>(args.data());
        auto count = static_cast<unsigned short>(args.size());
        _checked(JsConstructObject(fn, args_ptr, count, &result));
      }
      return result;
    }

    Var global_property(const std::string& name) {
      return get_property(global_object(), name);
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
      _checked(JsCreateString(buffer, length, &value));
      return value;
    }

    Var create_string(const std::string& name) {
      return create_string(name.data(), name.length());
    }

    Var empty_string() {
      return create_string("", 0);
    }

    template<typename T>
    Var create_function(Var hidden = nullptr) {
      auto name = create_string(T::name);
      JsNativeFunction native_func = native_func_callback<T>;
      Var func;
      _checked(JsCreateNamedFunction(name, native_func, hidden, &func));
      return func;
    }

    Var get_property(Var object, const std::string& name) {
      Var result;
      _checked(JsGetProperty(object, create_property_id(name), &result));
      return result;
    }

    void set_property(Var object, const std::string& name, Var value) {
      _checked(JsSetProperty(object, create_property_id(name), value, true));
    }

    void set_indexed_property(Var object, Var index, Var value) {
      _checked(JsSetIndexedProperty(object, index, value));
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
      _checked(JsEquals(value, undefined(), &equal));
      return equal;
    }

    Var to_string(Var value) {
      Var result;
      _checked(JsConvertValueToString(value, &result));
      return result;
    }

    template<typename I = int>
    I to_integer(Var value) {
      int i;
      _checked(JsNumberToInt(value, &i));
      return static_cast<I>(i);
    }

    std::string utf8_string(Var value) {
      Var string_value = to_string(value);
      size_t length;
      _checked(JsCopyString(string_value, nullptr, 0, &length));
      // TODO: Can we provide a buffer without initializing?
      // TODO: [CC] Too many conversions between CC's internal string data and UTF8
      std::string buffer;
      buffer.resize(length);
      _checked(JsCopyString(string_value, buffer.data(), length, nullptr));
      return buffer;
    }

    void enqueue_job(Job& job) {
      _realm_info.job_queue->enqueue(std::move(job));
    }

    // ## MODULES

    ModuleInfo* find_module_info(Var module) {
      auto& module_info = _realm_info.module_info;
      if (auto p = module_info.find(module); p != module_info.end()) {
        return &p->second;
      }
      return nullptr;
    }

    Var find_module_record(const std::string& url) {
      auto& module_map = _realm_info.module_map;
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
      JsModuleRecord importer = nullptr);

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
      auto& script_urls = _realm_info.script_urls;
      if (auto p = script_urls.find(script_id); p != script_urls.end()) {
        base_url = &p->second;
      }
      return resolve_module_specifier(specifier, base_url);
    }

    void set_module_load_callback(Var callback) {
      _realm_info.module_load_callback = VarRef {callback};
    }

    Var get_module_load_callback() {
      return _realm_info.module_load_callback.var();
    }

    void set_module_source(Var module, Var error, Var source);

    void parse_module(Var module);

    void evaluate_module(Var module, Var error);

    void initialize_import_meta(Var module, Var meta_object) {
      Var url_string;
      JsGetModuleHostInfo(module, JsModuleHostInfo_Url, &url_string);
      set_property(meta_object, "url", url_string);
    }

  };

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
    auto enter(F fn) {
      JsContextRef current = nullptr;
      JsGetCurrentContext(&current);
      JsSetCurrentContext(_context);
      auto cleanup = on_scope_exit([=]() {
        JsSetCurrentContext(current);
      });
      return fn(RealmAPI {_info});
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
    static auto enter_current(F fn) {
      return fn(RealmAPI {Realm::current()->info()});
    }

  };

  struct CallArgs {
    Var callee;
    Var* args;
    unsigned short count;
    Var undefined;

    Var operator[](size_t index) {
      return count > index ? args[index] : undefined;
    }

    const Var operator[](size_t index) const {
      return count > index ? args[index] : undefined;
    }
  };

  struct NativeFunc {
    inline static std::string name = "";

    static Var construct(RealmAPI& api, CallArgs& args, Var data) {
      auto err = api.construct(api.global_property("TypeError"), {
        api.undefined(),
        api.create_string("Function is not a constructor"),
      });
      api.set_exception(err);
      return nullptr;
    }

    static Var call(RealmAPI& api, CallArgs& args, Var data) {
      auto err = api.construct(api.global_property("TypeError"), {
        api.undefined(),
        api.create_string("Constructor cannot be called without the new keyword"),
      });
      api.set_exception(err);
      return nullptr;
    }
  };

  template<typename T>
  Var CHAKRA_CALLBACK native_func_callback(
    Var callee,
    bool construct,
    Var* args,
    unsigned short arg_count,
    void* data)
  {
    RealmAPI api {Realm::current()->info()};
    CallArgs call_args {callee, args, arg_count, api.undefined()};

    try {
      return construct
        ? T::construct(api, call_args, data)
        : T::call(api, call_args, data);
    } catch (const ScriptError&) {
      return nullptr;
    } catch (const HostError& error) {
      // Convert HostErrors to JS exceptions
      auto err = api.construct(api.global_property("Error"), {
        api.undefined(),
        api.create_string(error.message),
      });
      api.set_property(err, "code", api.create_string(error.code));
      api.set_exception(err);
      return nullptr;
    }

    // TODO: Crash if another kind of error is thrown?
  }

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

  inline Engine create_engine() {
    return Engine {};
  }

  inline Realm* current_realm() {
    return Realm::current();
  }

}
