#include <iostream>
#include <fstream>
#include <limits>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cassert>

#include <sys/stat.h>

#include "decision-tree.hpp"

using namespace std;

#define MEMORY_POOL_SIZE (1024 * 1024)

char *
allocate_in_chunks(size_t *offsets, size_t count, ...)
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

void
putsn(const char *string, size_t count)
{
  while (count-- > 0)
    cout << string;
}

bool
is_number(const char *str)
{
  return isdigit(str[0])
    || ((str[0] == '-' || str[0] == '+') && isdigit(str[1]));
}

int64_t
read_int64(const char *&str)
{
  if (!is_number(str))
    {
      cerr << "ERROR: no number to read.\n";
      exit(EXIT_FAILURE);
    }

  bool const should_be_negative = *str == '-';

  str += should_be_negative || *str == '+';

  int64_t val = 0;
  for (; isdigit(*str); str++)
    val = val * 10 + (*str - '0');

  if (should_be_negative)
    val = -val;

  return val;
}

size_t
read_zu(const char *&str)
{
  if (*str == '-' || !isdigit(*str)
      || (*str == '+' && !isdigit(str[1])))
    {
      cerr << "ERROR: no number to read.\n";
      exit(EXIT_FAILURE);
    }

  str += *str == '+';

  size_t val = 0;
  for (; isdigit(*str); str++)
    val = val * 10 + (*str - '0');

  return val;
}

double
read_float64(const char *&str)
{
  if (!is_number(str))
    {
      cerr << "ERROR: no number to read.\n";
      exit(EXIT_FAILURE);
    }

  bool const should_be_negative = *str == '-';

  str += should_be_negative || *str == '+';

  double val = 0;
  for (; isdigit(*str); str++)
    val = val * 10 + (*str - '0');

  if (*str == '.' && isdigit(str[1]))
    {
      double pow = 0.1;

      for (str++; isdigit(*str); str++, pow /= 10)
        val += (*str - '0') * pow;
    }

  if (should_be_negative)
    val = -val;

  return val;
}

void
require_char(char actual, char expected)
{
  if (expected != actual)
    {
      cerr << "ERROR: expected character `"
           << expected
           << "`, but got `"
           << actual
           << "`\n";
      exit(EXIT_FAILURE);
    }
}

char *
read_entire_file(const char *filepath)
{
  size_t file_size = 0;

  {
    struct stat stats;

    if (stat(filepath, &stats) == -1)
      {
        cerr << "ERROR: failed to stat the file `"
             << filepath
             << "`.\n";
        exit(EXIT_FAILURE);
      }

    file_size = stats.st_size;
  }

  char *const file_data = new char[file_size + 1];

  fstream file(filepath, fstream::in);

  if (!file.is_open())
    {
      cerr << "ERROR: failed to open the file `"
           << filepath
           << "`.\n";
      exit(EXIT_FAILURE);
    }

  file.read(file_data, file_size);
  file_data[file_size] = '\0';
  file.close();

  return file_data;
}

int32_t
compare(const String &left, const String &right)
{
  for (size_t i = 0; i < left.size && i < right.size; i++)
    {
      if (left.data[i] != right.data[i])
        return left.data[i] - right.data[i];
    }

  return left.size - right.size;
}

bool
StringComparator::operator()(const String &left, const String &right) const
{
  return compare(left, right) < 0;
}

MemoryPool
allocate(size_t size)
{
  return { new char[size], 0, size };
}

char *
push(MemoryPool &self, const char *string, size_t size)
{
  if (self.size + size + 1 >= self.reserved)
    {
      cerr << "ERROR: memory pool out of memory.\n";
      exit(EXIT_FAILURE);
    }

  char *const begin = self.data + self.size;
  memcpy(begin, string, size);
  begin[size] = '\0';
  self.size += size + 1;

  return begin;
}

String
push(MemoryPool &self, const String &string)
{
  return { push(self, string.data, string.size), string.size };
}

void
pop(MemoryPool &self, size_t size)
{
  if (self.size < size)
    {
      cerr << "ERROR: popping too much memory.\n";
      exit(EXIT_FAILURE);
    }

  self.size -= size;
}

