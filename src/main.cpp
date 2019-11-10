#include <iostream>
#include "event_loop.h"
#include "js_engine.h"
#include "main.js.h"

namespace sys_object {

  js::Var _stdout(js::Var callee, js::Var* args, unsigned arg_count, bool construct) {
    if (auto realm = js::current_realm()) {
      realm->enter([=](auto api) {
        for (unsigned i = 1; i < arg_count; ++i) {
          std::cout << api.utf8_string(args[i]);
        }
        return nullptr;
      });
    }
    return nullptr;
  }

  js::Var create(js::RealmInterface& api, int arg_count, char** args) {
    auto object = api.create_object();

    api.set_property(object, "global", api.global_object());

    auto fn = api.create_function("stdout", _stdout);
    api.set_property(object, "stdout", fn);

    auto args_array = api.create_array(arg_count);
    for (int i = 0; i < arg_count; ++i) {
      auto str = api.create_string(args[i]);
      api.set_indexed_property(args_array, i, str);
    }
    api.set_property(object, "args", args_array);

    return object;
  }

}

int main(int arg_count, char** args) {
  js::Engine engine;

  auto realm = engine.create_realm();

  realm.enter([=](auto api) {
    auto sys = sys_object::create(api, arg_count, args);
    auto source = api.create_string(main_js);
    auto result = api.eval(source, "zoe:main");
    auto callbacks = api.call_function(result, {api.undefined(), sys});
    return nullptr;
  });

  engine.flush_task_queue();

  return 0;
}
