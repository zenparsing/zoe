#include "common.h"
#include "os.h"
#include "url.h"
#include "sys_object.h"
#include "event_loop.h"

namespace {

  using js::Var;
  using js::VarRef;
  using js::RealmAPI;
  using js::CallArgs;
  using js::NativeFunc;

  std::string url_to_file_path(const std::string& url) {
    // TODO: Throw if url is not a file URL?
    return URLInfo::to_file_path(URLInfo::parse(url));
  }

  struct StdOutFunc : public NativeFunc {
    inline static std::string name = "stdout";
    static Var call(RealmAPI& api, CallArgs& args) {
      // TODO: Handle buffer types
      for (unsigned i = 1; i < args.count; ++i) {
        std::cout << api.utf8_string(args[i]);
      }
      return api.undefined();
    }
  };

  struct ResolveFilePathFunc : public NativeFunc {
    inline static std::string name = "resolveFilePath";
    static Var call(RealmAPI& api, CallArgs& args) {
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
    static Var call(RealmAPI& api, CallArgs& args) {
      auto url_string = api.utf8_string(args[1]);
      auto path = url_to_file_path(url_string);
      auto content = os::read_text_file_sync(path);
      return api.create_string(content);
    }
  };

  struct CwdFunc : public NativeFunc {
    inline static std::string name = "cwd";
    static Var call(RealmAPI& api, CallArgs& args) {
      auto url_info = URLInfo::from_file_path(os::cwd() + "/");
      return api.create_string(URLInfo::stringify(url_info));
    }
  };

  struct OpenDirectoryFunc : public NativeFunc {
    inline static std::string name = "openDirectory";
    static Var call(RealmAPI& api, CallArgs& args) {
      auto url_string = api.utf8_string(args[1]);
      auto path = url_to_file_path(url_string);
      auto callback = VarRef {args[2]};
      // TODO: This results in a bunch of unnecessary VarRef copies.
      // We really only need to have one VarRef. Also, the only
      // interesting thing we're going to do here is map the result
      // object (DirectoryHandle here) into a JS object. The common
      // stuff should be factored out. And maybe after that we don't
      // really need the template lambda callbacks after all.
      os::open_directory(path, [=](os::DirectoryHandle dir) {
        js::Realm::from_object(callback.var())->enter([&](auto& api) {
          // TODO: Wrap the result
          event_loop::dispatch_event(callback.var(), api.create_object());
        });
      }, [=](HostError& error) {
        js::Realm::from_object(callback.var())->enter([&](auto& api) {
          // TODO: Wrap the error
          event_loop::dispatch_event(callback.var(), api.create_object());
        });
      });
      return nullptr;
    }
  };

  struct ReadDirectoryFunc : public NativeFunc {
    inline static std::string name = "readDirectory";
    static Var call(RealmAPI& api, CallArgs& args) {
      return nullptr;
    }
  };

  struct CloseDirectoryFunc : public NativeFunc {
    inline static std::string name = "closeDirectory";
    static Var call(RealmAPI& api, CallArgs& args) {
      return nullptr;
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
  builder.add_method<OpenDirectoryFunc>();
  builder.add_method<ReadDirectoryFunc>();
  builder.add_method<CloseDirectoryFunc>();

  return builder.object();
}
