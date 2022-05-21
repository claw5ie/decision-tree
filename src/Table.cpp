#include <iostream>
#include <fstream>
#include <limits>
#include <cassert>
#include <sys/stat.h>
#include "Table.hpp"
#include "Utils.hpp"

bool operator<(const Interval &left, const Interval &right)
{
  if (left.min >= right.min)
    return left.max < right.max;
  else
    return true;
}

bool is_delimiter(char ch)
{
  return ch == ',' || ch == '\n' || ch == '\0';
}

struct Cell
{
  Table::Attribute::Type type;

  union
  {
    struct
    {
      const char *text;
      size_t size;
    } string;

    int32_t int32;

    double float64;

    Interval interval;
  } as;
};

const char *parse_cell(const char *str, Cell &cell)
{
  auto const guess_type =
    [](const char *str)
    {
      auto const is_number =
        [](const char *str) -> bool
        {
          return std::isdigit(str[0]) ||
            ((str[0] == '-' || str[0] == '+') && std::isdigit(str[1]));
        };

      if (is_number(str))
      {
        str += str[0] == '-' || str[0] == '+';

        while (std::isdigit(str[0]))
          str++;

        if (str[0] == '.')
        {
          ++str;
          while (std::isdigit(str[0]))
            str++;

          if (str[0] == '-' && is_number(str + 1))
            return Table::Attribute::INTERVAL;
          else
            return Table::Attribute::FLOAT64;
        }
        else if (str[0] == '-' && is_number(str + 1))
        {
          return Table::Attribute::INTERVAL;
        }
        else
        {
          return Table::Attribute::INT32;
        }
      }
      else if ((str[0] == '<' || str[0] == '>') && is_number(str + 1))
      {
        return Table::Attribute::INTERVAL;
      }
      else if (str[0] == '$')
      {
        return Table::Attribute::INT32;
      }
      else
      {
        return Table::Attribute::CATEGORY;
      }
    };

  cell.type = guess_type(str);

  switch (cell.type)
  {
  case Table::Attribute::CATEGORY:
  {
    cell.as.string.text = str;

    while (!is_delimiter(*str))
      str++;

    cell.as.string.size = str - cell.as.string.text;

    return str;
  }
  case Table::Attribute::INT32:
  {
    if (*str == '$')
    {
      for (cell.as.int32 = 0; *str== '$'; str++)
        cell.as.int32++;
    }
    else
    {
      str = parse_int32(str, cell.as.int32);
    }

    return str;
  }
  case Table::Attribute::FLOAT64:
  {
    str = parse_float64(str, cell.as.float64);

    return str;
  }
  case Table::Attribute::INTERVAL:
  {
    if (*str == '<')
    {
      cell.as.interval.min = std::numeric_limits<double>::lowest();
      str = parse_float64(++str, cell.as.interval.max);

      return str;
    }
    else if (*str == '>')
    {
      cell.as.interval.max = std::numeric_limits<double>::max();
      str = parse_float64(++str, cell.as.interval.min);

      return str;
    }
    else
    {
      str = parse_float64(str, cell.as.interval.min);
      str = parse_float64(++str, cell.as.interval.max);

      if (cell.as.interval.min > cell.as.interval.max)
        std::swap(cell.as.interval.min, cell.as.interval.max);

      return str;
    }
  }
  default:
    assert(false);
  }
}

void Table::Attribute::insert_category(size_t row, const char *string, size_t size)
{
  auto const it = as.category.names->emplace(
    std::string(string, size), as.category.names->size()
    );
  as.category.data[row] = it.first->second;
}

