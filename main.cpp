#include <iostream>
#include <cassert>

#include "src/Table.hpp"
#include "src/DecisionTree.hpp"

int main(int argc, char **argv)
{
  assert(argc >= 2);

  Table table = read_csv(argv[1]);

  size_t columns_to_exclude[1] = { 0 };
  DecisionTree tree = construct(table, { 0, 3, columns_to_exclude, 1 });

  print(table);
  print(tree);

  if (argc == 3)
  {
    char *const file_content = read_entire_file(argv[2]);
    Samples samples = read_samples_from_string(file_content);

    delete[] file_content;

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
