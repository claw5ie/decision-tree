#include <iostream>
#include <map>
#include <string>
#include <limits>

#include <cstring>
#include <cmath>
#include <cassert>
#include <cstdarg>

#include <sys/stat.h>

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
    std::fputs(string, stdout);
}

size_t binary_search_interval(
  double value, const double *array, size_t size
  )
{
  assert(size >= 2);

  size_t lower = 0,
    upper = size - 1;

  while (lower < upper)
  {
    size_t const mid = lower + (upper - lower) / 2;

    if (value < array[mid])
      upper = mid;
    else if (value >= array[mid + 1])
      lower = mid + 1;
    else
      return mid;
  }

  return size - 2;
}

struct Intervalf
{
  double min,
    max;
};

bool operator<(const Intervalf &left, const Intervalf &right)
{
  if (left.min >= right.min)
    return left.max < right.max;
  else
    return true;
}

struct Table
{
  struct Attribute
  {
    enum Type
    {
      CATEGORY,
      INT32,
      FLOAT64,
      INTERVAL
    };

    union Data
    {
      uint32_t category;
      int32_t int32;
      double float64;
      Intervalf interval;
    };

    Type type;

    union
    {
      struct
      {
        std::map<std::string, uint32_t> *names;
        uint32_t *data;

        void insert(size_t row, const char *string, size_t size)
        {
          auto const it =
            names->emplace(std::string(string, size), names->size());
          data[row] = it.first->second;
        }
      } category;

      int32_t *int32s;

      double *float64s;

      Intervalf *intervals;
    } as;

    std::string name;
    bool is_initialized;

    Data get(size_t row) const
    {
      Data data;

      switch (type)
      {
      case CATEGORY:
        data.category = as.category.data[row];
        break;
      case INT32:
        data.int32 = as.int32s[row];
        break;
      case FLOAT64:
        data.float64 = as.float64s[row];
        break;
      case INTERVAL:
        data.interval = as.intervals[row];
        break;
      }

      return data;
    }
  };

  Attribute *columns;
  size_t cols,
    rows;

