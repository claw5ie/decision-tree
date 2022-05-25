#include <iostream>
#include <fstream>
#include <limits>
#include <cassert>
#include <sys/stat.h>
#include "Table.hpp"
#include "Utils.hpp"

const char *to_string(Attribute::Type type)
{
  static const char *const lookup[] = {
    "String",
    "Int64",
    "Float64",
    "Interval"
  };

  return type < sizeof (lookup) / (sizeof (*lookup)) ? lookup[type] : nullptr;
}

bool is_delimiter(char ch)
{
  return ch == ',' || ch == '\n' || ch == '\0';
}

Attribute::Value read_attribute_value(const char *&str)
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
            return Attribute::INTERVAL;
          else
            return Attribute::FLOAT64;
        }
        else if (str[0] == '-' && is_number(str + 1))
        {
          return Attribute::INTERVAL;
        }
        else
        {
          return Attribute::INT64;
        }
      }
      else if ((str[0] == '<' || str[0] == '>') && is_number(str + 1))
      {
        return Attribute::INTERVAL;
      }
      else if (str[0] == '$')
      {
        return Attribute::INT64;
      }
      else
      {
        return Attribute::STRING;
      }
    };

  Attribute::Value value;

  value.type = guess_type(str);

  switch (value.type)
  {
  case Attribute::STRING:
  {
    const char *const start = str;

    while (!is_delimiter(*str))
      str++;

    value.as.string = copy(start, str);
  } break;
  case Attribute::INT64:
    if (*str == '$')
    {
      for (value.as.int64 = 0; *str== '$'; str++)
        value.as.int64++;
    }
    else
    {
      value.as.int64 = read_int64(str);
    }
    break;
  case Attribute::FLOAT64:
    value.as.float64 = read_float64(str);
    break;
  case Attribute::INTERVAL:
    if (*str == '<')
    {
      value.as.interval.min = std::numeric_limits<double>::lowest();
      value.as.interval.max = read_float64(++str);
    }
    else if (*str == '>')
    {
      value.as.interval.min = read_float64(++str);
      value.as.interval.max = std::numeric_limits<double>::max();
    }
    else
    {
      value.as.interval.min = read_float64(str);
      value.as.interval.max = read_float64(++str);

      if (value.as.interval.min > value.as.interval.max)
        std::swap(value.as.interval.min, value.as.interval.max);
    }
    break;
  }

  return value;
}

void find_table_size(const char *string, size_t &rows, size_t &cols)
{
  cols = 1;
  rows = 1;

  // Count columns.
  while (*string != '\n' && *string != '\0')
  {
    cols += *string == ',';
    string++;
  }

  // Count rows.
  while (*string != '\0')
  {
    rows += *string == '\n';
    string++;
  }
}

TableColumnMajor read_csv_column_major(const char *filepath)
{
  char *const file_data = read_entire_file(filepath);

  size_t cols;
  size_t rows;

  find_table_size(file_data, rows, cols);

  // Exclude the first row.
  --rows;

  auto *const columns = new TableColumnMajor::Column[cols]{ };

  const char *curr = file_data;

  for (size_t i = 0; i < cols; i++)
  {
    const char *const start = curr;

    while (!is_delimiter(*curr))
      curr++;

    if (i + 1 < cols)
      require_char(*curr, ',');
    else
      require_char(*curr, '\n');

    columns[i].name = copy(start, curr);
    curr += *curr != '\0';
  }

  // Could loop one time and initialize all columns, but requires copy-pasting
  // some code. Probably not worth it, although this will eliminate the need of
  // "is_initialized" variable. Also, making redundant checks each time, even
  // though the "else" statement will only be executed once is kinda meh...
  for (size_t i = 0; i < rows; i++)
  {
    for (size_t j = 0; j < cols; j++)
    {
      Attribute::Value value = read_attribute_value(curr);

      auto &column = columns[j];

      if (!column.is_initialized)
      {
        column.type = value.type;
        column.is_initialized = true;

        switch (value.type)
        {
        case Attribute::STRING:
          column.as.strings = new String[rows];
          break;
        case Attribute::INT64:
          column.as.int64s = new int64_t[rows];
          break;
        case Attribute::FLOAT64:
          column.as.float64s = new double[rows];
          break;
        case Attribute::INTERVAL:
          column.as.intervals = new Interval[rows];
          break;
        }
      }

      if (column.type != value.type)
      {
        std::cerr << "ERROR: value at column "
                  << j + 1
                  << ", row "
                  << i + 2
                  << " expects to see `"
                  << to_string(column.type)
                  << "`, but got `"
                  << to_string(value.type)
                  << "`.\n  Note: different types of values in the same column "
          "are not allowed.\n";
        std::exit(EXIT_FAILURE);
      }

      switch (column.type)
      {
      case Attribute::STRING:
        column.as.strings[i] = value.as.string;
        break;
      case Attribute::INT64:
        column.as.int64s[i] = value.as.int64;
        break;
      case Attribute::FLOAT64:
        column.as.float64s[i] = value.as.float64;
        break;
      case Attribute::INTERVAL:
        column.as.intervals[i] = value.as.interval;
        break;
      }

      if (j + 1 < cols)
        require_char(*curr, ',');
      else if (i + 1 < rows)
        require_char(*curr, '\n');
      else
        require_char(*curr, '\0');

      curr += *curr != '\0';
    }
  }

  delete[] file_data;

  return { columns, cols, rows };
}

