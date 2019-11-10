#pragma once

#include <string>
#include <map>
#include <list>
#include <vector>
#include <utility>
#include "ChakraCore.h"
#include "on_scope_exit.h"
#include "url.h"

using url::URLInfo;

namespace js {

  using Var = JsValueRef;

  // Forward
  struct RealmInterface;

  struct EngineError {};

  struct ModuleInfo {
    URLInfo url;
  };

  struct RealmInfo {
    JsSourceContext next_script_id = 0;
    std::map<std::string, JsModuleRecord> module_map;
    std::map<JsModuleRecord, ModuleInfo> module_info;
    std::map<JsSourceContext, URLInfo> script_urls;
  };

  struct Realm {
    JsContextRef _context;
    RealmInfo _info;

    explicit Realm(JsRuntimeHandle runtime) {
      if (JsCreateContext(runtime, &_context) != JsNoError) {
        throw EngineError {};
      }
      JsSetContextData(_context, this);
    }

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
    JsValueRef enter(F fn) {
      JsContextRef current = nullptr;
      JsGetCurrentContext(&current);
      JsSetCurrentContext(_context);
      auto cleanup = on_scope_exit([=]() {
        JsSetCurrentContext(current);
      });
      return fn(RealmInterface {this});
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

    static Realm* from_object(JsValueRef object) {
      JsContextRef context = nullptr;
      JsGetContextOfObject(object, &context);
      return Realm::from_context_ref(context);
    }

  };

  struct ResolveModuleResult {
    JsModuleRecord module_record;
    std::string url;
    bool is_new;
  };

  struct RealmInterface {
    Realm* _realm;

    explicit RealmInterface(Realm* realm) : _realm {realm} {}

    JsValueRef eval(JsValueRef source, const std::string& url = "") {
      auto id = _realm->info().next_script_id++;
      _realm->info().script_urls[id] = URLInfo::parse(url);
      JsValueRef result;
      JsRun(source, id, create_string(url), JsParseScriptAttributeNone, &result);
      return result;
    }

    JsValueRef call_function(JsValueRef fn, const std::vector<JsValueRef>& args) {
      JsValueRef result;
      JsValueRef* args_ptr = const_cast<JsValueRef*>(args.data());
      auto count = static_cast<unsigned short>(args.size());
      JsCallFunction(fn, args_ptr, count, &result);
      return result;
    }

    JsValueRef create_object() {
      JsValueRef result;
      JsCreateObject(&result);
      return result;
    }

    JsValueRef create_array(unsigned length = 0) {
      JsValueRef result;
      JsCreateArray(length, &result);
      return result;
    }

    JsValueRef create_number(int value) {
      JsValueRef result;
      JsIntToNumber(value, &result);
      return result;
    }

    JsPropertyIdRef create_property_id(const std::string& name) {
      JsPropertyIdRef id;
      JsCreatePropertyId(name.c_str(), name.length(), &id);
      return id;
    }

    JsValueRef create_string(const char* buffer) {
      return create_string(buffer, std::char_traits<char>::length(buffer));
    }

    JsValueRef create_string(const char* buffer, size_t length) {
      JsValueRef value;
      JsCreateString(buffer, length, &value);
      return value;
    }

    JsValueRef create_string(const std::string& name) {
      return create_string(name.data(), name.length());
    }

    JsValueRef empty_string() {
      return create_string("", 0);
    }

    template<typename F>
    JsValueRef create_function(const std::string& name, F callback) {
      JsValueRef func;
      JsCreateNamedFunction(create_string(name), native_func_callback<F>, callback, &func);
      return func;
    }

    void set_property(JsValueRef object, const std::string& name, JsValueRef value) {
      JsSetProperty(object, create_property_id(name), value, true);
    }

    void set_indexed_property(JsValueRef object, JsValueRef index, JsValueRef value) {
      JsSetIndexedProperty(object, index, value);
    }

    void set_indexed_property(JsValueRef object, int index, JsValueRef value) {
      return set_indexed_property(object, create_number(index), value);
    }

    JsValueRef undefined() {
      JsValueRef value;
      JsGetUndefinedValue(&value);
      return value;
    }

    JsValueRef global_object() {
      JsValueRef global;
      JsGetGlobalObject(&global);
      return global;
    }

    JsValueRef to_string(JsValueRef value) {
      JsValueRef result;
      JsConvertValueToString(value, &result);
      return result;
    }

    std::string utf8_string(JsValueRef value) {
      JsValueRef string_value = to_string(value);
      size_t length;
      JsCopyString(string_value, nullptr, 0, &length);
      // TODO: This will pre-initialize the data to zeros. Can we get around
      // that somehow?
      std::string buffer;
      buffer.resize(length);
      JsCopyString(string_value, buffer.data(), length, nullptr);
      return std::move(buffer);
    }

    ResolveModuleResult resolve_module_specifier(
      JsValueRef specifier,
      const URLInfo* base_url,
      JsModuleRecord importer = nullptr)
    {
      auto url_info = URLInfo::parse(utf8_string(specifier), base_url);
      auto url = URLInfo::stringify(url_info);

      // TODO: handle failure to parse as URL

      auto& module_map = _realm->_info.module_map;
      if (auto p = module_map.find(url); p != module_map.end()) {
        return {p->second, std::move(url), false};
      }

      JsValueRef url_string = create_string(url);
      JsModuleRecord module;
      JsInitializeModuleRecord(importer, url_string, &module);
      JsSetModuleHostInfo(module, JsModuleHostInfo_Url, url_string);

      module_map[url] = module;
      _realm->info().module_info[module].url = std::move(url_info);

      return {module, std::move(url), true};
    }

