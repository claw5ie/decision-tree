std::string
read_entire_file(const char *filepath)
{
  auto file = std::ifstream{ filepath };
  auto result = std::string{ };

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
parse_csv_from_string(const char *filepath, std::string &source)
{
  auto table = Table{ };
  auto t = Tokenizer{ };
  t.filepath = filepath;
  t.source = source;

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

Table
parse_csv_from_file(const char *filepath)
{
  auto string = read_entire_file(filepath);
  return parse_csv_from_string(filepath, string);
}

Table
parse_csv_from_stdin()
{
  auto string = std::string{ };
  auto buffer = std::string{ };

  while (std::getline(std::cin, buffer))
    {
      string.append(buffer);
      string.push_back('\n');
    }

  return parse_csv_from_string("<stdin>", string);
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

void build_decision_tree(DecisionTree &tree, DecisionTreeBuildDataNode &node, DecisionTreeBuildData &data);

DecisionTree
build_decision_tree(Table &table, Categories &categories)
{
  assert(categories.rows >= 1 && categories.cols >= 2);

  size_t max_category_count = 0;
  for (auto &category: categories.data)
    max_category_count = std::max(category.category_count(), max_category_count);

  auto tree = DecisionTree{ };
  tree.root = std::make_unique<DecisionTreeNode>();
  tree.categories = &categories;
  tree.goal_index = categories.cols - 1;

  auto categories_in_goal = categories.data[tree.goal_index].category_count();
  auto data = DecisionTreeBuildData{ };
  // Table is in column major order, so 'rows' and 'cols' are swapped.
  data.table.resize(categories.cols, categories.rows);
  data.samples_matrix.data.resize(max_category_count * categories_in_goal);
  data.front_samples_count.resize(max_category_count);
  data.back_samples_count.resize(max_category_count);

  data.used_columns.resize(categories.cols);
  data.row_indices.resize(categories.rows);
  data.sample_count_threshold = SAMPLE_COUNT_THRESHOLD;

  for (size_t col = 0; col < categories.cols; col++)
    {
      auto &category = categories.data[col];

      for (size_t row = 0; row < categories.rows; row++)
        {
          // Add 1 to ignore first column and row.
          auto &cell = table.grab(row + 1, col + 1);
          data.table.grab(col, row) = category.to_category_no_fail(cell);
        }
    }

  for (size_t i = 0; i < data.row_indices.size(); i++)
    data.row_indices[i] = i;

  data.used_columns[tree.goal_index] = true;

  auto node = DecisionTreeBuildDataNode{ };
  node.parent = nullptr;
  node.to_fill = tree.root.get();
  node.start_row = &data.row_indices.front();
  node.end_row = &data.row_indices.back() + 1;

  build_decision_tree(tree, node, data);

  return tree;
}

f64
compute_average_entropy_after_split(DecisionTree &tree, DecisionTreeBuildData &data, size_t column_index, size_t *start_row, size_t *end_row)
{
  {
    auto new_rows = tree.categories->data[column_index].category_count();
    auto new_cols = tree.categories->data[tree.goal_index].category_count();
    data.samples_matrix.resize(new_rows, new_cols);
    data.front_samples_count.resize(new_rows);
  }

  std::fill(data.samples_matrix.data.begin(), data.samples_matrix.data.end(), 0);
  std::fill(data.front_samples_count.begin(), data.front_samples_count.end(), 0);

  size_t samples_count = end_row - start_row;

  for (; start_row < end_row; start_row++)
    {
      auto row = data.table.grab(column_index, *start_row);
      auto col = data.table.grab(tree.goal_index, *start_row);

      ++data.samples_matrix.grab(row, col);
      ++data.front_samples_count[row];
    }

  f64 average_entropy = 0;

  for (size_t row = 0; row < data.samples_matrix.rows; row++)
    {
      auto samples_in_category = data.front_samples_count[row];
      f64  entropy = 0;

      // Compute 'entropy * samples_in_category', not just entropy. Since I need to compute weighted average of entropy anyway.
      for (size_t col = 0; col < data.samples_matrix.cols; col++)
        {
          auto samples = data.samples_matrix.grab(row, col);
          if (samples != 0)
            entropy += samples * std::log((f64)samples / samples_in_category) / log(2);
        }

      average_entropy += entropy / samples_count;
    }

  return -average_entropy;
}

CategoryId
find_best_goal_category(DecisionTree &tree, DecisionTreeBuildData &data, size_t *start_row, size_t *end_row)
{
  auto   best_goal_category = INVALID_CATEGORY_ID;
  size_t best_sample_count = 0;

  {
    auto count = tree.categories->data[tree.goal_index].category_count();
    data.front_samples_count.resize(count);
  }

  std::fill(data.front_samples_count.begin(), data.front_samples_count.end(), 0);

  for (; start_row < end_row; start_row++)
    {
      auto category = data.table.grab(tree.goal_index, *start_row);
      auto sample_count = ++data.front_samples_count[category];

      if (best_sample_count < sample_count)
        {
          best_sample_count = sample_count;
          best_goal_category = category;
        }
    }

  assert(best_goal_category != INVALID_CATEGORY_ID);

  return best_goal_category;
}

void
build_decision_tree(DecisionTree &tree, DecisionTreeBuildDataNode &node, DecisionTreeBuildData &data)
{
  auto all_columns_are_used = true;
  for (auto is_used: data.used_columns)
    all_columns_are_used = is_used && all_columns_are_used;

  size_t sample_count = node.end_row - node.start_row;

  if (all_columns_are_used || sample_count <= data.sample_count_threshold)
    {
      if (sample_count == 0)
        {
          // Root node should have at least one sample.
          assert(node.parent != nullptr);
          auto old_start_row = node.parent->start_row;
          auto old_end_row = node.parent->end_row;
          auto category = find_best_goal_category(tree, data, old_start_row, old_end_row);
          node.to_fill->column_index = tree.goal_index;
          node.to_fill->category = category;
          node.to_fill->sample_count = old_end_row - old_start_row;
          return;
        }
      else
        {
          auto category = find_best_goal_category(tree, data, node.start_row, node.end_row);
          node.to_fill->column_index = tree.goal_index;
          node.to_fill->category = category;
          node.to_fill->sample_count = sample_count;
          return;
        }
    }

  auto best_column = INVALID_COLUMN_INDEX;

  {
    f64 best_entropy = DBL_MAX;

    for (size_t i = 0; i < tree.categories->data.size(); i++)
      {
        if (!data.used_columns[i])
          {
            auto entropy = compute_average_entropy_after_split(tree, data, i, node.start_row, node.end_row);
            if (best_entropy > entropy)
              {
                std::swap(data.front_samples_count, data.back_samples_count);
                best_entropy = entropy;
                best_column = i;
              }
          }
      }
  }

  assert(best_column != INVALID_COLUMN_INDEX);

  auto category_count = tree.categories->data[best_column].category_count();
  node.to_fill->children.resize(category_count);
  node.to_fill->column_index = best_column;
  node.to_fill->category = INVALID_CATEGORY_ID;
  node.to_fill->sample_count = sample_count;

  std::vector<size_t *> offsets;
  offsets.resize(category_count + 1);

  offsets[0] = node.start_row;
  for (size_t i = 0; i < category_count; i++)
    {
      auto samples = data.back_samples_count[i];
      offsets[i + 1] = offsets[i] + samples;
    }

  auto const column_sorting_function =
    [&data, best_column](size_t left, size_t right) -> bool
    {
      return data.table.grab(best_column, left) < data.table.grab(best_column, right);
    };

  std::sort(node.start_row, node.end_row, column_sorting_function);

  data.used_columns[best_column] = true;

  for (size_t i = 0; i < category_count; i++)
    {
      auto subnode = DecisionTreeBuildDataNode{ };
      subnode.parent = &node;
      subnode.to_fill = &node.to_fill->children[i];
      subnode.start_row = offsets[i];
      subnode.end_row = offsets[i + 1];
      build_decision_tree(tree, subnode, data);
    }

  data.used_columns[best_column] = false;
}
