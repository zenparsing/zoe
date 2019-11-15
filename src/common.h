#pragma once

#include <iostream>
#include <cassert>
#include <cstdint>
#include <utility>
#include <string>

template<typename F>
struct OnScopeExit {
  OnScopeExit(F fn) : fn(fn) {}
  ~OnScopeExit() { fn(); }
  F fn;
};

template<typename F>
OnScopeExit<F> on_scope_exit(F fn) {
  return OnScopeExit<F>(fn);
}

enum class HostErrorKind {
  js_engine,
  libuv,
};

struct HostError {
  HostErrorKind kind;
  std::string message;
  std::string code;

  HostError(HostErrorKind kind, std::string&& message) :
    kind {kind},
    message {std::move(message)},
    code {""}
  {}

  HostError(HostErrorKind kind, std::string&& message, std::string&& code) :
    kind {kind},
    message {std::move(message)},
    code {std::move(code)}
  {}
};
