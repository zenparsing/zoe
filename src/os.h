#pragma once

#include "common.h"

namespace os {

#ifdef _WIN32
  constexpr unsigned PATH_MAX_BYTES = _MAX_PATH * 4;
#else
  constexpr unsigned PATH_MAX_BYTES = PATH_MAX;
#endif

  inline HostError error_from_uv_result(int code) {
    assert(code < 0);
    return HostError {
      HostErrorKind::libuv,
      uv_strerror(code),
      uv_err_name(code),
    };
  }

  struct FileHandle {
    int _fd;
  };

  struct DirectoryHandle {
    uv_dir_t* _dir;
  };

  // Returns the current working directory
  std::string cwd();

  // Synchronously reads a text file into a string
  std::string read_text_file_sync(const std::string& path);

  template<typename F, typename G, typename Traits>
  struct FsReq {
    uv_fs_t req;
    F on_success;
    G on_error;

    FsReq(F& on_success, G& on_error) :
      on_success {on_success},
      on_error {on_error}
    {}

    static uv_fs_t* create_req(F& on_success, G& on_error) {
      FsReq* instance = new FsReq(on_success, on_error);
      return &instance->req;
    }

    static void callback(uv_fs_t* req) {
      static_assert(offsetof(struct FsReq, req) == 0);

      auto* instance = reinterpret_cast<FsReq*>(req);
      auto cleanup = on_scope_exit([=]() {
        uv_fs_req_cleanup(req);
        delete instance;
      });

      if (req->result < 0) {
        int code = static_cast<int>(req->result);
        instance->on_error(error_from_uv_result(code));
      } else {
        instance->on_success(Traits::map(req));
      }
    }
  };

  // Opens a directory
  template<typename F, typename G>
  void open_directory(const std::string& path, F& on_success, G& on_error) {
    struct CallbackTraits {
      static DirectoryHandle map(uv_fs_t* req) {
        return DirectoryHandle {reinterpret_cast<uv_dir_t*>(req->ptr)};
      }
    };

    using R = FsReq<F, G, CallbackTraits>;
    auto* req = R::create_req(on_success, on_error);
    uv_fs_opendir(uv_default_loop(), req, path.c_str(), R::callback);
  }

}
