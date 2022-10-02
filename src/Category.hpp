#ifndef CATEGORY_HPP
#define CATEGORY_HPP

#include <map>
#include "String.hpp"
#include "Table.hpp"

#define INVALID_CATEGORY (std::numeric_limits<size_t>::max())
#define INTEGER_CATEGORY_LIMIT 7u
#define BINS_COUNT 4u

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

size_t to_category(const Category &self, const Table::Cell &value);

std::pair<const Table::Cell, bool>
from_category(const Category &self, size_t category);

struct Categories
{
  Category *data;
  size_t count;
  MemoryPool pool;
};

Categories discretize(const Table &table, const Table::Selection &sel);

void clean(const Categories &self);

#endif // CATEGORY_HPP
