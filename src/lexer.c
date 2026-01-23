#include "lexer.h"

static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static char peek(Lexer* l) {
  if (l->i >= l->len) return '\0';
  return l->src[l->i];
}
static char peek2(Lexer* l) {
  if (l->i + 1 >= l->len) return '\0';
  return l->src[l->i + 1];
}
static char advance(Lexer* l) {
  char c = peek(l);
  if (c == '\0') return c;
  l->i++;
  if (c == '\n') { l->line++; l->col = 1; }
  else { l->col++; }
  return c;
}

static void skip_ws_and_comments(Lexer* l) {
  for (;;) {
    char c = peek(l);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance(l);
      continue;
    }
    if (c == '/' && peek2(l) == '/') {
      while (peek(l) != '\0' && peek(l) != '\n') advance(l);
      continue;
    }
    break;
  }
}

static Token make_token(Lexer* l, TokenType type, const char* start, int length, int line, int col) {
  Token t;
  memset(&t, 0, sizeof(t));
  t.type = type;
  t.start = start;
  t.length = length;
  t.line = line;
  t.col = col;
  return t;
}

static Token lex_string(Lexer* l, int start_line, int start_col, const char* start_ptr) {
  size_t cap = 32, n = 0;
  char* buf = (char*)xmalloc(cap);

  while (true) {
    char c = peek(l);
    if (c == '\0') {
      err_at(l->err, start_line, start_col, "unterminated string literal");
      free(buf);
      return make_token(l, TOK_EOF, start_ptr, 0, start_line, start_col);
    }
    if (c == '"') {
      advance(l);
      break;
    }
    if (c == '\\') {
      advance(l);
      char e = peek(l);
      if (e == '\0') break;
      advance(l);
      char out = e;
      switch (e) {
        case 'n': out = '\n'; break;
        case 't': out = '\t'; break;
        case 'r': out = '\r'; break;
        case '"': out = '"'; break;
        case '\\': out = '\\'; break;
        default: out = e; break;
      }
      if (n + 1 >= cap) { cap *= 2; buf = (char*)xrealloc(buf, cap); }
      buf[n++] = out;
      continue;
    }
    advance(l);
    if (n + 1 >= cap) { cap *= 2; buf = (char*)xrealloc(buf, cap); }
    buf[n++] = c;
  }

  buf[n] = '\0';
  Token t = make_token(l, TOK_STRING, start_ptr, (int)(l->src + l->i - start_ptr), start_line, start_col);
  t.string = buf;
  return t;
}

static Token lex_number(Lexer* l, int start_line, int start_col, const char* start_ptr) {
  while (is_digit(peek(l))) advance(l);
  if (peek(l) == '.' && is_digit(peek2(l))) {
    advance(l);
    while (is_digit(peek(l))) advance(l);
  }
  int length = (int)(l->src + l->i - start_ptr);
  Token t = make_token(l, TOK_NUMBER, start_ptr, length, start_line, start_col);
  char* tmp = strndup2(start_ptr, (size_t)length);
  t.number = strtod(tmp, NULL);
  free(tmp);
  return t;
}

static TokenType keyword_type(const char* s, int n) {
  // core
  if (n == 3 && memcmp(s, "let", 3) == 0) return TOK_LET;
  if (n == 5 && memcmp(s, "print", 5) == 0) return TOK_PRINT;
  if (n == 2 && memcmp(s, "if", 2) == 0) return TOK_IF;
  if (n == 4 && memcmp(s, "else", 4) == 0) return TOK_ELSE;
  if (n == 5 && memcmp(s, "while", 5) == 0) return TOK_WHILE;
  if (n == 4 && memcmp(s, "true", 4) == 0) return TOK_TRUE;
  if (n == 5 && memcmp(s, "false", 5) == 0) return TOK_FALSE;
  if (n == 3 && memcmp(s, "nil", 3) == 0) return TOK_NIL;
  if (n == 3 && memcmp(s, "and", 3) == 0) return TOK_AND;
  if (n == 2 && memcmp(s, "or", 2) == 0) return TOK_OR;

  // ui
  if (n == 2 && memcmp(s, "ui", 2) == 0) return TOK_UI;
  if (n == 4 && memcmp(s, "init", 4) == 0) return TOK_INIT;
  if (n == 4 && memcmp(s, "text", 4) == 0) return TOK_TEXT;
  if (n == 6 && memcmp(s, "button", 6) == 0) return TOK_BUTTON;
  if (n == 5 && memcmp(s, "input", 5) == 0) return TOK_INPUT;
  if (n == 3 && memcmp(s, "run", 3) == 0) return TOK_RUN;
  if (n == 5 && memcmp(s, "clear", 5) == 0) return TOK_CLEAR;
  if (n == 5 && memcmp(s, "title", 5) == 0) return TOK_TITLE;
  if (n == 6 && memcmp(s, "select", 6) == 0) return TOK_SELECT;
  if (n == 8 && memcmp(s, "checkbox", 8) == 0) return TOK_CHECKBOX;

  return TOK_IDENT;
}

static Token lex_ident_or_kw(Lexer* l, int start_line, int start_col, const char* start_ptr) {
  while (is_alpha(peek(l)) || is_digit(peek(l))) advance(l);
  int length = (int)(l->src + l->i - start_ptr);
  TokenType tt = keyword_type(start_ptr, length);
  return make_token(l, tt, start_ptr, length, start_line, start_col);
}

