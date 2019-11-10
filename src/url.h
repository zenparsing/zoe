#pragma once

#include <string>
#include <vector>

namespace url {

  enum class ParseState {
    unknown = -1,
    scheme_start,
    scheme,
    no_scheme,
    special_relative_or_authority,
    path_or_authority,
    relative,
    relative_slash,
    special_authority_slashes,
    special_authority_ignore_slashes,
    authority,
    host,
    hostname,
    port,
    file,
    file_slash,
    file_host,
    path_start,
    path,
    cannot_be_base,
    query,
    fragment,
  };

  namespace Flags {
    constexpr uint32_t none = 0;
    constexpr uint32_t failed = 0x1;
    constexpr uint32_t cannot_be_base = 0x02;
    constexpr uint32_t invalid_parse_state = 0x04;
    constexpr uint32_t terminated = 0x08;
    constexpr uint32_t special = 0x10;
    constexpr uint32_t has_username = 0x20;
    constexpr uint32_t has_password = 0x40;
    constexpr uint32_t has_host = 0x80;
    constexpr uint32_t has_path = 0x100;
    constexpr uint32_t has_query = 0x200;
    constexpr uint32_t has_fragment = 0x400;
    constexpr uint32_t is_default_scheme_port = 0x800;
  }

  struct URLInfo {
    uint32_t flags = Flags::none;
    int port = -1;
    std::string scheme;
    std::string username;
    std::string password;
    std::string host;
    std::string query;
    std::string fragment;
    std::vector<std::string> path;

    static URLInfo parse(const std::string& url, const URLInfo* base = nullptr);
    static std::string stringify(const URLInfo& info);
  };

}
