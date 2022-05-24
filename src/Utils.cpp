#include <iostream>
#include <cstdarg>
#include <cassert>
#include "Utils.hpp"

char *allocate_in_chunks(size_t *offsets, size_t count, ...)
{
  assert(count > 0);

  va_list args;
  va_start(args, count);

  offsets[0] = va_arg(args, size_t);
  for (size_t i = 1; i < count; i++)
    offsets[i] = offsets[i - 1] + va_arg(args, size_t);

  va_end(args);

  char *const data = new char[offsets[count - 1]];

  return data;
}

void putsn(const char *string, size_t count)
{
  while (count-- > 0)
    std::cout << string;
}

int64_t read_int64(const char *&str)
{
  bool const should_be_negative = *str == '-';

  str += should_be_negative || *str == '+';

  int64_t val = 0;
  for (; std::isdigit(*str); str++)
    val = val * 10 + (*str - '0');

  if (should_be_negative)
    val = -val;

  return val;
}

double read_float64(const char *&str)
{
  bool const should_be_negative = *str == '-';

  str += should_be_negative || *str == '+';

  double val = 0;
  for (; std::isdigit(*str); str++)
    val = val * 10 + (*str - '0');

  if (*str == '.' && std::isdigit(str[1]))
  {
    double pow = 0.1;

    for (str++; std::isdigit(*str); str++, pow /= 10)
      val += (*str - '0') * pow;
  }

  if (should_be_negative)
    val = -val;

  return val;
}
