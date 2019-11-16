#include <climits>

#include "common.h"
#include "sys_object.h"
#include "url.h"
#include "fs.h"

namespace {

  using js::Var;
  using js::RealmAPI;
  using js::CallArgs;
  using js::NativeFunc;

  struct StdOutFunc : public NativeFunc {
    inline static std::string name = "stdout";
    static Var call(RealmAPI& api, CallArgs& args, Var data) {
      for (unsigned i = 1; i < args.count; ++i) {
        std::cout << api.utf8_string(args[i]);
      }
      return api.undefined();
    }
  };

  struct ResolveFilePathFunc : public NativeFunc {
    inline static std::string name = "resolveFilePath";
    static Var call(RealmAPI& api, CallArgs& args, Var data) {
      auto path = api.utf8_string(args[1]);
      auto base = api.utf8_string(args[2]);

      // TODO: throw if URL parsing fails
      auto base_url = URLInfo::parse(base);
      auto info = URLInfo::from_file_path(path, &base_url);
      return api.create_string(URLInfo::stringify(info));
    }
  };

  struct ReadTextFileSyncFunc : public NativeFunc {
    inline static std::string name = "readTextFileSync";
    static Var call(RealmAPI& api, CallArgs& args, Var data) {
      auto url_string = api.utf8_string(args[1]);
      URLInfo url = URLInfo::parse(url_string);
      auto path = URLInfo::to_file_path(url);
      auto content = fs::read_text_file_sync(path);
      return api.create_string(content);
    }
  };

  struct CwdFunc : public NativeFunc {
    inline static std::string name = "cwd";
    static Var call(RealmAPI& api, CallArgs& args, Var data) {
      auto url_info = URLInfo::from_file_path(fs::cwd() + "/");
      return api.create_string(URLInfo::stringify(url_info));
    }
  };

  struct ObjectBuilder {
    RealmAPI& _api;
    Var _object;

    ObjectBuilder(RealmAPI& api) : _api {api} {
      _object = _api.create_object();
    }

    Var object() { return _object; }

    template<typename T>
    void add_method() {
      auto fn = _api.create_function<T>();
      _api.set_property(_object, T::name, fn);
    }

    void add_property(const std::string& name, Var value) {
      _api.set_property(_object, name, value);
    }
  };

  Var create_args(RealmAPI& api, int arg_count, char** args) {
    auto args_array = api.create_array(arg_count);
    for (int i = 0; i < arg_count; ++i) {
      auto str = api.create_string(args[i]);
      api.set_indexed_property(args_array, i, str);
    }
    return args_array;
  }

}

Var sys_object::create(RealmAPI& api, int arg_count, char** args) {
  ObjectBuilder builder {api};

  builder.add_property("args", create_args(api, arg_count, args));
  builder.add_property("global", api.global_object());
  builder.add_method<StdOutFunc>();
  builder.add_method<ResolveFilePathFunc>();
  builder.add_method<CwdFunc>();
  builder.add_method<ReadTextFileSyncFunc>();

  return builder.object();
}
