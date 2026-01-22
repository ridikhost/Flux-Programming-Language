#include "eval.h"

// ----------------- UI state -----------------

static void ui_ensure(UiState* u, int extra) {
  if (u->count + extra <= u->cap) return;
  int newcap = (u->cap == 0) ? 8 : u->cap;
  while (newcap < u->count + extra) newcap *= 2;
  u->items = (UiItem*)xrealloc(u->items, sizeof(UiItem) * (size_t)newcap);
  // zero-init new range
  for (int i = u->cap; i < newcap; i++) {
    u->items[i].kind = UI_ITEM_TEXT;
    u->items[i].a = NULL;
    u->items[i].b = NULL;
  }
  u->cap = newcap;
}

void ui_state_init(UiState* u) {
  u->title = xstrdup("Flux UI");
  u->backend = xstrdup("tui");
  u->items = NULL;
  u->count = 0;
  u->cap = 0;
  u->requested_run = 0;
}

void ui_state_free(UiState* u) {
  if (!u) return;
  free(u->title);
  free(u->backend);
  for (int i = 0; i < u->count; i++) {
    free(u->items[i].a);
    free(u->items[i].b);
  }
  free(u->items);
  u->title = NULL;
  u->backend = NULL;
  u->items = NULL;
  u->count = u->cap = 0;
  u->requested_run = 0;
}

void ui_state_add_text(UiState* u, const char* text) {
  ui_ensure(u, 1);
  UiItem* it = &u->items[u->count++];
  it->kind = UI_ITEM_TEXT;
  it->a = xstrdup(text ? text : "");
  it->b = NULL;
}

void ui_state_add_button(UiState* u, const char* label) {
  ui_ensure(u, 1);
  UiItem* it = &u->items[u->count++];
  it->kind = UI_ITEM_BUTTON;
  it->a = xstrdup(label ? label : "");
  it->b = NULL;
}

void ui_state_add_input(UiState* u, const char* label, const char* value) {
  ui_ensure(u, 1);
  UiItem* it = &u->items[u->count++];
  it->kind = UI_ITEM_INPUT;
  it->a = xstrdup(label ? label : "");
  it->b = xstrdup(value ? value : "");
}

void ui_state_set_title(UiState* u, const char* title) {
  if (!u) return;
  free(u->title);
  u->title = xstrdup(title ? title : "Flux UI");
}

void ui_state_request_run(UiState* u, const char* backend) {
  if (!u) return;
  u->requested_run = 1;
  if (backend && backend[0] != '\0') {
    free(u->backend);
    u->backend = xstrdup(backend);
  }
}

static void append_str(char** buf, size_t* len, size_t* cap, const char* s) {
  size_t n = strlen(s);
  if (*len + n + 1 > *cap) {
    size_t newcap = (*cap == 0) ? 256 : *cap;
    while (*len + n + 1 > newcap) newcap *= 2;
    *buf = (char*)xrealloc(*buf, newcap);
    *cap = newcap;
  }
  memcpy(*buf + *len, s, n);
  *len += n;
  (*buf)[*len] = '\0';
}

static void append_json_escaped(char** buf, size_t* len, size_t* cap, const char* s) {
  append_str(buf, len, cap, "\"");
  for (const char* p = s ? s : ""; *p; p++) {
    char c = *p;
    if (c == '\\') append_str(buf, len, cap, "\\\\");
    else if (c == '"') append_str(buf, len, cap, "\\\"");
    else if (c == '\n') append_str(buf, len, cap, "\\n");
    else if (c == '\r') append_str(buf, len, cap, "\\r");
    else if (c == '\t') append_str(buf, len, cap, "\\t");
    else {
      char tmp[2] = {c, 0};
      append_str(buf, len, cap, tmp);
    }
  }
  append_str(buf, len, cap, "\"");
}