void lexer_init(Lexer* l, const char* filename, const char* src, ErrorCtx* err) {
  l->filename = filename;
  l->src = src;
  l->len = strlen(src);
  l->i = 0;
  l->line = 1;
  l->col = 1;
  l->err = err;
}

Token lexer_next(Lexer* l) {
  skip_ws_and_comments(l);

  int start_line = l->line;
  int start_col  = l->col;
  const char* start_ptr = l->src + l->i;

  char c = advance(l);
  if (c == '\0') return make_token(l, TOK_EOF, start_ptr, 0, start_line, start_col);

  if (is_alpha(c)) return lex_ident_or_kw(l, start_line, start_col, start_ptr);
  if (is_digit(c)) return lex_number(l, start_line, start_col, start_ptr);

  switch (c) {
    case '(': return make_token(l, TOK_LPAREN, start_ptr, 1, start_line, start_col);
    case ')': return make_token(l, TOK_RPAREN, start_ptr, 1, start_line, start_col);
    case '{': return make_token(l, TOK_LBRACE, start_ptr, 1, start_line, start_col);
    case '}': return make_token(l, TOK_RBRACE, start_ptr, 1, start_line, start_col);
    case ';': return make_token(l, TOK_SEMI,   start_ptr, 1, start_line, start_col);
    case ',': return make_token(l, TOK_COMMA,  start_ptr, 1, start_line, start_col);
    case '.': return make_token(l, TOK_DOT,    start_ptr, 1, start_line, start_col);

    case '+': return make_token(l, TOK_PLUS,   start_ptr, 1, start_line, start_col);
    case '-': return make_token(l, TOK_MINUS,  start_ptr, 1, start_line, start_col);
    case '*': return make_token(l, TOK_STAR,   start_ptr, 1, start_line, start_col);
    case '/': return make_token(l, TOK_SLASH,  start_ptr, 1, start_line, start_col);

    case '=':
      if (peek(l) == '=') { advance(l); return make_token(l, TOK_EQUAL_EQUAL, start_ptr, 2, start_line, start_col); }
      return make_token(l, TOK_EQUAL,  start_ptr, 1, start_line, start_col);

    case '!':
      if (peek(l) == '=') { advance(l); return make_token(l, TOK_BANG_EQUAL, start_ptr, 2, start_line, start_col); }
      return make_token(l, TOK_BANG, start_ptr, 1, start_line, start_col);

    case '<':
      if (peek(l) == '=') { advance(l); return make_token(l, TOK_LESS_EQUAL, start_ptr, 2, start_line, start_col); }
      return make_token(l, TOK_LESS, start_ptr, 1, start_line, start_col);

    case '>':
      if (peek(l) == '=') { advance(l); return make_token(l, TOK_GREATER_EQUAL, start_ptr, 2, start_line, start_col); }
      return make_token(l, TOK_GREATER, start_ptr, 1, start_line, start_col);

    case '"': return lex_string(l, start_line, start_col, start_ptr);

    default:
      err_at(l->err, start_line, start_col, "unexpected character '%c'", c);
      return make_token(l, TOK_EOF, start_ptr, 0, start_line, start_col);
  }
}

void token_free(Token* t) {
  if (t->type == TOK_STRING && t->string) {
    free(t->string);
    t->string = NULL;
  }
}

const char* token_type_name(TokenType t) {
  switch (t) {
    case TOK_EOF: return "EOF";
    case TOK_IDENT: return "IDENT";
    case TOK_NUMBER: return "NUMBER";
    case TOK_STRING: return "STRING";
    case TOK_LET: return "LET";
    case TOK_PRINT: return "PRINT";
    case TOK_IF: return "IF";
    case TOK_ELSE: return "ELSE";
    case TOK_WHILE: return "WHILE";
    case TOK_TRUE: return "TRUE";
    case TOK_FALSE: return "FALSE";
    case TOK_NIL: return "NIL";
    case TOK_AND: return "AND";
    case TOK_OR: return "OR";

    case TOK_UI: return "UI";
    case TOK_INIT: return "INIT";
    case TOK_TEXT: return "TEXT";
    case TOK_BUTTON: return "BUTTON";
    case TOK_INPUT: return "INPUT";
    case TOK_RUN: return "RUN";
    case TOK_CLEAR: return "CLEAR";
    case TOK_TITLE: return "TITLE";
    case TOK_SELECT: return "SELECT";
    case TOK_CHECKBOX: return "CHECKBOX";

    case TOK_LPAREN: return "(";
    case TOK_RPAREN: return ")";
    case TOK_LBRACE: return "{";
    case TOK_RBRACE: return "}";
    case TOK_SEMI: return ";";
    case TOK_COMMA: return ",";
    case TOK_DOT: return ".";

    case TOK_EQUAL: return "=";
    case TOK_EQUAL_EQUAL: return "==";
    case TOK_BANG: return "!";
    case TOK_BANG_EQUAL: return "!=";
    case TOK_LESS: return "<";
    case TOK_LESS_EQUAL: return "<=";
    case TOK_GREATER: return ">";
    case TOK_GREATER_EQUAL: return ">=";

    case TOK_PLUS: return "+";
    case TOK_MINUS: return "-";
    case TOK_STAR: return "*";
    case TOK_SLASH: return "/";

    default: return "?";
  }
}
