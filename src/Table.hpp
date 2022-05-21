#ifndef TABLE_HPP
#define TABLE_HPP

#include <string>
#include <map>

using CategoryValue = uint32_t;

#define INVALID_CATEGORY (std::numeric_limits<CategoryValue>::max())

struct Interval
{
  double min,
    max;
};

bool operator<(const Interval &left, const Interval &right);

struct Table
{
  struct Attribute
  {
    enum Type
    {
      CATEGORY,
      INT32,
      FLOAT64,
      INTERVAL
    };

    union Value
    {
      uint32_t category;
      int32_t int32;
      double float64;
      Interval interval;
    };

    Type type;

    union
    {
      struct
      {
        std::map<std::string, CategoryValue> *names;
        CategoryValue *data;
      } category;

      int32_t *int32s;

      double *float64s;

      Interval *intervals;
    } as;

    std::string name;
    bool is_initialized;

    void insert_category(size_t row, const char *string, size_t size);
  };

  Attribute *columns;
  size_t cols,
    rows;

  void read_csv(const char *filepath);

  Attribute::Value get(size_t col, size_t row) const;

  void clean();

  void print() const;
};

#endif // TABLE_HPP
