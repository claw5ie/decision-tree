#define SAMPLE_COUNT_THRESHOLD 3

constexpr size_t INVALID_COLUMN_INDEX = (size_t)-1;

struct DecisionTreeNode
{
  std::vector<DecisionTreeNode> children;
  size_t column_index;
  CategoryId category;
  size_t sample_count;

  void print(Categories &categories, size_t offset)
  {
    for (size_t i = offset; i-- > 0; )
      std::cout << ' ';

    if (!children.empty())
      std::cout << "<" << categories.labels[column_index] << " " << sample_count << ">\n";
    else
      std::cout << "'" << categories.data[column_index].to_string(category) << "' " << sample_count << '\n';

    for (auto &child: children)
      child.print(categories, offset + 4);
  }
};

struct DecisionTree
{
  struct ClassifyResult
  {
    std::string string;
    bool is_ok;
  };

  std::unique_ptr<DecisionTreeNode> root;
  Categories *categories;
  size_t goal_index;

  CategoryId classify(TableCell *data, size_t count)
  {
    // Account for goal column.
    assert(count + 1 >= categories->cols);

    auto node = root.get();

    do
      {
        if (node->children.empty())
          return node->category;
        else
          {
            assert(node->column_index < count);
            auto column = node->column_index;
            auto category = categories->data[column].to_category(data[column]);

            if (category == INVALID_CATEGORY_ID)
              return INVALID_CATEGORY_ID;

            node = &node->children[category];
          }
      }
    while (true);

    UNREACHABLE();
  }

  ClassifyResult classify_as_string(TableCell *data, size_t count)
  {
    auto category = classify(data, count);
    auto result = ClassifyResult{ };
    result.is_ok = true;

    if (category == INVALID_CATEGORY_ID)
      {
        result.is_ok = false;
        return result;
      }

    result.string = categories->data[goal_index].to_string(category);

    return result;
  }

  void print()
  {
    root->print(*categories, 0);
  }
};

// Data needed to build decision tree.
struct DecisionTreeBuildData
{
  Flattened2DArray<CategoryId> table;
  Flattened2DArray<size_t> samples_matrix;
  std::vector<size_t> front_samples_count;
  std::vector<size_t> back_samples_count;

  std::vector<bool> used_columns;
  std::vector<size_t> row_indices;
  size_t sample_count_threshold;
};

// Node info and sample range for the node that needs to be processed.
struct DecisionTreeBuildDataNode
{
  DecisionTreeBuildDataNode *parent;
  DecisionTreeNode *to_fill;
  size_t *start_row, *end_row;
};

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