  static Table read_csv(const char *filepath)
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
          exit(EXIT_FAILURE);
        }

        return stats.st_size;
      }();

    FILE *const file = std::fopen(filepath, "r");

    if (file == nullptr)
    {
      std::cerr << "ERROR: failed to open the file `" << filepath << "`.\n";
      exit(EXIT_FAILURE);
    }

    char *const file_data = new char[file_size + 1];

    if (std::fread(file_data, 1, file_size, file) < file_size)
      std::cerr << "warning: couldn't read the entire file.\n";

    file_data[file_size] = '\0';
    std::fclose(file);

    Table table;

    table.rows = 0;
    table.cols = 1;

    {
      const char *ch = file_data;

      // Count columns.
      while (*ch != '\n' && *ch != '\0')
      {
        table.cols += *ch == ',';
        ch++;
      }

      // Count rows. First line is not counted.
      while (*ch != '\0')
      {
        table.rows += *ch == '\n';
        ch++;
      }
    }

    auto const is_delimiter =
      [](char ch) -> bool
      {
        return ch == ',' || ch == '\n' || ch == '\0';
      };

    auto const find_next_delimiter =
      [&is_delimiter](const char *string) -> const char *
      {
        while (!is_delimiter(*string))
          string++;

        return string;
      };

    table.columns = new Table::Attribute[table.cols]{ };

    const char *curr = file_data;

    for (size_t i = 0; i < table.cols; i++)
    {
      const char *end = find_next_delimiter(curr);

      assert(*end != '\0');
      table.columns[i].name = std::string(curr, end);

      curr = end + (*end != '\0');
    }

    struct Data
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

        Intervalf interval;
      } as;
    };

    auto const parse_cell =
      [&curr, &is_delimiter]() -> Data
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

        auto const parse_float64 =
          [](const char *str, double &val) -> const char *
          {
            bool const should_be_negative = *str == '-';

            str += should_be_negative || *str == '+';

            for (val = 0; std::isdigit(*str); str++)
              val = val * 10 + (*str - '0');

            if (*str == '.' && std::isdigit(str[1]))
            {
              double pow = 0.1;

              for (str++; std::isdigit(*str); str++, pow /= 10)
                val += (*str - '0') * pow;
            }

            if (should_be_negative)
              val = -val;

            return str;
          };

        auto const parse_int32 =
          [](const char *str, int32_t &val) -> const char *
          {
            bool const should_be_negative = *str == '-';

            str += should_be_negative || *str == '+';

            for (val = 0; std::isdigit(*str); str++)
              val = val * 10 + (*str - '0');

            if (should_be_negative)
              val = -val;

            return str;
          };

        Data data;
        data.type = guess_type(curr);

        switch (data.type)
        {
        case Table::Attribute::CATEGORY:
        {
          data.as.string.text = curr;

          while (!is_delimiter(*curr))
            curr++;

          data.as.string.size = curr - data.as.string.text;

          return data;
        }
        case Table::Attribute::INT32:
        {
          if (*curr == '$')
          {
            for (data.as.int32 = 0; *curr== '$'; curr++)
              data.as.int32++;
          }
          else
          {
            curr = parse_int32(curr, data.as.int32);
          }

          return data;
        }
        case Table::Attribute::FLOAT64:
        {
          curr = parse_float64(curr, data.as.float64);

          return data;
        }
        case Table::Attribute::INTERVAL:
        {
          if (*curr == '<')
          {
            data.as.interval.min = std::numeric_limits<double>::lowest();
            curr = parse_float64(++curr, data.as.interval.max);

            return data;
          }
          else if (*curr == '>')
          {
            data.as.interval.max = std::numeric_limits<double>::max();
            curr = parse_float64(++curr, data.as.interval.min);

            return data;
          }
          else
          {
            curr = parse_float64(curr, data.as.interval.min);
            curr = parse_float64(++curr, data.as.interval.max);

            if (data.as.interval.min > data.as.interval.max)
              std::swap(data.as.interval.min, data.as.interval.max);

            return data;
          }
        }
        default:
          assert(false);
        }
      };

    // Could loop one time and initialize all columns, but requires copy-pasting
    // some code. Probably not worth it, although this will eliminate the need in
    // "is_initialized" variable. Also, making redundant checks each time, even
    // though the "else" statement will only be executed once is kinda meh...
    for (size_t j = 0; j < table.rows; j++)
    {
      for (size_t i = 0; i < table.cols; i++)
      {
        Data const value = parse_cell();

        auto &column = table.columns[i];
        if (column.is_initialized)
        {
          assert(column.type == value.type);

          switch (column.type)
          {
          case Table::Attribute::CATEGORY:
            column.as.category.insert(
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
              new std::map<std::string, uint32_t>;
            column.as.category.data = new uint32_t[table.rows];
            column.as.category.insert(
              0, value.as.string.text, value.as.string.size
              );
            break;
          case Table::Attribute::INT32:
            column.as.int32s = new int32_t[table.rows];
            column.as.int32s[0] = value.as.int32;
            break;
          case Table::Attribute::FLOAT64:
            column.as.float64s = new double[table.rows];
            column.as.float64s[0] = value.as.float64;
            break;
          case Table::Attribute::INTERVAL:
            column.as.intervals = new Intervalf[table.rows];
            column.as.intervals[0] = value.as.interval;
            break;
          }
        }

        assert(is_delimiter(*curr));
        curr += *curr != '\0';
      }
    }

    delete[] file_data;

    return table;
  }

  Attribute::Data get(size_t col, size_t row) const
  {
    Attribute::Data data;

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

  void clean()
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

  void print() const
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
};

void fill_bins(double min, double max, double *bins, size_t count)
{
  --count;
  bins[0] = std::numeric_limits<double>::lowest();
  bins[count] = std::numeric_limits<double>::max();

  double const interval_length = (max - min) / count;
  for (size_t i = 1; i < count; i++)
    bins[i] = min + i * interval_length;
}

#define INTEGER_CATEGORY_LIMIT 7
#define BINS_COUNT 4