void *
reserve_array(MemoryPool &self, size_t bytes_per_element, size_t count)
{
  if (self.size + bytes_per_element * count >= self.reserved)
    {
      cerr << "ERROR: memory pool out of memory.\n";
      exit(EXIT_FAILURE);
    }

  void *const begin = (void *)(self.data + self.size);
  self.size += bytes_per_element * count;

  return begin;
}

size_t &
at_row_major(Matzu &self, size_t row, size_t column)
{
  assert(row < self.rows && column < self.cols);

  return self.data[row * self.cols + column];
}

size_t &
at_column_major(Matzu &self, size_t column, size_t row)
{
  assert(row < self.rows && column < self.cols);

  return self.data[column * self.rows + row];
}

size_t
at_column_major(const Matzu &self, size_t column, size_t row)
{
  assert(row < self.rows && column < self.cols);

  return self.data[column * self.rows + row];
}

bool
operator<(const Interval &left, const Interval &right)
{
  if (left.min >= right.min)
    return left.max < right.max;
  else
    return true;
}

size_t
binary_search_interval(double value, const SearchInterval &interval)
{
  assert(interval.count >= 2);

  size_t lower = 0,
    upper = interval.count - 1;

  while (lower < upper)
    {
      size_t const mid = lower + (upper - lower) / 2;
      double const beg = interval.min + mid * interval.step;

      if (value < beg)
        upper = mid;
      else if (value > beg + interval.step)
        lower = mid + 1;
      else
        return mid;
    }

  return size_t(-1);
}

const char *
to_string(AttributeType type)
{
  static const char *const lookup[] = {
    "String",
    "Int64",
    "Float64",
    "Interval"
  };

  return type < sizeof (lookup) / sizeof (*lookup) ? lookup[type] : nullptr;
}

void
read_and_emplace_cell(MemoryPool &pool, Table::Cell &cell, const char *&str)
{
  auto const guess_type =
    [](const char *str)
    {
      if (is_number(str))
        {
          str += str[0] == '-' || str[0] == '+';

          while (isdigit(str[0]))
            str++;

          if (str[0] == '.')
            {
              ++str;
              while (isdigit(str[0]))
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
          cell.as.interval.min = numeric_limits<double>::lowest();
          cell.as.interval.max = read_float64(++str);
        }
      else if (*str == '>')
        {
          cell.as.interval.min = read_float64(++str);
          cell.as.interval.max = numeric_limits<double>::max();
        }
      else
        {
          cell.as.interval.min = read_float64(str);
          cell.as.interval.max = read_float64(++str);

          if (cell.as.interval.min > cell.as.interval.max)
            swap(cell.as.interval.min, cell.as.interval.max);
        }
      break;
    }
}

Table
read_csv(const char *filepath)
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

void
clean(const Table &self)
{
  delete[] self.data;
  delete[] self.pool.data;
}

const Table::Cell &
get(const Table &self, size_t row, size_t column)
{
  assert(row < self.rows && column < self.cols);

  return self.data[row * self.cols + column];
}

Table::Cell &
get(Table &self, size_t row, size_t column)
{
  assert(row < self.rows && column < self.cols);

  return self.data[row * self.cols + column];
}

void
print(const Table &self)
{
  for (size_t i = 0; i < self.rows; i++)
    {
      for (size_t j = 0; j < self.cols; j++)
        {
          auto &cell = self.data[i * self.cols + j];

          cout << to_string(cell.type) << ": ";

          switch (cell.type)
            {
            case AttributeType::STRING:
              cout << cell.as.string.data;
              break;
            case AttributeType::INT64:
              cout << cell.as.int64;
              break;
            case AttributeType::FLOAT64:
              cout << cell.as.int64;
              break;
            case AttributeType::INTERVAL:
              cout << '[' << cell.as.interval.min
                   << ", "
                   << cell.as.interval.max << ']';
              break;
            }

          cout << (j + 1 < self.cols ? ',' : '\n');
        }
    }
}

StringView
to_string(const Table::Cell &value)
{
  constexpr size_t buffer_size = 128;
  static char buffer[buffer_size];

  int bytes_written = -1;

  switch (value.type)
    {
    case AttributeType::STRING:
      bytes_written
        = snprintf(buffer, buffer_size,
                   "%s", value.as.string.data);
      break;
    case AttributeType::INT64:
      bytes_written
        = snprintf(buffer, buffer_size, "%li", value.as.int64);
      break;
    case AttributeType::FLOAT64:
      bytes_written = snprintf(buffer, buffer_size,
                               "%g", value.as.float64);
      break;
    case AttributeType::INTERVAL:
      bytes_written
        = snprintf(buffer,
                   buffer_size,
                   "[%g, %g]",
                   value.as.interval.min,
                   value.as.interval.max);
      break;
    }

  if (bytes_written < 0 || bytes_written >= (int)buffer_size)
    {
      cerr << "ERROR: failed to convert value to a string.\n";
      exit(EXIT_FAILURE);
    }

  return { buffer, (size_t)bytes_written };
}

void
assert_selection_is_valid(const Table &self, const Table::Selection &sel)
{
  if (sel.row_beg >= self.rows || sel.row_end > self.rows
      || sel.col_beg >= self.cols || sel.col_end > self.cols
      || sel.row_beg > sel.row_end || sel.col_beg > sel.col_end)
    {
      cerr << "ERROR: invalid selection.\n";
      exit(EXIT_FAILURE);
    }
}

bool
promote(Table &self, AttributeType to, const Table::Selection &sel)
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
                    cell.as.interval.min = numeric_limits<double>::lowest();
                    cell.as.interval.max = value;
                  } break;
                case AttributeType::FLOAT64:
                  {
                    double const value = cell.as.float64;

                    cell.type = to;
                    cell.as.interval.min = numeric_limits<double>::lowest();
                    cell.as.interval.max = value;
                  } break;
                case AttributeType::INTERVAL:
                  break;
                }
            }
        }
      break;
    default:
      cerr << "ERROR: cannot convert cells to type `"
           << to_string(to)
           << "`.\n";
      exit(EXIT_FAILURE);
    }

  return converted_every_cell;
}

