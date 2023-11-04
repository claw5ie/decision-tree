#ifndef DECISION_TREE_HPP
#define DECISION_TREE_HPP

#define UNREACHABLE() assert(false && "unreachable")
#define PRINT_ERROR(filepath, line_info, message, ...) fprintf(stderr, "%s:%zu:%zu: error: " message "\n", filepath, (line_info).line, (line_info).column, __VA_ARGS__)
#define PRINT_ERROR0(filepath, line_info, message) fprintf(stderr, "%s:%zu:%zu: error: " message "\n", filepath, (line_info).line, (line_info).column)

#define MAX_CATEGORIES_FOR_INTEGERS 7
#define BINS_COUNT 4

struct LineInfo
{
  size_t offset = 0, line = 1, column = 1;
};

enum TokenType
  {
    Token_Integer,
    Token_Decimal,
    Token_String,
    Token_Comma,
    Token_New_Line,
    Token_End_Of_File,
  };

struct Token
{
  TokenType type;
  std::string_view text;
  LineInfo line_info;
};

struct Tokenizer
{
  constexpr static uint8_t LOOKAHEAD = 2;

  Token tokens_buffer[LOOKAHEAD];
  uint8_t token_start = 0;
  uint8_t token_count = 0;
  LineInfo line_info;
  const char *filepath;
  std::string source;

  TokenType peek()
  {
    if (token_count == 0)
      buffer_token();

    return tokens_buffer[token_start].type;
  }

  Token grab()
  {
    if (token_count == 0)
      buffer_token();

    return tokens_buffer[token_start];
  }

  void advance()
  {
    assert(token_count > 0);
    ++token_start;
    token_start %= LOOKAHEAD;
    --token_count;
  }

  void expect_comma_or_new_line()
  {
    switch (peek())
      {
      case Token_Comma:
        advance();
        break;
      case Token_New_Line:
      case Token_End_Of_File:
        break;
      default:
        {
          auto token = grab();
          PRINT_ERROR(filepath, token.line_info, "expected ',', new line or EOF, but got '%.*s'.", (int)token.text.size(), token.text.data());
          exit(EXIT_FAILURE);
        }
      }
  }

  void advance_line_info(char ch)
  {
    ++line_info.offset;
    ++line_info.column;
    if (ch == '\n')
      {
        ++line_info.line;
        line_info.column = 1;
      }
  }

  void buffer_token()
  {
    auto at = &source[line_info.offset];
    auto has_new_line = false;

    while (isspace(*at))
      {
        has_new_line = (*at == '\n') || has_new_line;
        advance_line_info(*at++);
      }

    auto token = Token{};
    token.type = Token_End_Of_File;
    token.text = { at, 0 };
    token.line_info = line_info;

    if (has_new_line)
      {
        token.type = Token_New_Line;
        // Line info is not properly set, but I don't think it is needed.
      }
    else if (*at == '\0')
      ;
    else if (*at == ',')
      {
        advance_line_info(*at++);

        token.type = Token_Comma;
        token.text = { token.text.data(), size_t(at - token.text.data()) };
      }
    else if (isdigit(*at))
      {
        do
          advance_line_info(*at++);
        while (isdigit(*at));

        token.type = Token_Integer;
        token.text = { token.text.data(), size_t(at - token.text.data()) };

        if (*at == '.')
          {
            do
              advance_line_info(*at++);
            while (isdigit(*at));

            token.type = Token_Decimal;
            token.text = { token.text.data(), size_t(at - token.text.data()) };
          }
      }
    else if (isalpha(*at))
      {
        do
          advance_line_info(*at++);
        while (isalnum(*at) || *at == '-' || *at == '_');

        token.type = Token_String;
        token.text = { token.text.data(), size_t(at - token.text.data()) };
      }
    else
      {
        PRINT_ERROR(filepath, token.line_info, "unrecognized token '%c'.", *at);
        exit(EXIT_FAILURE);
      }

    assert(token_count < LOOKAHEAD);
    uint8_t index = (token_start + token_count) % LOOKAHEAD;
    tokens_buffer[index] = token;
    ++token_count;
  }
};

enum TableCellType
  {
    Table_Cell_Integer,
    Table_Cell_Decimal,
    Table_Cell_String,
  };

union TableCellData
{
  i64 integer;
  f64 decimal;
  std::string_view string;

  TableCellData()
  {
    // Union is ill-formed without constructor.
  }
};

struct TableCell
{
  TableCellType type;
  TableCellData as;

