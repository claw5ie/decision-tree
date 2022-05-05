#include <iostream>
#include <map>
#include <string>
#include <limits>
#include <cmath>
#include <cassert>
#include <sys/stat.h>

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

struct Table
{
  struct Attribute
  {
    enum Type
    {
      CATEGORY,
      INTEGER,
      DECIMAL
    };

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

      int32_t *ints;

      double *doubles;
    } as;

    Type type;
    std::string name;
    bool is_initialized;

    union Data
    {
      uint32_t category;
      int32_t integer;
      double decimal;
    };

    Data get(size_t row) const
    {
      Data data;

      switch (type)
      {
      case CATEGORY:
        data.category = as.category.data[row];
        break;
      case INTEGER:
        data.integer = as.ints[row];
        break;
      case DECIMAL:
        data.decimal = as.doubles[row];
      }

      return data;
    }
  };

  Attribute *columns;
  size_t cols,
    rows;

  void print()
  {
    for (size_t i = 0; i < cols; i++)
    {
      auto &column = columns[i];
      for (size_t j = 0; j < rows; j++)
      {
        switch (column.type)
        {
        case Table::Attribute::CATEGORY:
          std::cout << column.as.category.data[j];
          break;
        case Table::Attribute::INTEGER:
          std::cout << column.as.ints[j];
          break;
        case Table::Attribute::DECIMAL:
          std::cout << column.as.doubles[j];
          break;
        }

        std::cout << (j + 1 < rows ? ' ' : '\n');
      }
    }
  }

  void deallocate()
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
      case Table::Attribute::INTEGER:
        delete[] column.as.ints;
        break;
      case Table::Attribute::DECIMAL:
        delete[] column.as.doubles;
        break;
      }
    }

    delete[] columns;
  }
};

Table read_csv(const char *filepath)
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

      int32_t integer;

      double decimal;
    } as;
  };

  auto const parse_cell =
    [&curr, &is_delimiter]() -> Data
    {
      if (std::isdigit(*curr) ||
          ((*curr == '-' || *curr == '+') && std::isdigit(curr[1])))
      {
        curr += *curr == '+';
        Data attr;
        attr.type = Table::Attribute::INTEGER;
        attr.as.integer = 0;

        bool const should_be_negative = *curr == '-';
        for (curr += should_be_negative; std::isdigit(*curr); curr++)
          attr.as.integer = attr.as.integer * 10 + (*curr - '0');

        if (*curr == '.' && std::isdigit(curr[1]))
        {
          double pow = 0.1;

          attr.type = Table::Attribute::DECIMAL;
          attr.as.decimal = (double)attr.as.integer;

          for (++curr; std::isdigit(*curr); curr++, pow /= 10)
            attr.as.decimal += (*curr - '0') * pow;

          if (should_be_negative)
            attr.as.decimal = -attr.as.decimal;

          return attr;
        }
        else
        {
          if (should_be_negative)
            attr.as.integer = -attr.as.integer;

          return attr;
        }
      }
      else
      {
        Data attr;
        attr.type = Table::Attribute::CATEGORY;
        attr.as.string.text = curr;

        while (!is_delimiter(*curr))
          curr++;

        attr.as.string.size = curr - attr.as.string.text;

        return attr;
      }
    };

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
        case Table::Attribute::INTEGER:
          column.as.ints[j] = value.as.integer;
          break;
        case Table::Attribute::DECIMAL:
          column.as.doubles[j] = value.as.decimal;
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
        case Table::Attribute::INTEGER:
          column.as.ints = new int32_t[table.rows];
          column.as.ints[0] = value.as.integer;
          break;
        case Table::Attribute::DECIMAL:
          column.as.doubles = new double[table.rows];
          column.as.doubles[0] = value.as.decimal;
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

#define INTEGER_CATEGORY_LIMIT 7
#define BINS_COUNT 4

