#ifndef FLUX_COMMON_H
#define FLUX_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

static inline void* xmalloc(size_t n) {
  void* p = malloc(n);
  if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
  return p;
}

static inline void* xrealloc(void* p, size_t n) {
  void* q = realloc(p, n);
  if (!q) { fprintf(stderr, "out of memory\n"); exit(1); }
  return q;
}

static inline char* xstrdup(const char* s) {
  size_t n = strlen(s);
  char* out = (char*)xmalloc(n + 1);
  memcpy(out, s, n + 1);
  return out;
}

static inline char* strndup2(const char* s, size_t n) {
  char* out = (char*)xmalloc(n + 1);
  memcpy(out, s, n);
  out[n] = '\0';
  return out;
}

#endif // FLUX_COMMON_H
