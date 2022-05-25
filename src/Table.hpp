#ifndef TABLE_HPP
#define TABLE_HPP

#include <map>
#include "String.hpp"
#include "Intervals.hpp"

namespace Attribute
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
}

const char *to_string(Attribute::Type type);

struct TableColumnMajor
{
  struct Column
  {
    Attribute::Type type;

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

  Column *columns;
  size_t cols,
    rows;
};

TableColumnMajor read_csv_column_major(const char *filepath);

void clean(const TableColumnMajor &self);

Attribute::Value get(const TableColumnMajor &self, size_t column, size_t row);

void print(const TableColumnMajor &self);

struct TableRowMajor
{
  Attribute::Value *data;
  size_t rows,
    cols;
};

/*
  The name is misleading. It may look like this function works the same as
  column major version, but actually this one doesn't read the first row to gather
  information about column names.
*/
TableRowMajor read_csv_row_major(const char *filepath);

void clean(const TableRowMajor &samples);

#endif // TABLE_HPP
