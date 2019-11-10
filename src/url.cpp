#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <array>
#include "url.h"

using namespace url;

namespace {

  constexpr char EOL = -1;

  // https://infra.spec.whatwg.org/#ascii-tab-or-newline
  template<typename T>
  bool is_ascii_tab_or_newline(const T ch) {
    return ch == '\t' || ch == '\n' || ch == '\r';
  }

  // https://infra.spec.whatwg.org/#c0-control-or-space
  template<typename T>
  bool is_c0_control_or_space(const T ch) {
    return ch >= '\0' && ch <= ' ';
  }

  // https://infra.spec.whatwg.org/#ascii-digit
  template<typename T>
  bool is_ascii_digit(const T ch) {
    return ch >= '0' && ch <= '9';
  }

  // https://infra.spec.whatwg.org/#ascii-hex-digit
  template<typename T>
  bool is_ascii_hex_digit(const T ch) {
    return
      is_ascii_digit(ch) ||
      (ch >= 'A' && ch <= 'F') ||
      (ch >= 'a' && ch <= 'f');
  }

  // https://infra.spec.whatwg.org/#ascii-alpha
  template<typename T>
  bool is_ascii_alpha(const T ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
  }

  // https://infra.spec.whatwg.org/#ascii-alphanumeric
  template<typename T>
  bool is_ascii_alphanumeric(const T ch) {
    return is_ascii_digit(ch) || is_ascii_alpha(ch);
  }

  // https://infra.spec.whatwg.org/#ascii-lowercase
  template <typename T>
  T ascii_lowercase(T ch) {
    return is_ascii_alpha(ch) ? (ch | 0x20) : ch;
  }

  // https://url.spec.whatwg.org/#forbidden-host-code-point
  template<typename T>
  bool is_forbidden_host_codepoint(const T ch) {
    return
      ch == '\0' || ch == '\t' || ch == '\n' || ch == '\r' ||
      ch == ' ' || ch == '#' || ch == '%' || ch == '/' ||
      ch == ':' || ch == '?' || ch == '@' || ch == '[' ||
      ch == '\\' || ch == ']';
  }

  // https://url.spec.whatwg.org/#windows-drive-letter
  template<typename T>
  bool is_windows_drive_letter(const T ch1, const T ch2) {
    return is_ascii_alpha(ch1) && (ch2 == ':' || ch2 == '|');
  }

  template<typename T>
  bool is_windows_drive_letter(const std::basic_string<T>& str) {
    return str.length() >= 2 && is_windows_drive_letter(str[0], str[1]);
  }

  // https://url.spec.whatwg.org/#normalized-windows-drive-letter
  template<typename T>
  bool is_normalized_windows_drive_letter(const T ch1, const T ch2) {
    return is_ascii_alpha(ch1) && ch2 == ':';
  }

  template<typename T>
  bool is_normalized_windows_drive_letter(const std::basic_string<T>& str) {
    return
      str.length() >= 2 &&
      is_normalized_windows_drive_letter(str[0], str[1]);
  }

  // https://url.spec.whatwg.org/#start-with-a-windows-drive-letter
  bool starts_with_windows_drive_letter(const char* p, const char* end) {
    const size_t length = end - p;
    return
      length >= 2 &&
      is_windows_drive_letter(p[0], p[1]) &&
      (length == 2 || p[2] == '/' || p[2] == '\\' || p[2] == '?' || p[2] == '#');
  }

