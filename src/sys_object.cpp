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

  Var os_error_to_js_error(RealmAPI& api, const os::Error& error) {
    auto e = api.create_error(error.message);
    api.set_property(e, "code", api.create_string(error.code));
    return e;
  }

  Var exception_from_os_error(RealmAPI& api, const os::Error& error) {
    api.set_exception(os_error_to_js_error(api, error));
    return nullptr;
  }

  std::string url_to_file_path(const std::string& url) {
    // TODO: Throw if url is not a file URL?
    return URLInfo::to_file_path(URLInfo::parse(url));
  }

  struct OsCallback {
    static void on_success(void* data) {
      auto callback = reinterpret_cast<Var>(data);
      VarRef::decrement(callback);
      js::enter_object_realm(callback, [&](auto& api) {
        event_loop::dispatch_event(callback, api.undefined());
      });
    }

    static void on_error(const os::Error& error, void* data) {
      auto callback = reinterpret_cast<Var>(data);
      VarRef::decrement(callback);
      js::enter_object_realm(callback, [&](auto& api) {
        auto obj = os_error_to_js_error(api, error);
        event_loop::dispatch_event(callback, obj);
      });
    }
  };

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
      try {
        auto url_string = api.utf8_string(args[1]);
        auto path = url_to_file_path(url_string);
        auto content = os::read_text_file_sync(path);
        return api.create_string(content);
      } catch (const os::Error& err) {
        return exception_from_os_error(api, err);
      }
    }
  };

  struct CwdFunc : public NativeFunc {
    inline static std::string name = "cwd";
    static Var call(RealmAPI& api, CallArgs& args) {
      try {
        auto url_info = URLInfo::from_file_path(os::cwd() + "/");
        return api.create_string(URLInfo::stringify(url_info));
      } catch (const os::Error& err) {
        return exception_from_os_error(api, err);
      }
    }
  };

  enum class HostObjectKind : unsigned {
    directory_handle,
  };

  template<HostObjectKind kind_value>
  struct HostObjectInfo {
    const unsigned kind = HostObjectInfo::instance_kind;
    static constexpr unsigned instance_kind = static_cast<unsigned>(kind_value);
  };

  struct DirectoryObjectInfo :
    public HostObjectInfo<HostObjectKind::directory_handle>
  {
    os::DirectoryHandle handle;
    bool open = true;

    explicit DirectoryObjectInfo(os::DirectoryHandle handle) :
      handle {handle}
    {}

    ~DirectoryObjectInfo() {
      // TODO: maybe call os::close_directory?
    }
  };

  struct OpenDirectoryFunc : public NativeFunc {
    inline static std::string name = "openDirectory";

    struct Callback : public OsCallback {
      static void on_success(os::DirectoryHandle dir, void* data) {
        auto callback = reinterpret_cast<Var>(data);
        VarRef::decrement(callback);
        js::enter_object_realm(callback, [&](auto& api) {
          Var obj = api.create_host_object<DirectoryObjectInfo>(dir);
          event_loop::dispatch_event(callback, obj);
        });
      }
    };

    static Var call(RealmAPI& api, CallArgs& args) {
      auto url_string = api.utf8_string(args[1]);
      auto path = url_to_file_path(url_string);
      auto callback = args[2];
      VarRef::increment(callback);
      os::open_directory<Callback>(path, callback);
      return nullptr;
    }
  };

  struct ReadDirectoryFunc : public NativeFunc {
    inline static std::string name = "readDirectory";

    struct Callback : public OsCallback {
      static void on_success(std::vector<std::string>& entries, void* data) {
        auto callback = reinterpret_cast<Var>(data);
        VarRef::decrement(callback);
        js::enter_object_realm(callback, [&](auto& api) {
          Var array = api.create_array(static_cast<int>(entries.size()));
          for (int i = 0; i < entries.size(); ++i) {
            auto entry = api.create_string(entries[i]);
            api.set_indexed_property(array, i, entry);
          }
          event_loop::dispatch_event(callback, array);
        });
      }
    };

    static Var call(RealmAPI& api, CallArgs& args) {
      auto* dir = api.get_host_object_data<DirectoryObjectInfo>(args[1]);
      if (!dir || !dir->open) {
        // TODO: Throw type error
        return nullptr;
      }
      auto count = api.to_integer<size_t>(args[2]);
      auto callback = args[3];
      VarRef::increment(callback);
      os::read_directory<Callback>(dir->handle, count, callback);
      return nullptr;
    }
  };

  struct CloseDirectoryFunc : public NativeFunc {
    inline static std::string name = "closeDirectory";

    static Var call(RealmAPI& api, CallArgs& args) {
      auto* dir = api.get_host_object_data<DirectoryObjectInfo>(args[1]);
      if (!dir || !dir->open) {
        // TODO: Throw type error
        return nullptr;
      }
      dir->open = false;
      auto callback = args[2];
      VarRef::increment(callback);
      os::close_directory<OsCallback>(dir->handle, callback);
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
