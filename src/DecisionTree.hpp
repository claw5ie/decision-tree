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

  Category **categories;
  String *names;
  size_t count;
  size_t goal;
  Node *root;
};

DecisionTree construct(const Table &table, const DecisionTree::Params &params);

void clean(DecisionTree &self);

// Data *parse_samples(cons char *samples);

// size_t classify(const DecisionTree &self, const Samples *sample);

void print(const DecisionTree &self);

#endif // DECISION_TREE_HPP
