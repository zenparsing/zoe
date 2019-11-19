#include <utility>
#include <string>
#include <climits>
#include <cstdlib>

#include "os.h"

namespace os {

#ifdef _WIN32
  constexpr unsigned PATH_MAX_BYTES = _MAX_PATH * 4;
#else
  constexpr unsigned PATH_MAX_BYTES = PATH_MAX;
#endif

  inline Error error_from_uv_result(int code) {
    assert(code < 0);
    return Error {
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
    int fd;
  };

  // TODO: Can we do using DirectoryHandle = uv_dir_t?
  struct DirectoryHandle {
    uv_dir_t* dir;

    DirectoryHandle(uv_dir_t* dir) : dir {dir} {}
  };

  template<typename Traits>
  struct FsReq {
    uv_fs_t req;
    void* data; // TODO: I think we can just store this on req
    OnOpenDirectory on_success;
    OnError on_error;

    FsReq(void* data, OnOpenDirectory on_success, OnError on_error) :
      data {data},
      on_success {on_success},
      on_error {on_error}
    {}

    static uv_fs_t* create_req(
      void* data,
      OnOpenDirectory on_success,
      OnError on_error)
    {
      FsReq* instance = new FsReq(data, on_success, on_error);
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
        instance->on_error(error_from_uv_result(code), instance->data);
      } else {
        instance->on_success(Traits::map(req), instance->data);
      }
    }
  };

  void open_directory(
    const std::string& path,
    void* data,
    OnOpenDirectory on_success,
    OnError on_error)
  {
    struct CallbackTraits {
      static DirectoryHandle* map(uv_fs_t* req) {
        return new DirectoryHandle(reinterpret_cast<uv_dir_t*>(req->ptr));
      }
    };

    uv_fs_opendir(
      uv_default_loop(),
      FsReq<CallbackTraits>::create_req(data, on_success, on_error),
      path.c_str(),
      FsReq<CallbackTraits>::callback);
  }

}
