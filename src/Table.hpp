#ifndef TABLE_HPP
#define TABLE_HPP

#include <map>
#include "String.hpp"

struct Interval
{
  double min,
    max;
};

bool operator<(const Interval &left, const Interval &right);

struct Attribute
{
  enum Type
  {
    STRING,
    INT64,
    FLOAT64,
    INTERVAL
  };

  struct Value
  {
    Type type;

    union
    {
      String string;
      int64_t int64;
      double float64;
      Interval interval;
    } as;
  };

  Type type;

  union
  {
    String *strings;
    int64_t *int64s;
    double *float64s;
    Interval *intervals;
  } as;

  String name;
  bool is_initialized;
};

struct Table
{
  Attribute *columns;
  size_t cols,
    rows;
};

Attribute::Value get(const Table &self, size_t column, size_t row);

Table read_csv(const char *filepath);

void clean(Table &self);

void print(const Table &self);

#endif // TABLE_HPP
