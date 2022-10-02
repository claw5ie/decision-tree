#ifndef STRING_HPP
#define STRING_HPP

#include <cstddef>
#include <cstdint>

struct StringView
{
  const char *data;
  size_t size;
};

struct String
{
  char *data;
  size_t size;
};

struct StringComparator
{
  bool operator()(const String &left, const String &right) const;
};

int32_t compare(const String &left, const String &right);

struct MemoryPool
{
  char *data;
  size_t size,
    reserved;
};

MemoryPool allocate(size_t size);

char *push(MemoryPool &self, const char *string, size_t size);

String push(MemoryPool &self, const String &string);

void pop(MemoryPool &self, size_t size);

void *reserve_array(MemoryPool &self, size_t bytes_per_element, size_t count);

#endif // STRING_HPP
