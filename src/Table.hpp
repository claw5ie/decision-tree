#ifndef TABLE_HPP
#define TABLE_HPP

#include <map>
#include "String.hpp"
#include "Intervals.hpp"

enum AttributeType
{
  STRING,
  INT64,
  FLOAT64,
  INTERVAL
};

const char *to_string(AttributeType type);

struct Table
{
  struct Selection
  {
    size_t row_beg,
      row_end,
      col_beg,
      col_end;
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
  size_t rows,
    cols;
  MemoryPool pool;
};

Table read_csv(const char *filepath);

void clean(const Table &samples);

const Table::Cell &get(const Table &self, size_t row, size_t column);

Table::Cell &get(Table &self, size_t row, size_t column);

void print(const Table &self);

StringView to_string(const Table::Cell &value);

bool promote(Table &self, AttributeType to, const Table::Selection &sel);

void assert_selection_is_valid(
  const Table &table, const Table::Selection &selection
  );

bool have_same_type(const Table &self, const Table::Selection &sel);

#endif // TABLE_HPP
