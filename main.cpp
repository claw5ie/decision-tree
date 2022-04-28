#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include <cassert>
#include <sys/stat.h>

struct Tokenizer
{
  struct Token
  {
    enum Type
    {
      INTEGER,
      DOUBLE,
      IDENTIFIER,

      COMMA,
      NEWLINE,

      END_OF_FILE
    };

    Type type;
    const char *text;
    uint32_t size;

    void assert_type(Type type) const
    {
      if (this->type != type)
      {
        std::cerr << "ERROR: expected type \"" << type << "\", but got \"" <<
          this->type << "\".\n";
        exit(EXIT_FAILURE);
      }
    }

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

    const char *begin = at;

    if (*at == '\0')
    {
      return { Token::END_OF_FILE, begin, 0 };
    }
    else if (*at == ',')
    {
      at++;
      return { Token::COMMA, begin, 1 };
    }
    else if (*at == '\n')
    {
      at++;
      return { Token::NEWLINE, begin, 1 };
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

struct Table
{
  struct Category
  {
    std::string name;
    std::map<std::string, uint32_t> values;

    uint32_t insert(const char *string, uint32_t size)
    {
      return values.insert(
        std::pair<std::string, uint32_t>(
          std::string(string, size), values.size()
          )
        ).first->second;
    }
  };

  enum AttributeType
  {
    ATTRIBUTE,
    INT32,
    DOUBLE
  };

  struct AttributeData
  {
    union
    {
      uint32_t attribute;

      int32_t int32;

      double decimal;
    } as;
  };

  Category *categories;
  AttributeType *types;
  AttributeData *data;
  size_t rows,
    cols;
  char *raw_data;

  void allocate(size_t rows, size_t cols)
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

  inline AttributeData &get(size_t row, size_t col)
  {
    return data[row * cols + col];
  }

  inline const AttributeData &get(size_t row, size_t col) const
  {
    return data[row * cols + col];
  }

  void deallocate()
  {
    delete[] raw_data;
  }
};

void parse_table(Table &table, const char *data)
{
  assert(table.cols >= 2 && table.rows > 1);

  Tokenizer tokenizer = { data };

  for (size_t i = 0; i < table.cols; i++)
  {
    Tokenizer::Token token = tokenizer.next_token();

    token.assert_type(Tokenizer::Token::IDENTIFIER);

    table.categories[i].insert(token.text, token.size);
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
    {
      new (&table.categories[i]) Table::Category;
      uint32_t value = table.categories[i].insert(token.text, token.size);
      table.types[i] = Table::AttributeType::ATTRIBUTE;
      table.get(0, i).as.attribute = value;
      break;
    }
    default:
      assert(false);
    }

    token = tokenizer.next_token();

    assert(
      token.type == Tokenizer::Token::COMMA ||
      token.type == Tokenizer::Token::NEWLINE ||
      token.type == Tokenizer::Token::END_OF_FILE
      );
  }

  for (size_t i = 2; i < table.rows; i++)
  {
    for (size_t j = 0; j < table.cols; j++)
    {
      Tokenizer::Token token = tokenizer.next_token();

      switch (token.type)
      {
      case Tokenizer::Token::INTEGER:
        assert(table.types[j] == Table::AttributeType::INT32);
        table.get(0, i).as.int32 = token.to_integer();
        break;
      case Tokenizer::Token::DOUBLE:
        assert(table.types[j] == Table::AttributeType::DOUBLE);
        table.get(0, i).as.decimal = token.to_double();
        break;
      case Tokenizer::Token::IDENTIFIER:
      {
        assert(table.types[j] == Table::AttributeType::ATTRIBUTE);
        uint32_t value = table.categories[i].insert(token.text, token.size);
        table.get(0, i).as.attribute = value;
        break;
      }
      default:
        assert(false);
      }

      token = tokenizer.next_token();

      assert(
        token.type == Tokenizer::Token::COMMA ||
        token.type == Tokenizer::Token::NEWLINE ||
        token.type == Tokenizer::Token::END_OF_FILE
        );
    }
  }
}

Table read_csv(const char *const filepath)
{
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
    bool column_set = false;

    for (size_t i = 0; i < file_size; i++)
    {
      if (!column_set && file_data[i] == '\n')
        column_set = true;
      else
        cols += file_data[i] == ',';

      rows += file_data[i] == '\n';
    }
  }

  table.allocate(rows - 1, cols);

  parse_table(table, file_data);

  delete[] file_data;

  return Table{ };
}

int main(int, char **)
{

}
