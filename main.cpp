#include <iostream>
#include <cassert>

#include "src/Table.hpp"
#include "src/DecisionTree.hpp"

int main(int argc, char **argv)
{
  assert(argc >= 2);

  TableColumnMajor table = read_csv_column_major(argv[1]);

  size_t columns_to_exclude[1] = { 0 };
  DecisionTree tree = construct(table, { 2, 3, columns_to_exclude, 1 });

  print(table);
  print(tree);

  if (argc == 3)
  {
    TableRowMajor samples = read_csv_row_major(argv[2]);
    size_t *const classes = classify(tree, samples);

    for (size_t i = 0; i < samples.rows; i++)
    {
      std::cout << "row " << i << ": ";
      print_goal_category(tree, classes[i]);
      std::cout << '\n';
    }

    delete[] classes;
    clean(samples);
  }

  clean(table);
  clean(tree);
}
