#include "js_engine.h"

namespace js {

  Realm::Realm(JsContextRef context, std::shared_ptr<JobQueue>& job_queue) {
    _context = context;
    _info.job_queue = job_queue;

    JsSetContextData(_context, this);

    this->enter([&](auto api) {
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

      return nullptr;
    });
  }

  void Engine::flush_job_queue() {
    std::list<Var> rejections;
    std::map<Var, Var> rejection_reasons;

    while (!_job_queue->empty()) {
      Job job = std::move(_job_queue->dequeue());
      auto func = job.func();
      assert(func);
      Realm::from_object(func)->enter([&](auto api) {
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
        return nullptr;
      });
    }

    if (!rejection_reasons.empty()) {
      for (auto promise : rejections) {
        if (
          auto it = rejection_reasons.find(promise);
          it != rejection_reasons.end())
        {
          auto reason = it->second;
          Realm::from_object(promise)->enter([=](auto api) {
            api.throw_exception(reason);
            return nullptr;
          });
        }
      }
    }

  }

  Engine create_engine() {
    return Engine {};
  }

  Realm* current_realm() {
    return Realm::current();
  }

}
