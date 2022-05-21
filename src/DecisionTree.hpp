#ifndef DECISION_TREE_HPP
#define DECISION_TREE_HPP

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

  Category *categories;
  std::string *names;
  size_t count;
  size_t goal;

  Node root;

  void construct(
    const Table &table,
    size_t goal,
    size_t *columns_to_exclude,
    size_t columns_count
    );

  void clean();

  // Data *parse_samples(cons char *samples) const;

  // size_t classify(const Samples *sample) const;

  void print() const;

private:
  struct Data
  {
    const Table &table;

    size_t *header,
      *theader,
      *samples;

    bool *used_columns;

    size_t *rows,
      *trows,
      **bins;
  };

  void construct(Node &node, Data &data, size_t start, size_t end);

  void clean(Node &root);

  void print(const Node &node, size_t category, int offset) const;

  double compute_entropy_after_split(
    size_t attr, const Data &data, const size_t *start, const size_t *end
    ) const;

  size_t to_category(const Table &table, size_t column, size_t row) const;
};

#endif // DECISION_TREE_HPP
