#ifndef FLUX_LEXER_H
#define FLUX_LEXER_H

#include "common.h"
#include "flux_error.h"

typedef enum {
  TOK_EOF = 0,
  TOK_IDENT,
  TOK_NUMBER,
  TOK_STRING,

  // keywords
  TOK_LET,
  TOK_PRINT,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_TRUE,
  TOK_FALSE,
  TOK_NIL,
  TOK_AND,
  TOK_OR,

  // ui keywords
  TOK_UI,
  TOK_INIT,
  TOK_TEXT,
  TOK_BUTTON,
  TOK_INPUT,
  TOK_RUN,
  TOK_CLEAR,
  TOK_TITLE,
  TOK_SELECT,
  TOK_CHECKBOX,

  // punctuation / operators
  TOK_LPAREN, TOK_RPAREN,
  TOK_LBRACE, TOK_RBRACE,
  TOK_SEMI,
  TOK_COMMA,
  TOK_DOT,

  TOK_EQUAL,
  TOK_EQUAL_EQUAL,
  TOK_BANG,
  TOK_BANG_EQUAL,
  TOK_LESS,
  TOK_LESS_EQUAL,
  TOK_GREATER,
  TOK_GREATER_EQUAL,

  TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH
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
