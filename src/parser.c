#include "parser.h"

static void advance_p(Parser* p) {
  token_free(&p->previous);
  p->previous = p->current;
  p->current = lexer_next(&p->lex);
}

static bool check(Parser* p, TokenType t) { return p->current.type == t; }

static bool match(Parser* p, TokenType t) {
  if (!check(p, t)) return false;
  advance_p(p);
  return true;
}

static void error_here(Parser* p, const char* fmt, ...) {
  p->err->had_error = 1;
  fprintf(stderr, "%s:%d:%d: error: ",
          p->lex.filename ? p->lex.filename : "<input>",
          p->current.line, p->current.col);
  va_list args; va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  p->panic = 1;
}

static void consume(Parser* p, TokenType t, const char* msg) {
  if (p->current.type == t) { advance_p(p); return; }
  error_here(p, "%s (got %s)", msg, token_type_name(p->current.type));
}

static char* token_text_dup(Token* t) {
  return strndup2(t->start, (size_t)t->length);
}

// Forward decl
static Expr* expression(Parser* p);

static Expr* primary(Parser* p) {
  if (match(p, TOK_NUMBER)) {
    double x = p->previous.number;
    return expr_literal(value_number(x), p->previous.line, p->previous.col);
  }
  if (match(p, TOK_STRING)) {
    char* s = p->previous.string;
    p->previous.string = NULL;
    return expr_literal(value_string_take(s), p->previous.line, p->previous.col);
  }
  if (match(p, TOK_IDENT)) {
    char* name = token_text_dup(&p->previous);
    Expr* e = expr_var(name, p->previous.line, p->previous.col);
    free(name);
    return e;
  }
  if (match(p, TOK_LPAREN)) {
    int line = p->previous.line, col = p->previous.col;
    Expr* inner = expression(p);
    consume(p, TOK_RPAREN, "expected ')'");
    return expr_group(inner, line, col);
  }

  error_here(p, "expected expression");
  return expr_literal(value_nil(), p->current.line, p->current.col);
}

static Expr* unary(Parser* p) {
  if (match(p, TOK_MINUS)) {
    int line = p->previous.line, col = p->previous.col;
    Expr* right = unary(p);
    Expr* zero = expr_literal(value_number(0.0), line, col);
    return expr_binary(OP_SUB, zero, right, line, col);
  }
  return primary(p);
}

static Expr* factor(Parser* p) {
  Expr* expr = unary(p);
  for (;;) {
    if (match(p, TOK_STAR)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = unary(p);
      expr = expr_binary(OP_MUL, expr, right, line, col);
      continue;
    }
    if (match(p, TOK_SLASH)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = unary(p);
      expr = expr_binary(OP_DIV, expr, right, line, col);
      continue;
    }
    break;
  }
  return expr;
}

static Expr* term(Parser* p) {
  Expr* expr = factor(p);
  for (;;) {
    if (match(p, TOK_PLUS)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = factor(p);
      expr = expr_binary(OP_ADD, expr, right, line, col);
      continue;
    }
    if (match(p, TOK_MINUS)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = factor(p);
      expr = expr_binary(OP_SUB, expr, right, line, col);
      continue;
    }
    break;
  }
  return expr;
}

static Expr* expression(Parser* p) { return term(p); }

static void synchronize(Parser* p) {
  p->panic = 0;
  while (p->current.type != TOK_EOF) {
    if (p->previous.type == TOK_SEMI) return;
    switch (p->current.type) {
      case TOK_LET:
      case TOK_PRINT:
      case TOK_UI:
        return;
      default:
        break;
    }
    advance_p(p);
  }
}

static char* parse_string_arg(Parser* p, const char* msg) {
  consume(p, TOK_STRING, msg);
  if (p->previous.type != TOK_STRING) return NULL;
  char* s = p->previous.string;
  p->previous.string = NULL;
  return s;
}

