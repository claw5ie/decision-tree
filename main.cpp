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

    union Data
    {
      uint32_t category;
      int32_t integer;
      double decimal;
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

          if (curr[0] == '.' && std::isdigit(curr[1]))
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
};

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
    } integer;

    struct
    {
      double *bins;
    } decimal;
  } as;

  size_t count;

  static Category discretize(const Table::Attribute &attr, size_t count)
  {
    Category res;

    res.type = attr.type;

    switch (attr.type)
    {
    case Table::Attribute::CATEGORY:
      res.count = attr.as.category.names->size();
      break;
    case Table::Attribute::INTEGER:
    {
      auto &integer = res.as.integer;
      integer.values = new std::map<int32_t, uint32_t>;

      int32_t min = std::numeric_limits<int32_t>::max();
      int32_t max = std::numeric_limits<int32_t>::lowest();

      for (size_t i = 0; i < count; i++)
      {
        auto const value = attr.as.ints[i];

        if (integer.values->size() < INTEGER_CATEGORY_LIMIT)
          integer.values->emplace(value, integer.values->size());

        min = std::min(min, value);
        max = std::max(max, value);
      }

      res.count = integer.values->size();

      if (res.count >= INTEGER_CATEGORY_LIMIT)
      {
        delete integer.values;
        integer.values = nullptr;

        double val = min;
        integer.bins = new double[BINS_COUNT];
        double const interval_length = (max - min) / (BINS_COUNT - 1);
        for (size_t i = 0; i < BINS_COUNT; i++, val += interval_length)
          integer.bins[i] = val;

        res.count = BINS_COUNT - 1;
      }
    } break;
    case Table::Attribute::DECIMAL:
    {
      auto &decimal = res.as.decimal;

      double min = std::numeric_limits<double>::max();
      double max = std::numeric_limits<double>::lowest();

      for (size_t i = 0; i < count; i++)
      {
        auto const value = attr.as.doubles[i];

        min = std::min(min, value);
        max = std::max(max, value);
      }

      decimal.bins = new double[BINS_COUNT];
      double const interval_length = (max - min) / (BINS_COUNT - 1);
      for (size_t i = 0; i < BINS_COUNT; i++, min += interval_length)
        decimal.bins[i] = min;

      res.count = BINS_COUNT - 1;
    } break;
    }

    return res;
  }

  void clean()
  {
    switch (type)
    {
    case Table::Attribute::CATEGORY:
      return;
    case Table::Attribute::INTEGER:
      if (as.integer.values != nullptr)
        delete as.integer.values;
      else
        delete[] as.integer.bins;

      return;
    case Table::Attribute::DECIMAL:
      delete[] as.decimal.bins;
      return;
    }
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
        return binary_search_interval(value.integer, as.integer.bins, count + 1);
    case Table::Attribute::DECIMAL:
      return binary_search_interval(value.decimal, as.decimal.bins, count + 1);
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
  size_t attribute_count;
  size_t goal_index;

  Node root;

  void construct(
    const Table &table, size_t goal, size_t *columns_to_exclude, size_t count
    )
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

    Data data_ = {
      table,
      categories[goal],
      table.columns[goal],
      { (size_t *)data, (size_t *)(data + offsets[0]) },
      (size_t *)(data + offsets[1]),
      (bool *)(data + offsets[2]),
      { (size_t *)(data + offsets[3]), (size_t *)(data + offsets[4]) },
      (size_t **)(data + offsets[5])
    };

    std::memset(data_.used_columns, 0, offsets[4] - offsets[3]);
    data_.used_columns[goal] = true;

    for (size_t i = 0; i < count; i++)
      data_.used_columns[columns_to_exclude[i]] = true;

    for (size_t i = 0; i < table.rows; i++)
      data_.rows[0][i] = i;

    construct(root, data_, 0, table.rows);

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

  void clean()
  {
    for (size_t i = 0; i < attribute_count; i++)
      categories[i].clean();

    delete[] categories;
    delete[] names;

    clean(root);
  }

private:
  struct Data
  {
    const Table &table;

    const Category &discr_goal;
    const Table::Attribute &goal;

    size_t *header[2];
    size_t *samples;

    bool *used_columns;

    size_t *rows[2];
    size_t **offsets;
  };

  double compute_entropy_after_split(
    size_t attr_index, const Data &data, size_t start, size_t end
    ) const
  {
    const auto &discr_attr = categories[attr_index];
    size_t const cols = data.discr_goal.count;
    size_t const rows = discr_attr.count;

    std::memset(
      data.header[0], 0, rows * sizeof (size_t)
      );
    std::memset(
      data.samples, 0, cols * rows * sizeof (size_t)
      );

    const auto &attr = data.table.columns[attr_index];

    for (size_t i = start; i < end; i++)
    {
      size_t const index = data.rows[0][i];
      size_t const row =
        discr_attr.to_category(attr.get(index));
      size_t const column =
        data.discr_goal.to_category(data.goal.get(index));

      data.samples[row * cols + column]++;
      data.header[0][row]++;
    }

    size_t const samples_count = end - start;
    double mean_entropy = 0;

    for (size_t i = 0; i < rows; i++)
    {
      size_t const samples_in_category = data.header[0][i];
      size_t const offset = i * cols;
      double entropy = 0;

      for (size_t j = 0; j < cols; j++)
      {
        double const samples = (double)data.samples[offset + j];

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

      for (size_t i = 0; i < attribute_count; i++)
      {
        if (!data.used_columns[i])
        {
          double const entropy =
            compute_entropy_after_split(i, data, start, end);

          if (best_entropy > entropy)
          {
            std::swap(data.header[0], data.header[1]);
            best_entropy = entropy;
            best_attribute = i;
          }
        }
      }
    }

    size_t category_count = categories[best_attribute].count;

    {
      // Two remaining base cases.

      // No columns to process.
      if (best_attribute == size_t(-1))
      {
        size_t best_category = 0;

        for (size_t i = start, best_samples_count = 0; i < end; i++)
        {
          size_t const index =
            data.discr_goal.to_category(data.goal.get(data.rows[0][i]));

          // Reusing header.
          size_t const value = ++data.header[0][index];

          if (best_samples_count < value)
          {
            best_samples_count = value;
            best_category = index;
          }
        }

        node = { best_category, nullptr, 0, end - start };

        return;
      }

      // All samples are in one category.
      {
        uint8_t count = 0;
        size_t best_attribute = 0;
        for (size_t i = 0; i < category_count && count < 2; i++)
        {
          if (data.header[1][i] > 0)
          {
            count++;
            best_attribute = i;
          }
        }

        if (count == 1)
        {
          node = { best_attribute, nullptr, 0, data.header[1][best_attribute] };

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
      data.offsets[0] = data.rows[0] + start;
      for (size_t i = 0; i < category_count; i++)
      {
        offsets[i + 1] = offsets[i] + data.header[1][i];
        data.offsets[i + 1] = data.offsets[i] + data.header[1][i];
      }

      const auto &discr_attr = categories[best_attribute];
      const auto &attr = data.table.columns[best_attribute];

      std::memcpy(
        data.rows[1] + start,
        data.rows[0] + start,
        (end - start) * sizeof (size_t)
        );

      for (size_t i = start; i < end; i++)
      {
        size_t const index = data.rows[1][i];
        size_t const category =
          discr_attr.to_category(attr.get(index));

        auto **const slot = data.offsets + category;
        **slot = index;
        ++*slot;
      }
    }

    data.used_columns[best_attribute] = true;

    for (size_t i = 0; i < node.count; i++)
      construct(node.children[i], data, offsets[i], offsets[i + 1]);

    data.used_columns[best_attribute] = false;

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
