#include <cassert>
#include "Mat.hpp"

size_t &at_row_major(Matzu &self, size_t row, size_t column)
{
  assert(row < self.rows && column < self.cols);

  return self.data[row * self.cols + column];
}

size_t &at_column_major(Matzu &self, size_t column, size_t row)
{
  assert(row < self.rows && column < self.cols);

  return self.data[column * self.rows + row];
}

size_t at_column_major(const Matzu &self, size_t column, size_t row)
{
  assert(row < self.rows && column < self.cols);

  return self.data[column * self.rows + row];
}
