#include <iostream>
#include <limits>
#include <cassert>
#include "Category.hpp"

#define MEMORY_POOL_SIZE (1024 * 1024 / 2)

size_t to_category(const Category &self, const Table::Cell &value)
{
  if (self.type != value.type)
    assert(self.type == value.type);

  switch (self.type)
  {
  case AttributeType::STRING:
  {
    auto &string = *self.as.string;
    auto const it = string.to.find(value.as.string);

    return it != string.to.end() ? it->second : INVALID_CATEGORY;
  }
  case AttributeType::INT64:
  {
    auto &int64 = *self.as.int64;

    if (int64.to.size() > 0)
    {
      auto const it = int64.to.find(value.as.int64);

      return it != int64.to.end() ? it->second : INVALID_CATEGORY;
    }
    else
    {
      return binary_search_interval(value.as.int64, int64.interval);
    }
  };
  case AttributeType::FLOAT64:
    return binary_search_interval(value.as.float64, self.as.float64->interval);
  case AttributeType::INTERVAL:
  {
    auto &interval = *self.as.interval;
    auto const it = interval.to.find(value.as.interval);

    return it != interval.to.end() ? it->second : INVALID_CATEGORY;
  }
  }

  return INVALID_CATEGORY;
}

std::pair<const Table::Cell, bool>
from_category(const Category &self, size_t category)
{
  Table::Cell cell;
  bool succeeded = false;

  cell.type = self.type;

  switch (self.type)
  {
  case AttributeType::STRING:
    if (category < self.count)
    {
      cell.as.string = self.as.string->from[category];
      succeeded = true;
    }
    break;
  case AttributeType::INT64:
  {
    auto &int64 = *self.as.int64;

    if (int64.to.size() > 0 && category < int64.to.size())
    {
      cell.as.int64 = int64.from[category];
      succeeded = true;
    }
    else if (category < self.count)
    {
      cell.type = AttributeType::INTERVAL;
      cell.as.interval = {
        int64.interval.min + category * int64.interval.step,
        int64.interval.min + (category + 1) * int64.interval.step,
      };
      succeeded = true;
    }
  } break;
  case AttributeType::FLOAT64:
    if (category < self.count)
    {
      auto &interval = self.as.float64->interval;
      cell.type = AttributeType::INTERVAL;
      cell.as.interval = {
        interval.min + category * interval.step,
        interval.min + (category + 1) * interval.step
      };
      succeeded = true;
    }
    break;
  case AttributeType::INTERVAL:
    if (category < self.count)
    {
      cell.as.interval = self.as.interval->from[category];
      succeeded = true;
    }
    break;
  }

  return std::pair<Table::Cell, bool>(cell, succeeded);
}

Categories discretize(const Table &table, const Table::Selection &sel)
{
  assert_selection_is_valid(table, sel);

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
  {
    for (size_t row = sel.row_beg; row + 1 < sel.row_end; row++)
    {
      if (get(table, row, col).type != get(table, row + 1, col).type)
      {
        std::cerr << "ERROR: values at rows "
                  << row
                  << " and "
                  << row + 1
                  << ", column "
                  << col
                  << " have different types.\n";
        std::exit(EXIT_FAILURE);
      }
    }
  }

  Categories categories;

  categories.count = sel.col_end - sel.col_beg;
  categories.data = new Category[categories.count];
  categories.pool = allocate(MEMORY_POOL_SIZE);

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
  {
    auto &category = categories.data[col - sel.col_beg];

    category.type = get(table, sel.row_beg, col).type;

    switch (category.type)
    {
    case AttributeType::STRING:
      category.as.string = new Category::StringC;
      break;
    case AttributeType::INT64:
      category.as.int64 = new Category::Int64C;
      break;
    case AttributeType::FLOAT64:
      category.as.float64 = new Category::Float64C;
      break;
    case AttributeType::INTERVAL:
      category.as.interval = new Category::IntervalC;
      break;
    }
  }

  for (size_t col = sel.col_beg; col < sel.col_end; col++)
  {
    auto &category = categories.data[col - sel.col_beg];

    switch (category.type)
    {
    case AttributeType::STRING:
    {
      auto &string = *category.as.string;

      for (size_t row = sel.row_beg; row < sel.row_end; row++)
      {
        auto &value = get(table, row, col);

        auto const it = string.to.emplace(
          push(categories.pool, value.as.string),
          string.to.size()
          );

        if (!it.second)
          pop(categories.pool, value.as.string.size);
      }

      string.from = (String *)reserve_array(
        categories.pool, sizeof (String), string.to.size()
        );

      for (auto &elem: string.to)
        string.from[elem.second] = push(categories.pool, elem.first);

      category.count = string.to.size();
    } break;
    case AttributeType::INT64:
    {
      auto &int64 = *category.as.int64;

      int64_t min = std::numeric_limits<int64_t>::max(),
        max = std::numeric_limits<int64_t>::lowest();

      for (size_t row = sel.row_beg; row < sel.row_end; row++)
      {
        auto &value = get(table, row, col);

        if (int64.to.size() < INTEGER_CATEGORY_LIMIT)
          int64.to.emplace(value.as.int64, int64.to.size());

        min = std::min(min, value.as.int64);
        max = std::max(max, value.as.int64);
      }

      int64.interval = {
        (double)min,
        (double)(max - min) / (BINS_COUNT - 1),
        BINS_COUNT
      };

      if (int64.to.size() >= INTEGER_CATEGORY_LIMIT)
      {
        int64.from = nullptr;
        int64.to.clear();
        category.count = BINS_COUNT - 1;
      }
      else
      {
        int64.from = (int64_t *)reserve_array(
          categories.pool, sizeof (int64_t), int64.to.size()
          );

        for (auto &elem: int64.to)
          int64.from[elem.second] = elem.first;

        category.count = int64.to.size();
      }
    } break;
    case AttributeType::FLOAT64:
    {
      double min = std::numeric_limits<double>::max(),
        max = std::numeric_limits<double>::lowest();

      for (size_t row = sel.row_beg; row < sel.row_end; row++)
      {
        auto &value = get(table, row, col);

        min = std::min(min, value.as.float64);
        max = std::max(max, value.as.float64);
      }

      category.as.float64->interval = {
        min,
        (max - min) / (BINS_COUNT - 1),
        BINS_COUNT
      };

      category.count = BINS_COUNT - 1;
    } break;
    case AttributeType::INTERVAL:
    {
      auto &interval = *category.as.interval;

      for (size_t row = sel.row_beg; row < sel.row_end; row++)
        interval.to.emplace(get(table, row, col).as.interval, interval.to.size());

      interval.from = (Interval *)reserve_array(
        categories.pool, sizeof (Interval), interval.to.size()
        );

      for (auto &elem: interval.to)
        interval.from[elem.second] = elem.first;

      category.count = interval.to.size();
    } break;
    }
  }

  return categories;
}

void clean(const Categories &self)
{
  for (size_t i = 0; i < self.count; i++)
  {
    auto &category = self.data[i];

    switch (category.type)
    {
    case AttributeType::STRING:
      delete category.as.string;
      break;
    case AttributeType::INT64:
      delete category.as.int64;
      break;
    case AttributeType::FLOAT64:
      delete category.as.float64;
      break;
    case AttributeType::INTERVAL:
      delete category.as.interval;
      break;
    }
  }

  delete[] self.data;
  delete[] self.pool.data;
}
