#ifndef UTILS_H
#define UTILS_H

void
make_string_replacements(char *out, unsigned out_size, char const *in,
    char **patterns, char **replacements, int num_patterns);
bool is_whitespace(char const *buf);
void *umalloc(size_t size, int unused);
int strstart(const char *str, const char *val, const char **ptr);
long strtol(char const *nptr, char **endptr, int base);
long long strtoll(char const *nptr, char **endptr, int base);
int atoi (const char *s);
size_t strlcpy (char *dst, const char *src, size_t size);
size_t strlcat (char *dst, const char *src, size_t size);

#define in_range(val, begin, end, type) \
  ((type)(val) >= (type)(begin) && (type)(val) < (type)(end))


#endif /* utils.h */