static Stmt* ui_statement(Parser* p) {
  // we already consumed TOK_UI
  int line = p->previous.line, col = p->previous.col;

  if (match(p, TOK_INIT)) {
    consume(p, TOK_LPAREN, "expected '(' after ui init");
    char* title = parse_string_arg(p, "expected string title");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui init(...)");
    Stmt* s = stmt_ui(UI_INIT, title ? title : "", NULL, line, col);
    free(title);
    return s;
  }

  if (match(p, TOK_TEXT)) {
    consume(p, TOK_LPAREN, "expected '(' after ui text");
    char* text = parse_string_arg(p, "expected string for ui text");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui text(...)");
    Stmt* s = stmt_ui(UI_TEXT, text ? text : "", NULL, line, col);
    free(text);
    return s;
  }

  if (match(p, TOK_BUTTON)) {
    consume(p, TOK_LPAREN, "expected '(' after ui button");
    char* label = parse_string_arg(p, "expected string label for ui button");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui button(...)");
    Stmt* s = stmt_ui(UI_BUTTON, label ? label : "", NULL, line, col);
    free(label);
    return s;
  }

  if (match(p, TOK_INPUT)) {
    consume(p, TOK_LPAREN, "expected '(' after ui input");
    char* label = parse_string_arg(p, "expected string label for ui input");
    consume(p, TOK_COMMA, "expected ',' after input label");
    char* value = parse_string_arg(p, "expected string default for ui input");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui input(...)");
    Stmt* s = stmt_ui(UI_INPUT, label ? label : "", value ? value : "", line, col);
    free(label); free(value);
    return s;
  }

  if (match(p, TOK_RUN)) {
    consume(p, TOK_LPAREN, "expected '(' after ui run");
    char* backend = NULL;
    if (!check(p, TOK_RPAREN)) {
      backend = parse_string_arg(p, "expected backend string");
    }
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui run(...)");
    Stmt* s = stmt_ui(UI_RUN, backend ? backend : "tui", NULL, line, col);
    free(backend);
    return s;
  }

  error_here(p, "expected ui command: init/text/button/input/run");
  // best-effort recover: skip until semicolon
  while (p->current.type != TOK_EOF && p->current.type != TOK_SEMI) advance_p(p);
  match(p, TOK_SEMI);
  return stmt_expr(expr_literal(value_nil(), line, col), line, col);
}

static Stmt* statement(Parser* p) {
  if (match(p, TOK_UI)) {
    return ui_statement(p);
  }

  if (match(p, TOK_PRINT)) {
    int line = p->previous.line, col = p->previous.col;
    consume(p, TOK_LPAREN, "expected '(' after print");

    int cap = 4, n = 0;
    Expr** args = (Expr**)xmalloc(sizeof(Expr*) * (size_t)cap);

    if (!check(p, TOK_RPAREN)) {
      do {
        if (n >= cap) { cap *= 2; args = (Expr**)xrealloc(args, sizeof(Expr*) * (size_t)cap); }
        args[n++] = expression(p);
      } while (match(p, TOK_COMMA));
    }

    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after print(...)");

    return stmt_print(args, n, line, col);
  }

  // expression statement
  Expr* e = expression(p);
  int line = e->line, col = e->col;
  consume(p, TOK_SEMI, "expected ';' after expression");
  return stmt_expr(e, line, col);
}

static Stmt* declaration(Parser* p) {
  if (match(p, TOK_LET)) {
    int line = p->previous.line, col = p->previous.col;
    consume(p, TOK_IDENT, "expected variable name after let");
    char* name = token_text_dup(&p->previous);

    consume(p, TOK_EQUAL, "expected '=' after variable name");
    Expr* value = expression(p);
    consume(p, TOK_SEMI, "expected ';' after let declaration");

    Stmt* s = stmt_let(name, value, line, col);
    free(name);
    return s;
  }

  return statement(p);
}

void parser_init(Parser* p, const char* filename, const char* src, ErrorCtx* err) {
  memset(p, 0, sizeof(*p));
  p->err = err;
  lexer_init(&p->lex, filename, src, err);

  memset(&p->current, 0, sizeof(Token));
  memset(&p->previous, 0, sizeof(Token));
  p->panic = 0;

  p->current = lexer_next(&p->lex);
}

Program parse_program(Parser* p) {
  Program prog;
  prog.stmts = NULL;
  prog.count = 0;

  int cap = 8;
  prog.stmts = (Stmt**)xmalloc(sizeof(Stmt*) * (size_t)cap);

  while (p->current.type != TOK_EOF) {
    Stmt* s = declaration(p);
    if (prog.count >= cap) {
      cap *= 2;
      prog.stmts = (Stmt**)xrealloc(prog.stmts, sizeof(Stmt*) * (size_t)cap);
    }
    prog.stmts[prog.count++] = s;

    if (p->panic) synchronize(p);
  }

  token_free(&p->current);
  token_free(&p->previous);
  return prog;
}
