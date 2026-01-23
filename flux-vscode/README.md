# Flux Language Support

Official Visual Studio Code support for the **Flux programming language**.

Flux is an experimental programming language created by a single developer and is currently in its **early stages**.

This extension provides syntax highlighting and basic error diagnostics for **Flux v1.0.0**.

---

## Features

### Syntax Highlighting

- Flux keywords (`let`, `print`, `ui`)
- Strings, numbers, comments
- UI commands (`ui init`, `ui text`, `ui input`, `ui button`, `ui run`)

### Error Diagnostics (Early / Experimental)

This extension detects common Flux mistakes and highlights them in-editor:

- Missing `;` at end of statements
- Invalid `let` statements
- Invalid `print(...)` calls
- Invalid `ui ...(...)` commands
- Unclosed string literals
- Unbalanced parentheses

Errors appear as:

- Red squiggles
- Messages in the **Problems** panel

---

## Supported File Types

- `.fl` — Flux source files

Files with the `.fl` extension are automatically recognized as Flux.

---

## Example Flux Code

```flux
let x = 2 + 3 * 4;
print(x);

ui init("Flux Demo");
ui text("Hello from Flux!");
ui input("Name", "User");
ui button("OK");
ui run("tui");
```
