#pragma once

#include "common.h"

namespace os {

  struct FileHandle;
  struct DirectoryHandle;

  struct Error {
    std::string message;
    std::string code;

    Error(std::string&& message) :
      message {std::move(message)},
      code {""}
    {}

    Error(std::string&& message, std::string&& code) :
      message {std::move(message)},
      code {std::move(code)}
    {}
  };

  // Returns the current working directory
  std::string cwd();

  // Synchronously reads a text file into a string
  std::string read_text_file_sync(const std::string& path);

  using OnError = void (*) (const Error&, void*);
  using OnOpenDirectory = void (*) (DirectoryHandle*, void*);

  // Opens a directory
  void open_directory(
    const std::string& path,
    void* data,
    OnOpenDirectory on_success,
    OnError on_error);

  template<typename T>
  void open_directory(const std::string& path, void* data) {
    return open_directory(path, data, T::on_success, T::on_error);
  }

}
