#include "ChakraCore.h"
#include <string>
#include <iostream>

#include "js_engine.h"

using namespace std;

int main() {
  auto engine = create_js_engine();
  auto realm = engine.create_realm();

  realm.open([](auto& api) {
    auto result = api.eval(L"(()=>{return \'Hello world! I\\\'m Zoe!\';})()");
    std::wcout << api.convert_to_wstring(result) << L"\n";
  });

  return 0;
}
