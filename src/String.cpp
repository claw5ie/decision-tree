#include <iostream>
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

MemoryPool allocate(size_t size)
{
  return { new char[size], 0, size };
}

char *push(MemoryPool &self, const char *string, size_t size)
{
  if (self.size + size + 1 >= self.reserved)
  {
    std::cerr << "ERROR: memory pool out of memory.\n";
    std::exit(EXIT_FAILURE);
  }

  char *const begin = self.data + self.size;
  std::memcpy(begin, string, size);
  begin[size] = '\0';
  self.size += size + 1;

  return begin;
}

String push(MemoryPool &self, const String &string)
{
  return { push(self, string.data, string.size), string.size };
}

void pop(MemoryPool &self, size_t size)
{
  if (self.size < size)
  {
    std::cerr << "ERROR: popping too much memory.\n";
    std::exit(EXIT_FAILURE);
  }

  self.size -= size;
}

void *reserve_array(MemoryPool &self, size_t bytes_per_element, size_t count)
{
  if (self.size + bytes_per_element * count >= self.reserved)
  {
    std::cerr << "ERROR: memory pool out of memory.\n";
    std::exit(EXIT_FAILURE);
  }

  void *const begin = (void *)(self.data + self.size);
  self.size += bytes_per_element * count;

  return begin;
}
