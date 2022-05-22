#ifndef CATEGORY_HPP
#define CATEGORY_HPP

#include <map>
#include "Table.hpp"

#define INTEGER_CATEGORY_LIMIT 7u
#define BINS_COUNT 4u

struct Category
{
  Table::Attribute::Type type;

  union
  {
    std::map<std::string, CategoryValue> *category;

    struct
    {
      std::map<int32_t, CategoryValue> *values;
      double *bins;
    } int32;

    struct
    {
      double *bins;
    } float64;

    std::map<Interval, CategoryValue> *interval;
  } as;

  size_t count;

  void discretize(const Table &table, size_t column);

  void clean();
};

#endif // CATEGORY_HPP
