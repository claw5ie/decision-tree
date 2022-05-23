#include <limits>
#include <cassert>
#include "Category.hpp"
#include "Utils.hpp"

Category discretize(const Table &table, size_t column)
{
  auto const fill_bins =
    [](double min, double max, double *bins, size_t count) -> void
    {
      --count;
      bins[0] = std::numeric_limits<double>::lowest();
      bins[count] = std::numeric_limits<double>::max();

      double const interval_length = (max - min) / count;
      for (size_t i = 1; i < count; i++)
        bins[i] = min + i * interval_length;
    };

  const auto &attr = table.columns[column];

  Category category;

  category.type = attr.type;

  switch (category.type)
  {
  case Attribute::STRING:
  {
    auto &map = category.as.string;
    map = new std::map<String, size_t, StringComparator>;

    for (size_t i = 0; i < table.rows; i++)
    {
      const auto &value = attr.as.strings[i];

      if (map->find(value) == map->end())
        map->emplace(copy(value), map->size());
    }

    category.count = map->size();
  } break;
  case Attribute::INT64:
  {
    auto &int64 = category.as.int64;
    int64.map = new std::map<int64_t, size_t>;

    int64_t min = std::numeric_limits<int64_t>::max(),
      max = std::numeric_limits<int64_t>::lowest();

    for (size_t i = 0; i < table.rows; i++)
    {
      auto const value = attr.as.int64s[i];

      if (int64.map->size() < INTEGER_CATEGORY_LIMIT)
        int64.map->emplace(value, int64.map->size());

      min = std::min(min, value);
      max = std::max(max, value);
    }

    category.count = int64.map->size();

    if (category.count >= INTEGER_CATEGORY_LIMIT)
    {
      delete int64.map;
      int64.map = nullptr;

      int64.bins = new double[BINS_COUNT];
      fill_bins(min, max, int64.bins, BINS_COUNT);

      category.count = BINS_COUNT - 1;
    }
  } break;
  case Attribute::FLOAT64:
  {
    auto &float64 = category.as.float64;

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

    category.count = BINS_COUNT - 1;
  } break;
  case Attribute::INTERVAL:
  {
    auto &interval = category.as.interval;
    interval = new std::map<Interval, size_t>;

    for (size_t i = 0; i < table.rows; i++)
    {
      interval->emplace(
        attr.as.intervals[i],
        interval->size()
        );
    }

    category.count = interval->size();
  } break;
  }

  return category;
}

void clean(Category &self)
{
  switch (self.type)
  {
  case Attribute::STRING:
    for (auto &elem: *self.as.string)
      delete[] elem.first.data;
    delete self.as.string;
    return;
  case Attribute::INT64:
    if (self.as.int64.map != nullptr)
      delete self.as.int64.map;
    else
      delete[] self.as.int64.bins;
    return;
  case Attribute::FLOAT64:
    delete[] self.as.float64.bins;
    return;
  case Attribute::INTERVAL:
    delete self.as.interval;
    return;
  }
}

size_t to_category(const Category &self, const Attribute::Value &value)
{
  switch (self.type)
  {
  case Attribute::STRING:
  {
    auto const it = self.as.string->find(value.as.string);

    if (it != self.as.string->end())
      return it->second;
  } break;
  case Attribute::INT64:
  {
    if (self.as.int64.map != nullptr)
    {
      auto const it = self.as.int64.map->find(value.as.int64);

      if (it != self.as.int64.map->end())
        return it->second;
    }
    else
    {
      return binary_search_interval(
        value.as.int64, self.as.int64.bins, BINS_COUNT
        );
    }
  } break;
  case Attribute::FLOAT64:
    return binary_search_interval(
      value.as.float64, self.as.float64.bins, BINS_COUNT
      );
  case Attribute::INTERVAL:
    return self.as.interval->find(value.as.interval)->second;
  }

  return size_t(-1);
}
