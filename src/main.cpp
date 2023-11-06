#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <limits>
#include <algorithm>

#include <cmath>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cfloat>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using i64 = int64_t;
using f64 = double;

#include "utils.cpp"
#include "tokenizer.cpp"
#include "table.cpp"
#include "categories.cpp"
#include "decision-tree.cpp"

int
main(int argc, char **argv)
{
  auto table = parse_csv_from_file(argc > 1 ? argv[1] : "datasets/test.csv");
  table.print();
  auto categories = categorize(table);
  categories.print();
  auto dt = build_decision_tree(table, categories);
  dt.print();

  std::cout << "\nGive me some samples!\n";

  auto samples = parse_csv_from_stdin();
  for (size_t row = 0; row < samples.rows; row++)
    {
      auto row_ptr = &samples.grab(row, 0);
      auto [category, is_ok] = dt.classify_as_string(row_ptr, samples.cols);
      if (is_ok)
        std::cout << row << ": " << category << '\n';
      else
        std::cout << row << ": " << "Couldn't classify\n";
    }
}
