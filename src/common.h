#pragma once

#include <iostream>
#include <cassert>
#include <cstdint>
#include <utility>
#include <string>

// NOTE: We get errors if UV is included after CC
#include "uv.h"
#include "ChakraCore.h"

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
