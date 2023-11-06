#define PRINT_ERROR(filepath, line_info, message, ...) fprintf(stderr, "%s:%zu:%zu: error: " message "\n", filepath, (line_info).line, (line_info).column, __VA_ARGS__)
#define PRINT_ERROR0(filepath, line_info, message) fprintf(stderr, "%s:%zu:%zu: error: " message "\n", filepath, (line_info).line, (line_info).column)

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
  std::string_view source;

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
