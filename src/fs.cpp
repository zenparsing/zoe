#include <utility>
#include <string>
#include <climits>
#include <cstdlib>

#include "uv.h"
#include "fs.h"

namespace fs {

#ifdef _WIN32
  constexpr unsigned PATH_MAX_BYTES = _MAX_PATH * 4;
#else
  constexpr unsigned PATH_MAX_BYTES = PATH_MAX;
#endif

  inline void _check_uv(int code) {
    if (code < 0) {
      throw HostError {
        HostErrorKind::libuv,
        uv_strerror(code),
        uv_err_name(code),
      };
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

}
