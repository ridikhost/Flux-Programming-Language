#include "value.h"

Value value_nil(void) {
  Value v; v.type = VAL_NIL; return v;
}

Value value_bool(bool b) {
  Value v; v.type = VAL_BOOL; v.as.boolean = b; return v;
}

Value value_number(double x) {
  Value v; v.type = VAL_NUMBER; v.as.number = x; return v;
}

Value value_string(const char* s) {
  if (!s) s = "";
  char* copy = xstrdup(s);
  Value v; v.type = VAL_STRING; v.as.str = copy; return v;
}

Value value_string_take(char* s) {
  if (!s) return value_string("");
  Value v; v.type = VAL_STRING; v.as.str = s; return v;
}

void value_free(Value* v) {
  if (!v) return;
  if (v->type == VAL_STRING && v->as.str) {
    free(v->as.str);
    v->as.str = NULL;
  }
  v->type = VAL_NIL;
}

bool value_is_truthy(const Value* v) {
  if (!v) return false;
  switch (v->type) {
    case VAL_NIL: return false;
    case VAL_BOOL: return v->as.boolean;
    case VAL_NUMBER: return v->as.number != 0.0;
    case VAL_STRING: return v->as.str && v->as.str[0] != '\0';
    default: return false;
  }
}

bool value_equal(const Value* a, const Value* b) {
  if (!a || !b) return false;
  if (a->type != b->type) return false;
  switch (a->type) {
    case VAL_NIL: return true;
    case VAL_BOOL: return a->as.boolean == b->as.boolean;
    case VAL_NUMBER: return a->as.number == b->as.number;
    case VAL_STRING: return strcmp(a->as.str ? a->as.str : "", b->as.str ? b->as.str : "") == 0;
    default: return false;
  }
}

char* value_to_string(const Value* v) {
  if (!v) return xstrdup("nil");
  char buf[64];

  switch (v->type) {
    case VAL_NIL:
      return xstrdup("nil");
    case VAL_BOOL:
      return xstrdup(v->as.boolean ? "true" : "false");
    case VAL_NUMBER: {
      // Print like C: trim trailing .0 for integers-ish
      snprintf(buf, sizeof(buf), "%.15g", v->as.number);
      return xstrdup(buf);
    }
    case VAL_STRING:
      return xstrdup(v->as.str ? v->as.str : "");
    default:
      return xstrdup("nil");
  }
}