bool
have_same_type(const Table &self, const Table::Selection &sel)
{
  assert_selection_is_valid(self, sel);

  auto const expected = get(self, sel.row_beg, sel.col_beg).type;

  for (size_t row = sel.row_beg; row < sel.row_end; row++)
    {
      for (size_t col = sel.col_beg; col < sel.col_end; col++)
        {
          if (expected != get(self, row, col).type)
            return false;
        }
    }

  return true;
}

size_t
to_category(const Category &self, const Table::Cell &value)
{
  if (self.type != value.type)
    assert(self.type == value.type);

  switch (self.type)
    {
    case AttributeType::STRING:
      {
        auto &string = *self.as.string;
        auto const it = string.to.find(value.as.string);

        return it != string.to.end() ? it->second : INVALID_CATEGORY;
      }
    case AttributeType::INT64:
      {
        auto &int64 = *self.as.int64;

        if (int64.to.size() > 0)
          {
            auto const it = int64.to.find(value.as.int64);

            return it != int64.to.end() ? it->second : INVALID_CATEGORY;
          }
        else
          {
            return binary_search_interval(value.as.int64, int64.interval);
          }
      };
    case AttributeType::FLOAT64:
      return binary_search_interval(value.as.float64, self.as.float64->interval);
    case AttributeType::INTERVAL:
      {
        /*
          Reason for searching for lower bound is that "find" function can be extended
          in this way for any real number. That is, if "min" value is the smallest
          value for double, and "max" is some arbitrary value, this function should
          find the first interval to which "max" belongs to. Although, I am not sure
          that it will behave the same way as "find" when the interval is in the map.
        */
        auto &interval = *self.as.interval;
        auto const it = interval.to.lower_bound(value.as.interval);

        if (it != interval.to.end())
          {
            if (it->first.min <= value.as.interval.max &&
                value.as.interval.max <= it->first.max)
              return it->second;
          }

        return INVALID_CATEGORY;
      }
    }

  return INVALID_CATEGORY;
}