void Table::read_csv(const char *filepath)
{
  size_t const file_size =
    [filepath]() -> size_t
    {
      struct stat stats;

      if (stat(filepath, &stats) == -1)
      {
        std::cerr << "ERROR: failed to stat the file `"
                  << filepath
                  << "`.\n";
        std::exit(EXIT_FAILURE);
      }

      return stats.st_size;
    }();

  char *const file_data = new char[file_size + 1];

  {
    std::fstream file(filepath, std::fstream::in);

    if (!file.is_open())
    {
      std::cerr << "ERROR: failed to open the file `" << filepath << "`.\n";
      std::exit(EXIT_FAILURE);
    }

    file.read(file_data, file_size);
    file_data[file_size] = '\0';
    file.close();
  }

  rows = 0;
  cols = 1;

  {
    const char *ch = file_data;

    // Count columns.
    while (*ch != '\n' && *ch != '\0')
    {
      cols += *ch == ',';
      ch++;
    }

    // Count rows. First line is not counted.
    while (*ch != '\0')
    {
      rows += *ch == '\n';
      ch++;
    }
  }

  columns = new Table::Attribute[cols]{ };

  const char *curr = file_data;

  for (size_t i = 0; i < cols; i++)
  {
    const char *const start = curr;

    while (!is_delimiter(*curr))
      curr++;

    assert(*curr != '\0');
    columns[i].name = std::string(start, curr);

    curr += *curr != '\0';
  }

  // Could loop one time and initialize all columns, but requires copy-pasting
  // some code. Probably not worth it, although this will eliminate the need of
  // "is_initialized" variable. Also, making redundant checks each time, even
  // though the "else" statement will only be executed once is kinda meh...
  for (size_t j = 0; j < rows; j++)
  {
    for (size_t i = 0; i < cols; i++)
    {
      Cell value;
      curr = parse_cell(curr, value);

      auto &column = columns[i];
      if (column.is_initialized)
      {
        assert(column.type == value.type);

        switch (column.type)
        {
        case Table::Attribute::CATEGORY:
          column.insert_category(
            j, value.as.string.text, value.as.string.size
            );
          break;
        case Table::Attribute::INT32:
          column.as.int32s[j] = value.as.int32;
          break;
        case Table::Attribute::FLOAT64:
          column.as.float64s[j] = value.as.float64;
          break;
        case Table::Attribute::INTERVAL:
          column.as.intervals[j] = value.as.interval;
          break;
        }
      }
      else
      {
        column.type = value.type;
        column.is_initialized = true;

        switch (value.type)
        {
        case Table::Attribute::CATEGORY:
          column.as.category.names =
            new std::map<std::string, CategoryValue>;
          column.as.category.data = new CategoryValue[rows];
          column.insert_category(
            0, value.as.string.text, value.as.string.size
            );
          break;
        case Table::Attribute::INT32:
          column.as.int32s = new int32_t[rows];
          column.as.int32s[0] = value.as.int32;
          break;
        case Table::Attribute::FLOAT64:
          column.as.float64s = new double[rows];
          column.as.float64s[0] = value.as.float64;
          break;
        case Table::Attribute::INTERVAL:
          column.as.intervals = new Interval[rows];
          column.as.intervals[0] = value.as.interval;
          break;
        }
      }

      assert(is_delimiter(*curr));
      curr += *curr != '\0';
    }
  }

  delete[] file_data;
}

Table::Attribute::Value Table::get(size_t col, size_t row) const
{
  Attribute::Value data;

  auto &column = columns[col];

  switch (column.type)
  {
  case Attribute::CATEGORY:
    data.category = column.as.category.data[row];
    break;
  case Attribute::INT32:
    data.int32 = column.as.int32s[row];
    break;
  case Attribute::FLOAT64:
    data.float64 = column.as.float64s[row];
    break;
  case Attribute::INTERVAL:
    data.interval = column.as.intervals[row];
    break;
  }

  return data;
}

void Table::clean()
{
  for (size_t i = 0; i < cols; i++)
  {
    auto &column = columns[i];

    switch (column.type)
    {
    case Table::Attribute::CATEGORY:
      delete column.as.category.names;
      delete[] column.as.category.data;
      break;
    case Table::Attribute::INT32:
      delete[] column.as.int32s;
      break;
    case Table::Attribute::FLOAT64:
      delete[] column.as.float64s;
      break;
    case Table::Attribute::INTERVAL:
      delete[] column.as.intervals;
      break;
    }
  }

  delete[] columns;
}

void Table::print() const
{
  for (size_t i = 0; i < cols; i++)
  {
    auto &column = columns[i];

    std::cout << column.name << ", " << column.type << ": ";

    for (size_t j = 0; j < rows; j++)
    {
      switch (column.type)
      {
      case Table::Attribute::CATEGORY:
        std::cout << column.as.category.data[j];
        break;
      case Table::Attribute::INT32:
        std::cout << column.as.int32s[j];
        break;
      case Table::Attribute::FLOAT64:
        std::cout << column.as.float64s[j];
        break;
      case Table::Attribute::INTERVAL:
      {
        auto &interval = column.as.intervals[j];
        std::cout << interval.min << '-' << interval.max;
      } break;
      }

      std::cout << (j + 1 < rows ? ' ' : '\n');
    }
  }
}
