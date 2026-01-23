#ifndef FLUX_VALUE_H
#define FLUX_VALUE_H

#include "common.h"

typedef enum {
  VAL_NIL = 0,
  VAL_BOOL,
  VAL_NUMBER,
  VAL_STRING
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    char* str; // owned
  } as;
} Value;

Value value_nil(void);
Value value_bool(bool b);
Value value_number(double x);
Value value_string(const char* s);   // copies
Value value_string_take(char* s);    // takes ownership
void  value_free(Value* v);
char* value_to_string(const Value* v); // returns malloc'ed string

bool  value_is_truthy(const Value* v);
bool  value_equal(const Value* a, const Value* b);

#endif // FLUX_VALUE_H
