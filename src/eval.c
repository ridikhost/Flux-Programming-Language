#include "eval.h"

#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

// ----------------- UI state -----------------

static void ui_ensure(UiState* u, int extra) {
  if (u->count + extra <= u->cap) return;
  int newcap = (u->cap == 0) ? 8 : u->cap;
  while (newcap < u->count + extra) newcap *= 2;
  u->items = (UiItem*)xrealloc(u->items, sizeof(UiItem) * (size_t)newcap);
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

void ui_state_clear(UiState* u) {
  if (!u) return;
  for (int i = 0; i < u->count; i++) {
    free(u->items[i].a);
    free(u->items[i].b);
    u->items[i].a = NULL;
    u->items[i].b = NULL;
  }
  u->count = 0;
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

void ui_state_add_select(UiState* u, const char* label, const char* options_joined) {
  ui_ensure(u, 1);
  UiItem* it = &u->items[u->count++];
  it->kind = UI_ITEM_SELECT;
  it->a = xstrdup(label ? label : "");
  it->b = xstrdup(options_joined ? options_joined : "");
}

void ui_state_add_checkbox(UiState* u, const char* label, bool checked) {
  ui_ensure(u, 1);
  UiItem* it = &u->items[u->count++];
  it->kind = UI_ITEM_CHECKBOX;
  it->a = xstrdup(label ? label : "");
  it->b = xstrdup(checked ? "true" : "false");
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

static void append_json_string_array_from_joined(char** buf, size_t* len, size_t* cap, const char* joined) {
  append_str(buf, len, cap, "[");
  int first = 1;
  const char* s = joined ? joined : "";
  const char* p = s;
  while (true) {
    const char* nl = strchr(p, '\n');
    size_t seglen = nl ? (size_t)(nl - p) : strlen(p);
    char* seg = strndup2(p, seglen);
    if (!first) append_str(buf, len, cap, ",");
    append_json_escaped(buf, len, cap, seg);
    free(seg);
    first = 0;
    if (!nl) break;
    p = nl + 1;
  }
  append_str(buf, len, cap, "]");
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
    else if (it->kind == UI_ITEM_SELECT) kind = "select";
    else if (it->kind == UI_ITEM_CHECKBOX) kind = "checkbox";
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
    } else if (it->kind == UI_ITEM_SELECT) {
      append_str(&buf, &len, &cap, ",\"label\":");
      append_json_escaped(&buf, &len, &cap, it->a ? it->a : "");
      append_str(&buf, &len, &cap, ",\"options\":");
      append_json_string_array_from_joined(&buf, &len, &cap, it->b ? it->b : "");
    } else if (it->kind == UI_ITEM_CHECKBOX) {
      append_str(&buf, &len, &cap, ",\"label\":");
      append_json_escaped(&buf, &len, &cap, it->a ? it->a : "");
      append_str(&buf, &len, &cap, ",\"checked\":");
      append_str(&buf, &len, &cap, (it->b && strcmp(it->b, "true") == 0) ? "true" : "false");
    }

    append_str(&buf, &len, &cap, "}");
  }
  append_str(&buf, &len, &cap, "]");

  append_str(&buf, &len, &cap, "}");
  return buf;
}

// ----------------- Env -----------------

static Value value_copy(const Value* v) {
  if (!v) return value_nil();
  switch (v->type) {
    case VAL_NIL: return value_nil();
    case VAL_BOOL: return value_bool(v->as.boolean);
    case VAL_NUMBER: return value_number(v->as.number);
    case VAL_STRING: return value_string(v->as.str ? v->as.str : "");
    default: return value_nil();
  }
}

Env* env_new(Env* parent) {
  Env* e = (Env*)xmalloc(sizeof(Env));
  e->parent = parent;
  e->count = 0;
  e->cap = 8;
  e->names = (char**)xmalloc(sizeof(char*) * (size_t)e->cap);
  e->values = (Value*)xmalloc(sizeof(Value) * (size_t)e->cap);
  for (int i = 0; i < e->cap; i++) e->values[i] = value_nil();
  return e;
}

void env_free(Env* e) {
  if (!e) return;
  for (int i = 0; i < e->count; i++) {
    free(e->names[i]);
    value_free(&e->values[i]);
  }
  free(e->names);
  free(e->values);
  free(e);
}

static int env_find_local(Env* e, const char* name) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->names[i], name) == 0) return i;
  }
  return -1;
}

