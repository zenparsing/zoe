#include "js_engine.h"

namespace js {

  void JobQueue::flush() {
    while (!this->empty()) {
      Job job = std::move(this->dequeue());
      Realm::from_object(job.func())->enter([&](auto api) {
        switch (job.type()) {
          case JobType::call:
            api.call_function(
              job.func(),
              job.args().empty()
                ? std::vector<Var> {api.undefined()}
                : job.args());
            break;

          case JobType::parse_module:
            api.parse_module(job.args()[0]);
            break;

          case JobType::evaluate_module:
            api.evaluate_module(job.args()[0]);
            break;
        }
        return nullptr;
      });
    }
  }

  Realm::Realm(JsRuntimeHandle runtime, JobQueue* job_queue) {
    if (JsCreateContext(runtime, &_context) != JsNoError) {
      throw EngineError {};
    }

    _info.job_queue = job_queue;
    JsSetContextData(_context, this);

    this->enter([&](auto api) {
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
  }

  Engine create_engine() {
    return Engine {};
  }

  Realm* current_realm() {
    return Realm::current();
  }

}
