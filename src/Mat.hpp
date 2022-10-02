#ifndef MAT_HPP
#define MAT_HPP

#include <cstddef>

struct Matzu
{
  size_t *data;
  size_t rows,
    cols;
};

size_t &at_row_major(Matzu &self, size_t row, size_t column);

size_t &at_column_major(Matzu &self, size_t column, size_t row);

size_t at_column_major(const Matzu &self, size_t column, size_t row);

#endif // MAT_HPP
