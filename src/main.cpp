#include "decision-tree.cpp"

#include <iostream>
#include <iomanip>
#include <limits>
#include <cstring>
#include <cassert>

#define INVALID_OPTION (std::numeric_limits<size_t>::max())


Table::Selection read_selection(const char *&string)
{
  auto read_interval =
    [&string](size_t &start, size_t &end) -> void
      {
        if (*string == '-')
        {
          start = 0;
          ++string;

          if (*string != ',' && *string != '\0')
            end = read_zu(string);
          else
            end = std::numeric_limits<size_t>::max();
        }
        else
        {
          start = read_zu(string);
          require_char(*string, '-');
          string++;

          if (*string != ',' && *string != '\0')
            end = read_zu(string);
          else
            end = std::numeric_limits<size_t>::max();
        }
      };

  Table::Selection sel = {
    0,
    std::numeric_limits<size_t>::max(),
    0,
    std::numeric_limits<size_t>::max()
  };

  while (*string != '\0')
  {
    if (*string == 'r')
    {
      string++;
      read_interval(sel.row_beg, sel.row_end);
    }
    else if (*string == 'c')
    {
      string++;
      read_interval(sel.col_beg, sel.col_end);
    }
    else
    {
      std::cerr << "ERROR: invalid prefix: `" << *string << "`.\n";
      std::exit(EXIT_FAILURE);
    }

    string += *string == ',';
  }

  if (sel.row_beg > sel.row_end)
    std::swap(sel.row_beg, sel.row_end);

  if (sel.col_beg > sel.col_end)
    std::swap(sel.col_beg, sel.col_end);

  return sel;
}

int main(int argc, char **argv)
{
  assert(argc >= 0);

  struct Option
  {
    char short_opt;
    const char *long_opt;
    bool has_arg;
  };

  Option const option_list[] = {
    { 'd', "data", true },
    { 's', "samples", true },
    { '\0', "selection", true },
    { 't', "threshold", true },
    { '\0', "show-config", false },
    { '\0', "print-table", false },
    { '\0', "print-tree", false }
  };

  auto const find_option =
    [option_list](const char *arg) -> size_t
      {
        if (
          !(arg[0] == '-' &&
            ((std::isalpha(arg[1]) && arg[2] == '\0') ||
             (arg[1] == '-' && std::isalpha(arg[2]))))
          )
        {
          return INVALID_OPTION;
        }

        for (size_t i = 0; i < sizeof(option_list) / sizeof(*option_list); i++)
        {
          auto &option = option_list[i];

          if (
            arg[1] == option.short_opt ||
            (option.long_opt != nullptr &&
             !std::strcmp(arg + 2, option.long_opt))
            )
          {
            return i;
          }
        }

        return INVALID_OPTION;
      };

  size_t threshold = 3;
  const char *data = nullptr;
  const char *samples = nullptr;
  Table::Selection sel = {
    0,
    std::numeric_limits<size_t>::max(),
    0,
    std::numeric_limits<size_t>::max()
  };
  bool should_show_config = false;
  bool should_print_table = false;
  bool should_print_tree = false;

  for (size_t i = 1; i < (size_t)argc; i++)
  {
    size_t const option = find_option(argv[i]);

    if (option == INVALID_OPTION)
    {
      std::cerr << "ERROR: unrecognized option: \"" << argv[i] << "\".\n";

      return EXIT_FAILURE;
    }
    else if ((i += option_list[option].has_arg) >= (size_t)argc)
    {
      std::cerr << "ERROR: expected argument for the option \""
                << argv[i - 1]
                << "\".\n";

      return EXIT_FAILURE;
    }

    switch (option)
    {
    case 0:
      data = argv[i];
      break;
    case 1:
      samples = argv[i];
      break;
    case 2:
    {
      const char *arg = argv[i];
      sel = read_selection(arg);
    } break;
    case 3:
    {
      const char *arg = argv[i];
      threshold = read_zu(arg);
      assert(*arg == '\0');
    } break;
    case 4:
      should_show_config = true;
      break;
    case 5:
      should_print_table = true;
      break;
    case 6:
      should_print_tree = true;
      break;
    }
  }

  if (data == nullptr)
  {
    std::cerr << "ERROR: table was not provided, cannot proceed without it.\n";
    return EXIT_FAILURE;
  }

  Table table = read_csv(data);

  {
    // Fix selection.
    sel.row_end = std::min(sel.row_end, table.rows);
    sel.col_end = std::min(sel.col_end, table.cols);
  }

  if (should_show_config)
  {

    std::cout << "Config:\n - data file: "
              << data
              << "\n - samples file: "
              << (samples != nullptr ? samples : "(no file provided)")
              << "\n - selection: rows: "
              << sel.row_beg << '-' << sel.row_end
              << "; columns: "
              << sel.col_beg << '-' << sel.col_end
              << "\n - threshold: "
              << threshold
              << "\n\n";
  }

  DecisionTree tree = construct(table, sel,threshold);

  if (should_print_table)
    print(table);

  if (should_print_tree)
  {
    print(tree);
    std::cout << '\n';
  }

  if (samples != nullptr)
  {
    Table samples_table = read_csv(samples);
    size_t *const classes = classify(tree, samples_table);

    std::cout << " Row  |  Class\n";
    for (size_t i = 0; i < samples_table.rows; i++)
    {
      std::cout << std::setw(5) << i << " | ";

      auto value = from_class(tree, classes[i]);
      if (!value.second)
        std::cout << "(failed to categorize)\n";
      else
        std::cout << to_string(value.first).data << '\n';
    }

    delete[] classes;
    clean(samples_table);
  }

  clean(table);
  clean(tree);
}