char* ui_state_to_json(const UiState* u) {
  char* buf = NULL;
  size_t len = 0, cap = 0;

  append_str(&buf, &len, &cap, "{");

  append_str(&buf, &len, &cap, "\"title\":");
  append_json_escaped(&buf, &len, &cap, u && u->title ? u->title : "Flux UI");
  append_str(&buf, &len, &cap, ",");

  append_str(&buf, &len, &cap, "\"backend\":");
  append_json_escaped(&buf, &len, &cap, u && u->backend ? u->backend : "tui");
  append_str(&buf, &len, &cap, ",");

  append_str(&buf, &len, &cap, "\"items\":[");
  int count = u ? u->count : 0;
  for (int i = 0; i < count; i++) {
    if (i) append_str(&buf, &len, &cap, ",");
    UiItem* it = &u->items[i];
    append_str(&buf, &len, &cap, "{");
    append_str(&buf, &len, &cap, "\"kind\":");
    const char* kind = "text";
    if (it->kind == UI_ITEM_BUTTON) kind = "button";
    else if (it->kind == UI_ITEM_INPUT) kind = "input";
    append_json_escaped(&buf, &len, &cap, kind);

    if (it->kind == UI_ITEM_TEXT) {
      append_str(&buf, &len, &cap, ",\"text\":");
      append_json_escaped(&buf, &len, &cap, it->a ? it->a : "");
    } else if (it->kind == UI_ITEM_BUTTON) {
      append_str(&buf, &len, &cap, ",\"label\":");
      append_json_escaped(&buf, &len, &cap, it->a ? it->a : "");
    } else if (it->kind == UI_ITEM_INPUT) {
      append_str(&buf, &len, &cap, ",\"label\":");
      append_json_escaped(&buf, &len, &cap, it->a ? it->a : "");
      append_str(&buf, &len, &cap, ",\"value\":");
      append_json_escaped(&buf, &len, &cap, it->b ? it->b : "");
    }
    append_str(&buf, &len, &cap, "}");
  }
  append_str(&buf, &len, &cap, "]");

  append_str(&buf, &len, &cap, "}");
  return buf;
}

// ----------------- Values / Env / Interpreter -----------------

static Value value_copy(const Value* v) {
  if (!v) return value_nil();
  switch (v->type) {
    case VAL_NIL: return value_nil();
    case VAL_NUMBER: return value_number(v->as.number);
    case VAL_STRING: return value_string(v->as.str ? v->as.str : "");
    default: return value_nil();
  }
}

void env_init(Env* e) {
  e->count = 0;
  e->cap = 8;
  e->names = (char**)xmalloc(sizeof(char*) * (size_t)e->cap);
  e->values = (Value*)xmalloc(sizeof(Value) * (size_t)e->cap);
  for (int i = 0; i < e->cap; i++) e->values[i] = value_nil();
}

void env_free(Env* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->names[i]);
    value_free(&e->values[i]);
  }
  free(e->names);
  free(e->values);
  e->names = NULL;
  e->values = NULL;
  e->count = e->cap = 0;
}

static int env_find(Env* e, const char* name) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->names[i], name) == 0) return i;
  }
  return -1;
}

bool env_set(Env* e, const char* name, Value v) {
  int idx = env_find(e, name);
  if (idx >= 0) {
    value_free(&e->values[idx]);
    e->values[idx] = v;
    return true;
  }

  if (e->count >= e->cap) {
    e->cap *= 2;
    e->names = (char**)xrealloc(e->names, sizeof(char*) * (size_t)e->cap);
    e->values = (Value*)xrealloc(e->values, sizeof(Value) * (size_t)e->cap);
    for (int i = e->count; i < e->cap; i++) e->values[i] = value_nil();
  }

  e->names[e->count] = xstrdup(name);
  e->values[e->count] = v;
  e->count++;
  return true;
}

bool env_get(Env* e, const char* name, Value* out) {
  int idx = env_find(e, name);
  if (idx < 0) return false;
  *out = value_copy(&e->values[idx]);
  return true;
}

void interp_init(Interpreter* in, ErrorCtx* err, UiState* ui) {
  in->err = err;
  in->ui = ui;
  env_init(&in->env);
}