pair<const Table::Cell, bool>
from_category(const Category &self, size_t category)
{
  Table::Cell cell;
  bool succeeded = false;

  cell.type = self.type;

  switch (self.type)
    {
    case AttributeType::STRING:
      if (category < self.count)
        {
          cell.as.string = self.as.string->from[category];
          succeeded = true;
        }
      break;
    case AttributeType::INT64:
      {
        auto &int64 = *self.as.int64;

        if (int64.to.size() > 0 && category < int64.to.size())
          {
            cell.as.int64 = int64.from[category];
            succeeded = true;
          }
        else if (category < self.count)
          {
            cell.type = AttributeType::INTERVAL;
            cell.as.interval = {
              int64.interval.min + category * int64.interval.step,
              int64.interval.min + (category + 1) * int64.interval.step,
            };
            succeeded = true;
          }
      } break;
    case AttributeType::FLOAT64:
      if (category < self.count)
        {
          auto &interval = self.as.float64->interval;
          cell.type = AttributeType::INTERVAL;
          cell.as.interval = {
            interval.min + category * interval.step,
            interval.min + (category + 1) * interval.step
          };
          succeeded = true;
        }
      break;
    case AttributeType::INTERVAL:
      if (category < self.count)
        {
          cell.as.interval = self.as.interval->from[category];
          succeeded = true;
        }
      break;
    }

  return pair<Table::Cell, bool>(cell, succeeded);
}

Categories
discretize(const Table &table, const Table::Selection &sel)
{
  assert_selection_is_valid(table, sel);

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
    {
      for (size_t row = sel.row_beg; row + 1 < sel.row_end; row++)
        {
          if (get(table, row, col).type
              != get(table, row + 1, col).type)
            {
              cerr << "ERROR: values at rows "
                   << row
                   << " and "
                   << row + 1
                   << ", column "
                   << col
                   << ", have different types.\n";
              exit(EXIT_FAILURE);
            }
        }
    }

  Categories categories;

  categories.count = sel.col_end - sel.col_beg;
  categories.data = new Category[categories.count];
  categories.pool = allocate(MEMORY_POOL_SIZE);

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
    {
      auto &category = categories.data[col - sel.col_beg];

      category.type = get(table, sel.row_beg, col).type;

      switch (category.type)
        {
        case AttributeType::STRING:
          category.as.string = new Category::StringC;
          break;
        case AttributeType::INT64:
          category.as.int64 = new Category::Int64C;
          break;
        case AttributeType::FLOAT64:
          category.as.float64 = new Category::Float64C;
          break;
        case AttributeType::INTERVAL:
          category.as.interval = new Category::IntervalC;
          break;
        }
    }

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
    {
      auto &category = categories.data[col - sel.col_beg];

      switch (category.type)
        {
        case AttributeType::STRING:
          {
            auto &string = *category.as.string;

            for (size_t row = sel.row_beg; row < sel.row_end; row++)
              {
                auto &value = get(table, row, col);

                auto const it = string.to.emplace(
                                                  push(categories.pool, value.as.string),
                                                  string.to.size()
                                                  );

                if (!it.second)
                  pop(categories.pool, value.as.string.size);
              }

            string.from = (String *)reserve_array(
                                                  categories.pool, sizeof (String), string.to.size()
                                                  );

            for (auto &elem: string.to)
              string.from[elem.second] = push(categories.pool, elem.first);

            category.count = string.to.size();
          } break;
        case AttributeType::INT64:
          {
            auto &int64 = *category.as.int64;

            int64_t min = numeric_limits<int64_t>::max(),
              max = numeric_limits<int64_t>::lowest();

            for (size_t row = sel.row_beg; row < sel.row_end; row++)
              {
                auto &value = get(table, row, col);

                if (int64.to.size() < INTEGER_CATEGORY_LIMIT)
                  int64.to.emplace(value.as.int64, int64.to.size());

                min = std::min(min, value.as.int64);
                max = std::max(max, value.as.int64);
              }

            int64.interval = {
              (double)min,
              (double)(max - min) / (BINS_COUNT - 1),
              BINS_COUNT
            };

            if (int64.to.size() >= INTEGER_CATEGORY_LIMIT)
              {
                int64.from = nullptr;
                int64.to.clear();
                category.count = BINS_COUNT - 1;
              }
            else
              {
                int64.from = (int64_t *)reserve_array(
                                                      categories.pool, sizeof (int64_t), int64.to.size()
                                                      );

                for (auto &elem: int64.to)
                  int64.from[elem.second] = elem.first;

                category.count = int64.to.size();
              }
          } break;
        case AttributeType::FLOAT64:
          {
            double min = numeric_limits<double>::max(),
              max = numeric_limits<double>::lowest();

            for (size_t row = sel.row_beg; row < sel.row_end; row++)
              {
                auto &value = get(table, row, col);

                min = std::min(min, value.as.float64);
                max = std::max(max, value.as.float64);
              }

            category.as.float64->interval = {
              min,
              (max - min) / (BINS_COUNT - 1),
              BINS_COUNT
            };

            category.count = BINS_COUNT - 1;
          } break;
        case AttributeType::INTERVAL:
          {
            auto &interval = *category.as.interval;

            for (size_t row = sel.row_beg; row < sel.row_end; row++)
              interval.to.emplace(get(table, row, col).as.interval, interval.to.size());

            interval.from = (Interval *)reserve_array(
                                                      categories.pool, sizeof (Interval), interval.to.size()
                                                      );

            for (auto &elem: interval.to)
              interval.from[elem.second] = elem.first;

            category.count = interval.to.size();
          } break;
        }
    }

  return categories;
}

