#include <iostream>
#include <limits>
#include <cassert>
#include "Category.hpp"

void CategoryKind::StringKind::discretize(
  const TableColumnMajor &table, const TableColumnMajor::Column &column
  )
{
  assert(column.type == Attribute::STRING);

  for (size_t i = 0; i < table.rows; i++)
  {
    const auto &value = column.as.strings[i];

    if (to.find(value) == to.end())
      to.emplace(copy(value), to.size());
  }

  from = new String[to.size()];

  for (auto &elem: to)
    from[elem.second] = elem.first;
}

void CategoryKind::Int64::discretize(
  const TableColumnMajor &table, const TableColumnMajor::Column &column
  )
{
  assert(column.type == Attribute::INT64);

  int64_t min = std::numeric_limits<int64_t>::max(),
    max = std::numeric_limits<int64_t>::lowest();

  for (size_t i = 0; i < table.rows; i++)
  {
    auto const value = column.as.int64s[i];

    if (to.size() < INTEGER_CATEGORY_LIMIT)
      to.emplace(value, to.size());

    min = std::min(min, value);
    max = std::max(max, value);
  }

  interval = { (double)min, (double)(max - min) / (BINS_COUNT - 1), BINS_COUNT };

  if (to.size() >= INTEGER_CATEGORY_LIMIT)
  {
    from = nullptr;
    to.clear();
  }
  else
  {
    from = new int64_t[to.size()];

    for (auto &elem: to)
      from[elem.second] = elem.first;
  }
}

void CategoryKind::Float64::discretize(
  const TableColumnMajor &table, const TableColumnMajor::Column &column
  )
{
  assert(column.type == Attribute::FLOAT64);

  double min = std::numeric_limits<double>::max(),
    max = std::numeric_limits<double>::lowest();

  for (size_t i = 0; i < table.rows; i++)
  {
    auto const value = column.as.float64s[i];

    min = std::min(min, value);
    max = std::max(max, value);
  }

  interval = { min, (max - min) / (BINS_COUNT - 1), BINS_COUNT };
}

void CategoryKind::IntervalKind::discretize(
  const TableColumnMajor &table, const TableColumnMajor::Column &column
  )
{
  assert(column.type == Attribute::INTERVAL);

  for (size_t i = 0; i < table.rows; i++)
    to.emplace(column.as.intervals[i], to.size());

  from = new Interval[to.size()];

  for (auto &elem: to)
    from[elem.second] = elem.first;
}

void CategoryKind::StringKind::clean()
{
  for (size_t i = 0; i < to.size(); i++)
    delete[] from[i].data;

  delete[] from;
}

/*
  If the attribute was split into bins, form is set to "nullptr", "delete" should
  treat that for us.
*/
void CategoryKind::Int64::clean()
{
  delete[] from;
}

void CategoryKind::Float64::clean()
{
  // No need to clean things up.
}

void CategoryKind::IntervalKind::clean()
{
  delete[] from;
}

size_t CategoryKind::StringKind::count() const
{
  return to.size();
}

size_t CategoryKind::Int64::count() const
{
  return to.size() > 0 ? to.size() : interval.count - 1;
}

size_t CategoryKind::Float64::count() const
{
  return interval.count - 1;
}

size_t CategoryKind::IntervalKind::count() const
{
  return to.size();
}

size_t CategoryKind::StringKind::to_category(const Attribute::Value &value) const
{
  auto const it = to.find(value.as.string);

  return (it != to.end()) ? it->second : size_t(-1);
}

size_t CategoryKind::Int64::to_category(const Attribute::Value &value) const
{
  if (to.size() > 0)
  {
    auto const it = to.find(value.as.int64);

    return (it != to.end()) ? it->second : size_t(-1);
  }
  else
  {
    return binary_search_interval(value.as.int64, interval);
  }
}

size_t CategoryKind::Float64::to_category(const Attribute::Value &value) const
{
  return binary_search_interval(value.as.float64, interval);
}

size_t CategoryKind::IntervalKind::to_category(const Attribute::Value &value) const
{
  auto const it = to.find(value.as.interval);

  return (it != to.end()) ? it->second : size_t(-1);
}

void CategoryKind::StringKind::print_from_category(size_t category) const
{
  if (category < to.size())
    std::cout << from[category].data;
  else
    std::cout << "Invalid category";
}

void CategoryKind::Int64::print_from_category(size_t category) const
{
  if (to.size() > 0 && category < to.size())
    std::cout << from[category];
  else if (category + 1 < interval.count)
    std::cout << '['
              << interval.min + category * interval.step
              << ", "
              << interval.min + (category + 1) * interval.step
              << ']';
  else
    std::cout << "Invalid category";
}

void CategoryKind::Float64::print_from_category(size_t category) const
{
  if (category + 1 < interval.count)
    std::cout << '['
              << interval.min + category * interval.step
              << ", "
              << interval.min + (category + 1) * interval.step
              << ']';
  else
    std::cout << "Invalid category";
}

void CategoryKind::IntervalKind::print_from_category(size_t category) const
{
  if (category < to.size())
    std::cout << '[' << from[category].min << ", " << from[category].max << ']';
  else
    std::cout << "Invalid category";
}

Category *new_category(Attribute::Type type)
{
  switch (type)
  {
  case Attribute::STRING:
    return new CategoryKind::StringKind;
  case Attribute::INT64:
    return new CategoryKind::Int64;
  case Attribute::FLOAT64:
    return new CategoryKind::Float64;
  case Attribute::INTERVAL:
    return new CategoryKind::IntervalKind;
  default:
    ;
  }

  return nullptr;
}