bool env_define(Env* e, const char* name, Value v) {
  if (!e) { value_free(&v); return false; }
  int idx = env_find_local(e, name);
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

bool env_assign(Env* e, const char* name, Value v) {
  for (Env* cur = e; cur; cur = cur->parent) {
    int idx = env_find_local(cur, name);
    if (idx >= 0) {
      value_free(&cur->values[idx]);
      cur->values[idx] = v;
      return true;
    }
  }
  value_free(&v);
  return false;
}

bool env_get(Env* e, const char* name, Value* out) {
  for (Env* cur = e; cur; cur = cur->parent) {
    int idx = env_find_local(cur, name);
    if (idx >= 0) {
      *out = value_copy(&cur->values[idx]);
      return true;
    }
  }
  return false;
}

// ----------------- Interpreter -----------------

static Value eval_expr(Interpreter* in, Expr* e);
static void exec_stmt(Interpreter* in, Stmt* s);

void interp_init(Interpreter* in, ErrorCtx* err, UiState* ui) {
  in->err = err;
  in->ui = ui;
  in->env = env_new(NULL);
}

void interp_free(Interpreter* in) {
  // free whole chain
  while (in->env) {
    Env* parent = in->env->parent;
    env_free(in->env);
    in->env = parent;
  }
}

static double now_ms(void) {
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER uli;
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  // 100-ns intervals since 1601-01-01
  unsigned long long t = uli.QuadPart;
  // convert to ms since epoch 1970-01-01
  const unsigned long long EPOCH_DIFF_100NS = 116444736000000000ULL;
  if (t < EPOCH_DIFF_100NS) return 0.0;
  t -= EPOCH_DIFF_100NS;
  return (double)(t / 10000ULL);
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

static void sleep_ms(int ms) {
  if (ms <= 0) return;
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  usleep((useconds_t)ms * 1000);
#endif
}

static Value eval_builtin_call(Interpreter* in, Expr* e) {
  const char* fn = e->as.call.callee;
  int argc = e->as.call.arg_count;
  Expr** args = e->as.call.args;

  if (strcmp(fn, "input") == 0) {
    const char* prompt = "";
    char* prompt_owned = NULL;
    if (argc >= 1) {
      Value pv = eval_expr(in, args[0]);
      if (in->err->had_error) { value_free(&pv); return value_nil(); }
      prompt_owned = value_to_string(&pv);
      value_free(&pv);
      prompt = prompt_owned;
    }
    if (prompt && prompt[0]) {
      printf("%s", prompt);
      fflush(stdout);
    }
    free(prompt_owned);

    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) {
      return value_string("");
    }
    // trim \\r?\\n
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\\n' || buf[n-1] == '\\r')) { buf[n-1] = '\0'; n--; }
    return value_string(buf);
  }

  if (strcmp(fn, "len") == 0) {
    if (argc < 1) return value_number(0.0);
    Value v = eval_expr(in, args[0]);
    if (in->err->had_error) { value_free(&v); return value_nil(); }
    if (v.type == VAL_STRING) {
      double out = (double)strlen(v.as.str ? v.as.str : "");
      value_free(&v);
      return value_number(out);
    }
    char* s = value_to_string(&v);
    value_free(&v);
    double out = (double)strlen(s);
    free(s);
    return value_number(out);
  }

  if (strcmp(fn, "time.ms") == 0 || strcmp(fn, "time_ms") == 0) {
    (void)argc;
    return value_number(now_ms());
  }

  if (strcmp(fn, "sleep") == 0) {
    if (argc < 1) return value_nil();
    Value v = eval_expr(in, args[0]);
    if (in->err->had_error) { value_free(&v); return value_nil(); }
    int ms = 0;
    if (v.type == VAL_NUMBER) ms = (int)v.as.number;
    value_free(&v);
    sleep_ms(ms);
    return value_nil();
  }

  err_at(in->err, e->line, e->col, "unknown function '%s'", fn);
  return value_nil();
}

static Value eval_binary(Interpreter* in, Expr* e) {
  // short-circuit for and/or
  if (e->as.binary.op == OP_AND) {
    Value left = eval_expr(in, e->as.binary.left);
    if (in->err->had_error) { value_free(&left); return value_nil(); }
    if (!value_is_truthy(&left)) { value_free(&left); return value_bool(false); }
    value_free(&left);
    Value right = eval_expr(in, e->as.binary.right);
    if (in->err->had_error) { value_free(&right); return value_nil(); }
    bool out = value_is_truthy(&right);
    value_free(&right);
    return value_bool(out);
  }
  if (e->as.binary.op == OP_OR) {
    Value left = eval_expr(in, e->as.binary.left);
    if (in->err->had_error) { value_free(&left); return value_nil(); }
    if (value_is_truthy(&left)) { value_free(&left); return value_bool(true); }
    value_free(&left);
    Value right = eval_expr(in, e->as.binary.right);
    if (in->err->had_error) { value_free(&right); return value_nil(); }
    bool out = value_is_truthy(&right);
    value_free(&right);
    return value_bool(out);
  }

  Value left = eval_expr(in, e->as.binary.left);
  Value right = eval_expr(in, e->as.binary.right);
  if (in->err->had_error) { value_free(&left); value_free(&right); return value_nil(); }

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

  // equality
  if (e->as.binary.op == OP_EQ || e->as.binary.op == OP_NEQ) {
    bool eq = value_equal(&left, &right);
    value_free(&left); value_free(&right);
    return value_bool((e->as.binary.op == OP_EQ) ? eq : !eq);
  }

  // comparisons: require numbers
  if (e->as.binary.op == OP_LT || e->as.binary.op == OP_LTE ||
      e->as.binary.op == OP_GT || e->as.binary.op == OP_GTE) {
    if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
      err_at(in->err, e->line, e->col, "type error: expected numbers for comparison");
      value_free(&left); value_free(&right);
      return value_nil();
    }
    double a = left.as.number;
    double b = right.as.number;
    value_free(&left); value_free(&right);
    bool out = false;
    switch (e->as.binary.op) {
      case OP_LT: out = a < b; break;
      case OP_LTE: out = a <= b; break;
      case OP_GT: out = a > b; break;
      case OP_GTE: out = a >= b; break;
      default: break;
    }
    return value_bool(out);
  }

  // arithmetic: numbers only
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
      if (!env_get(in->env, e->as.var.name, &out)) {
        err_at(in->err, e->line, e->col, "undefined variable '%s'", e->as.var.name);
        return value_nil();
      }
      return out;
    }
    case EXPR_GROUP:
      return eval_expr(in, e->as.group.inner);
    case EXPR_UNARY: {
      Value v = eval_expr(in, e->as.unary.right);
      if (in->err->had_error) { value_free(&v); return value_nil(); }
      if (e->as.unary.op == UOP_NOT) {
        bool out = !value_is_truthy(&v);
        value_free(&v);
        return value_bool(out);
      }
      value_free(&v);
      return value_nil();
    }
    case EXPR_BINARY:
      return eval_binary(in, e);
    case EXPR_CALL:
      return eval_builtin_call(in, e);
    default:
      return value_nil();
  }
}

