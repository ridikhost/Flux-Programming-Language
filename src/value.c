#include "value.h"

Value value_nil(void) {
  Value v; v.type = VAL_NIL; return v;
}
Value value_number(double x) {
  Value v; v.type = VAL_NUMBER; v.as.number = x; return v;
}
Value value_string(const char* s) {
  Value v; v.type = VAL_STRING; v.as.str = xstrdup(s); return v;
}
Value value_string_take(char* s) {
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

char* value_to_string(const Value* v) {
  if (!v) return xstrdup("nil");
  switch (v->type) {
    case VAL_NIL: return xstrdup("nil");
    case VAL_NUMBER: {
      char buf[64];
      double x = v->as.number;
      if ((double)(int64_t)x == x) snprintf(buf, sizeof(buf), "%lld", (long long)(int64_t)x);
      else snprintf(buf, sizeof(buf), "%.15g", x);
      return xstrdup(buf);
    }
    case VAL_STRING:
      return xstrdup(v->as.str ? v->as.str : "");
    default:
      return xstrdup("<?>");
  }
}
