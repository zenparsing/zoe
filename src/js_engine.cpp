#include "js_engine.h"

namespace {
  using namespace js;

  void CHAKRA_CALLBACK enqueue_promise_callback(
    Var func,
    void* state)
  {
    enter_current_realm([=](auto& api) {
      api.enqueue_job(Job {JobKind::call, func});
    });
  }

  void CHAKRA_CALLBACK rejection_tracker_callback(
    Var promise,
    Var reason,
    bool handled,
    void* state)
  {
    enter_current_realm([=](auto& api) {
      JobKind job_type = handled
        ? JobKind::remove_unhandled_rejection
        : JobKind::add_unhandled_rejection;
      api.enqueue_job(Job {job_type, promise, {reason}});
    });
  }

  JsErrorCode CHAKRA_CALLBACK import_fetch_callback(
    JsModuleRecord importer,
    Var specifier,
    JsModuleRecord* module)
  {
    *module = enter_current_realm([=](auto& api) {
      return api.resolve_module(importer, specifier);
    });
    return JsNoError;
  }

  JsErrorCode CHAKRA_CALLBACK dynamic_import_fetch_callback(
    JsSourceContext script_id,
    Var specifier,
    JsModuleRecord* module)
  {
    *module = enter_current_realm([=](auto& api) {
      return api.resolve_module_from_script(script_id, specifier);
    });
    return JsNoError;
  }

  JsErrorCode CHAKRA_CALLBACK module_ready_callback(
    JsModuleRecord module,
    Var exception)
  {
    enter_current_realm([=](auto& api) {
      api.enqueue_job(Job {
        JobKind::evaluate_module,
        api.undefined(),
        {module, exception},
      });
    });
    return JsNoError;
  }

  JsErrorCode CHAKRA_CALLBACK initialize_import_meta_callback(
    JsModuleRecord module,
    Var meta_object)
  {
    enter_current_realm([=](auto& api) {
      api.initialize_import_meta(module, meta_object);
    });
    return JsNoError;
  }

  struct SetModuleSourceFunc : public NativeFunc {
    inline static std::string name = "setModuleSource";
    static Var call(RealmAPI& api, CallArgs& args) {
      api.set_module_source(args.state, args[1], args[2]);
      return nullptr;
    }
  };
}

namespace js {