    ResolveModuleResult resolve_module(
      JsModuleRecord importer,
      JsValueRef specifier)
    {
      const URLInfo* base_url = nullptr;
      auto& module_info = _realm->_info.module_info;
      if (auto p = module_info.find(importer); p != module_info.end()) {
        base_url = &p->second.url;
      }
      return resolve_module_specifier(specifier, base_url, importer);
    }

    ResolveModuleResult resolve_module_from_script(
      JsSourceContext script_id,
      JsValueRef specifier)
    {
      const URLInfo* base_url = nullptr;
      auto& script_urls = _realm->_info.script_urls;
      if (auto p = script_urls.find(script_id); p != script_urls.end()) {
        base_url = &p->second;
      }
      return resolve_module_specifier(specifier, base_url);
    }

    template<typename F>
    static JsValueRef CHAKRA_CALLBACK native_func_callback(
      JsValueRef callee,
      bool construct,
      JsValueRef* args,
      unsigned short arg_count,
      void* state)
    {
      return reinterpret_cast<F>(state)(callee, args, arg_count, construct);
    }

  };

  struct Engine {
    JsRuntimeHandle _runtime;
    std::list<JsValueRef> _task_queue;

    inline static Engine* instance = nullptr;

    Engine() {
      if (Engine::instance) {
        throw EngineError {};
      }
      if (JsCreateRuntime(JsRuntimeAttributeNone, nullptr, &_runtime) != JsNoError) {
        throw EngineError {};
      }
      Engine::instance = this;
    }

    Engine(const Engine& other) = delete;
    Engine& operator=(const Engine& other) = delete;

    Engine(Engine&& other) {
      _runtime = other._runtime;
      _task_queue = std::move(other._task_queue);
      other._runtime = JS_INVALID_RUNTIME_HANDLE;
    }

    Engine& operator=(Engine&& other) {
      if (this != &other) {
        _runtime = other._runtime;
        _task_queue = std::move(other._task_queue);
        other._runtime = JS_INVALID_RUNTIME_HANDLE;
      }
      return *this;
    }

    ~Engine() {
      if (_runtime != JS_INVALID_RUNTIME_HANDLE) {
        JsSetCurrentContext(nullptr);
        JsDisposeRuntime(_runtime);
        Engine::instance = nullptr;
        _runtime = JS_INVALID_RUNTIME_HANDLE;
      }
    }

    Realm create_realm() {
      Realm realm {_runtime};

      realm.enter([](auto api) {
        // Initialize promise callbacks
        JsSetPromiseContinuationCallback(enqueue_promise_callback, nullptr);
        JsSetHostPromiseRejectionTracker(rejection_tracker_callback, nullptr);

        // Initialize module callbacks
        JsModuleRecord module_record;
        JsInitializeModuleRecord(nullptr, nullptr, &module_record);

        JsSetModuleHostInfo(
          module_record,
          JsModuleHostInfo_FetchImportedModuleCallback,
          import_fetch_callback);

        JsSetModuleHostInfo(
          module_record,
          JsModuleHostInfo_FetchImportedModuleFromScriptCallback,
          dynamic_import_fetch_callback);

        JsSetModuleHostInfo(
          module_record,
          JsModuleHostInfo_NotifyModuleReadyCallback,
          module_ready_callback);

        JsSetModuleHostInfo(
          module_record,
          JsModuleHostInfo_InitializeImportMetaCallback,
          initialize_import_meta_callback);

        return nullptr;
      });

      return std::move(realm);
    }

    void flush_task_queue() {
      while (_task_queue.size() > 0) {
        JsValueRef front = _task_queue.front();
        _task_queue.pop_front();
        if (auto realm = Realm::from_object(front)) {
          realm->enter([=](auto api) {
            return api.call_function(front, {api.undefined()});
          });
        }
      }
    }

    static void CHAKRA_CALLBACK enqueue_promise_callback(
      JsValueRef task,
      void* state)
    {
      Engine::instance->_task_queue.push_back(task);
    }

    static void CHAKRA_CALLBACK rejection_tracker_callback(
      JsValueRef promise,
      JsValueRef reason,
      bool handled,
      void* state)
    {
      // TODO
    }

    static JsErrorCode CHAKRA_CALLBACK import_fetch_callback(
      JsModuleRecord importer,
      JsValueRef specifier,
      JsModuleRecord* module)
    {
      Realm::current()->enter([=](auto api) {
        auto result = api.resolve_module(importer, specifier);
        *module = result.module_record;
        if (result.is_new) {
          // TODO: enqueue a task to fetch the module and eventually call
          // JsParseModuleSource
        }
        return nullptr;
      });
      return JsNoError;
    }

    static JsErrorCode CHAKRA_CALLBACK dynamic_import_fetch_callback(
      JsSourceContext script_id,
      JsValueRef specifier,
      JsModuleRecord* module)
    {
      Realm::current()->enter([=](auto api) {
        auto result = api.resolve_module_from_script(script_id, specifier);
        *module = result.module_record;
        if (result.is_new) {
          // TODO: enqueue a task to fetch the module and eventually call
          // JsParseModuleSource
        }
        return nullptr;
      });
      return JsNoError;
    }

    static JsErrorCode CHAKRA_CALLBACK module_ready_callback(
      JsModuleRecord module,
      JsValueRef exception)
    {
      // Enqueue a task to perform JsModuleEvaluation
      return JsNoError;
    }

    static JsErrorCode CHAKRA_CALLBACK initialize_import_meta_callback(
      JsModuleRecord module,
      JsValueRef import_meta)
    {
      // TODO
      return JsNoError;
    }

  };

  Engine create_engine() {
    return Engine {};
  }

  Realm* current_realm() {
    return Realm::current();
  }

}
