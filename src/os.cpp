#include <utility>
#include <string>
#include <climits>
#include <cstdlib>
#include <unordered_set>

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

  inline void _check_uv(int code) {
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

  // Timers

  struct Timer {
    uv_timer_t req;
    bool repeating;
    OnTimer on_timer;

    Timer(void* data, bool repeating, OnTimer on_timer) :
      repeating {repeating},
      on_timer {on_timer}
    {
      req.data = data;
    }

    ~Timer() {
      timer_handles.erase(reinterpret_cast<TimerHandle>(this));
    }

    inline static std::unordered_set<TimerHandle> timer_handles;

    static TimerHandle start(
      uint64_t timeout,
      uint64_t repeat,
      void* data,
      OnTimer on_timer)
    {
      bool repeating = (repeat != 0);
      auto* timer = new Timer(data, repeating, on_timer);
      uv_timer_init(uv_default_loop(), &timer->req);
      uv_timer_start(&timer->req, callback, timeout, repeat);
      auto handle = reinterpret_cast<TimerHandle>(timer);
      timer_handles.insert(handle);
      return handle;
    }

    static void stop(TimerHandle handle) {
      if (timer_handles.count(handle) == 0) {
        return;
      }
      auto* timer = reinterpret_cast<Timer*>(handle);
      uv_timer_stop(&timer->req);
      delete timer;
    }

    static void callback(uv_timer_t* req) {
      static_assert(offsetof(struct Timer, req) == 0);
      auto* instance = reinterpret_cast<Timer*>(req);
      instance->on_timer(instance->req.data);
      if (!instance->repeating) {
        delete instance;
      }
    }
  };

  TimerHandle start_timer(
    uint64_t timeout,
    uint64_t repeat,
    void* data,
    OnTimer on_timer)
  {
    return Timer::start(timeout, repeat, data, on_timer);
  }

  void stop_timer(TimerHandle handle) {
    Timer::stop(handle);
  }

  void enqueue_error_callback(
    const Error& error,
    void* data,
    OnError on_error)
  {
    struct TimerInfo {
      uv_timer_t timer;
      Error error;
      void* data;
      OnError on_error;

      TimerInfo(const Error& error, void* data, OnError on_error) :
        error {error},
        data {data},
        on_error {on_error}
      {}

      static void callback(uv_timer_t* timer) {
        static_assert(offsetof(struct TimerInfo, timer) == 0);
        auto* info = reinterpret_cast<TimerInfo*>(timer);
        auto cleanup = on_scope_exit([=]() { delete info; });
        info->on_error(info->error, info->data);
      }
    };

    auto* info = new TimerInfo(error, data, on_error);
    uv_timer_init(uv_default_loop(), &info->timer);
    uv_timer_start(&info->timer, TimerInfo::callback, 0, 0);
  }

  // File system

  std::string read_text_file_sync(const std::string& path) {
    uv_fs_t req;

    // Open file
    uv_file file = uv_fs_open(nullptr, &req, path.c_str(), UV_FS_O_RDONLY, 0, nullptr);
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

  // Directory access

  std::unordered_set<DirectoryHandle> directory_handles;

  void open_directory(
    const std::string& path,
    void* data,
    OnOpenDirectory on_success,
    OnError on_error)
  {
    struct Traits {
      using OnSuccess = OnOpenDirectory;
      static DirectoryHandle map(uv_fs_t* req) {
        auto* dir = reinterpret_cast<uv_dir_t*>(req->ptr);
        dir->dirents = nullptr;
        dir->nentries = 0;
        auto handle = reinterpret_cast<DirectoryHandle>(dir);
        directory_handles.insert(handle);
        return handle;
      }
    };

    uv_fs_opendir(
      uv_default_loop(),
      FsReq<Traits>::create_req(data, on_success, on_error),
      path.c_str(),
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
        // TODO: This should be safe to run twice...
        uv_fs_req_cleanup(req);
        delete[] dir->dirents;
        dir->dirents = nullptr;
        dir->nentries = 0;
        return entries;
      }
    };

    if (directory_handles.count(handle) == 0) {
      return enqueue_error_callback(
        Error {"not an open directory"},
        data,
        on_error);
    }

    auto* dir = reinterpret_cast<uv_dir_t*>(handle);
    if (dir->dirents) {
      return enqueue_error_callback(
        Error {"read_directory in progress"},
        data,
        on_error);
    }

    dir->dirents = new uv_dirent_t[count];
    dir->nentries = count;

    uv_fs_readdir(
      uv_default_loop(),
      FsReq<Traits>::create_req(data, on_success, on_error),
      dir,
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

    auto iter = directory_handles.find(handle);
    if (iter == directory_handles.end()) {
      return enqueue_error_callback(
        Error {"not an open directory"},
        data,
        on_error);
    }

    auto* dir = reinterpret_cast<uv_dir_t*>(handle);
    if (dir->dirents) {
      return enqueue_error_callback(
        Error {"read_directory in progress"},
        data,
        on_error);
    }

    directory_handles.erase(iter);

    uv_fs_closedir(
      uv_default_loop(),
      FsReq<Traits>::create_req(data, on_success, on_error),
      dir,
      FsReq<Traits>::callback);
  }

  // Processes

  struct ProcessReq {
    uv_process_t req;
    OnProcessExit on_exit;

    ProcessReq(void* data, OnProcessExit on_exit) : on_exit {on_exit} {
      req.data = data;
    }

    static uv_process_t* create_req(void* data, OnProcessExit on_exit) {
      ProcessReq* instance = new ProcessReq(data, on_exit);
      return &instance->req;
    }

    static void exit_callback(uv_process_t* req, int64_t status, int signal) {
      static_assert(offsetof(struct ProcessReq, req) == 0);
      auto* instance = reinterpret_cast<ProcessReq*>(req);
      auto cleanup = on_scope_exit([=]() {
        uv_close(reinterpret_cast<uv_handle_t*>(req), close_callback);
      });
      instance->on_exit(status, signal, req->data);
    }

    static void close_callback(uv_handle_t* handle) {
      auto* instance = reinterpret_cast<ProcessReq*>(handle);
      delete instance;
    }
  };

  int spawn_process(
    const std::string& cmd,
    const std::vector<std::string>& args,
    void* data,
    OnProcessExit on_exit)
  {
    uv_process_t* child = ProcessReq::create_req(data, on_exit);

    uv_process_options_t options = {0};
    options.exit_cb = ProcessReq::exit_callback;
    options.file = cmd.c_str();

    char** cmd_args = new char*[args.size() + 1];
    auto cleanup = on_scope_exit([=]() { delete[] cmd_args; });
    cmd_args[args.size()] = nullptr;
    for (size_t i = 0; i < args.size(); ++i) {
      cmd_args[i] = const_cast<char*>(args[i].c_str());
    }

    options.args = cmd_args;

    uv_stdio_container_t child_stdio[3];
    child_stdio[0].flags = UV_INHERIT_FD;
    child_stdio[0].data.fd = 0;
    child_stdio[1].flags = UV_INHERIT_FD;
    child_stdio[1].data.fd = 1;
    child_stdio[2].flags = UV_INHERIT_FD;
    child_stdio[2].data.fd = 2;

    options.stdio_count = 3;
    options.stdio = child_stdio;

    _check_uv(uv_spawn(uv_default_loop(), child, &options));
    return child->pid;
  }

}
