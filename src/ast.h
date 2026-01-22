#ifndef FLUX_AST_H
#define FLUX_AST_H

#include "common.h"
#include "value.h"

typedef enum {
  EXPR_LITERAL = 0,
  EXPR_VAR,
  EXPR_BINARY,
  EXPR_GROUP
} ExprKind;

typedef enum {
  OP_ADD, OP_SUB, OP_MUL, OP_DIV
} BinOp;

typedef struct Expr Expr;

struct Expr {
  ExprKind kind;
  int line, col;
  union {
    Value literal; // EXPR_LITERAL
    struct { char* name; } var; // EXPR_VAR
    struct { BinOp op; Expr* left; Expr* right; } binary; // EXPR_BINARY
    struct { Expr* inner; } group; // EXPR_GROUP
  } as;
};

typedef enum {
  STMT_LET = 0,
  STMT_PRINT,
  STMT_EXPR,
  STMT_UI
} StmtKind;

typedef enum {
  UI_INIT = 0,
  UI_TEXT,
  UI_BUTTON,
  UI_INPUT,
  UI_RUN
} UiStmtKind;

typedef struct Stmt Stmt;

struct Stmt {
  StmtKind kind;
  int line, col;
  union {
    struct { char* name; Expr* value; } let_stmt;
    struct { Expr** args; int arg_count; } print_stmt;
    struct { Expr* expr; } expr_stmt;

    struct {
      UiStmtKind ui_kind;
      // strings are owned
      char* a;
      char* b;
    } ui_stmt;
  } as;
};

typedef struct {
  Stmt** stmts;
  int count;
} Program;

Expr* expr_literal(Value v, int line, int col);
Expr* expr_var(const char* name, int line, int col);
Expr* expr_binary(BinOp op, Expr* left, Expr* right, int line, int col);
Expr* expr_group(Expr* inner, int line, int col);

Stmt* stmt_let(const char* name, Expr* value, int line, int col);
Stmt* stmt_print(Expr** args, int arg_count, int line, int col);
Stmt* stmt_expr(Expr* expr, int line, int col);
Stmt* stmt_ui(UiStmtKind kind, const char* a, const char* b, int line, int col);

void expr_free(Expr* e);
void stmt_free(Stmt* s);
void program_free(Program* p);

#endif // FLUX_AST_H