double compute_entropy(
  const Table::Attribute &attr,
  const Table::Attribute &goal,
  size_t table_rows
  )
{
  static double bins[BINS_COUNT];

  struct Category
  {
    Table::Attribute::Type type;

    union
    {
      struct
      {
        size_t count;
      } category;

      struct
      {
        std::map<int32_t, uint32_t> *values;
        int32_t min;
        int32_t max;
      } integer;

      struct
      {
        double min;
        double max;
      } decimal;
    } as;

    static Category discretize(const Table::Attribute &attr, size_t count)
    {
      Category res;

      res.type = attr.type;

      switch (attr.type)
      {
      case Table::Attribute::CATEGORY:
        res.as.category.count = attr.as.category.names->size();
        break;
      case Table::Attribute::INTEGER:
      {
        auto &integer = res.as.integer;
        integer.values = new std::map<int32_t, uint32_t>;
        integer.min = std::numeric_limits<int32_t>::max();
        integer.max = std::numeric_limits<int32_t>::lowest();

        for (size_t i = 0; i < count; i++)
        {
          auto const value = attr.as.ints[i];

          if (integer.values->size() < INTEGER_CATEGORY_LIMIT)
            integer.values->emplace(value, integer.values->size());

          integer.min = std::min(integer.min, value);
          integer.max = std::max(integer.max, value);
        }

        if (integer.values->size() >= INTEGER_CATEGORY_LIMIT)
        {
          delete integer.values;
          integer.values = nullptr;
        }
        else
        {
          double const interval_length =
            (integer.max - integer.min) / (BINS_COUNT - 1);
          for (size_t i = 0; i < BINS_COUNT; i++)
            bins[i] = integer.min + i * interval_length;
        }
      } break;
      case Table::Attribute::DECIMAL:
      {
        auto &decimal = res.as.decimal;
        decimal.min = std::numeric_limits<double>::max();
        decimal.max = std::numeric_limits<double>::lowest();

        for (size_t i = 0; i < count; i++)
        {
          auto const value = attr.as.doubles[i];

          decimal.min = std::min(decimal.min, value);
          decimal.max = std::max(decimal.max, value);
        }

        double const interval_length =
          (decimal.max - decimal.min) / (BINS_COUNT - 1);
        for (size_t i = 0; i < BINS_COUNT; i++)
          bins[i] = decimal.min + i * interval_length;
      } break;
      }

      return res;
    }

    size_t to_category(Table::Attribute::Data value) const
    {
      switch (type)
      {
      case Table::Attribute::CATEGORY:
        return value.category;
      case Table::Attribute::INTEGER:
        if (as.integer.values != nullptr)
          return as.integer.values->find(value.integer)->second;
        else
          return binary_search_interval(value.integer, bins, BINS_COUNT);
      case Table::Attribute::DECIMAL:
        return binary_search_interval(value.decimal, bins, BINS_COUNT);
      }

      assert(false);

      return 0;
    }

    size_t count() const
    {
      switch (type)
      {
      case Table::Attribute::CATEGORY:
        return as.category.count;
      case Table::Attribute::INTEGER:
        if (as.integer.values != nullptr)
          return as.integer.values->size();
        else
          return BINS_COUNT - 1;
      case Table::Attribute::DECIMAL:
        return BINS_COUNT - 1;
      }

      assert(false);

      return 0;
    }

    void clean()
    {
      if (type == Table::Attribute::INTEGER)
        delete as.integer.values;
    }
  };

  Category discr_attr = Category::discretize(attr, table_rows);
  Category discr_goal = Category::discretize(goal, table_rows);

  size_t const rows = discr_attr.count();
  size_t const cols = discr_goal.count() + 1;
  size_t *const samples = new size_t[rows * cols]{ };

  for (size_t i = 0; i < table_rows; i++)
  {
    size_t const offset =
      discr_attr.to_category(attr.get(i)) * cols;

    samples[offset + 1 + discr_goal.to_category(goal.get(i))]++;
    samples[offset]++;
  }

  double mean_entropy = 0;

  for (size_t i = 0; i < rows; i++)
  {
    size_t const offset = i * cols;
    size_t const sample_size = samples[offset];
    double entropy = 0;

    for (size_t j = 1; j < cols; j++)
    {
      double const count = (double)samples[offset + j];

      if (count != 0)
        entropy += count * log(count / sample_size) / log(2);
    }

    mean_entropy += -entropy / table_rows;
  }

  delete[] samples;
  discr_attr.clean();
  discr_goal.clean();

  return mean_entropy;
}

int main(int argc, char **argv)
{
  assert(argc == 2);

  Table table = read_csv(argv[1]);

  for (size_t i = 0; i + 1 < table.cols; i++)
  {
    double val = compute_entropy(
      table.columns[i], table.columns[table.cols - 1], table.rows
      );
    std::cout << "entropy of column " << i << ":\n  " << val << "\n";
  }

  table.print();
  table.deallocate();
}
