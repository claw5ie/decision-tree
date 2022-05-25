#ifndef UTILS_HPP
#define UTILS_HPP

char *allocate_in_chunks(size_t *offsets, size_t count, ...);

void putsn(const char *string, size_t count);

int64_t read_int64(const char *&str);

double read_float64(const char *&str);

void require_char(char actual, char expected);

char *read_entire_file(const char *filepath);

#endif // UTILS_HPP
