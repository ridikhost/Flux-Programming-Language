#include "ast.h"

Expr* expr_literal(Value v, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  e->kind = EXPR_LITERAL;
  e->line = line; e->col = col;
  e->as.literal = v;
  return e;
}

Expr* expr_var(const char* name, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  e->kind = EXPR_VAR;
  e->line = line; e->col = col;
  e->as.var.name = xstrdup(name);
  return e;
}

Expr* expr_binary(BinOp op, Expr* left, Expr* right, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  e->kind = EXPR_BINARY;
  e->line = line; e->col = col;
  e->as.binary.op = op;
  e->as.binary.left = left;
  e->as.binary.right = right;
  return e;
}

Expr* expr_group(Expr* inner, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  e->kind = EXPR_GROUP;
  e->line = line; e->col = col;
  e->as.group.inner = inner;
  return e;
}

Stmt* stmt_let(const char* name, Expr* value, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  s->kind = STMT_LET;
  s->line = line; s->col = col;
  s->as.let_stmt.name = xstrdup(name);
  s->as.let_stmt.value = value;
  return s;
}

Stmt* stmt_print(Expr** args, int arg_count, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  s->kind = STMT_PRINT;
  s->line = line; s->col = col;
  s->as.print_stmt.args = args;
  s->as.print_stmt.arg_count = arg_count;
  return s;
}

Stmt* stmt_expr(Expr* expr, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  s->kind = STMT_EXPR;
  s->line = line; s->col = col;
  s->as.expr_stmt.expr = expr;
  return s;
}

Stmt* stmt_ui(UiStmtKind kind, const char* a, const char* b, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  s->kind = STMT_UI;
  s->line = line; s->col = col;
  s->as.ui_stmt.ui_kind = kind;
  s->as.ui_stmt.a = a ? xstrdup(a) : NULL;
  s->as.ui_stmt.b = b ? xstrdup(b) : NULL;
  return s;
}

void expr_free(Expr* e) {
  if (!e) return;
  switch (e->kind) {
    case EXPR_LITERAL:
      value_free(&e->as.literal);
      break;
    case EXPR_VAR:
      free(e->as.var.name);
      break;
    case EXPR_BINARY:
      expr_free(e->as.binary.left);
      expr_free(e->as.binary.right);
      break;
    case EXPR_GROUP:
      expr_free(e->as.group.inner);
      break;
  }
  free(e);
}

void stmt_free(Stmt* s) {
  if (!s) return;
  switch (s->kind) {
    case STMT_LET:
      free(s->as.let_stmt.name);
      expr_free(s->as.let_stmt.value);
      break;
    case STMT_PRINT:
      for (int i = 0; i < s->as.print_stmt.arg_count; i++) expr_free(s->as.print_stmt.args[i]);
      free(s->as.print_stmt.args);
      break;
    case STMT_EXPR:
      expr_free(s->as.expr_stmt.expr);
      break;
    case STMT_UI:
      free(s->as.ui_stmt.a);
      free(s->as.ui_stmt.b);
      break;
  }
  free(s);
}

void program_free(Program* p) {
  if (!p) return;
  for (int i = 0; i < p->count; i++) stmt_free(p->stmts[i]);
  free(p->stmts);
  p->stmts = NULL;
  p->count = 0;
}
