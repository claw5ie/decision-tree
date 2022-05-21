#include <limits>
#include <cassert>
#include "Category.hpp"
#include "Utils.hpp"

void Category::discretize(const Table &table, size_t column)
{
  auto const fill_bins =
    [](double min, double max, double *bins, size_t count)
    {
      --count;
      bins[0] = std::numeric_limits<double>::lowest();
      bins[count] = std::numeric_limits<double>::max();

      double const interval_length = (max - min) / count;
      for (size_t i = 1; i < count; i++)
        bins[i] = min + i * interval_length;
    };

  const auto &attr = table.columns[column];

  type = attr.type;

  switch (type)
  {
  case Table::Attribute::CATEGORY:
    count = attr.as.category.names->size();
    break;
  case Table::Attribute::INT32:
  {
    auto &int32 = as.int32;
    int32.values = new std::map<int32_t, CategoryValue>;

    int32_t min = std::numeric_limits<int32_t>::max(),
      max = std::numeric_limits<int32_t>::lowest();

    for (size_t i = 0; i < table.rows; i++)
    {
      auto const value = attr.as.int32s[i];

      if (int32.values->size() < INTEGER_CATEGORY_LIMIT)
        int32.values->emplace(value, int32.values->size());

      min = std::min(min, value);
      max = std::max(max, value);
    }

    count = int32.values->size();

    if (count >= INTEGER_CATEGORY_LIMIT)
    {
      delete int32.values;
      int32.values = nullptr;

      int32.bins = new double[BINS_COUNT];
      fill_bins(min, max, int32.bins, BINS_COUNT);

      count = BINS_COUNT - 1;
    }
  } break;
  case Table::Attribute::FLOAT64:
  {
    auto &float64 = as.float64;

    double min = std::numeric_limits<double>::max(),
      max = std::numeric_limits<double>::lowest();

    for (size_t i = 0; i < table.rows; i++)
    {
      auto const value = attr.as.float64s[i];

      min = std::min(min, value);
      max = std::max(max, value);
    }

    float64.bins = new double[BINS_COUNT];
    fill_bins(min, max, float64.bins, BINS_COUNT);

    count = BINS_COUNT - 1;
  } break;
  case Table::Attribute::INTERVAL:
  {
    auto &interval = as.interval;
    interval = new std::map<Interval, CategoryValue>;

    for (size_t i = 0; i < table.rows; i++)
    {
      interval->emplace(
        attr.as.intervals[i],
        interval->size()
        );
    }

    count = interval->size();
  } break;
  }
}

void Category::clean()
{
  switch (type)
  {
  case Table::Attribute::CATEGORY:
    return;
  case Table::Attribute::INT32:
    if (as.int32.values != nullptr)
      delete as.int32.values;
    else
      delete[] as.int32.bins;
    return;
  case Table::Attribute::FLOAT64:
    delete[] as.float64.bins;
    return;
  case Table::Attribute::INTERVAL:
    delete as.interval;
    return;
  }
}

CategoryValue Category::to_category(const Table::Attribute::Value &value) const
{
  switch (type)
  {
  case Table::Attribute::CATEGORY:
    return value.category;
  case Table::Attribute::INT32:
    if (as.int32.values != nullptr)
      return as.int32.values->find(value.int32)->second;
    else
      return binary_search_interval(value.int32, as.int32.bins, BINS_COUNT);
  case Table::Attribute::FLOAT64:
    return binary_search_interval(value.float64, as.float64.bins, BINS_COUNT);
  case Table::Attribute::INTERVAL:
  {
    auto const it = as.interval->lower_bound(value.interval);

    if (it != as.interval->end())
    {
      if (it->first.min <= value.interval.max &&
          value.interval.max <= it->first.max)
        return it->second;
    }

    return INVALID_CATEGORY;
  }
  }

  return INVALID_CATEGORY;
}
