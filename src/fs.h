#pragma once

#include <climits>
#include <cstdlib>
#include <string>

namespace fs {

  struct UvError {
    int code;
    std::string name;
    std::string message;
  };

#ifdef _WIN32
  constexpr unsigned PATH_MAX_BYTES = _MAX_PATH * 4;
#else
  constexpr unsigned PATH_MAX_BYTES = PATH_MAX;
#endif

  std::string cwd();
  std::string read_text_file_sync(const std::string& path);

}
