std::string
read_entire_file(const char *filepath)
{
  std::ifstream file(filepath);
  std::string result;

  if (!file.is_open())
    {
      fprintf(stderr, "error: couldn't open '%s'.", filepath);
      exit(EXIT_FAILURE);
    }

  {
    struct stat stats;
    if (stat(filepath, &stats) == -1)
      goto report_error;
    result.resize(stats.st_size + 1);
  }

  file.read(&result[0], result.size() - 1);
  result.back() = '\0';
  file.close();

  return result;

 report_error:
  std::cerr << strerror(errno);
  exit(EXIT_FAILURE);
}

Table
parse_csv(const char *filepath)
{
  auto table = Table{ };
  auto t = Tokenizer{ };
  t.filepath = filepath;
  t.source = read_entire_file(filepath);

  size_t cells_in_row = 0;

  if (t.peek() == Token_New_Line)
    t.advance();

  while (t.peek() != Token_End_Of_File)
    {
      auto token = t.grab();
      t.advance();

      switch (token.type)
        {
        case Token_Integer:
          {
            i64 value = 0;
            for (auto ch: token.text)
              value = 10 * value + (ch - '0');

            auto cell = TableCell{ };
            cell.type = Table_Cell_Integer;
            cell.as.integer = value;
            table.data.push_back(cell);
            ++cells_in_row;

            t.expect_comma_or_new_line();
          }

          break;
        case Token_Decimal:
          {
            i64 integral_part = 0;
            f64 fractional_part = 0;
            f64 pow10 = 0.1;

            size_t i = 0;
            auto text = token.text;
            for (; i < text.size() && text[i] != '.'; i++)
              integral_part = 10 * integral_part + (text[i] - '0');

            for (++i; i < text.size(); i++, pow10 /= 10)
              fractional_part += pow10 * (text[i] - '0');

            auto cell = TableCell{ };
            cell.type = Table_Cell_Decimal;
            cell.as.decimal = integral_part + fractional_part;
            table.data.push_back(cell);
            ++cells_in_row;

            t.expect_comma_or_new_line();
          }

          break;
        case Token_String:
          {
            auto [it, _] = table.string_pool.emplace(token.text);
            auto cell = TableCell{ };
            cell.type = Table_Cell_String;
            cell.as.string = *it;
            table.data.push_back(cell);
            ++cells_in_row;

            t.expect_comma_or_new_line();
          }

          break;
        case Token_Comma:
          {
            PRINT_ERROR0(t.filepath, token.line_info, "unexpected ','.");
            exit(EXIT_FAILURE);
          }

          break;
        case Token_New_Line:
          {
            if (table.cols == 0)
              {
                // Columns count should only be zero on first iteration.
                assert(cells_in_row > 0);
                table.cols = cells_in_row;
              }
            else if (cells_in_row != table.cols)
              {
                PRINT_ERROR(t.filepath, token.line_info, "expected %zu row(s), but got %zu.", table.cols, cells_in_row);
                exit(EXIT_FAILURE);
              }

            ++table.rows;
            cells_in_row = 0;
          }

          break;
        case Token_End_Of_File:
          UNREACHABLE();
          break;
        }
    }

  return table;
}

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

// Moves the string pool from table.
CategorizedTable
categorize(Table &table)
{
  assert(table.cols >= 3 && table.rows >= 2);

  CategorizedTable ct;
  ct.cols = table.cols - 1;
  ct.rows = table.rows - 1;
  ct.data.reserve(ct.cols);
  ct.labels.reserve(ct.cols);

  // Extract columns name.
  for (size_t i = 1; i < table.cols; i++)
    ct.labels.push_back(table.grab(0, i).stringify());

  for (size_t i = 1; i < table.cols; i++)
    {
      switch (table.grab(1, i).type)
        {
        case Table_Cell_Integer:
          {
            std::map<i64, CategoryId> to;
            i64 min = INT64_MAX, max = INT64_MIN;

            for (size_t j = 1; j < table.rows; j++)
              {
                auto &cell = table.grab(j, i);

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
                std::vector<i64> from;
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

            for (size_t j = 1; j < table.rows; j++)
              {
                auto &cell = table.grab(j, i);

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
            std::map<std::string, CategoryId> to;

            for (size_t j = 1; j < table.rows; j++)
              {
                auto &cell = table.grab(j, i);

                if (cell.type != Table_Cell_String)
                  {
                    exit(EXIT_FAILURE);
                  }

                to.emplace(cell.as.string, to.size());
              }

            std::vector<std::string_view> from;
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
