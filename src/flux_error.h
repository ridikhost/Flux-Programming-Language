#ifndef FLUX_ERROR_H
#define FLUX_ERROR_H

#include "common.h"

typedef struct ErrorCtx {
    const char* filename;
    const char* source;
    int had_error;
} ErrorCtx;

void err_init(ErrorCtx* e, const char* filename, const char* source);
void err_at(ErrorCtx* e, int line, int col, const char* fmt, ...);

#endif
