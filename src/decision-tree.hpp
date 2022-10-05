#ifndef DECISION_TREE_HPP
#define DECISION_TREE_HPP

#include <map>
#include <cstdint>
#include <cstddef>

typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;
typedef float f32;
typedef double f64;

struct String
{
  char *data;
  size_t size;
};

struct StringView
{
  const char *data;
  size_t size;
};

enum TableCellType
  {
    Table_Cell_Integer,
    Table_Cell_Decimal,
    Table_Cell_String
  };

struct TableCell
{
  TableCellType type;

  union
  {
    i64 integer;
    f64 decimal;
    StringView string;
  } as;
};

struct Table
{
  TableCell *data;
  size_t rows, cols;
  std::map<String, u32> pool;
};

#endif // DECISION_TREE_HPP
