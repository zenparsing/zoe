#include "fs.h"
#include "uv.h"

#include <iostream> // Debug only

namespace fs {

  void throw_on_error(int code) {
    if (code < 0) {
      throw UvError {
        code,
        uv_err_name(code),
        uv_strerror(code)
      };
    }
  }

  std::string cwd() {
    char buffer[PATH_MAX_BYTES];
    size_t cwd_len = sizeof(buffer);
    throw_on_error(uv_cwd(buffer, &cwd_len));
    return {buffer};
  }

  std::string read_text_file_sync(const std::string& path) {
    uv_fs_t req;

    // Open file
    uv_file file = uv_fs_open(nullptr, &req, path.c_str(), O_RDONLY, 0, nullptr);
    uv_fs_req_cleanup(&req);
    throw_on_error(file);

    char buffer_memory[4096];
    uv_buf_t buffer = uv_buf_init(buffer_memory, sizeof(buffer_memory));
    std::string str;

    while (true) {
      // Read into buffer
      int r = uv_fs_read(nullptr, &req, file, &buffer, 1, str.length(), nullptr);
      uv_fs_req_cleanup(&req);
      if (r > 0) {
        str.append(buffer.base, r);
      } else {
        break;
      }
    }

    // Close file
    uv_fs_close(nullptr, &req, file, nullptr);
    uv_fs_req_cleanup(&req);

    return str;
  }

}
