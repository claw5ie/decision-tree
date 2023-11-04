#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>

#include <cstring>
#include <cstdint>
#include <cassert>
#include <cfloat>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using i64 = int64_t;
using f64 = double;

#include "decision-tree.hpp"
#include "decision-tree.cpp"

int
main(int argc, char **argv)
{
  auto table = parse_csv(argc > 1 ? argv[1] : "datasets/test.csv");
  table.print();
  auto ctable = categorize(table);
  ctable.print();
}
