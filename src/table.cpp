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
    std::cout << "Rows:    " << rows
              << "\nColumns: " << cols
              << '\n';

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

Table
parse_csv_from_string(const char *filepath, std::string &source)
{
  auto table = Table{ };
  auto t = Tokenizer{ };
  t.filepath = filepath;
  t.source = source;

  size_t cells_in_row = 0;

  if (t.peek() == Token_New_Line)
    t.advance();

  while (t.peek() != Token_End_Of_File)
    {
      auto token = t.grab();
      t.advance();

      switch (token.type)
        {
        case Token_Integer:
          {
            i64 value = 0;
            for (auto ch: token.text)
              value = 10 * value + (ch - '0');

            auto cell = TableCell{ };
            cell.type = Table_Cell_Integer;
            cell.as.integer = value;
            table.data.push_back(cell);
            ++cells_in_row;

            t.expect_comma_or_new_line();
          }

          break;
        case Token_Decimal:
          {
            i64 integral_part = 0;
            f64 fractional_part = 0;
            f64 pow10 = 0.1;

            size_t i = 0;
            auto text = token.text;
            for (; i < text.size() && text[i] != '.'; i++)
              integral_part = 10 * integral_part + (text[i] - '0');

            for (++i; i < text.size(); i++, pow10 /= 10)
              fractional_part += pow10 * (text[i] - '0');

            auto cell = TableCell{ };
            cell.type = Table_Cell_Decimal;
            cell.as.decimal = integral_part + fractional_part;
            table.data.push_back(cell);
            ++cells_in_row;

            t.expect_comma_or_new_line();
          }

          break;
        case Token_String:
          {
            auto [it, _] = table.string_pool.emplace(token.text);
            auto cell = TableCell{ };
            cell.type = Table_Cell_String;
            cell.as.string = *it;
            table.data.push_back(cell);
            ++cells_in_row;

            t.expect_comma_or_new_line();
          }

          break;
        case Token_Comma:
          {
            PRINT_ERROR0(t.filepath, token.line_info, "unexpected ','.");
            exit(EXIT_FAILURE);
          }

          break;
        case Token_New_Line:
          {
            if (table.cols == 0)
              {
                // Columns count should only be zero on first iteration.
                assert(cells_in_row > 0);
                table.cols = cells_in_row;
              }
            else if (cells_in_row != table.cols)
              {
                PRINT_ERROR(t.filepath, token.line_info, "expected %zu row(s), but got %zu.", table.cols, cells_in_row);
                exit(EXIT_FAILURE);
              }

            ++table.rows;
            cells_in_row = 0;
          }

          break;
        case Token_End_Of_File:
          UNREACHABLE();
          break;
        }
    }

  return table;
}

Table
parse_csv_from_file(const char *filepath)
{
  auto string = read_entire_file(filepath);
  return parse_csv_from_string(filepath, string);
}

Table
parse_csv_from_stdin()
{
  auto string = std::string{ };
  auto buffer = std::string{ };

  while (std::getline(std::cin, buffer))
    {
      string.append(buffer);
      string.push_back('\n');
    }

  return parse_csv_from_string("<stdin>", string);
}
