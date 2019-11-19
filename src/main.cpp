#include "common.h"
#include "js_engine.h"
#include "sys_object.h"
#include "event_loop.h"
#include "main.js.h"

template<typename T>
void print_error(T& out, js::RealmAPI& api) {
  auto info = api.pop_exception_info();
  auto exception = api.get_property(info, "exception");
  auto stack_string = api.get_property(exception, "stack");
  auto url_string = api.get_property(info, "url");
  auto line = api.get_property(info, "line");
  auto column = api.get_property(info, "column");
  auto source = api.get_property(info, "source");

  if (stack_string == api.undefined()) {
    stack_string = api.to_string(exception);
  }

  out << api.utf8_string(stack_string) << "\n";

  // TODO: [CC] Testing for api.undefined doesn't work. Why?
  auto source_utf8 = api.utf8_string(source);
  if (source_utf8 != "undefined") {
    out
      << "\n[" << api.utf8_string(url_string)
      << ":" << api.utf8_string(line)
      << ":" << api.utf8_string(column) << "]\n"
      << source_utf8 << "\n";

    int col = api.to_integer(column);
    for (int i = 0; i < col; ++i) {
      out << " ";
    }

    out << "^\n";
  }
}

int main(int arg_count, char** args) {
  js::Engine engine;
  js::Realm realm = engine.create_realm();
  int error_code = 0;

  realm.enter([&](auto& api) {

    try {

      auto sys = sys_object::create(api, arg_count, args);
      auto source = api.create_string(main_js);
      auto result = api.eval(source, "zoe:main");
      auto callbacks = api.call_function(result, {api.undefined(), sys});

      auto load_module = api.get_property(callbacks, "loadModule");
      api.set_module_load_callback(load_module);

      auto main_func = api.get_property(callbacks, "main");
      auto main_result = api.call_function(main_func, {api.undefined()});

      event_loop::run();

    } catch (const js::ScriptError&) {

      error_code = 1;
      print_error(std::cout, api);

    }

  });

  // TODO: Unique error codes?
  return error_code;
}
