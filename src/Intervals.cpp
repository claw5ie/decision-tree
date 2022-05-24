#include <cassert>
#include "Intervals.hpp"

bool operator<(const Interval &left, const Interval &right)
{
  if (left.min >= right.min)
    return left.max < right.max;
  else
    return true;
}

size_t binary_search_interval(double value, const SearchInterval &interval)
{
  assert(interval.count >= 2);

  size_t lower = 0,
    upper = interval.count - 1;

  while (lower < upper)
  {
    size_t const mid = lower + (upper - lower) / 2;
    double const beg = interval.min + mid * interval.step;

    if (value < beg)
      upper = mid;
    else if (value > beg + interval.step)
      lower = mid + 1;
    else
      return mid;
  }

  return size_t(-1);
}
