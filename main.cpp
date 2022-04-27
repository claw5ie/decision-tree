#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include <cassert>
#include <sys/stat.h>

struct Category
{
  std::string name;
  std::map<std::string, size_t> values;
};

struct Table
{
  enum AttributeType
  {
    ENUM,
    BOOL,
    INT32,
    DOUBLE
  };

  struct AttributeData
  {
    union
    {
      const Category *category;
      size_t value;
    } as_enum;

    union
    {
      bool value;
    } as_bool;

    union
    {
      int32_t value;
    } as_int32;

    union
    {
      double value;
    } as_double;
  };

  AttributeType *types;
  AttributeData *data;
  size_t cols,
    rows;
  char *raw_data;

  void allocate(size_t cols, size_t rows)
  {
    this->raw_data = new char[
      cols * sizeof(AttributeType) + (cols * rows) * sizeof(AttributeData)
      ];
    this->types = (AttributeType *)raw_data;
    this->data = (AttributeData *)(raw_data + cols * sizeof(AttributeType));
    this->cols = cols;
    this->rows = rows;
  }

  inline AttributeData &get(size_t col, size_t row)
  {
    return data[row * cols + col];
  }

  inline const AttributeData &get(size_t col, size_t row) const
  {
    return data[row * cols + col];
  }

  void deallocate()
  {
    delete[] raw_data;
  }
};

const char *read_int32(const char *stream, int32_t &dest)
{
  if (
    !(std::isdigit(stream[0]) ||
      ((stream[0] == '-' || stream[0] == '+') && std::isdigit(stream[1])))
    )
  {
    return nullptr;
  }

  const bool should_be_negative = *stream == '-';
  stream += should_be_negative;

  int32_t value = 0;

  for (; std::isdigit(*stream); stream++)
    value = value * 10 + (*stream - '0');

  dest = should_be_negative ? -value : value;

  return stream;
}

const char *read_double(const char *stream, double &dest)
{
  if (
    !(std::isdigit(stream[0]) ||
      ((stream[0] == '-' || stream[0] == '+' || stream[0] == '.') &&
       std::isdigit(stream[1])))
    )
  {
    return nullptr;
  }

  const bool should_be_negative = *stream == '-';
  stream += should_be_negative;

  double value = 0;

  for (; std::isdigit(*stream); stream++)
    value = value * 10 + (*stream - '0');

  stream += *stream == '.';

  for (double pow = 0.1; std::isdigit(*stream); pow /= 10, stream++)
    value += (*stream - '0') * pow;

  dest = should_be_negative ? -value : value;

  return stream;
}

Table read_csv(const char *const filepath)
{
  const size_t file_size =
    [filepath]() -> size_t
    {
      struct stat stats;

      if (stat(filepath, &stats) == -1)
      {
        std::cerr << "error: couldn't stat the file \"" << filepath << "\".\n";
        exit(EXIT_FAILURE);
      }

      return stats.st_size;
    }();

  std::fstream file(filepath, std::fstream::in);

  if (!file.is_open())
  {
    std::cerr << "error: failed to open the file \"" << filepath << "\".\n";
    exit(EXIT_FAILURE);
  }

  char *const file_data = new char[(file_size + 1) * sizeof(char)];
  file.read(file_data, file_size);
  file_data[file_size] = '\0';
  file.close();

  // validate_and_collect_some_info(file_data, file_size);

  delete[] file_data;

  return Table{ };
}

int main(int argc, char **argv)
{
  assert(argc == 2);

  double value;

  auto end = read_double(argv[1], value);

  printf("value: %f, %s\n", value, end);
}