static void exec_block(Interpreter* in, Stmt* block) {
  // push env
  Env* child = env_new(in->env);
  in->env = child;

  for (int i = 0; i < block->as.block_stmt.count; i++) {
    exec_stmt(in, block->as.block_stmt.stmts[i]);
    if (in->err->had_error) break;
  }

  // pop env
  Env* parent = in->env->parent;
  env_free(in->env);
  in->env = parent;
}

static void exec_stmt(Interpreter* in, Stmt* s) {
  if (in->err->had_error) return;

  switch (s->kind) {
    case STMT_LET: {
      Value v = eval_expr(in, s->as.let_stmt.value);
      if (in->err->had_error) { value_free(&v); return; }
      env_define(in->env, s->as.let_stmt.name, v);
      return;
    }
    case STMT_ASSIGN: {
      Value v = eval_expr(in, s->as.assign_stmt.value);
      if (in->err->had_error) { value_free(&v); return; }
      if (!env_assign(in->env, s->as.assign_stmt.name, v)) {
        err_at(in->err, s->line, s->col, "undefined variable '%s'", s->as.assign_stmt.name);
      }
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
        case UI_TITLE: ui_state_set_title(in->ui, a); break;
        case UI_CLEAR: ui_state_clear(in->ui); break;
        case UI_TEXT: ui_state_add_text(in->ui, a); break;
        case UI_BUTTON: ui_state_add_button(in->ui, a); break;
        case UI_INPUT: ui_state_add_input(in->ui, a, b); break;
        case UI_SELECT: ui_state_add_select(in->ui, a, b); break;
        case UI_CHECKBOX: ui_state_add_checkbox(in->ui, a, b && strcmp(b,"true")==0); break;
        case UI_RUN: ui_state_request_run(in->ui, a); break;
      }
      return;
    }
    case STMT_BLOCK:
      exec_block(in, s);
      return;
    case STMT_IF: {
      Value c = eval_expr(in, s->as.if_stmt.cond);
      if (in->err->had_error) { value_free(&c); return; }
      bool truthy = value_is_truthy(&c);
      value_free(&c);
      if (truthy) exec_stmt(in, s->as.if_stmt.then_branch);
      else if (s->as.if_stmt.else_branch) exec_stmt(in, s->as.if_stmt.else_branch);
      return;
    }
    case STMT_WHILE: {
      while (!in->err->had_error) {
        Value c = eval_expr(in, s->as.while_stmt.cond);
        if (in->err->had_error) { value_free(&c); return; }
        bool truthy = value_is_truthy(&c);
        value_free(&c);
        if (!truthy) break;
        exec_stmt(in, s->as.while_stmt.body);
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
