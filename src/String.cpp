#include <cstring>
#include "String.hpp"

bool StringComparator::operator()(const String &left, const String &right) const
{
  return compare(left, right) < 0;
}

int32_t compare(const String &left, const String &right)
{
  for (size_t i = 0; i < left.size && i < right.size; i++)
  {
    if (left.data[i] != right.data[i])
      return left.data[i] - right.data[i];
  }

  return left.size - right.size;
}

String copy(const String &string)
{
  char *const data = new char[string.size + 1];

  std::memcpy(data, string.data, string.size);
  data[string.size] = '\0';

  return { data, string.size };
}

String copy(const char *begin, const char *end)
{
  size_t const size = end - begin;
  char *const data = new char[size + 1];

  std::memcpy(data, begin, size);
  data[size] = '\0';

  return { data, size };
}
