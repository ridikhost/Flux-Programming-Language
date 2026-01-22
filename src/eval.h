#ifndef FLUX_EVAL_H
#define FLUX_EVAL_H

#include "common.h"
#include "ast.h"
#include "value.h"
#include "flux_error.h"

typedef enum {
  UI_ITEM_TEXT = 0,
  UI_ITEM_BUTTON,
  UI_ITEM_INPUT
} UiItemKind;

typedef struct {
  UiItemKind kind;
  char* a;   // text or label
  char* b;   // for input value (owned), else NULL
} UiItem;

typedef struct {
  char* title;      // owned
  char* backend;    // owned ("tui" or "window")
  UiItem* items;
  int count;
  int cap;
  int requested_run;
} UiState;

void ui_state_init(UiState* u);
void ui_state_free(UiState* u);
void ui_state_add_text(UiState* u, const char* text);
void ui_state_add_button(UiState* u, const char* label);
void ui_state_add_input(UiState* u, const char* label, const char* value);
void ui_state_set_title(UiState* u, const char* title);
void ui_state_request_run(UiState* u, const char* backend);

// Serialize UI to JSON. Returns malloc'ed string (caller frees).
char* ui_state_to_json(const UiState* u);

// ----------------- Environment / Interpreter -----------------

typedef struct {
  char** names;
  Value* values;
  int count;
  int cap;
} Env;

void env_init(Env* e);
void env_free(Env* e);
bool env_set(Env* e, const char* name, Value v);     // takes ownership of v
bool env_get(Env* e, const char* name, Value* out);  // copies out (deep for strings)

typedef struct {
  ErrorCtx* err;
  Env env;
  UiState* ui; // borrowed
} Interpreter;

void interp_init(Interpreter* in, ErrorCtx* err, UiState* ui);
void interp_free(Interpreter* in);
bool interp_run(Interpreter* in, const Program* p);

#endif // FLUX_EVAL_H
