#include <iostream>
#include <fstream>
#include <limits>
#include <cassert>
#include <sys/stat.h>
#include "Table.hpp"
#include "Utils.hpp"

#define MEMORY_POOL_SIZE (1024 * 1024)

const char *to_string(AttributeType type)
{
  static const char *const lookup[] = {
    "String",
    "Int64",
    "Float64",
    "Interval"
  };

  return type < sizeof (lookup) / sizeof (*lookup) ? lookup[type] : nullptr;
}

void read_and_emplace_cell(MemoryPool &pool, Table::Cell &cell, const char *&str)
{
  auto const guess_type =
    [](const char *str)
    {
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
            return AttributeType::INTERVAL;
          else
            return AttributeType::FLOAT64;
        }
        else if (str[0] == '-' && is_number(str + 1))
        {
          return AttributeType::INTERVAL;
        }
        else
        {
          return AttributeType::INT64;
        }
      }
      else if ((str[0] == '<' || str[0] == '>') && is_number(str + 1))
      {
        return AttributeType::INTERVAL;
      }
      else if (str[0] == '$')
      {
        return AttributeType::INT64;
      }
      else
      {
        return AttributeType::STRING;
      }
    };

  cell.type = guess_type(str);

  switch (cell.type)
  {
  case AttributeType::STRING:
  {
    const char *const start = str;

    while (*str != ',' && *str != '\n' && *str != '\0')
      str++;

    cell.as.string.size = str - start;
    cell.as.string.data = push(pool, start, cell.as.string.size);
  } break;
  case AttributeType::INT64:
    if (*str == '$')
    {
      for (cell.as.int64 = 0; *str== '$'; str++)
        cell.as.int64++;
    }
    else
    {
      cell.as.int64 = read_int64(str);
    }
    break;
  case AttributeType::FLOAT64:
    cell.as.float64 = read_float64(str);
    break;
  case AttributeType::INTERVAL:
    if (*str == '<')
    {
      cell.as.interval.min = std::numeric_limits<double>::lowest();
      cell.as.interval.max = read_float64(++str);
    }
    else if (*str == '>')
    {
      cell.as.interval.min = read_float64(++str);
      cell.as.interval.max = std::numeric_limits<double>::max();
    }
    else
    {
      cell.as.interval.min = read_float64(str);
      cell.as.interval.max = read_float64(++str);

      if (cell.as.interval.min > cell.as.interval.max)
        std::swap(cell.as.interval.min, cell.as.interval.max);
    }
    break;
  }
}

Table read_csv(const char *filepath)
{
  const char *const file_data = read_entire_file(filepath);
  const char *curr = file_data;

  Table table;

  {
    table.cols = 1;
    table.rows = 1;

    const char *curr = file_data;

    // Count columns.
    while (*curr != '\n' && *curr != '\0')
    {
      table.cols += *curr == ',';
      curr++;
    }

    // Count rows.
    while (*curr != '\0')
    {
      table.rows += *curr == '\n';
      curr++;
    }
  }

  table.data = new Table::Cell[table.cols * table.rows];
  table.pool = allocate(MEMORY_POOL_SIZE);

  for (size_t i = 0; i < table.rows; i++)
  {
    for (size_t j = 0; j < table.cols; j++)
    {
      read_and_emplace_cell(table.pool, table.data[i * table.cols + j], curr);

      require_char(
        *curr,
        j + 1 < table.cols ? ',' : (i + 1 < table.rows ? '\n' : '\0')
        );

      curr += *curr != '\0';
    }
  }

  delete[] file_data;

  return table;
}

void clean(const Table &self)
{
  delete[] self.data;
  delete[] self.pool.data;
}

const Table::Cell &get(const Table &self, size_t row, size_t column)
{
  assert(row < self.rows && column < self.cols);

  return self.data[row * self.cols + column];
}

Table::Cell &get(Table &self, size_t row, size_t column)
{
  assert(row < self.rows && column < self.cols);

  return self.data[row * self.cols + column];
}