struct Category
{
  Table::Attribute::Type type;

  union
  {
    struct
    {
      std::map<int32_t, uint32_t> *values;
      double *bins;
    } int32;

    struct
    {
      double *bins;
    } float64;

    std::map<Intervalf, uint32_t> *interval;
  } as;

  size_t count;

  void discretize(const Table &table, size_t column)
  {
    const auto &attr = table.columns[column];

    type = attr.type;

    switch (type)
    {
    case Table::Attribute::CATEGORY:
      count = attr.as.category.names->size();
      break;
    case Table::Attribute::INT32:
    {
      auto &int32 = as.int32;
      int32.values = new std::map<int32_t, uint32_t>;

      int32_t min = std::numeric_limits<int32_t>::max(),
        max = std::numeric_limits<int32_t>::lowest();

      for (size_t i = 0; i < table.rows; i++)
      {
        auto const value = attr.as.int32s[i];

        if (int32.values->size() < INTEGER_CATEGORY_LIMIT)
          int32.values->emplace(value, int32.values->size());

        min = std::min(min, value);
        max = std::max(max, value);
      }

      count = int32.values->size();

      if (count >= INTEGER_CATEGORY_LIMIT)
      {
        delete int32.values;
        int32.values = nullptr;

        int32.bins = new double[BINS_COUNT];
        fill_bins(min, max, int32.bins, BINS_COUNT);

        count = BINS_COUNT - 1;
      }
    } break;
    case Table::Attribute::FLOAT64:
    {
      auto &float64 = as.float64;

      double min = std::numeric_limits<double>::max(),
        max = std::numeric_limits<double>::lowest();

      for (size_t i = 0; i < table.rows; i++)
      {
        auto const value = attr.as.float64s[i];

        min = std::min(min, value);
        max = std::max(max, value);
      }

      float64.bins = new double[BINS_COUNT];
      fill_bins(min, max, float64.bins, BINS_COUNT);

      count = BINS_COUNT - 1;
    } break;
    case Table::Attribute::INTERVAL:
    {
      auto &interval = as.interval;
      interval = new std::map<Intervalf, uint32_t>();

      for (size_t i = 0; i < table.rows; i++)
      {
        interval->emplace(
          attr.as.intervals[i],
          interval->size()
          );
      }

      count = interval->size();
    } break;
    }
  }

  void clean()
  {
    switch (type)
    {
    case Table::Attribute::CATEGORY:
      return;
    case Table::Attribute::INT32:
      if (as.int32.values != nullptr)
        delete as.int32.values;
      else
        delete[] as.int32.bins;

      return;
    case Table::Attribute::FLOAT64:
      delete[] as.float64.bins;
      return;
    case Table::Attribute::INTERVAL:
      delete as.interval;
      return;
    }
  }

  size_t to_category(Table::Attribute::Data value) const
  {
    switch (type)
    {
    case Table::Attribute::CATEGORY:
      return value.category;
    case Table::Attribute::INT32:
      if (as.int32.values != nullptr)
        return as.int32.values->find(value.int32)->second;
      else
        return binary_search_interval(value.int32, as.int32.bins, BINS_COUNT);
    case Table::Attribute::FLOAT64:
      return binary_search_interval(value.float64, as.float64.bins, BINS_COUNT);
    case Table::Attribute::INTERVAL:
    {
      auto const it = as.interval->lower_bound(value.interval);

      if (it != as.interval->end())
      {
        if (it->first.min <= value.interval.max &&
            value.interval.max <= it->first.max)
          return it->second;
      }

      return size_t(-1);
    }
    }

    assert(false);

    return 0;
  }
};

#define TAB_WIDTH 2

struct DecisionTree
{
  struct Node
  {
    size_t column;
    Node *children;
    size_t count;
    size_t samples;
  };

  Category *categories;
  std::string *names;
  size_t count;
  size_t goal;

  Node root;