void clean(const TableColumnMajor &self)
{
  for (size_t i = 0; i < self.cols; i++)
  {
    auto &column = self.columns[i];

    switch (column.type)
    {
    case Attribute::STRING:
      for (size_t i = 0; i < self.rows; i++)
        delete[] column.as.strings[i].data;
      delete[] column.as.strings;
      break;
    case Attribute::INT64:
      delete[] column.as.int64s;
      break;
    case Attribute::FLOAT64:
      delete[] column.as.float64s;
      break;
    case Attribute::INTERVAL:
      delete[] column.as.intervals;
      break;
    }

    delete[] column.name.data;
  }

  delete[] self.columns;
}

Attribute::Value get(const TableColumnMajor &self, size_t col, size_t row)
{
  assert(col < self.cols && row < self.rows);

  auto &column = self.columns[col];
  Attribute::Value value;

  value.type = column.type;

  switch (column.type)
  {
  case Attribute::STRING:
    value.as.string = column.as.strings[row];
    break;
  case Attribute::INT64:
    value.as.int64 = column.as.int64s[row];
    break;
  case Attribute::FLOAT64:
    value.as.float64 = column.as.float64s[row];
    break;
  case Attribute::INTERVAL:
    value.as.interval = column.as.intervals[row];
    break;
  }

  return value;
}

void print(const TableColumnMajor &self)
{
  for (size_t i = 0; i < self.cols; i++)
  {
    auto &column = self.columns[i];

    std::cout << column.name.data << ", " << column.type << ": ";

    for (size_t j = 0; j < self.rows; j++)
    {
      switch (column.type)
      {
      case Attribute::STRING:
        std::cout << column.as.strings[j].data;
        break;
      case Attribute::INT64:
        std::cout << column.as.int64s[j];
        break;
      case Attribute::FLOAT64:
        std::cout << column.as.float64s[j];
        break;
      case Attribute::INTERVAL:
      {
        auto &interval = column.as.intervals[j];
        std::cout << interval.min << '-' << interval.max;
      } break;
      }

      std::cout << (j + 1 < self.rows ? ' ' : '\n');
    }
  }
}

TableRowMajor read_csv_row_major(const char *filepath)
{
  char *const file_data = read_entire_file(filepath);
  const char *curr = file_data;

  TableRowMajor table;

  find_table_size(curr, table.rows, table.cols);

  table.data = new Attribute::Value[table.cols * table.rows];

  for (size_t i = 0; i < table.rows; i++)
  {
    for (size_t j = 0; j < table.cols; j++)
    {
      table.data[i * table.cols + j] = read_attribute_value(curr);

      if (j + 1 < table.cols)
        require_char(*curr, ',');
      else if (i + 1 < table.rows)
        require_char(*curr, '\n');
      else
        require_char(*curr, '\0');

      curr += *curr != '\0';
    }
  }

  delete[] file_data;

  return table;
}

void clean(const TableRowMajor &self)
{
  for (size_t i = 0; i < self.cols * self.rows; i++)
  {
    if (self.data[i].type == Attribute::STRING)
      delete[] self.data[i].as.string.data;
  }

  delete[] self.data;
}
