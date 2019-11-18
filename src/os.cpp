#include <utility>
#include <string>
#include <climits>
#include <cstdlib>

#include "uv.h"

#include "os.h"

namespace os {

#ifdef _WIN32
  constexpr unsigned PATH_MAX_BYTES = _MAX_PATH * 4;
#else
  constexpr unsigned PATH_MAX_BYTES = PATH_MAX;
#endif

  HostError error_from_uv_result(int code) {
    assert(code < 0);
    return HostError {
      HostErrorKind::libuv,
      uv_strerror(code),
      uv_err_name(code),
    };
  }

  void _check_uv(int code) {
    if (code < 0) {
      throw error_from_uv_result(code);
    }
  }

  std::string cwd() {
    char buffer[PATH_MAX_BYTES];
    size_t cwd_len = sizeof(buffer);
    _check_uv(uv_cwd(buffer, &cwd_len));
    return {buffer};
  }

  std::string read_text_file_sync(const std::string& path) {
    uv_fs_t req;

    // Open file
    uv_file file = uv_fs_open(nullptr, &req, path.c_str(), O_RDONLY, 0, nullptr);
    uv_fs_req_cleanup(&req);
    _check_uv(file);

    char buffer_memory[4096];
    uv_buf_t buffer = uv_buf_init(buffer_memory, sizeof(buffer_memory));
    std::string str;

    int bytes;
    while (true) {
      // Read into buffer
      bytes = uv_fs_read(nullptr, &req, file, &buffer, 1, str.length(), nullptr);
      uv_fs_req_cleanup(&req);
      if (bytes > 0) {
        str.append(buffer.base, bytes);
      } else {
        break;
      }
    }

    _check_uv(bytes);

    // Close file
    uv_fs_close(nullptr, &req, file, nullptr);
    uv_fs_req_cleanup(&req);

    return str;
  }

  struct FileHandle {
    int _fd;
  };

  struct DirectoryHandle {
    uv_dir_t* _dir;
  };

  template<typename F, typename G, typename Traits>
  struct FsReq {
    uv_fs_t req;
    F on_success;
    G on_error;

    FsReq(F&& on_success, F&& on_error) :
      on_success {std::move(on_success)},
      on_error {std::move(on_error)}
    {}

    static uv_fs_t* create_req(F&& on_success, F&& on_error) {
      auto instance = new FsReq {
        std::move(on_success),
        std::move(on_error),
      };
      return instance->req;
    }

    static void callback(uv_fs_t* req) {
      static_assert(offsetof(struct FsReq, req) == 0);

      auto* instance = reinterpret_cast<FsReq*>(req);
      auto cleanup = on_scope_exit([=]() {
        uv_fs_req_cleanup(req);
        delete instance;
      });

      if (req->result < 0) {
        instance->on_error(error_from_uv_result(req->result));
        return;
      }

      instance->on_success(Traits::map(req));
    }
  };

  template<typename F, typename G>
  void open_directory(const std::string& path, F&& on_success, G&& on_error) {
    struct CallbackTraits {
      DirectoryHandle map(uv_fs_t* req) {
        return DirectoryHandle {reinterpret_cast<uv_dir_t*>(req->ptr)};
      }
    };

    using R = FsReq<F, G, CallbackTraits>;
    auto* req = R::create_req(std::move(on_success), std::move(on_error));
    uv_fs_opendir(uv_default_loop(), req, path.c_str(), R::callback);
  }

}