  void construct(
    const Table &table,
    size_t goal,
    size_t *columns_to_exclude,
    size_t columns_count
    )
  {
    this->categories = new Category[table.cols];
    this->names = new std::string[table.cols];
    this->count = table.cols;
    this->goal = goal;

    size_t max_rows_count = 0;

    for (size_t i = 0; i < table.cols; i++)
    {
      categories[i].discretize(table, i);
      names[i] = table.columns[i].name;
      max_rows_count =
        std::max(max_rows_count, categories[i].count);
    }

    size_t offsets[7];

    char *const data = allocate_in_chunks(
      offsets,
      7,
      max_rows_count * sizeof (size_t),
      max_rows_count * sizeof (size_t),
      max_rows_count * categories[goal].count * sizeof (size_t),
      table.cols * sizeof (bool),
      table.rows * sizeof (size_t),
      table.rows * sizeof (size_t),
      (max_rows_count + 1) * sizeof (size_t *)
      );

    Data tree_data = {
      table,
      (size_t *)data,
      (size_t *)(data + offsets[0]),
      (size_t *)(data + offsets[1]),
      (bool *)(data + offsets[2]),
      (size_t *)(data + offsets[3]),
      (size_t *)(data + offsets[4]),
      (size_t **)(data + offsets[5])
    };

    std::memset(tree_data.used_columns, 0, offsets[3] - offsets[2]);

    tree_data.used_columns[goal] = true;
    for (size_t i = 0; i < columns_count; i++)
      tree_data.used_columns[columns_to_exclude[i]] = true;

    for (size_t i = 0; i < table.rows; i++)
      tree_data.rows[i] = i;

    construct(root, tree_data, 0, table.rows);

    delete[] data;
  }

  // Data *parse_samples(cons char *samples) const
  // {
  //   return nullptr;
  // }

  // size_t classify(const Samples *sample) const
  // {
  //   const Node *curr = &root;

  //   while (curr->children != nullptr)
  //   {
  //     size_t const category =
  //       categories[curr->column].to_category(sample[curr->column]);

  //     if (category < curr->count)
  //     {
  //       curr = curr->children + category;
  //     }
  //     else
  //     {
  //       std::cerr << "ERROR: cannot classify the sample: value at column "
  //                 << curr->column
  //                 << " doesn't fit into any category.\n";

  //       return size_t(-1);
  //     }
  //   }

  //   return curr->column;
  // }

  void print() const
  {
    std::cout << "<" << names[root.column] << " (" << root.samples << ")>\n";

    for (size_t i = 0; i < root.count; i++)
      print(root.children[i], i, TAB_WIDTH);
  }

  void clean()
  {
    for (size_t i = 0; i < count; i++)
      categories[i].clean();

    delete[] categories;
    delete[] names;

    clean(root);
  }

private:
  struct Data
  {
    const Table &table;

    size_t *header,
      *theader,
      *samples;

    bool *used_columns;

    size_t *rows,
      *trows,
      **bins;
  };

  double compute_entropy_after_split(
    size_t attr, const Data &data, const size_t *start, const size_t *end
    ) const
  {
    size_t const rows = categories[attr].count,
      cols = categories[goal].count;

    std::memset(
      data.theader, 0, rows * sizeof (size_t)
      );
    std::memset(
      data.samples, 0, cols * rows * sizeof (size_t)
      );

    size_t const samples_count = end - start;

    // Reused "start", be careful.
    while (start < end)
    {
      size_t const row = to_category(data.table, attr, *start),
        col = to_category(data.table, goal, *start);

      data.samples[row * cols + col]++;
      data.theader[row]++;
      start++;
    }

    double mean_entropy = 0;

    for (size_t i = 0; i < rows; i++)
    {
      size_t const samples_in_category = data.theader[i];
      size_t const offset = i * cols;

      double entropy = 0;

      // Compute "entropy * samples_in_category", not just entropy.
      for (size_t j = 0; j < cols; j++)
      {
        double const samples = data.samples[offset + j];

        if (samples != 0)
        {
          entropy +=
            samples * std::log(samples / samples_in_category) / std::log(2);
        }
      }

      mean_entropy += entropy / samples_count;
    }

    return -mean_entropy;
  }