void print(const Table &self)
{
  for (size_t i = 0; i < self.rows; i++)
  {
    for (size_t j = 0; j < self.cols; j++)
    {
      auto &cell = self.data[i * self.cols + j];

      std::cout << to_string(cell.type) << ": ";

      switch (cell.type)
      {
      case AttributeType::STRING:
        std::cout << cell.as.string.data;
        break;
      case AttributeType::INT64:
        std::cout << cell.as.int64;
        break;
      case AttributeType::FLOAT64:
        std::cout << cell.as.int64;
        break;
      case AttributeType::INTERVAL:
        std::cout << '[' << cell.as.interval.min
                  << ", "
                  << cell.as.interval.max << ']';
        break;
      }

      std::cout << (j + 1 < self.cols ? ',' : '\n');
    }
  }
}

StringView to_string(const Table::Cell &value)
{
  constexpr size_t buffer_size = 128;
  static char buffer[buffer_size];

  int bytes_written = -1;

  switch (value.type)
  {
  case AttributeType::STRING:
    bytes_written = std::snprintf(
      buffer, buffer_size, "%s", value.as.string.data
      );
    break;
  case AttributeType::INT64:
    bytes_written = std::snprintf(
      buffer, buffer_size, "%li", value.as.int64
      );
    break;
  case AttributeType::FLOAT64:
    bytes_written = std::snprintf(
      buffer, buffer_size, "%f", value.as.float64
      );
    break;
  case AttributeType::INTERVAL:
    bytes_written = std::snprintf(
      buffer,
      buffer_size,
      "[%f, %f]",
      value.as.interval.min,
      value.as.interval.max
      );
    break;
  }

  if (bytes_written < 0 || bytes_written >= (int)buffer_size)
  {
    std::cerr << "ERROR: failed to convert value to a string.\n";
    std::exit(EXIT_FAILURE);
  }

  return { buffer, (size_t)bytes_written };
}

bool promote(Table &self, AttributeType to, const Table::Selection &sel)
{
  assert_selection_is_valid(self, sel);

  bool converted_every_cell = true;

  switch (to)
  {
  case AttributeType::INTERVAL:
    for (size_t row = sel.row_beg; row < sel.row_end; row++)
    {
      for (size_t col = sel.col_beg; col < sel.col_end; col++)
      {
        auto &cell = get(self, row, col);

        switch (cell.type)
        {
        case AttributeType::STRING:
          converted_every_cell = false;
          break;
        case AttributeType::INT64:
        {
          int64_t const value = cell.as.int64;

          cell.type = to;
          cell.as.interval.min = std::numeric_limits<double>::lowest();
          cell.as.interval.max = value;
        } break;
        case AttributeType::FLOAT64:
        {
          double const value = cell.as.float64;

          cell.type = to;
          cell.as.interval.min = std::numeric_limits<double>::lowest();
          cell.as.interval.max = value;
        } break;
        case AttributeType::INTERVAL:
          break;
        }
      }
    }
    break;
  default:
    std::cerr << "ERROR: cannot convert cells to type `"
              << to_string(to)
              << "`.\n";
    std::exit(EXIT_FAILURE);
  }

  return converted_every_cell;
}

void assert_selection_is_valid(const Table &self, const Table::Selection &sel)
{
  if (sel.row_beg >= self.rows || sel.row_end > self.rows ||
      sel.col_beg >= self.cols || sel.col_end > self.cols ||
      sel.row_beg > sel.row_end || sel.col_beg > sel.col_end)
  {
    std::cerr << "ERROR: invalid selection.\n";
    std::exit(EXIT_FAILURE);
  }
}

bool have_same_type(const Table &self, const Table::Selection &sel)
{
  assert_selection_is_valid(self, sel);

  size_t beg = sel.row_beg * self.cols + sel.col_beg;
  size_t const end = sel.row_end * self.cols + sel.col_end;

  for (; beg + 1 < end; beg++)
  {
    if (self.data[beg].type != self.data[beg + 1].type)
      return false;
  }

  return true;
}
