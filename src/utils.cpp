#define UNREACHABLE() assert(false && "unreachable")

template<typename T>
struct Flattened2DArray
{
  std::vector<T> data;
  size_t rows, cols;

  void resize(size_t rows, size_t cols)
  {
    this->rows = rows;
    this->cols = cols;
    data.resize(rows * cols);
  }

  T &grab(size_t row, size_t col)
  {
    assert(row < rows && col < cols);
    return data[row * cols + col];
  }
};

std::string
read_entire_file(const char *filepath)
{
  auto file = std::ifstream{ filepath };
  auto result = std::string{ };

  if (!file.is_open())
    {
      fprintf(stderr, "error: couldn't open '%s'.", filepath);
      exit(EXIT_FAILURE);
    }

  {
    struct stat stats;
    if (stat(filepath, &stats) == -1)
      goto report_error;
    result.resize(stats.st_size + 1);
  }

  file.read(&result[0], result.size() - 1);
  result.back() = '\0';
  file.close();

  return result;

 report_error:
  std::cerr << strerror(errno);
  exit(EXIT_FAILURE);
}