  void construct(Node &node, Data &data, size_t start, size_t end)
  {
    if (end - start <= 0)
    {
      node = { size_t(-1), nullptr, 0, 0 };

      return;
    }

    size_t best_attribute = size_t(-1);

    {
      double best_entropy = std::numeric_limits<double>::max();

      for (size_t i = 0; i < count; i++)
      {
        if (!data.used_columns[i])
        {
          double const entropy = compute_entropy_after_split(
            i, data, data.rows + start, data.rows + end
            );

          if (best_entropy > entropy)
          {
            std::swap(data.header, data.theader);
            best_entropy = entropy;
            best_attribute = i;
          }
        }
      }
    }

    size_t const category_count = categories[best_attribute].count;

    {
      // Two remaining base cases.

      // No columns to process.
      if (best_attribute == size_t(-1))
      {
        best_attribute = 0;

        // Reusing header.
        std::memset(
          data.theader, 0, categories[goal_index].count * sizeof (size_t)
          );

        for (size_t i = start, best_samples_count = 0; i < end; i++)
        {
          size_t const index = to_category(data.table, goal, data.rows[i]),
            value = ++data.theader[index];

          if (best_samples_count < value)
          {
            best_samples_count = value;
            best_attribute = index;
          }
        }

        node = { best_attribute, nullptr, 0, end - start };

        return;
      }

      // All samples are in one category.
      {
        uint8_t count = 0;
        size_t best_attribute = 0;
        for (size_t i = 0; i < category_count && count < 2; i++)
        {
          if (data.header[i] > 0)
          {
            count++;
            best_attribute = i;
          }
        }

        if (count == 1)
        {
          node = { best_attribute, nullptr, 0, data.header[best_attribute] };

          return;
        }
      }
    }

    node.column = best_attribute;
    node.children = new Node[category_count];
    node.count = category_count;
    node.samples = end - start;

    size_t *const offsets = new size_t[category_count + 1];

    {
      offsets[0] = start;
      data.bins[0] = data.rows + start;
      for (size_t i = 0; i < category_count; i++)
      {
        offsets[i + 1] = offsets[i] + data.header[i];
        data.bins[i + 1] = data.bins[i] + data.header[i];
      }

      std::memcpy(
        data.trows + start,
        data.rows + start,
        (end - start) * sizeof (size_t)
        );

      for (size_t i = start; i < end; i++)
      {
        size_t const row = data.trows[i],
          category = to_category(data.table, best_attribute, row);

        *data.bins[category] = row;
        ++data.bins[category];
      }
    }

    data.used_columns[best_attribute] = true;

    for (size_t i = 0; i < node.count; i++)
      construct(node.children[i], data, offsets[i], offsets[i + 1]);

    data.used_columns[best_attribute] = false;

    delete[] offsets;
  }

  size_t to_category(const Table &table, size_t column, size_t row) const
  {
    return categories[column].to_category(table.get(column, row));
  }

  void print(const Node &node, size_t category, int offset) const
  {
    putsn(" ", offset);
    std::cout << category << ':';

    if (node.count != 0)
    {
      std::cout << '\n';
      putsn(" ", offset + TAB_WIDTH);
      std::cout << '<' << names[node.column] << " (" << node.samples << ")>\n";

      for (size_t i = 0; i < node.count; i++)
        print(node.children[i], i, offset + 2 * TAB_WIDTH);
    }
    else
    {
      std::cout << ' '
                << node.column
                << " ("
                << node.samples
                << ")\n";
    }
  }

  void clean(Node &root)
  {
    for (size_t i = 0; i < root.count; i++)
      clean(root.children[i]);

    delete[] root.children;
  }
};

int main(int argc, char **argv)
{
  assert(argc == 2);

  Table table = Table::read_csv(argv[1]);

  table.print();
  std::cout << '\n';

  DecisionTree tree;

  size_t columns_to_exclude[1] = { 0 };
  tree.construct(table, 0, columns_to_exclude, 1);
  tree.print();

  table.clean();
  tree.clean();
}
