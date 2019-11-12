#pragma once

#include "js_engine.h"

namespace sys_object {
  js::Var create(js::RealmAPI& api, int arg_count, char** args);
}
