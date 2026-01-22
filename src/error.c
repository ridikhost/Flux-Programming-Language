#include "flux_error.h"

void err_init(ErrorCtx* e, const char* filename, const char* source) {
  e->filename = filename;
  e->source = source;
  e->had_error = 0;
}

void err_at(ErrorCtx* e, int line, int col, const char* fmt, ...) {
  e->had_error = 1;
  fprintf(stderr, "%s:%d:%d: error: ", e->filename ? e->filename : "<input>", line, col);

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}
