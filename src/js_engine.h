#pragma once

#include <string>
#include "ChakraCore.h"
#include "on_scope_exit.h"

struct JsEngineError {};

struct JsRealm {
  JsContextRef _context;
  JsSourceContext _cookie = 0;

  struct API {

    JsRealm* realm;

    explicit API(JsRealm* realm) : realm(realm) {}

    JsValueRef eval(const wchar_t* source, const wchar_t* url = L"") {
      JsValueRef result;
      JsRunScript(source, realm->_cookie++, url, &result);
      return result;
    }

    JsValueRef convert_to_string(JsValueRef value) {
      JsValueRef result;
      JsConvertValueToString(value, &result);
      return result;
    }

    std::wstring convert_to_wstring(JsValueRef value) {
      const wchar_t* data;
      size_t length;
      JsStringToPointer(convert_to_string(value), &data, &length);
      return std::wstring(data);
    }

  };

  explicit JsRealm(JsContextRef context) : _context(context) {}

  template<typename F>
  void open(F fn) {
    auto cleanup = on_scope_exit([]() {
      JsSetCurrentContext(JS_INVALID_REFERENCE);
    });
    JsSetCurrentContext(_context);
    fn(API(this));
  }

};

struct JsEngine {

  JsRuntimeHandle _runtime;

  JsEngine() {
    if (JsCreateRuntime(JsRuntimeAttributeNone, nullptr, &_runtime) != JsNoError) {
      throw JsEngineError();
    }
  }

  JsEngine(const JsEngine& other) = delete;
  JsEngine& operator=(const JsEngine& other) = delete;

  JsEngine(JsEngine&& other) : _runtime(other._runtime) {
    other._runtime = JS_INVALID_REFERENCE;
  }

  JsEngine& operator=(JsEngine&& other) {
    if (this != &other) {
      _runtime = other._runtime;
      other._runtime = JS_INVALID_REFERENCE;
    }
  }

  ~JsEngine() {
    if (_runtime != JS_INVALID_REFERENCE) {
      JsSetCurrentContext(JS_INVALID_REFERENCE);
      JsDisposeRuntime(_runtime);
      _runtime = JS_INVALID_REFERENCE;
    }
  }

  JsRealm create_realm() {
    JsContextRef context;
    if (JsCreateContext(_runtime, &context) != JsNoError) {
      throw JsEngineError();
    }
    return JsRealm(context);
  }

};

JsEngine create_js_engine() {
  return JsEngine();
}
