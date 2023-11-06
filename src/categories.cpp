#define MAX_CATEGORIES_FOR_INTEGERS 7
#define BINS_COUNT 4

using CategoryId = size_t;

constexpr CategoryId INVALID_CATEGORY_ID = std::numeric_limits<CategoryId>::max();

struct SubdividedInterval
{
  f64 min, step;
  size_t count;
};

struct CategoryOfIntegers
{
  std::map<i64, CategoryId> to;
  std::vector<i64> from;
};

struct CategoryOfDecimals
{
  SubdividedInterval interval;
};

struct CategoryOfStrings
{
  std::map<std::string, CategoryId> to;
  std::vector<std::string_view> from;
};

union CategoryData
{
  CategoryOfIntegers integers;
  CategoryOfDecimals decimals;
  CategoryOfStrings strings;

  CategoryData()
  {
    // Union is ill-formed without constructor.
  }

  ~CategoryData()
  {
    // Union is ill-formed without destructor.
  }
};

enum CategoryType
  {
    Category_Of_Integers,
    Category_Of_Decimals,
    Category_Of_Strings,
  };

struct Category
{
  CategoryType type;
  CategoryData as;

  Category(CategoryType type)
    : type(type)
  {
    switch (type)
      {
      case Category_Of_Integers:
        new (&as.integers) CategoryOfIntegers{ };
        break;
      case Category_Of_Decimals:
        new (&as.decimals) CategoryOfDecimals{ };
        break;
      case Category_Of_Strings:
        new (&as.strings) CategoryOfStrings{ };
        break;
      }
  }

  Category(Category &&other)
  {
    type = other.type;
    switch (other.type)
      {
      case Category_Of_Integers:
        new (&as.integers) CategoryOfIntegers{ std::move(other.as.integers) };
        break;
      case Category_Of_Decimals:
        new (&as.decimals) CategoryOfDecimals{ std::move(other.as.decimals) };
        break;
      case Category_Of_Strings:
        new (&as.strings) CategoryOfStrings{ std::move(other.as.strings) };
        break;
      }
  }

  ~Category()
  {
    switch (type)
      {
      case Category_Of_Integers:
        as.integers.~CategoryOfIntegers();
        break;
      case Category_Of_Decimals:
        as.decimals.~CategoryOfDecimals();
        break;
      case Category_Of_Strings:
        as.strings.~CategoryOfStrings();
        break;
      }
  }

  size_t category_count()
  {
    switch (type)
      {
      case Category_Of_Integers:
        return as.integers.to.size();
      case Category_Of_Decimals:
        return as.decimals.interval.count;
      case Category_Of_Strings:
        return as.strings.to.size();
      }

    UNREACHABLE();
  }

  // Returns sentinel value if couldn't convert to category.
  CategoryId to_category(TableCell &cell)
  {
    switch (type)
      {
      case Category_Of_Integers:
        {
          if (cell.type != Table_Cell_Integer)
            return INVALID_CATEGORY_ID;

          auto it = as.integers.to.find(cell.as.integer);

          if (it == as.integers.to.end())
            return INVALID_CATEGORY_ID;

          auto &[_, id] = *it;

          return id;
        }

        break;
      case Category_Of_Decimals:
        {
          f64 value = 0;

          switch (cell.type)
            {
            case Table_Cell_Integer:
              value = cell.as.integer;
              break;
            case Table_Cell_Decimal:
              value = cell.as.decimal;
              break;
            case Table_Cell_String:
              return INVALID_CATEGORY_ID;
              break;
            }

          auto interval = as.decimals.interval;
          auto curr = interval.min, next = curr + interval.step;
          // Could binary search here.
          CategoryId i = 0;
          for (; i + 1 < interval.count; i++)
            {
              if (curr <= value && value < next)
                return i;

              curr = next;
              next += interval.step;
            }

          // Should use epsilon?
          return value <= next ? i : INVALID_CATEGORY_ID;
        }

        break;
      case Category_Of_Strings:
        {
          if (cell.type != Table_Cell_String)
            return INVALID_CATEGORY_ID;

          auto it = as.strings.to.find(std::string{ cell.as.string });

          if (it == as.strings.to.end())
            return INVALID_CATEGORY_ID;

          auto &[_, id] = *it;

          return id;
        }

        break;
      }

    UNREACHABLE();
  }

  CategoryId to_category_no_fail(TableCell &cell)
  {
    auto result = to_category(cell);
    assert(result != INVALID_CATEGORY_ID);
    return result;
  }

  std::string to_string(CategoryId id)
  {
    switch (type)
      {
      case Category_Of_Integers:
        return std::to_string(as.integers.from[id]);
      case Category_Of_Decimals:
        {
          auto interval = as.decimals.interval;
          auto curr = interval.min + id * interval.step;
          auto result = std::string{ };
          result.push_back('[');
          result.append(std::to_string(curr));
          result.push_back(',');
          result.append(std::to_string(curr + interval.step));
          result.push_back(']');

          return result;
        }
      case Category_Of_Strings:
        return std::string{ as.strings.from[id] };
      }

    UNREACHABLE();
  }
};

