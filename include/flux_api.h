#ifndef FLUX_API_H
#define FLUX_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxProgram FluxProgram;

// Compile source into an executable program object.
// Returns NULL if there are lex/parse errors (printed to stderr).
FluxProgram* flux_compile(const char* filename, const char* source);

// Run compiled program. Returns 1 on success, 0 on runtime error (printed to stderr).
int flux_run(FluxProgram* program);

// If the script requested a UI via `ui run(...)`, this returns 1 and outputs:
// - out_json: malloc'ed JSON string describing UI (caller frees with flux_free_cstr)
// - out_backend: malloc'ed backend string ("tui" or "window") (caller frees with flux_free_cstr)
int flux_ui_take_request(FluxProgram* program, const char** out_json, const char** out_backend);

// Free strings returned from Flux APIs (malloc'ed).
void flux_free_cstr(const char* s);

// Free program object.
void flux_free_program(FluxProgram* program);

#ifdef __cplusplus
}
#endif

#endif // FLUX_API_H
