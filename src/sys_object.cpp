#include <iostream>

#include "sys_object.h"
#include "url.h"
#include "fs.h"

namespace {

  js::Var CHAKRA_CALLBACK stdout_callback(
    js::Var callee,
    bool construct,
    js::Var* args,
    unsigned short arg_count,
    js::Var data)
  {
    return js::native_call([=](auto api) {
      for (unsigned i = 1; i < arg_count; ++i) {
        std::cout << api.utf8_string(args[i]);
      }
      return api.undefined();
    });
  }

  js::Var CHAKRA_CALLBACK resolve_file_path_callback(
    js::Var callee,
    bool construct,
    js::Var* args,
    unsigned short arg_count,
    js::Var data)
  {
    return js::native_call([=](auto api) {
      if (arg_count < 3) {
        // TODO: Throw
        return api.undefined();
      }

      auto path = api.utf8_string(args[1]);
      auto base = api.utf8_string(args[2]);

      // TODO: throw if URL parsing fails
      auto base_url = URLInfo::parse(base);
      auto info = URLInfo::from_file_path(path, &base_url);
      return api.create_string(URLInfo::stringify(info));
    });
  }

  js::Var CHAKRA_CALLBACK read_text_file_sync_callback(
    js::Var callee,
    bool construct,
    js::Var* args,
    unsigned short arg_count,
    js::Var data)
  {
    return js::native_call([=](auto api) {
      if (arg_count < 2) {
        // TODO: Throw
        return api.undefined();
      }

      auto url_string = api.utf8_string(args[1]);
      URLInfo url = URLInfo::parse(url_string);
      auto path = URLInfo::to_file_path(url);
      auto content = fs::read_text_file_sync(path);
      return api.create_string(content);
    });
  }

  js::Var CHAKRA_CALLBACK cwd_callback(
    js::Var callee,
    bool construct,
    js::Var* args,
    unsigned short arg_count,
    js::Var data)
  {
    return js::native_call([=](auto api) {
      auto url_info = URLInfo::from_file_path(fs::cwd() + "/");
      return api.create_string(URLInfo::stringify(url_info));
    });
  }

}

js::Var sys_object::create(js::RealmAPI& api, int arg_count, char** args) {
  js::Var object = api.create_object();

  js::Var fn;

  fn = api.create_function("stdout", stdout_callback);
  api.set_property(object, "stdout", fn);

  fn = api.create_function("resolveFilePath", resolve_file_path_callback);
  api.set_property(object, "resolveFilePath", fn);

  fn = api.create_function("cwd", cwd_callback);
  api.set_property(object, "cwd", fn);

  fn = api.create_function("readTextFileSync", read_text_file_sync_callback);
  api.set_property(object, "readTextFileSync", fn);

  auto args_array = api.create_array(arg_count);
  for (int i = 0; i < arg_count; ++i) {
    auto str = api.create_string(args[i]);
    api.set_indexed_property(args_array, i, str);
  }
  api.set_property(object, "args", args_array);

  api.set_property(object, "global", api.global_object());

  return object;
}