  const char* hex[256] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
    "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F",
    "%30", "%31", "%32", "%33", "%34", "%35", "%36", "%37",
    "%38", "%39", "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "%41", "%42", "%43", "%44", "%45", "%46", "%47",
    "%48", "%49", "%4A", "%4B", "%4C", "%4D", "%4E", "%4F",
    "%50", "%51", "%52", "%53", "%54", "%55", "%56", "%57",
    "%58", "%59", "%5A", "%5B", "%5C", "%5D", "%5E", "%5F",
    "%60", "%61", "%62", "%63", "%64", "%65", "%66", "%67",
    "%68", "%69", "%6A", "%6B", "%6C", "%6D", "%6E", "%6F",
    "%70", "%71", "%72", "%73", "%74", "%75", "%76", "%77",
    "%78", "%79", "%7A", "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
  };

  const uint8_t C0_CONTROL_ENCODE_SET[32] = {
    // 00     01     02     03     04     05     06     07
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 40     41     42     43     44     45     46     47
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
  };

  const uint8_t FRAGMENT_ENCODE_SET[32] = {
    // 00     01     02     03     04     05     06     07
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
      0x01 | 0x00 | 0x04 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
      0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
    // 40     41     42     43     44     45     46     47
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
      0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
  };

  const uint8_t PATH_ENCODE_SET[32] = {
    // 00     01     02     03     04     05     06     07
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
      0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
      0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x80,
    // 40     41     42     43     44     45     46     47
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
      0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
      0x00 | 0x00 | 0x00 | 0x08 | 0x00 | 0x20 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
  };

  const uint8_t USERINFO_ENCODE_SET[32] = {
    // 00     01     02     03     04     05     06     07
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
      0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 30     31     32     33     34     35     36     37
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
      0x00 | 0x00 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 40     41     42     43     44     45     46     47
      0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
      0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x40 | 0x00,
    // 60     61     62     63     64     65     66     67
      0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
      0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
  };

  const uint8_t QUERY_ENCODE_SET_NONSPECIAL[32] = {
    // 00     01     02     03     04     05     06     07
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
      0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
      0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
    // 40     41     42     43     44     45     46     47
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
  };

  // Same as QUERY_ENCODE_SET_NONSPECIAL, but with 0x27 (') encoded.
  const uint8_t QUERY_ENCODE_SET_SPECIAL[32] = {
    // 00     01     02     03     04     05     06     07
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
      0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x80,
    // 28     29     2A     2B     2C     2D     2E     2F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
      0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
    // 40     41     42     43     44     45     46     47
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
      0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
      0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
  };

  bool bit_at(const uint8_t a[], const uint8_t i) {
    return !!(a[i >> 3] & (1 << (i & 7)));
  }

  // Appends ch to str. If ch position in encode_set is set, the ch will
  // be percent-encoded then appended.
  void append_or_escape(
    std::string* str,
    const unsigned char ch,
    const uint8_t encode_set[])
  {
    if (bit_at(encode_set, ch)) {
      *str += hex[ch];
    } else {
      *str += ch;
    }
  }

  template <typename T>
  inline unsigned hex2bin(const T ch) {
    if (ch >= '0' && ch <= '9') {
      return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
      return 10 + (ch - 'A');
    }
    if (ch >= 'a' && ch <= 'f') {
      return 10 + (ch - 'a');
    }
    return static_cast<unsigned>(-1);
  }

  std::string percent_decode(const char* input, size_t len) {
    std::string dest;
    if (len == 0) {
      return dest;
    }
    dest.reserve(len);

    const char* pointer = input;
    const char* end = input + len;

    while (pointer < end) {
      const char ch = pointer[0];
      const size_t remaining = end - pointer - 1;
      if (
        ch != '%' ||
        remaining < 2 || (
          ch == '%' && (
            !is_ascii_hex_digit(pointer[1]) ||
            !is_ascii_hex_digit(pointer[2]))))
      {
        dest += ch;
        pointer++;
        continue;
      } else {
        unsigned a = hex2bin(pointer[1]);
        unsigned b = hex2bin(pointer[2]);
        char c = static_cast<char>(a * 16 + b);
        dest += c;
        pointer += 3;
      }
    }
    return dest;
  }

  bool is_special(const std::string& scheme) {
    return
      scheme == "ftp:" ||
      scheme == "file:" ||
      scheme == "gopher:" ||
      scheme == "http:" ||
      scheme == "https:" ||
      scheme == "ws:" ||
      scheme == "wss:";
  }

  int normalize_port(const std::string& scheme, int p) {
    return (
      scheme == "ftp:" && p == 21 ||
      scheme == "file:" && p == -1 ||
      scheme == "gopher:" && p == 70 ||
      scheme == "http:" && p == 80 ||
      scheme == "https:" && p == 443 ||
      scheme == "ws:" && p == 80 ||
      scheme == "wss:" && p == 443) ? -1 : p;
  }

  // Single dot segment can be ".", "%2e", or "%2E"
  bool is_single_dot_segment(const std::string& str) {
    switch (str.size()) {
      case 1:
        return str == ".";
      case 3:
        return str[0] == '%' && str[1] == '2' && ascii_lowercase(str[2]) == 'e';
      default:
        return false;
    }
  }

  // Double dot segment can be:
  //   "..", ".%2e", ".%2E", "%2e.", "%2E.",
  //   "%2e%2e", "%2E%2E", "%2e%2E", or "%2E%2e"
  bool is_double_dot_segment(const std::string& str) {
    switch (str.size()) {
      case 2:
        return str == "..";
      case 4:
        if (str[0] != '.' && str[0] != '%') {
          return false;
        }
        return (
          str[0] == '.' &&
          str[1] == '%' &&
          str[2] == '2' &&
          ascii_lowercase(str[3]) == 'e') || (
            str[0] == '%' &&
            str[1] == '2' &&
            ascii_lowercase(str[2]) == 'e' &&
            str[3] == '.');
      case 6:
        return
          str[0] == '%' &&
          str[1] == '2' &&
          ascii_lowercase(str[2]) == 'e' &&
          str[3] == '%' &&
          str[4] == '2' &&
          ascii_lowercase(str[5]) == 'e';
      default:
        return false;
    }
  }

  void shorten_url_path(URLInfo* url) {
    if (url->path.empty()) {
      return;
    }
    if (
      url->path.size() == 1 &&
      url->scheme == "file:" &&
      is_normalized_windows_drive_letter(url->path[0]))
    {
      return;
    }
    url->path.pop_back();
  }

  bool to_unicode(const std::string& input, std::string* output) {
    // TODO: Figure out how to use ChakraCore's ICU
    *output = input;
    return true;
  }

  bool to_ascii(const std::string& input, std::string* output) {
    // TODO: Figure out how to use ChakraCore's ICU
    *output = input;
    return true;
  }

  enum class HostType {
    failed,
    domain,
    ipv4,
    ipv6,
    opaque,
  };

  struct HostInfo {
    HostType type;
    uint32_t ipv4;
    std::array<uint16_t, 8> ipv6;
    std::string domain_or_opaque;
  };

  void parse_host_ipv6(HostInfo& info, const char* input, size_t length) {
    info.ipv6.fill(0);

    uint16_t* piece_pointer = info.ipv6.data();
    uint16_t* buffer_end = piece_pointer + info.ipv6.size();
    uint16_t* compress_pointer = nullptr;
    const char* pointer = input;
    const char* end = pointer + length;
    unsigned value;
    unsigned len;
    unsigned numbers_seen;
    char ch = pointer < end ? pointer[0] : EOL;

    if (ch == ':') {
      if (length < 2 || pointer[1] != ':') {
        return;
      }
      pointer += 2;
      ch = pointer < end ? pointer[0] : EOL;
      piece_pointer++;
      compress_pointer = piece_pointer;
    }

    while (ch != EOL) {
      if (piece_pointer >= buffer_end) {
        return;
      }

      if (ch == ':') {
        if (compress_pointer != nullptr) {
          return;
        }
        pointer++;
        ch = pointer < end ? pointer[0] : EOL;
        piece_pointer++;
        compress_pointer = piece_pointer;
        continue;
      }

      value = 0;
      len = 0;

      while (len < 4 && is_ascii_hex_digit(ch)) {
        value = value * 0x10 + hex2bin(ch);
        pointer++;
        ch = pointer < end ? pointer[0] : EOL;
        len++;
      }

      switch (ch) {
        case '.':
          if (len == 0) {
            return;
          }
          pointer -= len;
          ch = pointer < end ? pointer[0] : EOL;
          if (piece_pointer > buffer_end - 2) {
            return;
          }
          numbers_seen = 0;
          while (ch != EOL) {
            value = 0xffffffff;
            if (numbers_seen > 0) {
              if (ch == '.' && numbers_seen < 4) {
                pointer++;
                ch = pointer < end ? pointer[0] : EOL;
              } else {
                return;
              }
            }
            if (!is_ascii_digit(ch)) {
              return;
            }
            while (is_ascii_digit(ch)) {
              unsigned number = ch - '0';
              if (value == 0xffffffff) {
                value = number;
              } else if (value == 0) {
                return;
              } else {
                value = value * 10 + number;
              }
              if (value > 255) {
                return;
              }
              pointer++;
              ch = pointer < end ? pointer[0] : EOL;
            }
            *piece_pointer = *piece_pointer * 0x100 + value;
            numbers_seen++;
            if (numbers_seen == 2 || numbers_seen == 4) {
              piece_pointer++;
            }
          }
          if (numbers_seen != 4) {
            return;
          }
          continue;
        case ':':
          pointer++;
          ch = pointer < end ? pointer[0] : EOL;
          if (ch == EOL) {
            return;
          }
          break;
        case EOL:
          break;
        default:
          return;
      }
      *piece_pointer = value;
      piece_pointer++;
    }

    if (compress_pointer != nullptr) {
      uintptr_t swaps = piece_pointer - compress_pointer;
      piece_pointer = buffer_end - 1;
      while (piece_pointer != info.ipv6.data() && swaps > 0) {
        uint16_t temp = *piece_pointer;
        uint16_t* swap_piece = compress_pointer + swaps - 1;
        *piece_pointer = *swap_piece;
        *swap_piece = temp;
        piece_pointer--;
        swaps--;
      }
    } else if (compress_pointer == nullptr && piece_pointer != buffer_end) {
      return;
    }

    info.type = HostType::ipv6;
  }

  int64_t parse_number(const char* start, const char* end) {
    unsigned R = 10;
    if (end - start >= 2 && start[0] == '0' && (start[1] | 0x20) == 'x') {
      start += 2;
      R = 16;
    }

    if (end - start == 0) {
      return 0;
    }

    if (R == 10 && end - start > 1 && start[0] == '0') {
      start++;
      R = 8;
    }

    const char* p = start;

    while (p < end) {
      const char ch = p[0];
      switch (R) {
        case 8:
          if (ch < '0' || ch > '7') return -1;
          break;
        case 10:
          if (!is_ascii_digit(ch)) return -1;
          break;
        case 16:
          if (!is_ascii_hex_digit(ch)) return -1;
          break;
      }
      p++;
    }

    return strtoll(start, nullptr, R);
  }

  bool parse_host_ipv4(HostInfo& info, const char* input, size_t length) {
    const char* pointer = input;
    const char* mark = input;
    const char* end = pointer + length;
    int parts = 0;
    std::array<uint64_t, 4> numbers;
    int too_big_numbers = 0;

    if (length == 0) {
      return false;
    }

    while (pointer <= end) {
      const char ch = pointer < end ? pointer[0] : EOL;
      const int64_t remaining = end - pointer - 1;
      if (ch == '.' || ch == EOL) {
        if (++parts > numbers.size()) {
          return false;
        }
        if (pointer == mark) {
          return false;
        }
        int64_t n = parse_number(mark, pointer);
        if (n < 0) {
          return false;
        }
        if (n > 255) {
          too_big_numbers++;
        }
        numbers[parts - 1] = n;
        mark = pointer + 1;
        if (ch == '.' && remaining == 0) {
          break;
        }
      }
      pointer++;
    }

    // If any but the last item in numbers is greater than 255, return failure.
    // If the last item in numbers is greater than or equal to
    // 256^(5 - the number of items in numbers), return failure.
    if (
      too_big_numbers > 1 ||
      (too_big_numbers == 1 && numbers[parts - 1] <= 255) ||
      numbers[parts - 1] >= pow(256, static_cast<double>(5 - parts)))
    {
      return true;
    }

    info.type = HostType::ipv4;
    uint32_t val = static_cast<uint32_t>(numbers[parts - 1]);
    for (int n = 0; n < parts - 1; n++) {
      double b = 3 - n;
      val += static_cast<uint32_t>(numbers[n] * pow(256, b));
    }

    info.ipv4 = val;
    return true;
  }

  void parse_host_opaque(HostInfo& info, const char* input, size_t length) {
    std::string output;
    output.reserve(length);
    for (size_t i = 0; i < length; i++) {
      const char ch = input[i];
      if (ch != '%' && is_forbidden_host_codepoint(ch)) {
        return;
      } else {
        append_or_escape(&output, ch, C0_CONTROL_ENCODE_SET);
      }
    }
    info.type = HostType::opaque;
    info.domain_or_opaque = std::move(output);
  }

  void parse_host(
    HostInfo& info,
    const char* input,
    size_t length,
    bool is_special,
    bool unicode)
  {
    info.type = HostType::failed;
    const char* pointer = input;

    if (length == 0) {
      return;
    }

    if (pointer[0] == '[') {
      if (pointer[length - 1] != ']') {
        return;
      }
      return parse_host_ipv6(info, ++pointer, length - 2);
    }

    if (!is_special) {
      return parse_host_opaque(info, input, length);
    }

    // First, we have to percent decode
    std::string decoded = percent_decode(input, length);

    // Then we have to punycode toASCII
    if (!to_ascii(decoded, &decoded)) {
      return;
    }

    // If any of the following characters are still present, we have to fail
    for (size_t n = 0; n < decoded.size(); n++) {
      const char ch = decoded[n];
      if (is_forbidden_host_codepoint(ch)) {
        return;
      }
    }

    // Check to see if it's an IPv4 IP address
    if (parse_host_ipv4(info, decoded.c_str(), decoded.length())) {
      return;
    }

    // If the unicode flag is set, run the result through punycode
    if (unicode && !to_unicode(decoded, &decoded)) {
      return;
    }

    // It's not an IPv4 or IPv6 address, it must be a domain
    info.type = HostType::domain;
    info.domain_or_opaque = std::move(decoded);
  }

  // Locates the longest sequence of 0 segments in an IPv6 address
  // in order to use the :: compression when serializing
  template <typename T>
  inline T* find_longest_zero_sequence(T* values, size_t len) {
    T* start = values;
    T* end = start + len;
    T* result = nullptr;
    T* current = nullptr;
    unsigned counter = 0, longest = 1;

    while (start < end) {
      if (*start == 0) {
        if (current == nullptr) {
          current = start;
        }
        counter++;
      } else {
        if (counter > longest) {
          longest = counter;
          result = current;
        }
        counter = 0;
        current = nullptr;
      }
      start++;
    }
    if (counter > longest) {
      result = current;
    }
    return result;
  }

  std::string stringify_host(const HostInfo& info) {
    if (info.type == HostType::domain || info.type == HostType::opaque) {
      return std::move(info.domain_or_opaque);
    }

    if (info.type == HostType::ipv4) {
      std::string dest;
      dest.reserve(15);
      uint32_t value = info.ipv4;
      for (int n = 0; n < 4; n++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", value % 256);
        dest.insert(0, buf);
        if (n < 3) {
          dest.insert(0, 1, '.');
        }
        value /= 256;
      }
      return dest;
    }

    if (info.type == HostType::ipv6) {
      std::string dest;
      dest.reserve(41);
      dest += '[';
      const uint16_t* start = &info.ipv6[0];
      const uint16_t* compress_pointer = find_longest_zero_sequence(start, 8);
      bool ignore0 = false;
      for (int n = 0; n <= 7; n++) {
        const uint16_t* piece = &info.ipv6[n];
        if (ignore0 && *piece == 0) {
          continue;
        } else if (ignore0) {
          ignore0 = false;
        }
        if (compress_pointer == piece) {
          dest += n == 0 ? "::" : ":";
          ignore0 = true;
          continue;
        }
        char buf[5];
        snprintf(buf, sizeof(buf), "%x", *piece);
        dest += buf;
        if (n < 7) {
          dest += ':';
        }
      }
      dest += ']';
      return dest;
    }

    return "";
  }

  bool try_parse_host(
    const std::string& input,
    std::string* output,
    bool is_special,
    bool unicode = false)
  {
    if (input.length() == 0) {
      output->clear();
      return true;
    }
    HostInfo info;
    parse_host(info, input.c_str(), input.length(), is_special, unicode);
    if (info.type == HostType::failed) {
      return false;
    }
    *output = std::move(stringify_host(info));
    return true;
  }

  void parse_url(
    const char* input,
    size_t len,
    ParseState state_override,
    URLInfo* url,
    bool has_url,
    const URLInfo* base)
  {
    const char* p = input;
    const char* end = input + len;

    if (!has_url) {
      for (const char* ptr = p; ptr < end; ptr++) {
        if (is_c0_control_or_space(*ptr)) p++;
        else break;
      }

      for (const char* ptr = end - 1; ptr >= p; ptr--) {
        if (is_c0_control_or_space(*ptr)) end--;
        else break;
      }

      input = p;
      len = end - p;
    }

    bool has_base = base != nullptr;

    // The spec says we should strip out any ASCII tabs or newlines.
    // In those cases, we create another std::string instance with the filtered
    // contents, but in the general case we avoid the overhead.
    std::string whitespace_stripped;
    for (const char* ptr = p; ptr < end; ptr++) {
      if (!is_ascii_tab_or_newline(*ptr)) {
        continue;
      }

      // Hit tab or newline. Allocate storage, copy what we have until now,
      // and then iterate and filter all similar characters out.
      whitespace_stripped.reserve(len - 1);
      whitespace_stripped.assign(p, ptr - p);

      // 'ptr + 1' skips the current char, which we know to be tab or newline.
      for (ptr = ptr + 1; ptr < end; ptr++) {
        if (!is_ascii_tab_or_newline(*ptr)) {
          whitespace_stripped += *ptr;
        }
      }

      // Update variables like they should have looked like if the string
      // had been stripped of whitespace to begin with.
      input = whitespace_stripped.c_str();
      len = whitespace_stripped.size();
      p = input;
      end = input + len;
      break;
    }

    bool atflag = false;  // Set when @ has been seen.
    bool square_bracket_flag = false;  // Set inside of [...]
    bool password_token_seen_flag = false;  // Set after a : after an username.

    std::string buffer;

    // Set the initial parse state.
    const bool has_state_override = state_override != ParseState::unknown;
    ParseState state = has_state_override ? state_override : ParseState::scheme_start;

    if (state < ParseState::scheme_start || state > ParseState::fragment) {
      url->flags |= Flags::invalid_parse_state;
      return;
    }

    while (p <= end) {
      const char ch = p < end ? p[0] : EOL;
      bool special = (url->flags & Flags::special);
      bool cannot_be_base;
      const bool special_back_slash = (special && ch == '\\');

      switch (state) {

        case ParseState::scheme_start:
          if (is_ascii_alpha(ch)) {
            buffer += ascii_lowercase(ch);
            state = ParseState::scheme;
          } else if (!has_state_override) {
            state = ParseState::no_scheme;
            continue;
          } else {
            url->flags |= Flags::failed;
            return;
          }
          break;

        case ParseState::scheme:
          if (is_ascii_alphanumeric(ch) || ch == '+' || ch == '-' || ch == '.') {
            buffer += ascii_lowercase(ch);
          } else if (ch == ':' || (has_state_override && ch == EOL)) {
            if (has_state_override && buffer.size() == 0) {
              url->flags |= Flags::terminated;
              return;
            }

            buffer += ':';

            bool new_is_special = is_special(buffer);

            if (has_state_override) {
              if ((special != new_is_special) ||
                  ((buffer == "file:") &&
                  ((url->flags & Flags::has_username) ||
                    (url->flags & Flags::has_password) ||
                    (url->port != -1))))
              {
                url->flags |= Flags::terminated;
                return;
              }
              // File scheme && (host == empty or null) check left to JS-land
              // as it can be done before even entering C++ binding.
            }

            url->scheme = std::move(buffer);
            url->port = normalize_port(url->scheme, url->port);

            if (new_is_special) {
              url->flags |= Flags::special;
              special = true;
            } else {
              url->flags &= ~Flags::special;
              special = false;
            }

            buffer.clear();
            if (has_state_override) {
              return;
            }

            if (url->scheme == "file:") {
              state = ParseState::file;
            } else if (special && has_base && url->scheme == base->scheme) {
              state = ParseState::special_relative_or_authority;
            } else if (special) {
              state = ParseState::special_authority_slashes;
            } else if (p[1] == '/') {
              state = ParseState::path_or_authority;
              p++;
            } else {
              url->flags |= Flags::cannot_be_base;
              url->flags |= Flags::has_path;
              url->path.emplace_back("");
              state = ParseState::cannot_be_base;
            }
          } else if (!has_state_override) {
            buffer.clear();
            state = ParseState::no_scheme;
            p = input;
            continue;
          } else {
            url->flags |= Flags::failed;
            return;
          }
          break;

        case ParseState::no_scheme:
          cannot_be_base = has_base && (base->flags & Flags::cannot_be_base);
          if (!has_base || (cannot_be_base && ch != '#')) {
            url->flags |= Flags::failed;
            return;
          }

          if (cannot_be_base && ch == '#') {
            url->scheme = base->scheme;
            if (is_special(url->scheme)) {
              url->flags |= Flags::special;
              special = true;
            } else {
              url->flags &= ~Flags::special;
              special = false;
            }
            if (base->flags & Flags::has_path) {
              url->flags |= Flags::has_path;
              url->path = base->path;
            }
            if (base->flags & Flags::has_query) {
              url->flags |= Flags::has_query;
              url->query = base->query;
            }
            if (base->flags & Flags::has_fragment) {
              url->flags |= Flags::has_fragment;
              url->fragment = base->fragment;
            }
            url->flags |= Flags::cannot_be_base;
            state = ParseState::fragment;
          } else if (has_base && base->scheme != "file:") {
            state = ParseState::relative;
            continue;
          } else {
            url->scheme = "file:";
            url->flags |= Flags::special;
            special = true;
            state = ParseState::file;
            continue;
          }
          break;

        case ParseState::special_relative_or_authority:
          if (ch == '/' && p[1] == '/') {
            state = ParseState::special_authority_ignore_slashes;
            p++;
          } else {
            state = ParseState::relative;
            continue;
          }
          break;

        case ParseState::path_or_authority:
          if (ch == '/') {
            state = ParseState::authority;
          } else {
            state = ParseState::path;
            continue;
          }
          break;

        case ParseState::relative:
          url->scheme = base->scheme;
          if (is_special(url->scheme)) {
            url->flags |= Flags::special;
            special = true;
          } else {
            url->flags &= ~Flags::special;
            special = false;
          }

          switch (ch) {
            case EOL:
              if (base->flags & Flags::has_username) {
                url->flags |= Flags::has_username;
                url->username = base->username;
              }
              if (base->flags & Flags::has_password) {
                url->flags |= Flags::has_password;
                url->password = base->password;
              }
              if (base->flags & Flags::has_host) {
                url->flags |= Flags::has_host;
                url->host = base->host;
              }
              if (base->flags & Flags::has_query) {
                url->flags |= Flags::has_query;
                url->query = base->query;
              }
              if (base->flags & Flags::has_path) {
                url->flags |= Flags::has_path;
                url->path = base->path;
              }
              url->port = base->port;
              break;
            case '/':
              state = ParseState::relative_slash;
              break;
            case '?':
              if (base->flags & Flags::has_username) {
                url->flags |= Flags::has_username;
                url->username = base->username;
              }
              if (base->flags & Flags::has_password) {
                url->flags |= Flags::has_password;
                url->password = base->password;
              }
              if (base->flags & Flags::has_host) {
                url->flags |= Flags::has_host;
                url->host = base->host;
              }
              if (base->flags & Flags::has_path) {
                url->flags |= Flags::has_path;
                url->path = base->path;
              }
              url->port = base->port;
              state = ParseState::query;
              break;
            case '#':
              if (base->flags & Flags::has_username) {
                url->flags |= Flags::has_username;
                url->username = base->username;
              }
              if (base->flags & Flags::has_password) {
                url->flags |= Flags::has_password;
                url->password = base->password;
              }
              if (base->flags & Flags::has_host) {
                url->flags |= Flags::has_host;
                url->host = base->host;
              }
              if (base->flags & Flags::has_query) {
                url->flags |= Flags::has_query;
                url->query = base->query;
              }
              if (base->flags & Flags::has_path) {
                url->flags |= Flags::has_path;
                url->path = base->path;
              }
              url->port = base->port;
              state = ParseState::fragment;
              break;
            default:
              if (special_back_slash) {
                state = ParseState::relative_slash;
              } else {
                if (base->flags & Flags::has_username) {
                  url->flags |= Flags::has_username;
                  url->username = base->username;
                }
                if (base->flags & Flags::has_password) {
                  url->flags |= Flags::has_password;
                  url->password = base->password;
                }
                if (base->flags & Flags::has_host) {
                  url->flags |= Flags::has_host;
                  url->host = base->host;
                }
                if (base->flags & Flags::has_path) {
                  url->flags |= Flags::has_path;
                  url->path = base->path;
                  shorten_url_path(url);
                }
                url->port = base->port;
                state = ParseState::path;
                continue;
              }
          }
          break;

        case ParseState::relative_slash:
          if (is_special(url->scheme) && (ch == '/' || ch == '\\')) {
            state = ParseState::special_authority_ignore_slashes;
          } else if (ch == '/') {
            state = ParseState::authority;
          } else {
            if (base->flags & Flags::has_username) {
              url->flags |= Flags::has_username;
              url->username = base->username;
            }
            if (base->flags & Flags::has_password) {
              url->flags |= Flags::has_password;
              url->password = base->password;
            }
            if (base->flags & Flags::has_host) {
              url->flags |= Flags::has_host;
              url->host = base->host;
            }
            url->port = base->port;
            state = ParseState::path;
            continue;
          }
          break;

        case ParseState::special_authority_slashes:
          state = ParseState::special_authority_ignore_slashes;
          if (ch == '/' && p[1] == '/') {
            p++;
          } else {
            continue;
          }
          break;

        case ParseState::special_authority_ignore_slashes:
          if (ch != '/' && ch != '\\') {
            state = ParseState::authority;
            continue;
          }
          break;

        case ParseState::authority:
          if (ch == '@') {
            if (atflag) {
              buffer.reserve(buffer.size() + 3);
              buffer.insert(0, "%40");
            }
            atflag = true;
            const size_t blen = buffer.size();
            if (blen > 0 && buffer[0] != ':') {
              url->flags |= Flags::has_username;
            }
            for (size_t n = 0; n < blen; n++) {
              const char bch = buffer[n];
              if (bch == ':') {
                url->flags |= Flags::has_password;
                if (!password_token_seen_flag) {
                  password_token_seen_flag = true;
                  continue;
                }
              }
              if (password_token_seen_flag) {
                append_or_escape(&url->password, bch, USERINFO_ENCODE_SET);
              } else {
                append_or_escape(&url->username, bch, USERINFO_ENCODE_SET);
              }
            }
            buffer.clear();
          } else if (
            ch == EOL ||
            ch == '/' ||
            ch == '?' ||
            ch == '#' ||
            special_back_slash)
          {
            if (atflag && buffer.size() == 0) {
              url->flags |= Flags::failed;
              return;
            }
            p -= buffer.size() + 1;
            buffer.clear();
            state = ParseState::host;
          } else {
            buffer += ch;
          }
          break;

        case ParseState::host:
        case ParseState::hostname:
          if (has_state_override && url->scheme == "file:") {
            state = ParseState::file_host;
            continue;
          } else if (ch == ':' && !square_bracket_flag) {
            if (buffer.size() == 0) {
              url->flags |= Flags::failed;
              return;
            }
            url->flags |= Flags::has_host;
            if (!try_parse_host(buffer, &url->host, special)) {
              url->flags |= Flags::failed;
              return;
            }
            buffer.clear();
            state = ParseState::port;
            if (state_override == ParseState::hostname) {
              return;
            }
          } else if (
            ch == EOL ||
            ch == '/' ||
            ch == '?' ||
            ch == '#' ||
            special_back_slash)
          {
            p--;
            if (special && buffer.size() == 0) {
              url->flags |= Flags::failed;
              return;
            }
            if (
              has_state_override &&
              buffer.size() == 0 &&
              (url->username.size() > 0 || url->password.size() > 0 || url->port != -1))
            {
              url->flags |= Flags::terminated;
              return;
            }
            url->flags |= Flags::has_host;
            if (!try_parse_host(buffer, &url->host, special)) {
              url->flags |= Flags::failed;
              return;
            }
            buffer.clear();
            state = ParseState::path_start;
            if (has_state_override) {
              return;
            }
          } else {
            if (ch == '[') {
              square_bracket_flag = true;
            }
            if (ch == ']') {
              square_bracket_flag = false;
            }
            buffer += ch;
          }
          break;

        case ParseState::port:
          if (is_ascii_digit(ch)) {
            buffer += ch;
          } else if (
            has_state_override ||
            ch == EOL ||
            ch == '/' ||
            ch == '?' ||
            ch == '#' ||
            special_back_slash)
          {
            if (buffer.size() > 0) {
              unsigned port = 0;
              // the condition port <= 0xffff prevents integer overflow
              for (size_t i = 0; port <= 0xffff && i < buffer.size(); i++) {
                port = port * 10 + buffer[i] - '0';
              }
              if (port > 0xffff) {
                // TODO(TimothyGu): This hack is currently needed for the host
                // setter since it needs access to hostname if it is valid, and
                // if the FAILED flag is set the entire response to JS layer
                // will be empty.
                if (state_override == ParseState::host) {
                  url->port = -1;
                } else {
                  url->flags |= Flags::failed;
                }
                return;
              }
              // the port is valid
              url->port = normalize_port(url->scheme, static_cast<int>(port));
              if (url->port == -1) {
                url->flags |= Flags::is_default_scheme_port;
              }
              buffer.clear();
            } else if (has_state_override) {
              // TODO(TimothyGu): Similar case as above.
              if (state_override == ParseState::host) {
                url->port = -1;
              } else {
                url->flags |= Flags::terminated;
              }
              return;
            }
            state = ParseState::path_start;
            continue;
          } else {
            url->flags |= Flags::failed;
            return;
          }
          break;

        case ParseState::file:
          url->scheme = "file:";
          if (ch == '/' || ch == '\\') {
            state = ParseState::file_slash;
          } else if (has_base && base->scheme == "file:") {
            switch (ch) {
              case EOL:
                if (base->flags & Flags::has_host) {
                  url->flags |= Flags::has_host;
                  url->host = base->host;
                }
                if (base->flags & Flags::has_path) {
                  url->flags |= Flags::has_path;
                  url->path = base->path;
                }
                if (base->flags & Flags::has_query) {
                  url->flags |= Flags::has_query;
                  url->query = base->query;
                }
                break;
              case '?':
                if (base->flags & Flags::has_host) {
                  url->flags |= Flags::has_host;
                  url->host = base->host;
                }
                if (base->flags & Flags::has_path) {
                  url->flags |= Flags::has_path;
                  url->path = base->path;
                }
                url->flags |= Flags::has_query;
                url->query.clear();
                state = ParseState::query;
                break;
              case '#':
                if (base->flags & Flags::has_host) {
                  url->flags |= Flags::has_host;
                  url->host = base->host;
                }
                if (base->flags & Flags::has_path) {
                  url->flags |= Flags::has_path;
                  url->path = base->path;
                }
                if (base->flags & Flags::has_query) {
                  url->flags |= Flags::has_query;
                  url->query = base->query;
                }
                url->flags |= Flags::has_fragment;
                url->fragment.clear();
                state = ParseState::fragment;
                break;
              default:
                if (!starts_with_windows_drive_letter(p, end)) {
                  if (base->flags & Flags::has_host) {
                    url->flags |= Flags::has_host;
                    url->host = base->host;
                  }
                  if (base->flags & Flags::has_path) {
                    url->flags |= Flags::has_path;
                    url->path = base->path;
                  }
                  shorten_url_path(url);
                }
                state = ParseState::path;
                continue;
            }
          } else {
            state = ParseState::path;
            continue;
          }
          break;

        case ParseState::file_slash:
          if (ch == '/' || ch == '\\') {
            state = ParseState::file_host;
          } else {
            if (
              has_base &&
              base->scheme == "file:" &&
              !starts_with_windows_drive_letter(p, end))
            {
              if (is_normalized_windows_drive_letter(base->path[0])) {
                url->flags |= Flags::has_path;
                url->path.push_back(base->path[0]);
              } else {
                if (base->flags & Flags::has_host) {
                  url->flags |= Flags::has_host;
                  url->host = base->host;
                } else {
                  url->flags &= ~Flags::has_host;
                  url->host.clear();
                }
              }
            }
            state = ParseState::path;
            continue;
          }
          break;

        case ParseState::file_host:
          if (
            ch == EOL ||
            ch == '/' ||
            ch == '\\' ||
            ch == '?' ||
            ch == '#')
          {
            if (
              !has_state_override &&
              buffer.size() == 2 &&
              is_windows_drive_letter(buffer))
            {
              state = ParseState::path;
            } else if (buffer.size() == 0) {
              url->flags |= Flags::has_host;
              url->host.clear();
              if (has_state_override) {
                return;
              }
              state = ParseState::path_start;
            } else {
              std::string host;
              if (!try_parse_host(buffer, &host, special)) {
                url->flags |= Flags::failed;
                return;
              }
              if (host == "localhost") {
                host.clear();
              }
              url->flags |= Flags::has_host;
              url->host = host;
              if (has_state_override) {
                return;
              }
              buffer.clear();
              state = ParseState::path_start;
            }
            continue;
          } else {
            buffer += ch;
          }
          break;

        case ParseState::path_start:
          if (is_special(url->scheme)) {
            state = ParseState::path;
            if (ch != '/' && ch != '\\') {
              continue;
            }
          } else if (!has_state_override && ch == '?') {
            url->flags |= Flags::has_query;
            url->query.clear();
            state = ParseState::query;
          } else if (!has_state_override && ch == '#') {
            url->flags |= Flags::has_fragment;
            url->fragment.clear();
            state = ParseState::fragment;
          } else if (ch != EOL) {
            state = ParseState::path;
            if (ch != '/') {
              continue;
            }
          }
          break;

        case ParseState::path:
          if (
            ch == EOL ||
            ch == '/' ||
            special_back_slash ||
            (!has_state_override && (ch == '?' || ch == '#')))
          {
            if (is_double_dot_segment(buffer)) {
              shorten_url_path(url);
              if (ch != '/' && !special_back_slash) {
                url->flags |= Flags::has_path;
                url->path.emplace_back("");
              }
            } else if (
              is_single_dot_segment(buffer) &&
              ch != '/' &&
              !special_back_slash)
            {
              url->flags |= Flags::has_path;
              url->path.emplace_back("");
            } else if (!is_single_dot_segment(buffer)) {
              if (
                url->scheme == "file:" &&
                url->path.empty() &&
                buffer.size() == 2 &&
                is_windows_drive_letter(buffer))
              {
                if ((url->flags & Flags::has_host) && !url->host.empty()) {
                  url->host.clear();
                  url->flags |= Flags::has_host;
                }
                buffer[1] = ':';
              }
              url->flags |= Flags::has_path;
              url->path.emplace_back(std::move(buffer));
            }
            buffer.clear();
            if (
              url->scheme == "file:" &&
              (ch == EOL || ch == '?' || ch == '#'))
            {
              while (url->path.size() > 1 && url->path[0].length() == 0) {
                url->path.erase(url->path.begin());
              }
            }
            if (ch == '?') {
              url->flags |= Flags::has_query;
              state = ParseState::query;
            } else if (ch == '#') {
              state = ParseState::fragment;
            }
          } else {
            append_or_escape(&buffer, ch, PATH_ENCODE_SET);
          }
          break;

        case ParseState::cannot_be_base:
          switch (ch) {
            case '?':
              state = ParseState::query;
              break;
            case '#':
              state = ParseState::fragment;
              break;
            default:
              if (url->path.size() == 0) {
                url->path.emplace_back("");
              }
              if (url->path.size() > 0 && ch != EOL) {
                append_or_escape(&url->path[0], ch, C0_CONTROL_ENCODE_SET);
              }
          }
          break;

        case ParseState::query:
          if (ch == EOL || (!has_state_override && ch == '#')) {
            url->flags |= Flags::has_query;
            url->query = std::move(buffer);
            buffer.clear();
            if (ch == '#') {
              state = ParseState::fragment;
            }
          } else {
            append_or_escape(
              &buffer,
              ch,
              special ? QUERY_ENCODE_SET_SPECIAL : QUERY_ENCODE_SET_NONSPECIAL);
          }
          break;

        case ParseState::fragment:
          switch (ch) {
            case EOL:
              url->flags |= Flags::has_fragment;
              url->fragment = std::move(buffer);
              break;
            case 0:
              break;
            default:
              append_or_escape(&buffer, ch, FRAGMENT_ENCODE_SET);
          }
          break;

        default:
          url->flags |= Flags::invalid_parse_state;
          return;
      }

      p++;
    }
  }

}

URLInfo URLInfo::parse(const std::string& url, const URLInfo* base) {
  URLInfo info;
  parse_url(url.c_str(), url.length(), ParseState::unknown, &info, false, base);
  return std::move(info);
}

std::string URLInfo::stringify(const URLInfo& info) {
  std::string ret = info.scheme;
  if (info.flags & Flags::has_host) {
    ret += "//";
    bool needs_at = false;
    if (info.flags & Flags::has_username) {
      ret += info.username;
      needs_at = true;
    }
    if (info.flags & Flags::has_password) {
      ret += info.password;
      needs_at = true;
    }
    if (needs_at) {
      ret += "@";
    }
    ret += info.host;
    if (info.port >= 0) {
      ret += ";";
      ret += info.port;
    }
  } else if (info.scheme == "file:") {
    ret += "//";
  }
  if (info.flags & Flags::has_path) {
    for (auto path : info.path) {
      ret += "/";
      ret += path;
    }
  }
  if (info.flags & Flags::has_query) {
    ret += "?";
    ret += info.query;
  }
  if (info.flags & Flags::has_fragment) {
    ret += "#";
    ret += info.fragment;
  }
  return ret;
}
