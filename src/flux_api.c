#include "flux_api.h"

#include "common.h"
#include "flux_error.h"
#include "parser.h"
#include "eval.h"
#include "ast.h"

struct FluxProgram {
  Program program;
  ErrorCtx err;
  UiState ui;
  int ui_taken;
};

FluxProgram* flux_compile(const char* filename, const char* source) {
  FluxProgram* p = (FluxProgram*)xmalloc(sizeof(FluxProgram));
  memset(p, 0, sizeof(*p));

  err_init(&p->err, filename, source);
  ui_state_init(&p->ui);
  p->ui_taken = 0;

  Parser parser;
  parser_init(&parser, filename, source, &p->err);
  p->program = parse_program(&parser);

  if (p->err.had_error) {
    program_free(&p->program);
    ui_state_free(&p->ui);
    free(p);
    return NULL;
  }
  return p;
}

int flux_run(FluxProgram* prog) {
  if (!prog) return 0;

  Interpreter in;
  interp_init(&in, &prog->err, &prog->ui);
  bool ok = interp_run(&in, &prog->program);
  interp_free(&in);

  return (ok && !prog->err.had_error) ? 1 : 0;
}

int flux_ui_take_request(FluxProgram* program, const char** out_json, const char** out_backend) {
  if (!program || !out_json || !out_backend) return 0;
  if (program->ui_taken) return 0;
  if (!program->ui.requested_run) return 0;

  char* json = ui_state_to_json(&program->ui);
  char* backend = xstrdup(program->ui.backend ? program->ui.backend : "tui");

  *out_json = json;
  *out_backend = backend;

  program->ui_taken = 1;
  return 1;
}

void flux_free_cstr(const char* s) {
  if (!s) return;
  free((void*)s);
}

void flux_free_program(FluxProgram* prog) {
  if (!prog) return;
  program_free(&prog->program);
  ui_state_free(&prog->ui);
  free(prog);
}
