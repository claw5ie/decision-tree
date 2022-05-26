#include <iostream>
#include <fstream>
#include <cstdarg>
#include <cassert>
#include "sys/stat.h"
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

bool is_number(const char *str)
{
  return std::isdigit(str[0]) ||
    ((str[0] == '-' || str[0] == '+') && std::isdigit(str[1]));
}

int64_t read_int64(const char *&str)
{
  if (!is_number(str))
  {
    std::cerr << "ERROR: no number to read.\n";
    std::exit(EXIT_FAILURE);
  }

  bool const should_be_negative = *str == '-';

  str += should_be_negative || *str == '+';

  int64_t val = 0;
  for (; std::isdigit(*str); str++)
    val = val * 10 + (*str - '0');

  if (should_be_negative)
    val = -val;

  return val;
}

size_t read_zu(const char *&str)
{
  if (*str == '-' || !std::isdigit(*str) ||
      (*str == '+' && !std::isdigit(str[1])))
  {
    std::cerr << "ERROR: no number to read.\n";
    std::exit(EXIT_FAILURE);
  }

  str += *str == '+';

  size_t val = 0;
  for (; std::isdigit(*str); str++)
    val = val * 10 + (*str - '0');

  return val;
}

double read_float64(const char *&str)
{
  if (!is_number(str))
  {
    std::cerr << "ERROR: no number to read.\n";
    std::exit(EXIT_FAILURE);
  }

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

void require_char(char actual, char expected)
{
  if (expected != actual)
  {
    std::cerr << "ERROR: expected character `"
              << expected
              << "`, but got `"
              << actual
              << "`\n";
    std::exit(EXIT_FAILURE);
  }
}

char *read_entire_file(const char *filepath)
{
  size_t file_size = 0;

  {
    struct stat stats;

    if (stat(filepath, &stats) == -1)
    {
      std::cerr << "ERROR: failed to stat the file `"
                << filepath
                << "`.\n";
      std::exit(EXIT_FAILURE);
    }

    file_size = stats.st_size;
  }

  char *const file_data = new char[file_size + 1];

  std::fstream file(filepath, std::fstream::in);

  if (!file.is_open())
  {
    std::cerr << "ERROR: failed to open the file `" << filepath << "`.\n";
    std::exit(EXIT_FAILURE);
  }

  file.read(file_data, file_size);
  file_data[file_size] = '\0';
  file.close();

  return file_data;
}
