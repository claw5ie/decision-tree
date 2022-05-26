#include <iostream>
#include <limits>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cassert>
#include "Mat.hpp"
#include "Utils.hpp"
#include "DecisionTree.hpp"

#define MEMORY_POOL_SIZE (1024 * 1024 / 2)

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
      cons.samples.rows = self.categories.data[attr].count;
      cons.samples.cols = self.categories.data[self.goal].count;

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
      size_t best_goal_category = INVALID_CATEGORY;
      size_t best_samples_count = 0;

      // Reusing temporary header.
      std::memset(
        cons.theader, 0, self.categories.data[self.goal].count * sizeof (size_t)
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

      assert(best_goal_category != INVALID_CATEGORY);

      return best_goal_category;
    };

  if (mut.end - mut.start <= cons.threshold)
    return;

  size_t best_attribute = INVALID_CATEGORY;

  {
    double best_entropy = std::numeric_limits<double>::max();

    for (size_t i = 0; i < self.categories.count; i++)
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
  if (best_attribute == INVALID_CATEGORY)
  {
    mut.node = { find_best_goal_category(mut.start, mut.end),
                 nullptr, 0, mut.end - mut.start };

    return;
  }

  size_t const category_count = self.categories.data[best_attribute].count;

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

DecisionTree construct(
  const Table &table,
  const Table::Selection &sel,
  const DecisionTree::Params &params
  )
{
  assert_selection_is_valid(table, sel);

  DecisionTree tree;

  tree.categories = discretize(table, sel);
  tree.goal = params.goal - sel.col_beg;
  tree.pool = allocate(MEMORY_POOL_SIZE);
  tree.root = (DecisionTree::Node *)reserve_array(
    tree.pool, sizeof (DecisionTree::Node), 1
    );
  *tree.root = { INVALID_CATEGORY, nullptr, 0, 0 };

  if (sel.row_beg <= 0)
  {
    tree.names = nullptr;
  }
  else
  {
    tree.names = (String *)reserve_array(
      tree.pool, sizeof (String), tree.categories.count
      );
  }

  size_t max_matrix_rows_count = 0;

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
  {
    size_t const index = col - sel.col_beg;

    if (tree.names != nullptr)
    {
      StringView const view = to_string(get(table, sel.row_beg - 1, col));
      tree.names[index].data = push(tree.pool, view.data, view.size);
      tree.names[index].size = view.size;
    }

    max_matrix_rows_count =
      std::max(max_matrix_rows_count, tree.categories.data[index].count);
  }

  size_t const rows = sel.row_end - sel.row_beg,
    cols = sel.col_end - sel.col_beg;
  size_t offsets[6];

  char *const data = allocate_in_chunks(
    offsets,
    6,
    cols * rows * sizeof (size_t),
    max_matrix_rows_count * sizeof (size_t),
    max_matrix_rows_count * sizeof (size_t),
    max_matrix_rows_count * tree.categories.data[tree.goal].count *
      sizeof (size_t),
    cols * sizeof (bool),
    rows * sizeof (size_t)
    );

  ConstTreeData cons_data = {
    { (size_t *)data, rows, cols },
    params.threshold,
    (size_t *)(data + offsets[0]),
    (size_t *)(data + offsets[1]),
    { (size_t *)(data + offsets[2]), 0, 0 },
    (bool *)(data + offsets[3]),
    (size_t *)(data + offsets[4])
  };

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
  {
    size_t const index = col - sel.col_beg;
    auto &category = tree.categories.data[index];

    for (size_t row = sel.row_beg; row < sel.row_end; row++)
    {
      // Weird formating, assigning is happening here.
      size_t const value = (at_column_major(
                              cons_data.table, index, row - sel.row_beg
                              ) = to_category(
                                category, get(table, row, col))
        );

      if (value >= category.count)
      {
        std::cerr << "ERROR: couldn't categorize value at (row, column) = ("
                  << row << ", " << col
                  << "). Aborting, as it may cause undefined behaviour.\n";
        std::exit(EXIT_FAILURE);
      }
    }
  }

  std::memset(cons_data.used_columns, 0, offsets[4] - offsets[3]);

  cons_data.used_columns[tree.goal] = true;
  for (size_t i = 0; i < params.columns_count; i++)
    cons_data.used_columns[params.columns_to_exclude[i]] = true;

  for (size_t i = 0; i < rows; i++)
    cons_data.rows[i] = i;

  construct(tree, MutableTreeData { *tree.root, 0, rows }, cons_data);

  delete[] data;

  return tree;
}

void clean(const DecisionTree &self)
{
  clean(self.categories);
  delete[] self.pool.data;
}

std::pair<const Table::Cell, bool>
from_class(const DecisionTree &self, size_t category)
{
  return from_category(self.categories.data[self.goal], category);
}

size_t classify(const DecisionTree &self, const Table &samples, size_t row)
{
  const DecisionTree::Node *curr = self.root;

  while (curr->count > 0)
  {
    if (curr->column >= self.categories.count)
      return INVALID_CATEGORY;

    size_t const category = to_category(
      self.categories.data[curr->column],
      get(samples, row, curr->column)
      );

    if (category < curr->count)
      curr = curr->children + category;
    else
      return INVALID_CATEGORY;
  }

  return curr->column;
}

size_t *classify(const DecisionTree &self, Table &samples)
{
  if (self.categories.count != samples.cols)
  {
    std::cerr << "ERROR: the number of columns in decision tree doesn't match "
      "the number of columns in samples.\n";
    std::exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < samples.cols; i++)
  {
    if (self.categories.data[i].type == AttributeType::INTERVAL)
    {
      bool const promoted_everything = promote(
        samples,
        AttributeType::INTERVAL,
        { 0, samples.rows, i, i + 1 }
        );

      if (!promoted_everything)
      {
        std::cerr << "ERROR: couldn't promote column "
                  << i
                  << " to type `Interval`.\n";
        std::exit(EXIT_FAILURE);
      }
    }
    else
    {
      if (!have_same_type(samples, { 0, samples.rows, i, i + 1 }))
      {
        std::cerr << "ERROR: column "
                  << i
                  << " has values of different types.\n";
        std::exit(EXIT_FAILURE);
      }
    }
  }

  size_t *const classes = new size_t[samples.rows];

  for (size_t i = 0; i < samples.rows; i++)
    classes[i] = classify(self, samples, i);

  return classes;
}

void print(const DecisionTree &self, const DecisionTree::Node &node, int offset)
{
  auto const print_from_category =
    [&self, &node](const Category &category, size_t category_as_int) -> void
    {
      auto category_class = from_category(category, category_as_int);

      if (!category_class.second)
        std::cout << "(null)";
      else
        std::cout << to_string(category_class.first).data;
    };

  if (node.count > 0)
  {
    std::cout << '\n';
    putsn(" ", offset);

    if (self.names != nullptr)
      std::cout << '<' << self.names[node.column].data;
    else
      std::cout << "<unnamed" << node.column;

    std::cout << " (" << node.samples << ")>\n";

    offset += TAB_WIDTH;

    for (size_t i = 0; i < node.count; i++)
    {
      putsn(" ", offset);
      print_from_category(self.categories.data[node.column], i);
      std::cout << ':';

      print(self, node.children[i], offset);
    }
  }
  else
  {
    std::cout << ' ';
    print_from_category(self.categories.data[self.goal], node.column);
    std::cout << " ("
              << node.samples
              << ")\n";
  }
}

void print(const DecisionTree &self)
{
  print(self, *self.root, 0);
}
