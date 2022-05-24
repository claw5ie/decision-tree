#include <iostream>
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
    auto &maps = category.as.string;
    maps.to = new std::map<String, size_t, StringComparator>;

    for (size_t i = 0; i < table.rows; i++)
    {
      const auto &value = attr.as.strings[i];

      if (maps.to->find(value) == maps.to->end())
        maps.to->emplace(copy(value), maps.to->size());
    }

    category.count = maps.to->size();

    maps.from = new String[category.count];

    for (auto &elem: *maps.to)
      maps.from[elem.second] = elem.first;
  } break;
  case Attribute::INT64:
  {
    auto &int64 = category.as.int64;
    int64.maps.to = new std::map<int64_t, size_t>;

    int64_t min = std::numeric_limits<int64_t>::max(),
      max = std::numeric_limits<int64_t>::lowest();

    for (size_t i = 0; i < table.rows; i++)
    {
      auto const value = attr.as.int64s[i];

      if (int64.maps.to->size() < INTEGER_CATEGORY_LIMIT)
        int64.maps.to->emplace(value, int64.maps.to->size());

      min = std::min(min, value);
      max = std::max(max, value);
    }

    category.count = int64.maps.to->size();

    if (category.count >= INTEGER_CATEGORY_LIMIT)
    {
      delete int64.maps.to;
      int64.maps.to = nullptr;

      int64.bins = new double[BINS_COUNT];
      fill_bins(min, max, int64.bins, BINS_COUNT);

      category.count = BINS_COUNT - 1;
    }
    else
    {
      int64.maps.from = new int64_t[category.count];

      for (auto &elem: *int64.maps.to)
        int64.maps.from[elem.second] = elem.first;
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
    interval.to = new std::map<Interval, size_t>;

    for (size_t i = 0; i < table.rows; i++)
    {
      interval.to->emplace(
        attr.as.intervals[i],
        interval.to->size()
        );
    }

    category.count = interval.to->size();
    interval.from = new Interval[category.count];

    for (auto &elem: *interval.to)
      interval.from[elem.second] = elem.first;
  } break;
  }

  return category;
}

void clean(Category &self)
{
  switch (self.type)
  {
  case Attribute::STRING:
    for (size_t i = 0; i < self.count; i++)
      delete[] self.as.string.from[i].data;
    delete self.as.string.to;
    delete[] self.as.string.from;
    return;
  case Attribute::INT64:
    if (self.as.int64.maps.to != nullptr)
    {
      delete self.as.int64.maps.to;
      delete[] self.as.int64.maps.from;
    }
    else
    {
      delete[] self.as.int64.bins;
    }
    return;
  case Attribute::FLOAT64:
    delete[] self.as.float64.bins;
    return;
  case Attribute::INTERVAL:
    delete self.as.interval.to;
    delete[] self.as.interval.from;
    return;
  }
}

size_t to_category(const Category &self, const Attribute::Value &value)
{
  switch (self.type)
  {
  case Attribute::STRING:
  {
    auto const it = self.as.string.to->find(value.as.string);

    if (it != self.as.string.to->end())
      return it->second;
  } break;
  case Attribute::INT64:
  {
    if (self.as.int64.maps.to != nullptr)
    {
      auto const it = self.as.int64.maps.to->find(value.as.int64);

      if (it != self.as.int64.maps.to->end())
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
    return self.as.interval.to->find(value.as.interval)->second;
  }

  return size_t(-1);
}

void print_from_category(const Category &self, size_t category)
{
  auto const print_interval_from_bin =
    [&self, &category](const double *bins)
    {
      if (category == 0)
        std::cout << "-inf-" << bins[category + 1];
      else if (category + 1 == self.count)
        std::cout << bins[category] << "-+inf";
      else
        std::cout << bins[category] << '-' << bins[category + 1];
    };

  if (category < self.count)
  {
    switch (self.type)
    {
    case Attribute::STRING:
      std::cout << self.as.string.from[category].data;
      break;
    case Attribute::INT64:
      if (self.as.int64.maps.to != nullptr)
        std::cout << self.as.int64.maps.from[category];
      else
        print_interval_from_bin(self.as.int64.bins);
      break;
    case Attribute::FLOAT64:
    {
      print_interval_from_bin(self.as.float64.bins);
    } break;
    case Attribute::INTERVAL:
    {
      auto &interval = self.as.interval.from[category];
      std::cout << interval.min << '-' << interval.max;
    } break;
    }
  }
  else
  {
    std::cout << "Invalid category";
  }
}
