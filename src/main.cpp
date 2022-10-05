#include "decision-tree.cpp"

int
main(void)
{
  Table table = read_csv("datasets/test.csv");

  print(&table);
}
