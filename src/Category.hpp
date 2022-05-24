#ifndef CATEGORY_HPP
#define CATEGORY_HPP

#include <map>
#include "String.hpp"
#include "Table.hpp"

#define INTEGER_CATEGORY_LIMIT 7u
#define BINS_COUNT 4u

struct Category
{
  virtual void discretize(const Table &table, const Attribute &column) = 0;

  virtual void clean() = 0;

  virtual size_t count() const = 0;

  virtual size_t to_category(const Attribute::Value &value) const = 0;

  virtual void print_from_category(size_t category) const = 0;
};

namespace CategoryKind
{
  struct StringKind: public Category
  {
    std::map<String, size_t, StringComparator> to;
    String *from;

    void discretize(const Table &table, const Attribute &column);

    void clean();

    size_t count() const;

    size_t to_category(const Attribute::Value &value) const;

    void print_from_category(size_t category) const;
  };

  struct Int64: public Category
  {
    std::map<int64_t, size_t> to;
    int64_t *from;
    SearchInterval interval;

    void discretize(const Table &table, const Attribute &column);

    void clean();

    size_t count() const;

    size_t to_category(const Attribute::Value &value) const;

    void print_from_category(size_t category) const;
  };

  struct Float64: public Category
  {
    SearchInterval interval;

    void discretize(const Table &table, const Attribute &column);

    void clean();

    size_t count() const;

    size_t to_category(const Attribute::Value &value) const;

    void print_from_category(size_t category) const;
  };

  struct IntervalKind: public Category
  {
    std::map<Interval, size_t> to;
    Interval *from;

    void discretize(const Table &table, const Attribute &column);

    void clean();

    size_t count() const;

    size_t to_category(const Attribute::Value &value) const;

    void print_from_category(size_t category) const;
  };
}

Category *new_category(Attribute::Type type);

#endif // CATEGORY_HPP
