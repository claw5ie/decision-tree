#include <cassert>
#include "src/Table.hpp"

int main(int argc, char **argv)
{
  assert(argc == 2);

  Table table = read_csv(argv[1]);

  table.print();
}
