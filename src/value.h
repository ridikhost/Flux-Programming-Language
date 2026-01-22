#ifndef FLUX_VALUE_H
#define FLUX_VALUE_H

#include "common.h"

typedef enum {
  VAL_NIL = 0,
  VAL_NUMBER,
  VAL_STRING
} ValueType;

typedef struct {
  ValueType type;
  union {
    double number;
    char* str; // owned
  } as;
} Value;

Value value_nil(void);
Value value_number(double x);
Value value_string(const char* s);   // copies
Value value_string_take(char* s);    // takes ownership
void  value_free(Value* v);
char* value_to_string(const Value* v); // returns malloc'ed string

#endif // FLUX_VALUE_H
