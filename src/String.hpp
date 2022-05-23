#ifndef STRING_HPP
#define STRING_HPP

#include <cstddef>
#include <cstdint>

struct String
{
  char *data;
  size_t size;
};

struct StringComparator
{
  bool operator()(const String &left, const String &right);
};

int32_t compare(const String &left, const String &right);

String copy(const String &string);

String copy(const char *begin, const char *end);

#endif // STRING_HPP
