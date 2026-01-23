#ifndef FLUX_AST_H
#define FLUX_AST_H

#include "common.h"
#include "value.h"

typedef enum {
  EXPR_LITERAL = 0,
  EXPR_VAR,
  EXPR_BINARY,
  EXPR_UNARY,
  EXPR_GROUP,
  EXPR_CALL
} ExprKind;

typedef enum {
  OP_ADD, OP_SUB, OP_MUL, OP_DIV,
  OP_EQ, OP_NEQ,
  OP_LT, OP_LTE, OP_GT, OP_GTE,
  OP_AND, OP_OR
} BinOp;

typedef enum {
  UOP_NOT = 0
} UnOp;

typedef struct Expr Expr;

struct Expr {
  ExprKind kind;
  int line, col;
  union {
    Value literal; // EXPR_LITERAL
    struct { char* name; } var; // EXPR_VAR (owned)
    struct { BinOp op; Expr* left; Expr* right; } binary; // EXPR_BINARY
    struct { UnOp op; Expr* right; } unary; // EXPR_UNARY
    struct { Expr* inner; } group; // EXPR_GROUP
    struct { char* callee; Expr** args; int arg_count; } call; // EXPR_CALL
  } as;
};

typedef enum {
  STMT_LET = 0,
  STMT_ASSIGN,
  STMT_PRINT,
  STMT_EXPR,
  STMT_UI,
  STMT_BLOCK,
  STMT_IF,
  STMT_WHILE
} StmtKind;

typedef enum {
  UI_INIT = 0,
  UI_TEXT,
  UI_BUTTON,
  UI_INPUT,
  UI_RUN,
  UI_CLEAR,
  UI_TITLE,
  UI_SELECT,
  UI_CHECKBOX
} UiStmtKind;

typedef struct Stmt Stmt;

struct Stmt {
  StmtKind kind;
  int line, col;
  union {
    struct { char* name; Expr* value; } let_stmt;       // owns name
    struct { char* name; Expr* value; } assign_stmt;    // owns name
    struct { Expr** args; int arg_count; } print_stmt;
    struct { Expr* expr; } expr_stmt;

    struct {
      UiStmtKind ui_kind;
      // strings are owned
      char* a;
      char* b;
    } ui_stmt;

    struct { Stmt** stmts; int count; } block_stmt;

    struct { Expr* cond; Stmt* then_branch; Stmt* else_branch; } if_stmt; // branches are usually blocks; else_branch can be NULL
    struct { Expr* cond; Stmt* body; } while_stmt;
  } as;
};

typedef struct {
  Stmt** stmts;
  int count;
} Program;

// Expr constructors
Expr* expr_literal(Value v, int line, int col);
Expr* expr_var(const char* name, int line, int col);
Expr* expr_binary(BinOp op, Expr* left, Expr* right, int line, int col);
Expr* expr_unary(UnOp op, Expr* right, int line, int col);
Expr* expr_group(Expr* inner, int line, int col);
Expr* expr_call(const char* callee, Expr** args, int arg_count, int line, int col);

// Stmt constructors
Stmt* stmt_let(const char* name, Expr* value, int line, int col);
Stmt* stmt_assign(const char* name, Expr* value, int line, int col);
Stmt* stmt_print(Expr** args, int arg_count, int line, int col);
Stmt* stmt_expr(Expr* expr, int line, int col);
Stmt* stmt_ui(UiStmtKind kind, const char* a, const char* b, int line, int col);
Stmt* stmt_block(Stmt** stmts, int count, int line, int col);
Stmt* stmt_if(Expr* cond, Stmt* then_branch, Stmt* else_branch, int line, int col);
Stmt* stmt_while(Expr* cond, Stmt* body, int line, int col);

void expr_free(Expr* e);
void stmt_free(Stmt* s);
void program_free(Program* p);

#endif // FLUX_AST_H
