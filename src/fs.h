#pragma once

#include "common.h"

namespace fs {
  std::string cwd();
  std::string read_text_file_sync(const std::string& path);
}
