#ifndef FLUX_LEXER_H
#define FLUX_LEXER_H

#include "common.h"
#include "flux_error.h"

typedef enum {
  TOK_EOF = 0,
  TOK_IDENT,
  TOK_NUMBER,
  TOK_STRING,

  TOK_LET,
  TOK_PRINT,

  TOK_UI,
  TOK_INIT,
  TOK_TEXT,
  TOK_BUTTON,
  TOK_INPUT,
  TOK_RUN,

  TOK_LPAREN, TOK_RPAREN,
  TOK_SEMI,
  TOK_EQUAL,

  TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
  TOK_COMMA
} TokenType;

typedef struct {
  TokenType type;
  const char* start;  // points into source
  int length;
  int line;
  int col;
  double number;      // if TOK_NUMBER
  char* string;       // if TOK_STRING (owned)
} Token;

typedef struct {
  const char* filename;
  const char* src;
  size_t len;
  size_t i;
  int line;
  int col;
  ErrorCtx* err;
} Lexer;

void lexer_init(Lexer* l, const char* filename, const char* src, ErrorCtx* err);
Token lexer_next(Lexer* l);
void token_free(Token* t);
const char* token_type_name(TokenType t);

#endif // FLUX_LEXER_H
