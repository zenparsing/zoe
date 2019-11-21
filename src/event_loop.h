#pragma once

#include "common.h"
#include "js_engine.h"

namespace event_loop {

  void dispatch_event(js::Var callback, js::Var result = nullptr);
  void dispatch_error(js::Var callback, js::Var error);
  void run();

}
