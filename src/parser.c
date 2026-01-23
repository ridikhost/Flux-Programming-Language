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

static void synchronize(Parser* p) {
  p->panic = 0;
  while (p->current.type != TOK_EOF) {
    if (p->previous.type == TOK_SEMI) return;
    switch (p->current.type) {
      case TOK_LET:
      case TOK_PRINT:
      case TOK_UI:
      case TOK_IF:
      case TOK_WHILE:
      case TOK_LBRACE:
        return;
      default:
        break;
    }
    advance_p(p);
  }
}

// Forward decl
static Expr* expression(Parser* p);
static Stmt* declaration(Parser* p);
static Stmt* statement(Parser* p);
static Stmt* block_statement(Parser* p);

// ----------------- expressions -----------------

static char* parse_dotted_name(Parser* p, Token* first_ident) {
  // first_ident is previous token (TOK_IDENT)
  char* name = token_text_dup(first_ident);
  while (match(p, TOK_DOT)) {
    consume(p, TOK_IDENT, "expected identifier after '.'");
    char* part = token_text_dup(&p->previous);
    size_t n = strlen(name) + 1 + strlen(part);
    char* out = (char*)xmalloc(n + 1);
    snprintf(out, n + 1, "%s.%s", name, part);
    free(name);
    free(part);
    name = out;
  }
  return name;
}

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
  if (match(p, TOK_TRUE)) {
    return expr_literal(value_bool(true), p->previous.line, p->previous.col);
  }
  if (match(p, TOK_FALSE)) {
    return expr_literal(value_bool(false), p->previous.line, p->previous.col);
  }
  if (match(p, TOK_NIL)) {
    return expr_literal(value_nil(), p->previous.line, p->previous.col);
  }
  if (match(p, TOK_IDENT)) {
    int line = p->previous.line, col = p->previous.col;
    char* name = parse_dotted_name(p, &p->previous);

    // Call?
    if (match(p, TOK_LPAREN)) {
      int cap = 4, n = 0;
      Expr** args = (Expr**)xmalloc(sizeof(Expr*) * (size_t)cap);

      if (!check(p, TOK_RPAREN)) {
        do {
          if (n >= cap) { cap *= 2; args = (Expr**)xrealloc(args, sizeof(Expr*) * (size_t)cap); }
          args[n++] = expression(p);
        } while (match(p, TOK_COMMA));
      }

      consume(p, TOK_RPAREN, "expected ')'");
      Expr* call = expr_call(name, args, n, line, col);
      free(name);
      return call;
    }

    Expr* v = expr_var(name, line, col);
    free(name);
    return v;
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
  if (match(p, TOK_BANG)) {
    int line = p->previous.line, col = p->previous.col;
    Expr* right = unary(p);
    return expr_unary(UOP_NOT, right, line, col);
  }
  if (match(p, TOK_MINUS)) {
    // -x => 0 - x
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

static Expr* comparison(Parser* p) {
  Expr* expr = term(p);
  for (;;) {
    if (match(p, TOK_LESS)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = term(p);
      expr = expr_binary(OP_LT, expr, right, line, col);
      continue;
    }
    if (match(p, TOK_LESS_EQUAL)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = term(p);
      expr = expr_binary(OP_LTE, expr, right, line, col);
      continue;
    }
    if (match(p, TOK_GREATER)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = term(p);
      expr = expr_binary(OP_GT, expr, right, line, col);
      continue;
    }
    if (match(p, TOK_GREATER_EQUAL)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = term(p);
      expr = expr_binary(OP_GTE, expr, right, line, col);
      continue;
    }
    break;
  }
  return expr;
}

static Expr* equality(Parser* p) {
  Expr* expr = comparison(p);
  for (;;) {
    if (match(p, TOK_EQUAL_EQUAL)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = comparison(p);
      expr = expr_binary(OP_EQ, expr, right, line, col);
      continue;
    }
    if (match(p, TOK_BANG_EQUAL)) {
      int line = p->previous.line, col = p->previous.col;
      Expr* right = comparison(p);
      expr = expr_binary(OP_NEQ, expr, right, line, col);
      continue;
    }
    break;
  }
  return expr;
}

static Expr* logic_and(Parser* p) {
  Expr* expr = equality(p);
  while (match(p, TOK_AND)) {
    int line = p->previous.line, col = p->previous.col;
    Expr* right = equality(p);
    expr = expr_binary(OP_AND, expr, right, line, col);
  }
  return expr;
}

static Expr* logic_or(Parser* p) {
  Expr* expr = logic_and(p);
  while (match(p, TOK_OR)) {
    int line = p->previous.line, col = p->previous.col;
    Expr* right = logic_and(p);
    expr = expr_binary(OP_OR, expr, right, line, col);
  }
  return expr;
}

static Expr* expression(Parser* p) { return logic_or(p); }

// ----------------- ui statement -----------------

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
    char* title = NULL;
    if (!check(p, TOK_RPAREN)) title = parse_string_arg(p, "expected string title");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui init(...)");
    Stmt* s = stmt_ui(UI_INIT, title ? title : "Flux UI", NULL, line, col);
    free(title);
    return s;
  }

  if (match(p, TOK_TITLE)) {
    consume(p, TOK_LPAREN, "expected '(' after ui title");
    char* title = NULL;
    if (!check(p, TOK_RPAREN)) title = parse_string_arg(p, "expected string title");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui title(...)");
    Stmt* s = stmt_ui(UI_TITLE, title ? title : "Flux UI", NULL, line, col);
    free(title);
    return s;
  }

  if (match(p, TOK_CLEAR)) {
    consume(p, TOK_LPAREN, "expected '(' after ui clear");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui clear()");
    return stmt_ui(UI_CLEAR, NULL, NULL, line, col);
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

  if (match(p, TOK_SELECT)) {
    consume(p, TOK_LPAREN, "expected '(' after ui select");
    char* label = parse_string_arg(p, "expected string label for ui select");
    // options: one or more strings, separated by commas
    // store options joined by '\n' in b
    consume(p, TOK_COMMA, "expected ',' after select label");
    // gather options
    size_t cap = 64, len = 0;
    char* joined = (char*)xmalloc(cap);
    joined[0] = '\0';

    int opt_count = 0;
    do {
      char* opt = parse_string_arg(p, "expected string option");
      if (!opt) opt = xstrdup("");
      size_t opt_len = strlen(opt);
      size_t need = len + opt_len + 2;
      if (need > cap) { while (need > cap) cap *= 2; joined = (char*)xrealloc(joined, cap); }
      if (opt_count > 0) joined[len++] = '\n';
      memcpy(joined + len, opt, opt_len);
      len += opt_len;
      joined[len] = '\0';
      free(opt);
      opt_count++;
    } while (match(p, TOK_COMMA));

    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui select(...)");
    Stmt* s = stmt_ui(UI_SELECT, label ? label : "", joined, line, col);
    free(label);
    free(joined);
    return s;
  }

  if (match(p, TOK_CHECKBOX)) {
    consume(p, TOK_LPAREN, "expected '(' after ui checkbox");
    char* label = parse_string_arg(p, "expected string label for ui checkbox");
    consume(p, TOK_COMMA, "expected ',' after checkbox label");
    // accept true/false as literals
    bool def = false;
    if (match(p, TOK_TRUE)) def = true;
    else if (match(p, TOK_FALSE)) def = false;
    else error_here(p, "expected true or false for checkbox default");
    consume(p, TOK_RPAREN, "expected ')'");
    consume(p, TOK_SEMI, "expected ';' after ui checkbox(...)");
    Stmt* s = stmt_ui(UI_CHECKBOX, label ? label : "", def ? "true" : "false", line, col);
    free(label);
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

  error_here(p, "expected ui command: init/title/clear/text/button/input/select/checkbox/run");
  while (p->current.type != TOK_EOF && p->current.type != TOK_SEMI) advance_p(p);
  match(p, TOK_SEMI);
  return stmt_expr(expr_literal(value_nil(), line, col), line, col);
}

// ----------------- statements -----------------

static Stmt* if_statement(Parser* p) {
  int line = p->previous.line, col = p->previous.col;
  consume(p, TOK_LPAREN, "expected '(' after if");
  Expr* cond = expression(p);
  consume(p, TOK_RPAREN, "expected ')'");
  Stmt* then_branch = block_statement(p);
  Stmt* else_branch = NULL;
  if (match(p, TOK_ELSE)) {
    else_branch = block_statement(p);
  }
  return stmt_if(cond, then_branch, else_branch, line, col);
}

static Stmt* while_statement(Parser* p) {
  int line = p->previous.line, col = p->previous.col;
  consume(p, TOK_LPAREN, "expected '(' after while");
  Expr* cond = expression(p);
  consume(p, TOK_RPAREN, "expected ')'");
  Stmt* body = block_statement(p);
  return stmt_while(cond, body, line, col);
}

static Stmt* block_statement(Parser* p) {
  if (!match(p, TOK_LBRACE)) {
    error_here(p, "expected '{' to start block");
    // recover: treat single statement as a block with one statement
    Stmt* inner = statement(p);
    Stmt** list = (Stmt**)xmalloc(sizeof(Stmt*));
    list[0] = inner;
    return stmt_block(list, 1, inner->line, inner->col);
  }

  int line = p->previous.line, col = p->previous.col;
  int cap = 8, n = 0;
  Stmt** stmts = (Stmt**)xmalloc(sizeof(Stmt*) * (size_t)cap);

  while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
    if (n >= cap) { cap *= 2; stmts = (Stmt**)xrealloc(stmts, sizeof(Stmt*) * (size_t)cap); }
    stmts[n++] = declaration(p);
    if (p->panic) synchronize(p);
  }
  consume(p, TOK_RBRACE, "expected '}' after block");
  return stmt_block(stmts, n, line, col);
}

static Stmt* print_statement(Parser* p) {
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

static Stmt* statement(Parser* p) {
  if (match(p, TOK_UI)) return ui_statement(p);
  if (match(p, TOK_PRINT)) return print_statement(p);
  if (match(p, TOK_IF)) return if_statement(p);
  if (match(p, TOK_WHILE)) return while_statement(p);
  if (check(p, TOK_LBRACE)) return block_statement(p);

  // expression or assignment statement
  Expr* e = expression(p);
  int line = e->line, col = e->col;

  if (e->kind == EXPR_VAR && match(p, TOK_EQUAL)) {
    // assignment
    Expr* value = expression(p);
    consume(p, TOK_SEMI, "expected ';' after assignment");
    Stmt* s = stmt_assign(e->as.var.name, value, line, col);
    expr_free(e);
    return s;
  }

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

// ----------------- entry -----------------

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