  EngineError::EngineError(JsErrorCode code) : code {code} {
    #define CASE_ERR(n, msg) case n: this->message = msg; break;
    switch (code) {
      // Usage
      CASE_ERR(JsErrorInvalidArgument, "invalid argument")
      CASE_ERR(JsErrorNullArgument, "null argument")
      CASE_ERR(JsErrorNoCurrentContext, "no current context")
      CASE_ERR(JsErrorInExceptionState, "in exception state")
      CASE_ERR(JsErrorNotImplemented, "not implemented")
      CASE_ERR(JsErrorWrongThread, "wrong thread")
      CASE_ERR(JsErrorRuntimeInUse, "runtime in use")
      CASE_ERR(JsErrorBadSerializedScript, "bad serialized script")
      CASE_ERR(JsErrorInDisabledState, "in disabled state")
      CASE_ERR(JsErrorCannotDisableExecution, "cannot disable execution")
      CASE_ERR(JsErrorHeapEnumInProgress, "heap enum in progress")
      CASE_ERR(JsErrorArgumentNotObject, "argument not object")
      CASE_ERR(JsErrorInProfileCallback, "error in profile callback")
      CASE_ERR(JsErrorInThreadServiceCallback, "cannot serialize debug script")
      CASE_ERR(JsErrorCannotSerializeDebugScript, "cannot serialize debug script")
      CASE_ERR(JsErrorAlreadyDebuggingContext, "already debugging context")
      CASE_ERR(JsErrorAlreadyProfilingContext, "already profiling context")
      CASE_ERR(JsErrorIdleNotEnabled, "idle not enabled")
      CASE_ERR(JsCannotSetProjectionEnqueueCallback, "cannot set projection enqueue callback")
      CASE_ERR(JsErrorCannotStartProjection, "cannot start projection")
      CASE_ERR(JsErrorInObjectBeforeCollectCallback, "not supported in before collect callback")
      CASE_ERR(JsErrorObjectNotInspectable, "object not inspectable")
      CASE_ERR(JsErrorPropertyNotSymbol, "property is not a symbol")
      CASE_ERR(JsErrorPropertyNotString, "property is not a string")
      CASE_ERR(JsErrorInvalidContext, "invalid context")
      CASE_ERR(JsInvalidModuleHostInfoKind, "invalid ModuleHostInfoKind")
      CASE_ERR(JsErrorModuleParsed, "module already parsed")
      CASE_ERR(JsNoWeakRefRequired, "object not garbage collected")
      CASE_ERR(JsErrorPromisePending, "promise pending")
      CASE_ERR(JsErrorModuleNotEvaluated, "module has not evaluated")

      // Engine
      CASE_ERR(JsErrorOutOfMemory, "out of memory")
      CASE_ERR(JsErrorBadFPUState, "bad fpu state")

      // Script
      CASE_ERR(JsErrorScriptException, "script exception")
      CASE_ERR(JsErrorScriptCompile, "failed to compile")
      CASE_ERR(JsErrorScriptTerminated, "script terminated")
      CASE_ERR(JsErrorScriptEvalDisabled, "eval disabled")

      // Fatal
      CASE_ERR(JsErrorFatal, "fatal engine error")
      CASE_ERR(JsErrorWrongRuntime, "wrong runtime")

      // Diag
      CASE_ERR(JsErrorDiagAlreadyInDebugMode, "already in debug mode")
      CASE_ERR(JsErrorDiagNotInDebugMode, "not in debug mode")
      CASE_ERR(JsErrorDiagNotAtBreak, "not at break")
      CASE_ERR(JsErrorDiagInvalidHandle, "invalid diag handle")
      CASE_ERR(JsErrorDiagObjectNotFound, "diag object not found")
      CASE_ERR(JsErrorDiagUnableToPerformAction, "unabled to perform diag action")
      CASE_ERR(JsSerializerNotSupported, "serializer does not support data")
      CASE_ERR(JsTransferableNotSupported, "object not transferrable during serialization")
      CASE_ERR(JsTransferableAlreadyDetached, "object already detached when serialized")
      default: this->message = "??"; break;
    }
  }

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
      JobKind::call,
      get_module_load_callback(),
      {undefined(), url_string, fn},
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
      info->source = VarRef {source};
    } else {
      info->source = VarRef {empty_string()};
      JsSetModuleHostInfo(module, JsModuleHostInfo_Exception, error);
    }

    info->state = ModuleState::parsing;
    enqueue_job(Job {
      JobKind::parse_module,
      undefined(),
      {module},
    });
  }

  void RealmAPI::parse_module(Var module) {
    ModuleInfo* info = find_module_info(module);
    assert(info);
    if (info->state != ModuleState::parsing) {
      // TODO: throw error
    }

    auto source = utf8_string(info->source.var());
    info->source.release();

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

  void RealmAPI::evaluate_module(Var module, Var error) {
    if (error) {
      throw_exception(error);
    }

    ModuleInfo* info = find_module_info(module);
    assert(info);
    if (info->state != ModuleState::initializing) {
      // TODO: throw error
    }

    JsModuleEvaluation(module, nullptr);

    bool errored = has_exception();
    if (errored) {
      info->state = ModuleState::error;
      throw ScriptError {};
    }

    info->state = ModuleState::complete;
  }

  void JobQueue::flush() {
    // TODO: Make this non-reentrant?
    std::list<Var> rejections;
    std::map<Var, Var> rejection_reasons;

    while (!this->empty()) {
      Job job = std::move(this->dequeue());
      auto func = job.func();
      assert(func);
      enter_object_realm(func, [&](auto& api) {
        switch (job.kind()) {
          case JobKind::call: {
            if (job.args().empty()) {
              api.call_function(func, {api.undefined()});
            } else {
              api.call_function(func, job.args());
            }
            break;
          }

          case JobKind::parse_module: {
            auto module = job.args()[0];
            api.parse_module(module);
            break;
          }

          case JobKind::evaluate_module: {
            auto module = job.args()[0];
            auto error = job.args()[1];
            api.evaluate_module(module, error);
            break;
          }

          case JobKind::add_unhandled_rejection: {
            rejections.push_back(func);
            rejection_reasons[func] = job.args()[0];
            break;
          }

          case JobKind::remove_unhandled_rejection: {
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
          enter_object_realm(promise, [=](auto& api) {
            api.throw_exception(reason);
          });
        }
      }
    }
  }

}
