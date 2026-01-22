#ifndef FLUX_PARSER_H
#define FLUX_PARSER_H

#include "common.h"
#include "lexer.h"
#include "ast.h"
#include "flux_error.h"

typedef struct {
  Lexer lex;
  ErrorCtx* err;

  Token current;
  Token previous;

  int panic;
} Parser;

void parser_init(Parser* p, const char* filename, const char* src, ErrorCtx* err);
Program parse_program(Parser* p);

#endif // FLUX_PARSER_H
