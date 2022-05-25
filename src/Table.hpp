#ifndef TABLE_HPP
#define TABLE_HPP

#include <map>
#include "String.hpp"
#include "Intervals.hpp"

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

const char *to_string(Attribute::Type type);

void find_table_size(const char *string, size_t &rows, size_t &cols);

Attribute::Value read_attribute_value(const char *&str);

Table read_csv(const char *filepath);

Attribute::Value get(const Table &self, size_t column, size_t row);

void clean(Table &self);

void print(const Table &self);

#endif // TABLE_HPP
