#pragma once

#include <vector>

#include "common.h"

namespace os {

  using FileHandle = uintptr_t;
  using DirectoryHandle = uintptr_t;
  using TimerHandle = uintptr_t;

  struct Error {
    std::string message;
    std::string code;

    Error(const std::string& message) :
      message {message},
      code {""}
    {}

    Error(const std::string& message, const std::string& code) :
      message {message},
      code {code}
    {}
  };

  // Returns the current working directory
  std::string cwd();

  // Synchronously reads a text file into a string
  std::string read_text_file_sync(const std::string& path);

  using OnError = void (*) (const Error&, void*);
  using OnOpenDirectory = void (*) (DirectoryHandle, void*);
  using OnReadDirectory = void (*) (std::vector<std::string>&, void*);
  using OnCloseDirectory = void (*) (void*);
  using OnTimer = void (*) (void*);

  // Starts a timer
  TimerHandle start_timer(
    uint64_t timeout,
    uint64_t repeat,
    void* data,
    OnTimer on_timer);

  template<typename T>
  TimerHandle start_timer(uint64_t timeout, uint64_t repeat, void* data) {
    return start_timer(timeout, repeat, data, T::on_success);
  }

  // Stops a timer
  void stop_timer(TimerHandle handle);

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

  // Reads directory entries
  void read_directory(
    DirectoryHandle handle,
    size_t count,
    void* data,
    OnReadDirectory on_success,
    OnError on_error);

  template<typename T>
  void read_directory(DirectoryHandle handle, size_t count, void* data) {
    return read_directory(handle, count, data, T::on_success, T::on_error);
  }

  // Closes a directory
  void close_directory(
    DirectoryHandle handle,
    void* data,
    OnCloseDirectory on_success,
    OnError on_error);

  template<typename T>
  void close_directory(DirectoryHandle handle, void* data) {
    return close_directory(handle, data, T::on_success, T::on_error);
  }

}
