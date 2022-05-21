#include <iostream>
#include <cassert>

#include "src/Table.hpp"
#include "src/DecisionTree.hpp"

int main(int argc, char **argv)
{
  assert(argc == 2);

  Table table;

  table.read_csv(argv[1]);
  table.print();
  std::cout << '\n';

  DecisionTree tree;

  size_t columns_to_exclude[1] = { 0 };
  tree.construct(table, 0, columns_to_exclude, 1);
  tree.print();

  table.clean();
  tree.clean();
}
