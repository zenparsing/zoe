#include "event_loop.h"

namespace event_loop {

  using js::Var;

  void dispatch_event(Var callback, Var result) {
    // TODO: Handle thrown errors
    js::enter_object_realm(callback, [&](auto& api) {
      api.enqueue_job(callback, {
        api.undefined(),
        api.undefined(),
        result,
      });
      api.flush_job_queue();
    });
  }

  void dispatch_error(Var callback, Var error) {
    // TODO: Handle thrown errors
    js::enter_object_realm(callback, [&](auto& api) {
      api.enqueue_job(callback, {
        api.undefined(),
        error,
      });
      api.flush_job_queue();
    });
  }

  void run() {
    js::enter_current_realm([](auto& api) {
      api.flush_job_queue();
    });

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  }

}
