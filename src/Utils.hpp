#ifndef UTILS_HPP
#define UTILS_HPP

char *allocate_in_chunks(size_t *offsets, size_t count, ...);

void putsn(const char *string, size_t count);

/*
  Given a sorted array, returns index such that
  "array[index] <= value && value < array[index + 1]". Never fails, bacause we are
  assuming that every value belongs to a certain category, that is, the smallest
  and largest value in the array is the smallest and largest possible value for
  "double" datatype. In case of value equal to largest possible value of "double",
  last category is returned.
*/
size_t binary_search_interval(double value, const double *array, size_t size);

const char *parse_int32(const char *str, int32_t &val);

const char *parse_float64(const char *str, double &val);

#endif // UTILS_HPP
