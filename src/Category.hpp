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
    struct
    {
      std::map<String, size_t, StringComparator> *to;
      String *from;
    } string;

    struct
    {
      struct
      {
        std::map<int64_t, size_t> *to;
        int64_t *from;
      } maps;

      double *bins;
    } int64;

    struct
    {
      double *bins;
    } float64;

    struct
    {
      std::map<Interval, size_t> *to;
      Interval *from;
    } interval;
  } as;

  size_t count;
};

Category discretize(const Table &table, size_t column);

void clean(Category &category);

size_t to_category(const Category &self, const Attribute::Value &value);

void print_from_category(const Category &self, size_t category);

#endif // CATEGORY_HPP