void interp_free(Interpreter* in) {
  env_free(&in->env);
}

static Value eval_expr(Interpreter* in, Expr* e);

static Value eval_binary(Interpreter* in, Expr* e) {
  Value left = eval_expr(in, e->as.binary.left);
  Value right = eval_expr(in, e->as.binary.right);

  // string concat for +
  if (e->as.binary.op == OP_ADD && (left.type == VAL_STRING || right.type == VAL_STRING)) {
    char* ls = value_to_string(&left);
    char* rs = value_to_string(&right);
    size_t n = strlen(ls) + strlen(rs);
    char* out = (char*)xmalloc(n + 1);
    memcpy(out, ls, strlen(ls));
    memcpy(out + strlen(ls), rs, strlen(rs) + 1);
    free(ls); free(rs);
    value_free(&left); value_free(&right);
    return value_string_take(out);
  }

  if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
    err_at(in->err, e->line, e->col, "type error: expected numbers for arithmetic");
    value_free(&left); value_free(&right);
    return value_nil();
  }

  double a = left.as.number;
  double b = right.as.number;
  value_free(&left); value_free(&right);

  switch (e->as.binary.op) {
    case OP_ADD: return value_number(a + b);
    case OP_SUB: return value_number(a - b);
    case OP_MUL: return value_number(a * b);
    case OP_DIV:
      if (b == 0.0) {
        err_at(in->err, e->line, e->col, "division by zero");
        return value_nil();
      }
      return value_number(a / b);
    default:
      return value_nil();
  }
}

static Value eval_expr(Interpreter* in, Expr* e) {
  if (in->err->had_error) return value_nil();
  switch (e->kind) {
    case EXPR_LITERAL:
      return value_copy(&e->as.literal);
    case EXPR_VAR: {
      Value out;
      if (!env_get(&in->env, e->as.var.name, &out)) {
        err_at(in->err, e->line, e->col, "undefined variable '%s'", e->as.var.name);
        return value_nil();
      }
      return out;
    }
    case EXPR_GROUP:
      return eval_expr(in, e->as.group.inner);
    case EXPR_BINARY:
      return eval_binary(in, e);
    default:
      return value_nil();
  }
}

static void exec_stmt(Interpreter* in, Stmt* s) {
  if (in->err->had_error) return;

  switch (s->kind) {
    case STMT_LET: {
      Value v = eval_expr(in, s->as.let_stmt.value);
      if (in->err->had_error) { value_free(&v); return; }
      env_set(&in->env, s->as.let_stmt.name, v); // takes ownership
      return;
    }
    case STMT_PRINT: {
      for (int i = 0; i < s->as.print_stmt.arg_count; i++) {
        Value v = eval_expr(in, s->as.print_stmt.args[i]);
        if (in->err->had_error) { value_free(&v); return; }
        char* txt = value_to_string(&v);
        value_free(&v);
        if (i > 0) printf(" ");
        printf("%s", txt);
        free(txt);
      }
      printf("\n");
      return;
    }
    case STMT_EXPR: {
      Value v = eval_expr(in, s->as.expr_stmt.expr);
      value_free(&v);
      return;
    }
    case STMT_UI: {
      if (!in->ui) return;
      UiStmtKind k = s->as.ui_stmt.ui_kind;
      const char* a = s->as.ui_stmt.a;
      const char* b = s->as.ui_stmt.b;
      switch (k) {
        case UI_INIT: ui_state_set_title(in->ui, a); break;
        case UI_TEXT: ui_state_add_text(in->ui, a); break;
        case UI_BUTTON: ui_state_add_button(in->ui, a); break;
        case UI_INPUT: ui_state_add_input(in->ui, a, b); break;
        case UI_RUN: ui_state_request_run(in->ui, a); break;
      }
      return;
    }
  }
}

bool interp_run(Interpreter* in, const Program* p) {
  for (int i = 0; i < p->count; i++) {
    exec_stmt(in, p->stmts[i]);
    if (in->err->had_error) return false;
  }
  return true;
}
