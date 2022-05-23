#ifndef CATEGORY_HPP
#define CATEGORY_HPP

#include <map>
#include "String.hpp"
#include "Table.hpp"

#define INTEGER_CATEGORY_LIMIT 7u
#define BINS_COUNT 4u

struct Category
{
  Attribute::Type type;

  union
  {
    std::map<String, size_t, StringComparator> *string;

    struct
    {
      std::map<int64_t, size_t> *map;
      double *bins;
    } int64;

    struct
    {
      double *bins;
    } float64;

    std::map<Interval, size_t> *interval;
  } as;

  size_t count;
};

Category discretize(const Table &table, size_t column);

void clean(Category &category);

size_t to_category(const Category &self, const Attribute::Value &value);

#endif // CATEGORY_HPP
