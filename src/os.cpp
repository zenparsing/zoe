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

  template<typename Traits>
  struct FsReq {
    using OnSuccess = typename Traits::OnSuccess;

    uv_fs_t req;
    void* data; // TODO: I think we can just store this on req?
    OnSuccess on_success;
    OnError on_error;

    FsReq(void* data, OnSuccess on_success, OnError on_error) :
      data {data},
      on_success {on_success},
      on_error {on_error}
    {}

    static uv_fs_t* create_req(void* data, OnSuccess on_success, OnError on_error) {
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
        return;
      }

      using MapReturnType = decltype(Traits::map(std::declval<uv_fs_t*>()));

      if constexpr (std::is_void_v<MapReturnType>) {
        instance->on_success(instance->data);
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
    struct Traits {
      using OnSuccess = OnOpenDirectory;
      static DirectoryHandle map(uv_fs_t* req) {
        // TODO: We might want to have a map of directory handles
        // to uv_dir_t pointers, instead of direct pointers
        return reinterpret_cast<DirectoryHandle>(req->ptr);
      }
    };

    uv_fs_opendir(
      uv_default_loop(),
      FsReq<Traits>::create_req(data, on_success, on_error),
      path.c_str(),
      FsReq<Traits>::callback);
  }

  void close_directory(
    DirectoryHandle handle,
    void* data,
    OnCloseDirectory on_success,
    OnError on_error)
  {
    struct Traits {
      using OnSuccess = OnCloseDirectory;
      static void map(uv_fs_t*) {}
    };

    uv_fs_closedir(
      uv_default_loop(),
      FsReq<Traits>::create_req(data, on_success, on_error),
      reinterpret_cast<uv_dir_t*>(handle),
      FsReq<Traits>::callback);
  }

  void read_directory(
    DirectoryHandle handle,
    size_t count,
    void* data,
    OnReadDirectory on_success,
    OnError on_error)
  {
    struct Traits {
      using OnSuccess = OnReadDirectory;
      static std::vector<std::string> map(uv_fs_t* req) {
        auto* dir = reinterpret_cast<uv_dir_t*>(req->ptr);
        std::vector<std::string> entries;
        for (int i = 0; i < req->result; ++i) {
          entries.emplace_back(dir->dirents[i].name);
        }
        delete[] dir->dirents;
        dir->dirents = nullptr;
        dir->nentries = 0;
        return entries;
      }
    };

    auto* dir = reinterpret_cast<uv_dir_t*>(handle);
    dir->dirents = new uv_dirent_t[count];
    dir->nentries = count;

    uv_fs_readdir(
      uv_default_loop(),
      FsReq<Traits>::create_req(data, on_success, on_error),
      dir,
      FsReq<Traits>::callback);
  }

}
