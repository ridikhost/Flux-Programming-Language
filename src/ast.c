#include "ast.h"

Expr* expr_literal(Value v, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  memset(e, 0, sizeof(*e));
  e->kind = EXPR_LITERAL;
  e->line = line; e->col = col;
  e->as.literal = v; // takes ownership for strings
  return e;
}

Expr* expr_var(const char* name, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  memset(e, 0, sizeof(*e));
  e->kind = EXPR_VAR;
  e->line = line; e->col = col;
  e->as.var.name = xstrdup(name ? name : "");
  return e;
}

Expr* expr_binary(BinOp op, Expr* left, Expr* right, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  memset(e, 0, sizeof(*e));
  e->kind = EXPR_BINARY;
  e->line = line; e->col = col;
  e->as.binary.op = op;
  e->as.binary.left = left;
  e->as.binary.right = right;
  return e;
}

Expr* expr_unary(UnOp op, Expr* right, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  memset(e, 0, sizeof(*e));
  e->kind = EXPR_UNARY;
  e->line = line; e->col = col;
  e->as.unary.op = op;
  e->as.unary.right = right;
  return e;
}

Expr* expr_group(Expr* inner, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  memset(e, 0, sizeof(*e));
  e->kind = EXPR_GROUP;
  e->line = line; e->col = col;
  e->as.group.inner = inner;
  return e;
}

Expr* expr_call(const char* callee, Expr** args, int arg_count, int line, int col) {
  Expr* e = (Expr*)xmalloc(sizeof(Expr));
  memset(e, 0, sizeof(*e));
  e->kind = EXPR_CALL;
  e->line = line; e->col = col;
  e->as.call.callee = xstrdup(callee ? callee : "");
  e->as.call.args = args;
  e->as.call.arg_count = arg_count;
  return e;
}

Stmt* stmt_let(const char* name, Expr* value, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_LET;
  s->line = line; s->col = col;
  s->as.let_stmt.name = xstrdup(name ? name : "");
  s->as.let_stmt.value = value;
  return s;
}

Stmt* stmt_assign(const char* name, Expr* value, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_ASSIGN;
  s->line = line; s->col = col;
  s->as.assign_stmt.name = xstrdup(name ? name : "");
  s->as.assign_stmt.value = value;
  return s;
}

Stmt* stmt_print(Expr** args, int arg_count, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_PRINT;
  s->line = line; s->col = col;
  s->as.print_stmt.args = args;
  s->as.print_stmt.arg_count = arg_count;
  return s;
}

Stmt* stmt_expr(Expr* expr, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_EXPR;
  s->line = line; s->col = col;
  s->as.expr_stmt.expr = expr;
  return s;
}

Stmt* stmt_ui(UiStmtKind kind, const char* a, const char* b, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_UI;
  s->line = line; s->col = col;
  s->as.ui_stmt.ui_kind = kind;
  s->as.ui_stmt.a = a ? xstrdup(a) : NULL;
  s->as.ui_stmt.b = b ? xstrdup(b) : NULL;
  return s;
}

Stmt* stmt_block(Stmt** stmts, int count, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_BLOCK;
  s->line = line; s->col = col;
  s->as.block_stmt.stmts = stmts;
  s->as.block_stmt.count = count;
  return s;
}

Stmt* stmt_if(Expr* cond, Stmt* then_branch, Stmt* else_branch, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_IF;
  s->line = line; s->col = col;
  s->as.if_stmt.cond = cond;
  s->as.if_stmt.then_branch = then_branch;
  s->as.if_stmt.else_branch = else_branch;
  return s;
}

Stmt* stmt_while(Expr* cond, Stmt* body, int line, int col) {
  Stmt* s = (Stmt*)xmalloc(sizeof(Stmt));
  memset(s, 0, sizeof(*s));
  s->kind = STMT_WHILE;
  s->line = line; s->col = col;
  s->as.while_stmt.cond = cond;
  s->as.while_stmt.body = body;
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
    case EXPR_UNARY:
      expr_free(e->as.unary.right);
      break;
    case EXPR_GROUP:
      expr_free(e->as.group.inner);
      break;
    case EXPR_CALL:
      free(e->as.call.callee);
      for (int i = 0; i < e->as.call.arg_count; i++) expr_free(e->as.call.args[i]);
      free(e->as.call.args);
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
    case STMT_ASSIGN:
      free(s->as.assign_stmt.name);
      expr_free(s->as.assign_stmt.value);
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
    case STMT_BLOCK:
      for (int i = 0; i < s->as.block_stmt.count; i++) stmt_free(s->as.block_stmt.stmts[i]);
      free(s->as.block_stmt.stmts);
      break;
    case STMT_IF:
      expr_free(s->as.if_stmt.cond);
      stmt_free(s->as.if_stmt.then_branch);
      if (s->as.if_stmt.else_branch) stmt_free(s->as.if_stmt.else_branch);
      break;
    case STMT_WHILE:
      expr_free(s->as.while_stmt.cond);
      stmt_free(s->as.while_stmt.body);
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
