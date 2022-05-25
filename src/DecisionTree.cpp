#include <iostream>
#include <limits>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cassert>
#include "Mat.hpp"
#include "Utils.hpp"
#include "DecisionTree.hpp"

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

void construct(
  const DecisionTree &self, const MutableTreeData &mut, ConstTreeData &cons
  )
{
  auto const compute_entropy_after_split =
    [&self, &cons](size_t attr, const size_t *start, const size_t *end) -> double
    {
      cons.samples.rows = self.categories[attr]->count();
      cons.samples.cols = self.categories[self.goal]->count();

      std::memset(
        cons.theader, 0, cons.samples.rows * sizeof (size_t)
        );
      std::memset(
        cons.samples.data,
        0,
        cons.samples.cols * cons.samples.rows * sizeof (size_t)
        );

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
              samples * std::log(samples / samples_in_category) / std::log(2);
          }
        }

        mean_entropy += entropy / samples_count;
      }

      return -mean_entropy;
    };

  auto const find_best_goal_category =
    [&self, &cons](size_t start, size_t end)
    {
      size_t best_goal_category = size_t(-1);
      size_t best_samples_count = 0;

      // Reusing temporary header.
      std::memset(
        cons.theader, 0, self.categories[self.goal]->count() * sizeof (size_t)
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

      assert(best_goal_category != size_t(-1));

      return best_goal_category;
    };

  if (mut.end - mut.start <= cons.threshold)
    return;

  size_t best_attribute = size_t(-1);

  {
    double best_entropy = std::numeric_limits<double>::max();

    for (size_t i = 0; i < self.count; i++)
    {
      if (!cons.used_columns[i])
      {
        double const entropy = compute_entropy_after_split(
          i, cons.rows + mut.start, cons.rows + mut.end
          );

        if (best_entropy > entropy)
        {
          std::swap(cons.header, cons.theader);
          best_entropy = entropy;
          best_attribute = i;
        }
      }
    }
  }

  // No columns to process.
  if (best_attribute == size_t(-1))
  {
    mut.node = { find_best_goal_category(mut.start, mut.end),
                 nullptr, 0, mut.end - mut.start };

    return;
  }

  size_t const category_count = self.categories[best_attribute]->count();

  mut.node.column = best_attribute;
  mut.node.children = new DecisionTree::Node[category_count];
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

  std::sort(
    cons.rows + mut.start,
    cons.rows + mut.end,
    [&cons, &best_attribute](size_t left, size_t right) -> bool
    {
      return at_column_major(cons.table, best_attribute, left) <
        at_column_major(cons.table, best_attribute, right);
    }
    );

  cons.used_columns[best_attribute] = true;

  for (size_t i = 0; i < mut.node.count; i++)
  {
    construct(
      self,
      MutableTreeData { mut.node.children[i], offsets[i], offsets[i + 1] },
      cons
      );
  }

  cons.used_columns[best_attribute] = false;

  delete[] offsets;
}

