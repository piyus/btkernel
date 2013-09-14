#include <types.h>
#include <lib/utils.h>
#include <lib/ctype.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <debug.h>
#include "hypercall.h"

int
strstart(const char *str, const char *val, const char **ptr)
{
  const char *p, *q;
  p = str;
  q = val;
  while (*q != '\0') {
    if (*p != *q)
      return 0;
    p++;
    q++;
  }
  if (ptr)
    *ptr = p;
  return 1;
}

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 *
 * taken from git://android.git.kernel.org/platform/bionic.git/
 */
long
strtol(char const *nptr, char **endptr, int base)
{
  const char *s;
  long acc, cutoff;
  int c;
  int neg, any, cutlim;

  /*
   * Skip white space and pick up leading +/- sign if any.
   * If base is 0, allow 0x for hex and 0 for octal, else
   * assume decimal; if base is already 16, allow 0x.
   */
  s = nptr;
  do {
    c = (unsigned char) *s++;
  } while (isspace(c));
  if (c == '-') {
    neg = 1;
    c = *s++;
  } else {
    neg = 0;
    if (c == '+')
      c = *s++;
  }
  if ((base == 0 || base == 16) &&
      c == '0' && (*s == 'x' || *s == 'X')) {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == '0' ? 8 : 10;

  /*
   * Compute the cutoff value between legal numbers and illegal
   * numbers.  That is the largest legal value, divided by the
   * base.  An input number that is greater than this value, if
   * followed by a legal input character, is too big.  One that
   * is equal to this value may be valid or not; the limit
   * between valid and invalid numbers is then based on the last
   * digit.  For instance, if the range for longs is
   * [-2147483648..2147483647] and the input base is 10,
   * cutoff will be set to 214748364 and cutlim to either
   * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
   * a value > 214748364, or equal but the next digit is > 7 (or 8),
   * the number is too big, and we will return a range error.
   *
   * Set any if any `digits' consumed; make it negative to indicate
   * overflow.
   */
  cutoff = neg ? LONG_MIN : LONG_MAX;
  cutlim = cutoff % base;
  cutoff /= base;
  if (neg) {
    if (cutlim > 0) {
      cutlim -= base;
      cutoff += 1;
    }
    cutlim = -cutlim;
  }
  for (acc = 0, any = 0;; c = (unsigned char) *s++) {
    if (isdigit(c))
      c -= '0';
    else if (isalpha(c))
      c -= isupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0)
      continue;
    if (neg) {
      if (acc < cutoff || (acc == cutoff && c > cutlim)) {
        any = -1;
        acc = LONG_MIN;
        //errno = ERANGE;
      } else {
        any = 1;
        acc *= base;
        acc -= c;
      }
    } else {
      if (acc > cutoff || (acc == cutoff && c > cutlim)) {
        any = -1;
        acc = LONG_MAX;
        //errno = ERANGE;
      } else {
        any = 1;
        acc *= base;
        acc += c;
      }
    }
  }
  if (endptr != 0)
    *endptr = (char *) (any ? s - 1 : nptr);
  return (acc);
}


long long
strtoll(char const *nptr, char **endptr, int base)
{
  /* XXX: fix this. */
  return strtol(nptr, endptr, base);
}

/* Converts a string representation of a signed decimal integer
   in S into an `int', which is returned. */
int
atoi (const char *s) 
{
  bool negative;
  int value;

  ASSERT (s != NULL);

  /* Skip white space. */
  while (isspace ((unsigned char) *s))
    s++;

  /* Parse sign. */
  negative = false;
  if (*s == '+')
    s++;
  else if (*s == '-')
    {
      negative = true;
      s++;
    }

  /* Parse digits.  We always initially parse the value as
     negative, and then make it positive later, because the
     negative range of an int is bigger than the positive range
     on a 2's complement system. */
  for (value = 0; isdigit (*s); s++)
    value = value * 10 - (*s - '0');
  if (!negative)
    value = -value;

  return value;
}

#ifdef __PEEPGEN__
void *
umalloc(size_t size, int unused)
{
  return malloc(size);
}
#endif

void
make_string_replacements(char *out, unsigned out_size, char const *in,
    char **patterns, char **replacements, int num_patterns)
{
  char *ptr;
  int i;

  ASSERT(strlen(in) < out_size);

  //printf("in = %s", in);
  strlcpy(out, in, out_size);
  for (i = 0; i < num_patterns; i++) {
    int pattern_len, replacement_len;
    pattern_len = strlen(patterns[i]);
    replacement_len = strlen(replacements[i]);

    ASSERT(pattern_len > 0);
    ASSERT(replacement_len >= 0);

    //printf("%s <-- %s\n", patterns[i], replacements[i]);

    ptr = out;
    while ((ptr = strstr(ptr, patterns[i]))) {
      int j;

      memmove(ptr + replacement_len, ptr + pattern_len,
          strlen(ptr + pattern_len)+1);

      for (j = 0; j < replacement_len; j++) {
        *ptr++ = *(replacements[i] + j);
      }
    }
  }
  //printf("out = %s\n", out);
}

bool
is_whitespace(char const *buf)
{
  char const *ptr = buf;
  while (*ptr != '\0') {
    if (*ptr != ' ' && *ptr != '\t' && *ptr != 0x1) {
      return false;
    }
    ptr++;
  }
  return true;
}

/* Copies string SRC to DST.  If SRC is longer than SIZE - 1
   characters, only SIZE - 1 characters are copied.  A null
   terminator is always written to DST, unless SIZE is 0.
   Returns the length of SRC, not including the null terminator.

   strlcpy() is not in the standard C library, but it is an
   increasingly popular extension.  See
   http://www.courtesan.com/todd/papers/strlcpy.html for
   information on strlcpy(). */
size_t
strlcpy (char *dst, const char *src, size_t size) 
{
  size_t src_len;

  ASSERT (dst != NULL);
  ASSERT (src != NULL);

  src_len = strlen (src);
  if (size > 0) 
    {
      size_t dst_len = size - 1;
      if (src_len < dst_len)
        dst_len = src_len;
      memcpy (dst, src, dst_len);
      dst[dst_len] = '\0';
    }
  return src_len;
}

/* Concatenates string SRC to DST.  The concatenated string is
   limited to SIZE - 1 characters.  A null terminator is always
   written to DST, unless SIZE is 0.  Returns the length that the
   concatenated string would have assuming that there was
   sufficient space, not including a null terminator.

   strlcat() is not in the standard C library, but it is an
   increasingly popular extension.  See
   http://www.courtesan.com/todd/papers/strlcpy.html for
   information on strlcpy(). */
size_t
strlcat (char *dst, const char *src, size_t size) 
{
  size_t src_len, dst_len;

  ASSERT (dst != NULL);
  ASSERT (src != NULL);

  src_len = strlen (src);
  dst_len = strlen (dst);
  if (size > 0 && dst_len < size) 
    {
      size_t copy_cnt = size - dst_len - 1;
      if (src_len < copy_cnt)
        copy_cnt = src_len;
      memcpy (dst + dst_len, src, copy_cnt);
      dst[dst_len + copy_cnt] = '\0';
    }
  return src_len + dst_len;
}