void
clean(const Categories &self)
{
  for (size_t i = 0; i < self.count; i++)
    {
      auto &category = self.data[i];

      switch (category.type)
        {
        case AttributeType::STRING:
          delete category.as.string;
          break;
        case AttributeType::INT64:
          delete category.as.int64;
          break;
        case AttributeType::FLOAT64:
          delete category.as.float64;
          break;
        case AttributeType::INTERVAL:
          delete category.as.interval;
          break;
        }
    }

  delete[] self.data;
  delete[] self.pool.data;
}

struct ConstTreeData
{
  Matzu table;
  size_t const threshold;

  size_t *header,
    *theader;
  Matzu samples;

  bool *const used_columns;

  size_t *const rows;
};

struct MutableTreeData
{
  DecisionTree::Node &node;
  size_t start;
  size_t end;
};

void
construct(DecisionTree &self, const MutableTreeData &mut, ConstTreeData &cons)
{
  auto const compute_entropy_after_split =
    [&self, &cons](size_t attr, const size_t *start, const size_t *end) -> double
    {
      cons.samples.rows = self.categories.data[attr].count;
      cons.samples.cols = self.categories.data[self.goal].count;

      memset(cons.theader, 0, cons.samples.rows * sizeof (size_t));
      memset(cons.samples.data,
             0,
             cons.samples.cols
             * cons.samples.rows * sizeof (size_t));

      size_t const samples_count = end - start;

      while (start < end)
        {
          size_t const row = at_column_major(cons.table, attr, *start),
            col = at_column_major(cons.table, self.goal, *start);

          at_row_major(cons.samples, row, col)++;
          cons.theader[row]++;
          start++;
        }

      double mean_entropy = 0;

      for (size_t i = 0; i < cons.samples.rows; i++)
        {
          size_t const samples_in_category = cons.theader[i];

          double entropy = 0;

          // Compute "entropy * samples_in_category", not just entropy.
          for (size_t j = 0; j < cons.samples.cols; j++)
            {
              double const samples = (double)at_row_major(cons.samples, i, j);

              if (samples != 0)
                {
                  entropy +=
                    samples * log(samples / samples_in_category) / log(2);
                }
            }

          mean_entropy += entropy / samples_count;
        }

      return -mean_entropy;
    };

  auto const find_best_goal_category =
    [&self, &cons](size_t start, size_t end)
    {
      size_t best_goal_category = INVALID_CATEGORY;
      size_t best_samples_count = 0;

      // Reusing temporary header.
      memset(
             cons.theader, 0, self.categories.data[self.goal].count * sizeof (size_t)
             );

      for (size_t i = start; i < end; i++)
        {
          size_t const index = at_column_major(cons.table, self.goal, cons.rows[i]);
          size_t const value = ++cons.theader[index];

          if (best_samples_count < value)
            {
              best_samples_count = value;
              best_goal_category = index;
            }
        }

      assert(best_goal_category != INVALID_CATEGORY);

      return best_goal_category;
    };

  if (mut.end - mut.start <= cons.threshold)
    return;

  size_t best_attribute = INVALID_CATEGORY;

  {
    double best_entropy = numeric_limits<double>::max();

    for (size_t i = 0; i < self.categories.count; i++)
      {
        if (!cons.used_columns[i])
          {
            double const entropy
              = compute_entropy_after_split(i,
                                            cons.rows + mut.start,
                                            cons.rows + mut.end);

            if (best_entropy > entropy)
              {
                swap(cons.header, cons.theader);
                best_entropy = entropy;
                best_attribute = i;
              }
          }
      }
  }

  // No columns to process.
  if (best_attribute == INVALID_CATEGORY)
    {
      mut.node = { find_best_goal_category(mut.start, mut.end),
                   nullptr, 0, mut.end - mut.start };

      return;
    }

  size_t const category_count = self.categories.data[best_attribute].count;

  mut.node.column = best_attribute;
  mut.node.children
    = (DecisionTree::Node *)reserve_array(self.pool,
                                          sizeof (DecisionTree::Node),
                                          category_count);
  mut.node.count = category_count;
  mut.node.samples = mut.end - mut.start;

  size_t *const offsets = new size_t[category_count + 1];

  offsets[0] = mut.start;
  for (size_t i = 0; i < category_count; i++)
    {
      size_t const samples = cons.header[i];

      offsets[i + 1] = offsets[i] + samples;

      if (samples <= cons.threshold)
        {
          size_t const category = samples != 0 ?
            find_best_goal_category(offsets[i], offsets[i + 1]) :
            find_best_goal_category(mut.start, mut.end);

          mut.node.children[i] = {
            category,
            nullptr,
            0,
            samples
          };
        }
    }

  sort(cons.rows + mut.start,
       cons.rows + mut.end,
       [&cons, &best_attribute](size_t left, size_t right) -> bool
       {
         return at_column_major(cons.table, best_attribute, left) <
           at_column_major(cons.table, best_attribute, right);
       });

  cons.used_columns[best_attribute] = true;

  for (size_t i = 0; i < mut.node.count; i++)
    {
      construct(self,
                MutableTreeData { mut.node.children[i], offsets[i], offsets[i + 1] },
                cons);
    }

  cons.used_columns[best_attribute] = false;

  delete[] offsets;
}

