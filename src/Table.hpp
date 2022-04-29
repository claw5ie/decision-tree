#ifndef TABLE_HPP
#define TABLE_HPP

#include <string>
#include <map>

struct Table
{
  struct Category
  {
    std::string name;
    std::map<std::string, uint32_t> values;
#ifndef NDEBUG
    std::map<uint32_t, std::string> names;
#endif

    uint32_t insert(const char *string, uint32_t size);
  };

  enum AttributeType
  {
    ATTRIBUTE,
    INT32,
    DOUBLE
  };

  struct AttributeData
  {
    union
    {
      uint32_t attribute;
      int32_t int32;
      double decimal;
    } as;
  };

  Category *categories;
  AttributeType *types;
  AttributeData *data;
  size_t rows,
    cols;
  char *raw_data;

  void allocate(size_t rows, size_t cols);

  inline AttributeData &get(size_t row, size_t col);

  inline const AttributeData &get(size_t row, size_t col) const;

  void print() const;

  void deallocate();
};

Table read_csv(const char *const filepath);

#endif // TABLE_HPP
