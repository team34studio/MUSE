# MUSE — Modular Unreal Studio Engine

A native in-editor **MCP (Model Context Protocol) server** for the **HELIX Studio**
modkit. It lets AI assistants — Claude Code,
Claude Desktop, Cursor, Codex — work alongside the editor: inspect and edit assets,
actors, levels, blueprints, materials and more (200+ automation actions), over a
streamable-HTTP MCP endpoint served from inside the editor.

It also adds a one-click **connect panel** (toolbar button → browser UI) to register
those agents against the server.

## Repository layout

```
MUSE/
├── MUSE/                 ← the Unreal plugin (this is what ships)
│   ├── MUSE.uplugin
│   ├── Config/
│   ├── Source/McpAutomationBridge/   (C++ editor module)
│   └── LICENSE
├── tools/
│   ├── build.bat         compile the plugin against vanilla UE 5.7
│   └── deploy.py         install into HELIX + swap BuildId
├── build/                build output (git-ignored)
└── README.md
```

## Build & deploy

Requires: **Visual Studio 2022** and **vanilla UE 5.7** (path set in `tools/build.bat`).

```bat
:: from a terminal, with HELIX closed
tools\build.bat        :: ~10-20 min the first time
python tools\deploy.py
```

Then enable the plugin in your `.uproject`:

```json
"Plugins": [ { "Name": "MUSE", "Enabled": true } ]
```

Restart HELIX. Enable the server under **Edit → Project Settings → Plugins → MUSE**
(*Enable Native MCP*). It listens on `http://127.0.0.1:3000/mcp`.

> ⚠️ Every HELIX Steam update changes the engine BuildId — just re-run
> `python tools/deploy.py` to re-align it.

## Connect an agent

Click the **MUSE** toolbar button to open the connect panel, or configure manually:

- **Claude Code:** `claude mcp add --transport http muse http://127.0.0.1:3000/mcp`
- **Cursor:** add `{ "muse": { "url": "http://127.0.0.1:3000/mcp" } }` to `~/.cursor/mcp.json`
- **Claude Desktop / Codex:** via `npx mcp-remote http://127.0.0.1:3000/mcp` (Node.js required)

## Credits

MUSE is built by Prysma Studio.
