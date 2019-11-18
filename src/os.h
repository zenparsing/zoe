#pragma once

#include "common.h"

namespace os {

  // Returns the current working directory
  std::string cwd();

  // Synchronously reads a text file into a string
  std::string read_text_file_sync(const std::string& path);

  // Opens a directory
  //template<typename F, typename G>
  //void open_directory(const std::string& path, Var callback);

}
