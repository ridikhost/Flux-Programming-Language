"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const node_1 = require("vscode-languageserver/node");
const vscode_languageserver_textdocument_1 = require("vscode-languageserver-textdocument");
const connection = (0, node_1.createConnection)(node_1.ProposedFeatures.all);
const documents = new node_1.TextDocuments(vscode_languageserver_textdocument_1.TextDocument);
connection.onInitialize(() => {
    return {
        capabilities: {
            textDocumentSync: node_1.TextDocumentSyncKind.Incremental,
        },
    };
});
documents.onDidChangeContent((change) => validate(change.document));
documents.onDidOpen((e) => validate(e.document));
function diag(line, startChar, endChar, msg, severity = node_1.DiagnosticSeverity.Error) {
    return {
        severity,
        range: {
            start: { line, character: startChar },
            end: { line, character: endChar },
        },
        message: msg,
        source: "flux",
    };
}
function countCharOutsideStrings(s, ch) {
    let inStr = false;
    let escape = false;
    let c = 0;
    for (let i = 0; i < s.length; i++) {
        const x = s[i];
        if (escape) {
            escape = false;
            continue;
        }
        if (inStr && x === "\\") {
            escape = true;
            continue;
        }
        if (x === '"')
            inStr = !inStr;
        if (!inStr && x === ch)
            c++;
    }
    return c;
}
function hasUnclosedString(line) {
    // If number of unescaped quotes is odd => unclosed string
    let quotes = 0;
    let escape = false;
    for (let i = 0; i < line.length; i++) {
        const x = line[i];
        if (escape) {
            escape = false;
            continue;
        }
        if (x === "\\") {
            escape = true;
            continue;
        }
        if (x === '"')
            quotes++;
    }
    return quotes % 2 === 1;
}
function stripComment(line) {
    // Remove // comments but only if not inside string
    let inStr = false;
    let escape = false;
    for (let i = 0; i < line.length - 1; i++) {
        const a = line[i];
        const b = line[i + 1];
        if (escape) {
            escape = false;
            continue;
        }
        if (inStr && a === "\\") {
            escape = true;
            continue;
        }
        if (a === '"')
            inStr = !inStr;
        if (!inStr && a === "/" && b === "/")
            return line.slice(0, i);
    }
    return line;
}
function validate(doc) {
    const text = doc.getText();
    const lines = text.split(/\r?\n/);
    const diags = [];
    let parenBalance = 0;
    for (let i = 0; i < lines.length; i++) {
        const raw = lines[i];
        const line = stripComment(raw);
        const trimmed = line.trim();
        if (trimmed.length === 0)
            continue;
        // Unclosed string on this line
        if (hasUnclosedString(line)) {
            diags.push(diag(i, 0, Math.max(1, raw.length), "Unclosed string literal"));
            continue;
        }
        // Track parentheses balance across file (outside strings)
        parenBalance += countCharOutsideStrings(line, "(");
        parenBalance -= countCharOutsideStrings(line, ")");
        // Flux v1.0.0 statements are expected to end with ';'
        // (UI statements also end with ';')
        if (!trimmed.endsWith(";")) {
            diags.push(diag(i, 0, Math.max(1, raw.length), "Missing ';' at end of statement"));
            continue;
        }
        // Remove trailing ';'
        const stmt = trimmed.slice(0, -1).trim();
        // let <ident> = <expr>
        if (stmt.startsWith("let ")) {
            const m = /^let\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$/.exec(stmt);
            if (!m) {
                diags.push(diag(i, 0, raw.length, "Invalid let statement. Expected: let name = expr;"));
            }
            else {
                // Very light expr sanity check
                const rhs = m[2].trim();
                if (rhs.length === 0) {
                    diags.push(diag(i, 0, raw.length, "Let statement missing expression after '='"));
                }
            }
            continue;
        }
        // print(...)
        if (stmt.startsWith("print")) {
            const m = /^print\s*\((.*)\)$/.exec(stmt);
            if (!m) {
                diags.push(diag(i, 0, raw.length, "Invalid print call. Expected: print(...);"));
            }
            continue;
        }
        // ui <cmd>(...)
        if (stmt.startsWith("ui ")) {
            const m = /^ui\s+(init|text|input|button|run)\s*\((.*)\)$/.exec(stmt);
            if (!m) {
                diags.push(diag(i, 0, raw.length, "Invalid ui statement. Expected: ui init(...); ui text(...); ui run(...);"));
            }
            continue;
        }
        // Fallback: expression statement (allow anything but warn if weird)
        // (No-op)
    }
    if (parenBalance !== 0) {
        // Put error at end of file
        const lastLine = Math.max(0, lines.length - 1);
        diags.push(diag(lastLine, 0, Math.max(1, lines[lastLine]?.length ?? 1), "Unbalanced parentheses in file"));
    }
    connection.sendDiagnostics({ uri: doc.uri, diagnostics: diags });
}
documents.listen(connection);
connection.listen();
//# sourceMappingURL=server.js.map