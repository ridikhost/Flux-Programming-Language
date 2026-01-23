"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const path = require("path");
const vscode = require("vscode");
const node_1 = require("vscode-languageclient/node");
let client;
function activate(context) {
    const serverModule = context.asAbsolutePath(path.join("server", "out", "server.js"));
    const serverOptions = {
        run: { module: serverModule, transport: node_1.TransportKind.ipc },
        debug: {
            module: serverModule,
            transport: node_1.TransportKind.ipc,
            options: { execArgv: ["--nolazy", "--inspect=6009"] },
        },
    };
    const clientOptions = {
        documentSelector: [{ scheme: "file", language: "flux" }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher("**/*.fl"),
        },
    };
    client = new node_1.LanguageClient("fluxLanguageServer", "Flux Language Server", serverOptions, clientOptions);
    client.start();
    context.subscriptions.push({ dispose: () => client?.stop() });
}
function deactivate() {
    return client?.stop();
}
//# sourceMappingURL=extension.js.map