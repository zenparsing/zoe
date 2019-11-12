#include <iostream>
#include "event_loop.h"
#include "js_engine.h"
#include "sys_object.h"
#include "main.js.h"

int main(int arg_count, char** args) {
  js::Engine engine;

  auto realm = engine.create_realm();

  realm.enter([=](auto api) {
    auto sys = sys_object::create(api, arg_count, args);
    auto source = api.create_string(main_js);
    auto result = api.eval(source, "zoe:main");
    auto callbacks = api.call_function(result, {api.undefined(), sys});

    auto load_module = api.get_property(callbacks, "loadModule");
    api.set_module_load_callback(load_module);

    auto main_func = api.get_property(callbacks, "main");
    return api.call_function(main_func, {api.undefined()});
  });

  engine.flush_job_queue();

  return 0;
}
