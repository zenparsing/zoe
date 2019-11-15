#include "js_engine.h"

namespace {
  using namespace js;

  void CHAKRA_CALLBACK enqueue_promise_callback(
    Var func,
    void* state)
  {
    Realm::enter_current([=](auto& api) {
      api.enqueue_job(Job {JobType::call, func});
    });
  }

  void CHAKRA_CALLBACK rejection_tracker_callback(
    Var promise,
    Var reason,
    bool handled,
    void* state)
  {
    Realm::enter_current([=](auto& api) {
      JobType job_type = handled
        ? JobType::remove_unhandled_rejection
        : JobType::add_unhandled_rejection;
      api.enqueue_job(Job {job_type, promise, {reason}});
    });
  }

  JsErrorCode CHAKRA_CALLBACK import_fetch_callback(
    JsModuleRecord importer,
    Var specifier,
    JsModuleRecord* module)
  {
    *module = Realm::enter_current([=](auto& api) {
      return api.resolve_module(importer, specifier);
    });
    return JsNoError;
  }

  JsErrorCode CHAKRA_CALLBACK dynamic_import_fetch_callback(
    JsSourceContext script_id,
    Var specifier,
    JsModuleRecord* module)
  {
    *module = Realm::enter_current([=](auto& api) {
      return api.resolve_module_from_script(script_id, specifier);
    });
    return JsNoError;
  }

  JsErrorCode CHAKRA_CALLBACK module_ready_callback(
    JsModuleRecord module,
    Var exception)
  {
    Realm::enter_current([=](auto& api) {
      api.enqueue_job(Job {
        JobType::evaluate_module,
        api.undefined(),
        {module}
      });
    });
    return JsNoError;
  }

  JsErrorCode CHAKRA_CALLBACK initialize_import_meta_callback(
    JsModuleRecord module,
    Var meta_object)
  {
    Realm::enter_current([=](auto& api) {
      api.initialize_import_meta(module, meta_object);
    });
    return JsNoError;
  }

  struct SetModuleSourceFunc {
    inline static std::string name = "setModuleSource";
    static Var construct(RealmAPI& api, CallArgs& args, Var data) {
      return nullptr;
    }
    static Var call(RealmAPI& api, CallArgs& args, Var data) {
      api.set_module_source(data, args[1], args[2]);
      return api.undefined();
    }
  };
}

namespace js {

  Realm::Realm(JsContextRef context, std::shared_ptr<JobQueue>& job_queue) {
    _context = context;
    _info.job_queue = job_queue;

    JsSetContextData(_context, this);

    this->enter([&](auto& api) {
      // Initialize promise callbacks
      JsSetPromiseContinuationCallback(enqueue_promise_callback, nullptr);
      JsSetHostPromiseRejectionTracker(rejection_tracker_callback, nullptr);

      // TODO: [CC] This method of setting module callbacks is pretty odd

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
    });
  }

  JsModuleRecord RealmAPI::resolve_module_specifier(
    Var specifier,
    const URLInfo* base_url,
    JsModuleRecord importer)
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
    _realm_info.module_map[url] = VarRef {module};
    _realm_info.module_info[module].url = std::move(url_info);

    // Create a finisher callback
    Var fn = create_function<SetModuleSourceFunc>(module);

    // Enqueue a job to call the module load callback
    enqueue_job(Job {
      JobType::call,
      get_module_load_callback(),
      {undefined(), url_string, fn}
    });

    return module;
  }

  void RealmAPI::set_module_source(Var module, Var error, Var source) {
    ModuleInfo* info = find_module_info(module);
    assert(info);
    if (info->state != ModuleState::loading) {
      // TODO: Throw (module not loading)
    }

    if (is_null_or_undefined(error)) {
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

  void RealmAPI::parse_module(Var module) {
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
      _realm_info.next_script_id++,
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

  void RealmAPI::evaluate_module(Var module) {
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

  void Engine::flush_job_queue() {
    std::list<Var> rejections;
    std::map<Var, Var> rejection_reasons;

    while (!_job_queue->empty()) {
      Job job = std::move(_job_queue->dequeue());
      auto func = job.func();
      assert(func);
      Realm::from_object(func)->enter([&](auto& api) {
        switch (job.type()) {
          case JobType::call: {
            if (job.args().empty()) {
              api.call_function(func, {api.undefined()});
            } else {
              api.call_function(func, job.args());
            }
            break;
          }

          case JobType::parse_module: {
            auto module = job.args()[0];
            api.parse_module(module);
            break;
          }

          case JobType::evaluate_module: {
            auto module = job.args()[0];
            api.evaluate_module(module);
            break;
          }

          case JobType::add_unhandled_rejection: {
            rejections.push_back(func);
            rejection_reasons[func] = job.args()[0];
            break;
          }

          case JobType::remove_unhandled_rejection: {
            rejection_reasons.erase(func);
            break;
          }
        }
      });
    }

    if (!rejection_reasons.empty()) {
      for (auto promise : rejections) {
        if (
          auto it = rejection_reasons.find(promise);
          it != rejection_reasons.end())
        {
          auto reason = it->second;
          Realm::from_object(promise)->enter([=](auto& api) {
            api.throw_exception(reason);
          });
        }
      }
    }
  }

}