DecisionTree construct(const Table &table, const DecisionTree::Params &params)
{
  DecisionTree tree = {
    new Category *[table.cols],
    new String[table.cols],
    table.cols,
    params.goal,
    new DecisionTree::Node{ }
  };

  size_t max_rows_count = 0;

  for (size_t i = 0; i < table.cols; i++)
  {
    auto &column = table.columns[i];

    (tree.categories[i] = new_category(column.type))->discretize(table, column);
    tree.names[i] = copy(column.name);
    max_rows_count =
      std::max(max_rows_count, tree.categories[i]->count());
  }

  size_t offsets[6];

  char *const data = allocate_in_chunks(
    offsets,
    6,
    table.cols * table.rows * sizeof (size_t),
    max_rows_count * sizeof (size_t),
    max_rows_count * sizeof (size_t),
    max_rows_count * tree.categories[tree.goal]->count() * sizeof (size_t),
    table.cols * sizeof (bool),
    table.rows * sizeof (size_t)
    );

  ConstTreeData cons_data = {
    { (size_t *)data, table.rows, table.cols },
    params.threshold,
    (size_t *)(data + offsets[0]),
    (size_t *)(data + offsets[1]),
    { (size_t *)(data + offsets[2]), 0, 0 },
    (bool *)(data + offsets[3]),
    (size_t *)(data + offsets[4])
  };

  for (size_t i = 0; i < table.cols; i++)
  {
    auto &category = tree.categories[i];

    for (size_t j = 0; j < table.rows; j++)
    {
      at_column_major(cons_data.table, i, j) =
        category->to_category(get(table, i, j));
    }
  }

  std::memset(cons_data.used_columns, 0, offsets[4] - offsets[3]);

  cons_data.used_columns[tree.goal] = true;
  for (size_t i = 0; i < params.columns_count; i++)
    cons_data.used_columns[params.columns_to_exclude[i]] = true;

  for (size_t i = 0; i < table.rows; i++)
    cons_data.rows[i] = i;

  construct(tree, MutableTreeData { *tree.root, 0, table.rows }, cons_data);

  delete[] data;

  return tree;
}

void clean(DecisionTree::Node &root)
{
  for (size_t i = 0; i < root.count; i++)
    clean(root.children[i]);

  delete[] root.children;
}

void clean(DecisionTree &self)
{
  for (size_t i = 0; i < self.count; i++)
  {
    self.categories[i]->clean();
    delete[] self.names[i].data;
  }

  delete[] self.categories;
  delete[] self.names;

  clean(*self.root);
  delete self.root;
}

Samples read_samples_from_string(const char *string)
{
  Samples samples;

  find_table_size(string, samples.rows, samples.cols);

  samples.data = new Attribute::Value[samples.cols * samples.rows];

  for (size_t i = 0; i < samples.rows; i++)
  {
    for (size_t j = 0; j < samples.cols; j++)
    {
      samples.data[i * samples.cols + j] = read_attribute_value(string);

      if (j + 1 < samples.cols)
        require_char(*string, ',');
      else if (i + 1 < samples.rows)
        require_char(*string, '\n');
      else
        require_char(*string, '\0');

      string += *string != '\0';
    }
  }

  return samples;
}

void clean(Samples &self)
{
  for (size_t i = 0; i < self.cols * self.rows; i++)
  {
    if (self.data[i].type == Attribute::STRING)
      delete[] self.data[i].as.string.data;
  }

  delete[] self.data;
}

size_t classify(const DecisionTree &self, const Attribute::Value *sample)
{
  const DecisionTree::Node *curr = self.root;

  while (curr->count > 0)
  {
    size_t const category =
      self.categories[curr->column]->to_category(sample[curr->column]);

    if (category < curr->count)
      curr = curr->children + category;
    else
      return size_t(-1);
  }

  return curr->column;
}

size_t *classify(const DecisionTree &self, const Samples &samples)
{
  assert(self.count == samples.cols);

  size_t *const classes = new size_t[samples.rows];

  for (size_t i = 0; i < samples.rows; i++)
    classes[i] = classify(self, samples.data + i * samples.cols);

  return classes;
}

void print(const DecisionTree &self, const DecisionTree::Node &node, int offset)
{
  if (node.count != 0)
  {
    std::cout << '\n';
    putsn(" ", offset);
    std::cout << '<' << self.names[node.column].data
              << " (" << node.samples << ")>\n";

    offset += TAB_WIDTH;

    for (size_t i = 0; i < node.count; i++)
    {
      putsn(" ", offset);
      self.categories[node.column]->print_from_category(i);
      std::cout << ':';

      print(self, node.children[i], offset);
    }
  }
  else
  {
    std::cout << ' ';
    self.categories[self.goal]->print_from_category(node.column);
    std::cout << " ("
              << node.samples
              << ")\n";
  }
}

void print(const DecisionTree &self)
{
  print(self, *self.root, 0);
}

void print_goal_category(const DecisionTree &self, size_t category)
{
  self.categories[self.goal]->print_from_category(category);
}
