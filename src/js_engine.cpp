#include "js_engine.h"

using namespace js;

Engine create_engine() {
  return Engine {};
}

Realm* current_realm() {
  return Realm::current();
}
