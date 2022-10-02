#ifndef INTERVALS_HPP
#define INTERVALS_HPP

#include <cstddef>

struct Interval
{
  double min,
    max;
};

bool operator<(const Interval &left, const Interval &right);

/*
   "count" satisfies the equation "min + count * step == max"
*/
struct SearchInterval
{
  double min,
    step;
  size_t count;
};

/*
  Searches for index "i" such that
  "min + i * step <= value && value <= min + (i + 1) * step". "i" satisfies
  "0 <= i && i <= count - 1" if the index is in range, otherwise size_t(-1) is
  returned;
*/
size_t binary_search_interval(double value, const SearchInterval &interval);

#endif // INTERVALS_HPP
