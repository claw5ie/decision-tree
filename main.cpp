#include <iostream>
#include <cassert>

#include "src/Table.hpp"
#include "src/DecisionTree.hpp"

int main(int argc, char **argv)
{
  assert(argc == 2);

  Table table = read_csv(argv[1]);

  print(table);
  std::cout << '\n';

  size_t columns_to_exclude[1] = { 0 };
  DecisionTree tree = construct(table, { 0, 3, columns_to_exclude, 1 });

  print(tree);

  clean(table);
  clean(tree);
}
