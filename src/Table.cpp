#include <iostream>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include "Table.hpp"

struct Tokenizer
{
  struct Token
  {
    enum Type
    {
      INTEGER,
      DOUBLE,
      IDENTIFIER,
      END_OF_FILE
    };

    Type type;
    const char *text;
    uint32_t size;

    int32_t to_integer() const
    {
      assert(type == INTEGER);

      int32_t value = 0;

      for (size_t i = *text == '-' || *text == '+'; i < size; i++)
        value = value * 10 + (text[i] - '0');

      return *text == '-' ? -value : value;
    }

    double to_double() const
    {
      assert(type == DOUBLE);

      double value = 0;

      size_t i = *text == '-' || *text == '+';
      for (; text[i] != '.' && i < size; i++)
        value = value * 10 + (text[i] - '0');

      ++i;

      for (double pow = 0.1; i < size; i++, pow /= 10)
        value += (text[i] - '0') * pow;

      return *text == '-' ? -value : value;
    }
  };

  const char *at;

  Token next_token()
  {
    while (std::isspace(*at))
      at++;

    at += (*at == ',' || *at == '\n');

    const char *begin = at;

    if (*at == '\0')
    {
      return { Token::END_OF_FILE, begin, 0 };
    }
    else if (std::isdigit(*at) ||
             ((*at == '-' || *at == '+') && std::isdigit(at[1])))
    {
      at += *at == '-' || *at == '+';

      while (std::isdigit(*++at))
        ;

      if (at[0] == '.' && std::isdigit(at[1]))
      {
        while (std::isdigit(*(++at)))
          ;

        return { Token::DOUBLE, begin, uint32_t(at - begin) };
      }
      else
      {
        return { Token::INTEGER, begin, uint32_t(at - begin) };
      }
    }
    else if (std::isalpha(*at))
    {
      while (std::isprint(*at) && *at != ',' && *at != '\n')
        at++;

      return { Token::IDENTIFIER, begin, uint32_t(at - begin) };
    }
    else
    {
      at += *at != '\0';
      exit(EXIT_FAILURE);
    }
  }
};

uint32_t Table::Category::insert(const char *string, uint32_t size)
{
#ifndef NDEBUG
  auto it = values.insert(
    std::pair<std::string, uint32_t>(
      std::string(string, size), values.size()
      )
    );
  names.insert(
    std::pair<uint32_t, std::string>(
      it.first->second, std::string(string, size)
      )
    );

  return it.first->second;
#else
  return values.insert(
    std::pair<std::string, uint32_t>(
      std::string(string, size), values.size()
      )
    ).first->second;
#endif
}

void Table::allocate(size_t rows, size_t cols)
{
  this->raw_data = new char[
    cols * sizeof(Category) +
    cols * sizeof(AttributeType) +
    (rows * cols) * sizeof(AttributeData)
    ];
  this->categories = (Category *)raw_data;
  this->types = (AttributeType *)(categories + cols);
  this->data = (AttributeData *)(types + cols);
  this->rows = rows;
  this->cols = cols;
}

inline Table::AttributeData &Table::get(size_t row, size_t col)
{
  return data[row * cols + col];
}

inline const Table::AttributeData &Table::get(size_t row, size_t col) const
{
  return data[row * cols + col];
}

void Table::print() const
{
  for (size_t i = 0; i < cols; i++)
    std::cout << categories[i].name << (i + 1 < cols ? ' ' : '\n');

  for (size_t i = 0; i < rows; i++)
  {
    for (size_t j = 0; j < cols; j++)
    {
      switch (types[j])
      {
      case AttributeType::INT32:
        std::cout << get(i, j).as.int32;
        break;
      case AttributeType::DOUBLE:
        std::cout << get(i, j).as.decimal;
        break;
      case AttributeType::ATTRIBUTE:
#ifndef NDEBUG
      {
        auto it = categories[j].names.find(get(i, j).as.attribute);
        std::cout << it->second;
        break;
      }
#else
      std::cout << get(i, j).as.attribute;
      break;
#endif
      default:
        assert(false);
      }

      std::cout << (j + 1 < cols ? ' ' : '\n');
    }
  }
}

void Table::deallocate()
{
  for (size_t i = 0; i < cols; i++)
    categories[i].~Category();

  delete[] raw_data;
}

Table read_csv(const char *const filepath)
{
  const auto parse_table =
    [](Table &table, const char *data) -> void
    {
      assert(table.cols >= 2 && table.rows > 0);

      Tokenizer tokenizer = { data };

      for (size_t i = 0; i < table.cols; i++)
      {
        Tokenizer::Token token = tokenizer.next_token();

        assert(token.type != Tokenizer::Token::END_OF_FILE);

        new (&table.categories[i]) Table::Category;
        table.categories[i].name = std::string(token.text, token.size);
      }

      for (size_t i = 0; i < table.cols; i++)
      {
        Tokenizer::Token token = tokenizer.next_token();

        switch (token.type)
        {
        case Tokenizer::Token::INTEGER:
          table.types[i] = Table::AttributeType::INT32;
          table.get(0, i).as.int32 = token.to_integer();
          break;
        case Tokenizer::Token::DOUBLE:
          table.types[i] = Table::AttributeType::DOUBLE;
          table.get(0, i).as.decimal = token.to_double();
          break;
        case Tokenizer::Token::IDENTIFIER:
          table.types[i] = Table::AttributeType::ATTRIBUTE;
          table.get(0, i).as.attribute =
            table.categories[i].insert(token.text, token.size);;
          break;
        default:
          assert(false);
        }
      }

      for (size_t i = 1; i < table.rows; i++)
      {
        for (size_t j = 0; j < table.cols; j++)
        {
          Tokenizer::Token token = tokenizer.next_token();

          switch (token.type)
          {
          case Tokenizer::Token::INTEGER:
            assert(table.types[j] == Table::AttributeType::INT32);
            table.get(i, j).as.int32 = token.to_integer();
            break;
          case Tokenizer::Token::DOUBLE:
            assert(table.types[j] == Table::AttributeType::DOUBLE);
            table.get(i, j).as.decimal = token.to_double();
            break;
          case Tokenizer::Token::IDENTIFIER:
          {
            assert(table.types[j] == Table::AttributeType::ATTRIBUTE);
            uint32_t value = table.categories[j].insert(token.text, token.size);
            table.get(i, j).as.attribute = value;
            break;
          }
          default:
            assert(false);
          }
        }
      }
    };

  const size_t file_size =
    [filepath]() -> size_t
    {
      struct stat stats;

      if (stat(filepath, &stats) == -1)
      {
        std::cerr << "error: couldn't stat the file \"" << filepath << "\".\n";
        exit(EXIT_FAILURE);
      }

      return stats.st_size;
    }();

  std::fstream file(filepath, std::fstream::in);

  if (!file.is_open())
  {
    std::cerr << "error: failed to open the file \"" << filepath << "\".\n";
    exit(EXIT_FAILURE);
  }

  char *const file_data = new char[(file_size + 1) * sizeof(char)];
  file.read(file_data, file_size);
  file_data[file_size] = '\0';
  file.close();

  Table table;

  size_t rows = 0,
    cols = 1;

  {
    size_t i = 0;

    for (; file_data[i] != '\n' && i < file_size; i++)
      cols += file_data[i] == ',';

    for (; i < file_size; i++)
      rows += file_data[i] == '\n';

    rows += i > 0 && file_data[i - 1] != '\n' && file_data[i] == '\0';
  }

  table.allocate(rows - 1, cols);
  parse_table(table, file_data);

  delete[] file_data;

  return table;
}