DecisionTree
construct(const Table &table, const Table::Selection &sel, size_t threshold)
{
  assert_selection_is_valid(table, sel);

  if (sel.col_end - sel.col_beg <= 1)
    {
      cerr << "ERROR: table should contain at least 2 columns.\n";
      exit(EXIT_FAILURE);
    }

  DecisionTree tree;

  tree.categories = discretize(table, sel);
  tree.goal = tree.categories.count - 1;
  tree.pool = allocate(MEMORY_POOL_SIZE);
  tree.root
    = (DecisionTree::Node *)reserve_array(tree.pool,
                                          sizeof (DecisionTree::Node),
                                          1);
  *tree.root = { INVALID_CATEGORY, nullptr, 0, 0 };

  if (sel.row_beg <= 0)
    tree.names = nullptr;
  else
    tree.names
      = (String *)reserve_array(tree.pool,
                                sizeof (String),
                                tree.categories.count);

  size_t max_matrix_rows_count = 0;

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
    {
      size_t const index = col - sel.col_beg;

      if (tree.names != nullptr)
        {
          StringView const view = to_string(get(table, sel.row_beg - 1, col));
          tree.names[index].data = push(tree.pool, view.data, view.size);
          tree.names[index].size = view.size;
        }

      max_matrix_rows_count =
        max(max_matrix_rows_count, tree.categories.data[index].count);
    }

  size_t const rows = sel.row_end - sel.row_beg,
    cols = sel.col_end - sel.col_beg;
  size_t offsets[6];

  char *const data = allocate_in_chunks(offsets,
                                        6,
                                        cols * rows * sizeof (size_t),
                                        max_matrix_rows_count * sizeof (size_t),
                                        max_matrix_rows_count * sizeof (size_t),
                                        max_matrix_rows_count * tree.categories.data[tree.goal].count *
                                        sizeof (size_t),
                                        cols * sizeof (bool),
                                        rows * sizeof (size_t));

  ConstTreeData cons_data
    = { { (size_t *)data, rows, cols },
        threshold,
        (size_t *)(data + offsets[0]),
        (size_t *)(data + offsets[1]),
        { (size_t *)(data + offsets[2]), 0, 0 },
        (bool *)(data + offsets[3]),
        (size_t *)(data + offsets[4]) };

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
    {
      size_t const index = col - sel.col_beg;
      auto &category = tree.categories.data[index];

      for (size_t row = sel.row_beg; row < sel.row_end; row++)
        {
          // Weird formating, assigning is happening here.
          size_t const value = (at_column_major(cons_data.table, index, row - sel.row_beg)
                                = to_category(category, get(table, row, col)));

          if (value >= category.count)
            {
              cerr << "ERROR: couldn't categorize value at (row, column) = ("
                   << row << ", " << col
                   << "). Aborting, as it may cause undefined behaviour.\n";
              exit(EXIT_FAILURE);
            }
        }
    }

  memset(cons_data.used_columns, 0, offsets[4] - offsets[3]);
  cons_data.used_columns[tree.goal] = true;

  for (size_t i = 0; i < rows; i++)
    cons_data.rows[i] = i;

  construct(tree, MutableTreeData { *tree.root, 0, rows }, cons_data);

  delete[] data;

  return tree;
}

