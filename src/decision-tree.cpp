#include "decision-tree.hpp"

#include <iostream>
#include <fstream>
#include <cstring>
#include <cassert>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;

bool
operator<(const String &left, const String &right)
{
  return strcmp(left.data, right.data) < 0;
}

char *
read_entire_file(const char *filepath)
{
  ifstream file(filepath);

  if (!file.is_open())
    abort();

  size_t file_size;

  {
    struct stat stats;

    if (stat(filepath, &stats) == -1)
      abort();

    file_size = stats.st_size;
  }

  char *file_data = new char[file_size + 1];

  file.read(file_data, file_size);
  file_data[file_size] = '\0';
  file.close();

  return file_data;
}

Table
read_csv(const char *filepath)
{
  char *const file_data = read_entire_file(filepath);

  Table table = { nullptr, 1, 1, { } };

  {
    const char *at = file_data;

    while (*at != '\n' && *at != '\0')
      table.cols += *at++ == ',';

    while (*at != '\0')
      table.rows += *at++ == '\n';
  }

  table.data = new TableCell[table.rows * table.cols];

  size_t total_count = 0, elems_on_row = 0;
  const char *at = file_data;

  while (true)
    {
      TableCell cell;

      if (isdigit(*at))
        {
          i64 value = 0;

          do
            value = 10 * value + (*at++ - '0');
          while (isdigit(*at));

          if (*at == '.')
            {
              if (!isdigit(*++at))
                abort();

              f64 frac = 0, powah = 1;

              do
                frac += (*at++ - '0') * (powah /= 10);
              while (isdigit(*at));

              cell.type = Table_Cell_Decimal;
              cell.as.decimal = value + frac;
            }
          else
            {
              cell.type = Table_Cell_Integer;
              cell.as.integer = value;
            }
        }
      else if (isalpha(*at))
        {
          String str;
          str.size = 1;

          for (char ch = at[str.size];
               isalnum(ch) || ch == '-' || ch == '_';
               ch = at[++str.size])
            ;

          str.data = new char[str.size + 1];
          memcpy(str.data, at, str.size);
          str.data[str.size] = '\0';

          at += str.size;

          table.pool.emplace(str, table.pool.size());

          cell.type = Table_Cell_String;
          cell.as.string = { str.data, str.size };
        }
      else
        assert(false && "invalid token");

      ++elems_on_row;
      table.data[total_count++] = cell;

      if (elems_on_row < table.cols)
        assert(*at++ == ',');
      else if (total_count == table.rows * table.cols)
        {
          assert(*at == '\0');
          break;
        }
      else
        {
          // Elements are read one by one, so we can't overshoot.
          // That is, elements on single row can't be more than
          // the columns of table.
          assert(*at++ == '\n');
          elems_on_row = 0;
        }
    }

  delete[] file_data;

  return table;
}

void
print(const Table *t)
{
  for (size_t i = 0; i < t->rows; i++)
    {
      for (size_t j = 0; j < t->cols; j++)
        {
          auto *cell = &t->data[i * t->cols + j];

          switch (cell->type)
            {
            case Table_Cell_Integer:
              cout << cell->as.integer;
              break;
            case Table_Cell_Decimal:
              cout << cell->as.decimal;
              break;
            case Table_Cell_String:
              cout << cell->as.string.data;
              break;
            }

          cout << (j + 1 < t->cols ? ',' : '\n');
        }
    }
}
