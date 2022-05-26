#ifndef DECISION_TREE_HPP
#define DECISION_TREE_HPP

#include "String.hpp"
#include "Table.hpp"
#include "Category.hpp"
#include "Utils.hpp"

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

  struct Params
  {
    size_t goal;
    size_t threshold;
    size_t *columns_to_exclude;
    size_t columns_count;
  };

  Categories categories;
  size_t goal;
  Node *root;
  String *names;
  MemoryPool pool;
};

DecisionTree construct(
  const Table &table,
  const Table::Selection &sel,
  const DecisionTree::Params &params
  );

void clean(const DecisionTree &self);

std::pair<const Table::Cell, bool>
from_class(const DecisionTree &self, size_t category);

size_t *classify(const DecisionTree &self, Table &samples);

void print(const DecisionTree &self);

#endif // DECISION_TREE_HPP