  std::string stringify()
  {
    switch (type)
      {
      case Table_Cell_Integer:
        return std::to_string(as.integer);
      case Table_Cell_Decimal:
        return std::to_string(as.decimal);
      case Table_Cell_String:
        return std::string{ as.string };
      }

    UNREACHABLE();
  }
};

struct Table
{
  std::vector<TableCell> data;
  size_t rows = 0, cols = 0;
  std::set<std::string> string_pool;

  TableCell &grab(size_t row, size_t col)
  {
    assert(row < rows && col < cols);
    return data[row * cols + col];
  }

  void print()
  {
    std::cout << "Rows: " << rows;
    std::cout << "\nColumns:    " << cols;
    std::cout << '\n';

    for (size_t i = 0; i < rows; i++)
      {
        for (size_t j = 0; j < cols; j++)
          {
            auto &cell = grab(i, j);
            switch (cell.type)
              {
              case Table_Cell_Integer:
                std::cout << cell.as.integer;
                break;
              case Table_Cell_Decimal:
                std::cout << cell.as.decimal;
                break;
              case Table_Cell_String:
                std::cout << cell.as.string;
                break;
              }

            std::cout << (j + 1 < cols ? ',' : '\n');
          }
      }
  }
};

struct SubdividedInterval
{
  f64 min, step;
  size_t count;
};

using CategoryId = size_t;

struct CategoryOfIntegers
{
  std::map<i64, CategoryId> to;
  std::vector<i64> from;
};

struct CategoryOfDecimals
{
  SubdividedInterval interval;
};

struct CategoryOfStrings
{
  std::map<std::string, CategoryId> to;
  std::vector<std::string_view> from;
};

union CategoryData
{
  CategoryOfIntegers integers;
  CategoryOfDecimals decimals;
  CategoryOfStrings strings;

  CategoryData()
  {
    // Union is ill-formed without constructor.
  }

  ~CategoryData()
  {
    // Union is ill-formed without destructor.
  }
};

enum CategoryType
  {
    Category_Of_Integers,
    Category_Of_Decimals,
    Category_Of_Strings,
  };

struct Category
{
  CategoryType type;
  CategoryData as;

  Category(CategoryType type)
    : type(type)
  {
    switch (type)
      {
      case Category_Of_Integers:
        new (&as.integers) CategoryOfIntegers{ };
        break;
      case Category_Of_Decimals:
        new (&as.decimals) CategoryOfDecimals{ };
        break;
      case Category_Of_Strings:
        new (&as.strings) CategoryOfStrings{ };
        break;
      }
  }

  Category(Category &&other)
  {
    type = other.type;
    switch (other.type)
      {
      case Category_Of_Integers:
        new (&as.integers) CategoryOfIntegers{ std::move(other.as.integers) };
        break;
      case Category_Of_Decimals:
        new (&as.decimals) CategoryOfDecimals{ std::move(other.as.decimals) };
        break;
      case Category_Of_Strings:
        new (&as.strings) CategoryOfStrings{ std::move(other.as.strings) };
        break;
      }
  }

  ~Category()
  {
    switch (type)
      {
      case Category_Of_Integers:
        as.integers.~CategoryOfIntegers();
        break;
      case Category_Of_Decimals:
        as.decimals.~CategoryOfDecimals();
        break;
      case Category_Of_Strings:
        as.strings.~CategoryOfStrings();
        break;
      }
  }
};

struct CategorizedTable
{
  std::vector<Category> data;
  std::vector<std::string> labels;
  size_t cols = 0, rows = 0;

  void print()
  {
    std::cout << "\nRows: " << rows
              << "\nColumn: " << cols << '\n';

    size_t j = 0;
    for (auto &category: data)
      {
        std::cout << '\'' << labels[j++] << '\'' << ":\n";

        switch (category.type)
          {
          case Category_Of_Integers:
            {
              for (auto &[value, id]: category.as.integers.to)
                std::cout << "    " << value << " --> " << id << '\n';

              size_t i = 0;
              for (auto value: category.as.integers.from)
                std::cout << "    " << i++ << " --> " << value << '\n';
            }

            break;
          case Category_Of_Decimals:
            {
              auto interval = category.as.decimals.interval;
              std::cout << "    " << '[' << interval.min << ", " << interval.min + interval.count * interval.step << ']' << '\n';
            }

            break;
          case Category_Of_Strings:
            {
              for (auto &[value, id]: category.as.strings.to)
                std::cout << "    " << value << " --> " << id << '\n';

              size_t i = 0;
              for (auto value: category.as.strings.from)
                std::cout << "    " << i++ << " --> " << value << '\n';
            }

            break;
          }
      }
  }
};

#endif // DECISION_TREE_HPP