struct Categories
{
  std::vector<Category> data;
  std::vector<std::string> labels;
  size_t cols = 0, rows = 0;

  void print()
  {
    std::cout << "\nRows: " << rows
              << "\nColumn: " << cols << '\n';

    size_t j = 0;
    for (auto &category: data)
      {
        std::cout << '\'' << labels[j++] << '\'' << ":\n";

        switch (category.type)
          {
          case Category_Of_Integers:
            {
              for (auto &[value, id]: category.as.integers.to)
                std::cout << "    " << value << " --> " << id << '\n';

              size_t i = 0;
              for (auto value: category.as.integers.from)
                std::cout << "    " << i++ << " --> " << value << '\n';
            }

            break;
          case Category_Of_Decimals:
            {
              auto interval = category.as.decimals.interval;
              std::cout << "    " << '[' << interval.min << ", " << interval.min + interval.count * interval.step << ']' << '\n';
            }

            break;
          case Category_Of_Strings:
            {
              for (auto &[value, id]: category.as.strings.to)
                std::cout << "    " << value << " --> " << id << '\n';

              size_t i = 0;
              for (auto value: category.as.strings.from)
                std::cout << "    " << i++ << " --> " << value << '\n';
            }

            break;
          }
      }
  }
};

CategoryType
cell_type_to_category_type(TableCellType type)
{
  switch (type)
    {
    case Table_Cell_Integer: return Category_Of_Integers;
    case Table_Cell_Decimal: return Category_Of_Decimals;
    case Table_Cell_String:  return Category_Of_Strings;
    }

  UNREACHABLE();
}

SubdividedInterval
bucketize(f64 min, f64 max, size_t count)
{
  auto result = SubdividedInterval{};
  result.min = min;
  result.step = (max - min) / count;
  result.count = count;

  return result;
}

Categories
categorize(Table &table)
{
  assert(table.cols >= 3 && table.rows >= 2);

  auto ct = Categories{ };
  ct.cols = table.cols - 1;
  ct.rows = table.rows - 1;
  ct.data.reserve(ct.cols);
  ct.labels.reserve(ct.cols);

  // Extract columns name.
  for (size_t i = 1; i < table.cols; i++)
    ct.labels.push_back(table.grab(0, i).stringify());

  for (size_t col = 1; col < table.cols; col++)
    {
      switch (table.grab(1, col).type)
        {
        case Table_Cell_Integer:
          {
            auto to = std::map<i64, CategoryId>{ };
            i64 min = INT64_MAX, max = INT64_MIN;

            for (size_t row = 1; row < table.rows; row++)
              {
                auto &cell = table.grab(row, col);

                if (cell.type != Table_Cell_Integer)
                  {
                    exit(EXIT_FAILURE);
                  }

                min = std::min(min, cell.as.integer);
                max = std::max(max, cell.as.integer);

                if (to.size() <= MAX_CATEGORIES_FOR_INTEGERS)
                  to.emplace(cell.as.integer, to.size());
              }

            if (to.size() > MAX_CATEGORIES_FOR_INTEGERS)
              {
                auto category = Category{ Category_Of_Decimals };
                category.as.decimals.interval = bucketize(min, max, BINS_COUNT);
                ct.data.push_back(std::move(category));
              }
            else
              {
                auto from = std::vector<i64>{ };
                from.resize(to.size());

                for (auto &[key, id]: to)
                  from[id] = key;

                auto category = Category{ Category_Of_Integers };
                category.as.integers.to = std::move(to);
                category.as.integers.from = std::move(from);
                ct.data.push_back(std::move(category));
              }
          }

          break;
        case Table_Cell_Decimal:
          {
            f64 min = DBL_MAX, max = -DBL_MAX;

            for (size_t row = 1; row < table.rows; row++)
              {
                auto &cell = table.grab(row, col);

                if (cell.type != Table_Cell_Decimal)
                  {
                    exit(EXIT_FAILURE);
                  }

                min = std::min(min, cell.as.decimal);
                max = std::max(max, cell.as.decimal);
              }

            auto category = Category{ Category_Of_Decimals };
            category.as.decimals.interval = bucketize(min, max, BINS_COUNT);
            ct.data.push_back(std::move(category));
          }

          break;
        case Table_Cell_String:
          {
            auto to = std::map<std::string, CategoryId>{ };

            for (size_t row = 1; row < table.rows; row++)
              {
                auto &cell = table.grab(row, col);

                if (cell.type != Table_Cell_String)
                  {
                    exit(EXIT_FAILURE);
                  }

                to.emplace(cell.as.string, to.size());
              }

            auto from = std::vector<std::string_view>{ };
            from.resize(to.size());

            for (auto &[key, id]: to)
              from[id] = key;

            auto category = Category{ Category_Of_Strings };
            category.as.strings.to = std::move(to);
            category.as.strings.from = std::move(from);
            ct.data.push_back(std::move(category));
          }

          break;
        }
    }

  return ct;
}