void
clean(const DecisionTree &self)
{
  clean(self.categories);
  delete[] self.pool.data;
}

pair<const Table::Cell, bool>
from_class(const DecisionTree &self, size_t category)
{
  return from_category(self.categories.data[self.goal], category);
}

size_t
classify(const DecisionTree &self, const Table &samples, size_t row)
{
  const DecisionTree::Node *curr = self.root;

  while (curr->count > 0)
    {
      if (curr->column >= self.categories.count)
        return INVALID_CATEGORY;

      size_t const category = to_category(self.categories.data[curr->column],
                                          get(samples, row, curr->column));

      if (category < curr->count)
        curr = curr->children + category;
      else
        return INVALID_CATEGORY;
    }

  return curr->column;
}

size_t *
classify(const DecisionTree &self, Table &samples)
{
  if (self.categories.count != samples.cols + 1)
    {
      cerr << "ERROR: samples table should have "
           << self.categories.count - 1
           << " columns, but got "
           << samples.cols
           << ".\n";
      exit(EXIT_FAILURE);
    }

  for (size_t i = 0; i < samples.cols; i++)
    {
      if (self.categories.data[i].type == AttributeType::INTERVAL)
        {
          bool const promoted_everything = promote(samples,
                                                   AttributeType::INTERVAL,
                                                   { 0, samples.rows, i, i + 1 });

          if (!promoted_everything)
            {
              cerr << "ERROR: couldn't promote column "
                   << i
                   << " to type `Interval`.\n";
              exit(EXIT_FAILURE);
            }
        }
      else
        {
          if (!have_same_type(samples, { 0, samples.rows, i, i + 1 }))
            {
              cerr << "ERROR: column "
                   << i
                   << " has values of different types.\n";
              exit(EXIT_FAILURE);
            }
        }
    }

  size_t *const classes = new size_t[samples.rows];

  for (size_t i = 0; i < samples.rows; i++)
    classes[i] = classify(self, samples, i);

  return classes;
}

void
print(const DecisionTree &self, const DecisionTree::Node &node, int offset)
{
  auto const print_from_category =
    [&self, &node](const Category &category, size_t category_as_int) -> void
    {
      auto category_class = from_category(category, category_as_int);

      if (!category_class.second)
        cout << "(null)";
      else
        cout << to_string(category_class.first).data;
    };

  if (node.count > 0)
    {
      cout << '\n';
      putsn(" ", offset);

      if (self.names != nullptr)
        cout << '<' << self.names[node.column].data;
      else
        cout << "<unnamed" << node.column;

      cout << " (" << node.samples << ")>\n";

      offset += TAB_WIDTH;

      for (size_t i = 0; i < node.count; i++)
        {
          putsn(" ", offset);
          print_from_category(self.categories.data[node.column], i);
          cout << ':';

          print(self, node.children[i], offset);
        }
    }
  else
    {
      cout << ' ';
      print_from_category(self.categories.data[self.goal], node.column);
      cout << " ("
           << node.samples
           << ")\n";
    }
}

void
print(const DecisionTree &self)
{
  print(self, *self.root, 0);
}
