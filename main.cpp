#include <iostream>
#include <map>
#include <string>
#include <limits>
#include <cmath>
#include <cassert>
#include <sys/stat.h>

void *allocate_in_chunks(size_t *offsets, size_t count, ...)
{
  assert(count > 0);

  va_list args;
  va_start(args, count);

  offsets[0] = va_arg(args, size_t);
  for (size_t i = 1; i < count; i++)
    offsets[i] = offsets[i - 1] + va_arg(args, size_t);

  va_end(args);

  char *const data = new char[offsets[count - 1]];

  return (void *)data;
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

      int32_t *ints;

      double *doubles;
    } as;

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

  double bins[BINS_COUNT];

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

double compute_info_gain(
  const Category &attr,
  const Category &goal,
  size_t table_rows
  )
{
  size_t const rows = attr.count();
  size_t const cols = goal.count() + 1;
  std::memset();

  for (size_t i = 0; i < table_rows; i++)
  {
    size_t const offset =
      discr_attr.to_category(attr.get(i)) * cols + cols;
    size_t const column = discr_goal.to_category(goal.get(i)) + 1;

    samples[offset + column]++;
    samples[offset]++;
    samples[column]++;
  }

  double info_gain = 0;

  for (size_t i = 1; i < cols; i++)
  {
    double const prob = (double)samples[i] / table_rows;

    if (prob != 0)
      info_gain -= prob * std::log(prob) / std::log(2);
  }

  for (size_t i = 1; i < rows; i++)
  {
    size_t const offset = i * cols;
    size_t const sample_size = samples[offset];
    double entropy = 0;

    for (size_t j = 1; j < cols; j++)
    {
      double const count = (double)samples[offset + j];

      if (count != 0)
        entropy += count * std::log(count / sample_size) / std::log(2);
    }

    info_gain += entropy / table_rows;
  }

  return info_gain;
}

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
  size_t attribute_count;
  size_t goal_index;

  Node root;

  void construct(const Table &table, size_t goal)
  {
    categories = new Category[table.cols];
    names = new std::string[table.cols];
    attribute_count = table.cols;
    goal_index = goal;

    size_t max_rows_count = 0;

    for (size_t i = 0; i < table.cols; i++)
    {
      categories[i] = Category::discretize(table.columns[i], table.rows);
      names[i] = table.columns[i].name;
      max_rows_count =
        std::max(max_rows_count, categories[i].count());
    }

    max_rows_count++;
    size_t const cols = categories[goal].count() + 1;
    size_t const total_count = max_rows_count * cols;

    size_t offsets[5];
    void *const data = allocate_in_chunks(
      offsets,
      5,
      cols * sizeof (size_t),
      total_count * sizeof (size_t),
      total_count * sizeof (size_t),
      table.cols * sizeof (bool),
      table.rows * sizeof (size_t);
      );

    construct({
        &table,
        &root,
        { { data, data + offsets[0], data + offsets[1] }, 0 },
        data + offsets[2],
        data + offsets[3]
      });

    delete[] data;

    return tree;
  }

  Data *parse_samples(cons char *samples) const
  {
    return nullptr;
  }

  size_t classify(const Data *sample) const
  {
    Node *curr = &root;

    while (curr->children != nullptr)
    {
      size_t const category =
        categories[curr->column].to_category(sample[curr->column]);

      if (category < curr->count)
      {
        curr = curr->children + category;
      }
      else
      {
        std::cerr << "ERROR: cannot classify the sample: value at column "
                  << curr->column
                  << " doesn't fit into any category.\n";

        return size_t(-1);
      }
    }

    return curr->column;
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
  struct EntropyData
  {
    const Table *table;
    const Category *discr_goal;
    const Attribute *goal;
    size_t *samples;
    struct
    {
      size_t *data;
      size_t start;
      size_t end;
    } rows;
  };

  void compute_entropy_before_split(const EntropyData &data) const
  {
    std::memset(data.samples, data.discr_goal.count() * sizeof(size_t), 0);

    for (size_t i = data.rows.start; i < data.rows.end; i++)
      samples[discr_goal.to_category(goal.get(data.rows.data[i]))]++;

    double entropy = 0;

    for (size_t i = 0, cols = discr_goal.count(); i < cols; i++)
    {
      double const prob =
        (double)data.samples[i] / (data.rows.end - data.rows.start);

      if (prob != 0)
        entropy -= prob * std::log(prob) / std::log(2);
    }

    return entropy;
  }

  void compute_entropy_after_split(const EntropyData &data, size_t attr_index) const
  {

    auto &attr = data.table->columns[attr_index];
    auto &goal = data.table->columns[goal_index];

    size_t const rows = discr_attr.count() + 1;
    size_t const cols = discr_goal.count() + 1;

    std::memset(data.samples + cols, rows * cols * sizeof(size_t), 0);

    for (size_t i = data.rows.start; i < data.rows.end; i++)
    {
      size_t const row = data.rows.data[i];
      size_t const offset =
        discr_attr.to_category(attr.get(row)) * cols + cols;
      size_t const column = discr_goal.to_category(goal.get(row)) + 1;

      samples[offset + column]++;
      samples[offset]++;
    }

    for (size_t i = 1; i < rows; i++)
    {
      size_t const offset = i * cols;
      size_t const sample_size = samples[offset];
      double entropy = 0;

      for (size_t j = 1; j < cols; j++)
      {
        double const count = (double)samples[offset + j];

        if (count != 0)
          entropy += count * std::log(count / sample_size) / std::log(2);
      }

      info_gain += entropy / table_rows;
    }

    return -info_gain;
  }

  struct DecisionTreeData
  {
    const Table *table;
    Node *node;
    struct
    {
      size_t *header;
      size_t *sections[2];
      bool section;
    } samples;
    bool *used_columns;
    size_t *rows;
  };

  void construct(const DecisionTreeData &data)
  {
    double const entropy = compute_entropy_before_split(

      );
    double best_info_gain = std::numeric_limits<double>::lowest();
    size_t best_attribute = size_t(-1);

    for (size_t i = 0; i < cols; i++)
    {
      if (!used_cols[i])
      {
        double const info_gain =
          entropy - compute_entropy_after_split();

        if (best_info_gain < info_gain)
        {
          best_info_gain = info_gain;
          best_attribute = i;
        }
      }
    }

    // No columns to process.
    if (best_attribute == size_t(-1))
    {
      // Incomplete.
      return;
    }

    size_t const category_count = categories[best_attribute].count();

    node.column = best_attribute;
    node.children = new Node[category_count];
    node.count = category_count;

    size_t *const offsets = new size_t[category_count + 1];

    split();

    used_cols[best_attribute] = true;

    for (size_t i = 0; i < category_count; i++)
      construct(node.children[i], offsets[i], offsets[i + 1]);

    used_cols[best_attribute] = false;

    delete[] offsets;
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

  Table table = read_csv(argv[1]);

  for (size_t i = 0; i + 1 < table.cols; i++)
  {
    double val = compute_info_gain(
      table.columns[i], table.columns[table.cols - 1], table.rows
      );
    std::cout << "entropy of column " << i << ":\n  " << val << "\n";
  }

  table.print();
  table.clean();
}
