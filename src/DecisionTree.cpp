#include <iostream>
#include <limits>
#include <cstring>
#include <cmath>
#include "DecisionTree.hpp"

void DecisionTree::construct(
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

void DecisionTree::clean()
{
  for (size_t i = 0; i < count; i++)
    categories[i].clean();

  delete[] categories;
  delete[] names;

  clean(root);
}

// Data *DecisionTree::parse_samples(cons char *samples) const
// {
//   return nullptr;
// }

// size_t DecisionTree::classify(const Samples *sample) const
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

void DecisionTree::print() const
{
  std::cout << "<" << names[root.column] << " (" << root.samples << ")>\n";

  for (size_t i = 0; i < root.count; i++)
    print(root.children[i], i, TAB_WIDTH);
}

void DecisionTree::construct(Node &node, Data &data, size_t start, size_t end)
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
        data.theader, 0, categories[goal].count * sizeof (size_t)
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

void DecisionTree::clean(Node &root)
{
  for (size_t i = 0; i < root.count; i++)
    clean(root.children[i]);

  delete[] root.children;
}

void DecisionTree::print(const Node &node, size_t category, int offset) const
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

double DecisionTree::compute_entropy_after_split(
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

size_t DecisionTree::to_category(
  const Table &table, size_t column, size_t row
  ) const
{
  return categories[column].to_category(table.get(column, row));
}
