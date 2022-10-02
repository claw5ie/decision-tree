#ifndef DECISION_TREE_HPP
#define DECISION_TREE_HPP

#include <map>
#include <cstddef>

#define INVALID_CATEGORY (std::numeric_limits<size_t>::max())
#define INTEGER_CATEGORY_LIMIT 7u
#define BINS_COUNT 4u

struct StringView
{
  const char *data;
  size_t size;
};

struct String
{
  char *data;
  size_t size;
};

struct StringComparator
{
  bool operator()(const String &left, const String &right) const;
};

struct MemoryPool
{
  char *data;
  size_t size, reserved;
};

struct Matzu
{
  size_t *data;
  size_t rows, cols;
};

struct Interval
{
  double min, max;
};

// "count" satisfies the equation "min + count * step == max"
struct SearchInterval
{
  double min, step;
  size_t count;
};

// Searches for index "i" such that
// "min + i * step <= value && value <= min + (i + 1) * step". "i" satisfies
// "0 <= i && i <= count - 1" if the index is in range, otherwise size_t(-1) is
// returned;

enum AttributeType
  {
    STRING,
    INT64,
    FLOAT64,
    INTERVAL
  };

struct Table
{
  struct Selection
  {
    size_t row_beg, row_end, col_beg, col_end;
  };

  struct Cell
  {
    AttributeType type;

    union
    {
      String string;
      int64_t int64;
      double float64;
      Interval interval;
    } as;
  };

  Cell *data;
  size_t rows, cols;
  MemoryPool pool;
};

#define TAB_WIDTH 2

struct Category
{
  struct StringC
  {
    std::map<String, size_t, StringComparator> to;
    String *from;
  };

  struct Int64C
  {
    std::map<int64_t, size_t> to;
    int64_t *from;
    SearchInterval interval;
  };

  struct Float64C
  {
    SearchInterval interval;
  };

  struct IntervalC
  {
    std::map<Interval, size_t> to;
    Interval *from;
  };

  AttributeType type;

  union
  {
    StringC *string;
    Int64C *int64;
    Float64C *float64;
    IntervalC *interval;
  } as;

  size_t count;
};

struct Categories
{
  Category *data;
  size_t count;
  MemoryPool pool;
};

struct DecisionTree
{
  struct Node
  {
    size_t column;
    Node *children;
    size_t count;
    size_t samples;
  };

  Categories categories;
  size_t goal;
  Node *root;
  String *names;
  MemoryPool pool;
};

#endif // DECISION_TREE_HPP
