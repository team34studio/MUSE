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

## Installation

> HELIX Studio is a custom UE 5.7.4 build with the engine headers stripped, so the
> plugin **cannot** be compiled against it directly. You compile once against a normal
> (vanilla) UE 5.7 install, then `deploy.py` copies the result into HELIX and rewrites
> the binary's `BuildId` so HELIX accepts it. This is a one-time ~15 min setup.

### 1. Prerequisites

| Need | How to get it |
|------|----------------|
| **This repository** | Clone this repo anywhere on your pc |
| **HELIX Studio** | Installed via Steam (the modkit you're targeting). |
| **Visual Studio 2022** | Community edition is fine. Install the **"Game development with C++"** workload. |
| **Vanilla Unreal Engine 5.7** | Epic Games Launcher → Unreal Engine → install **5.7** (any 5.7.x patch). Needed only to compile. |
| **Python 3** | For `deploy.py` (`python --version` should work). |

### 2. Point the scripts at your machine

The two scripts use absolute paths — edit them if yours differ:

- **`tools/build.bat`** → `set "ENGINE=E:\UE\UE_5.7"` — set to your **vanilla UE 5.7** folder.
- **`tools/deploy.py`** → `HELIX_ENGINE = r"...\HELIX Studio\Engine"` — set to your **HELIX Studio** `Engine` folder.

### 3. Build the plugin

Close HELIX, then from this repo's folder run:

```bat
tools\build.bat
```

First build takes ~10–20 min. Wait for **`[OK] Build succeeded`**. The compiled plugin
lands in `build\MUSE\`.

### 4. Deploy into HELIX

```bat
python tools\deploy.py
```

This copies the plugin to `…\HELIX Studio\Engine\Plugins\MUSE\`, removes any old copy,
and swaps the `BuildId` to match your current HELIX build.

### 5. Enable it in your project

Open your project's `.uproject` and add MUSE to the plugins list:

```json
"Plugins": [
    { "Name": "MUSE", "Enabled": true }
]
```

### 6. Launch & turn on the server

1. Open HELIX with your project.
2. Go to **Edit → Project Settings → Plugins → MUSE** and tick **Enable Native MCP**.
3. The status bar (bottom of the editor) should show **`● MUSE :3000`**, and the
   **MUSE** button appears in the toolbar.

The MCP endpoint is now live at **`http://127.0.0.1:3000/mcp`**.

### 7. Connect an agent

Click the **MUSE** toolbar button (opens the connect panel) and hit **Connect** next to
your agent — or configure it manually (see *Connect an agent* below).

---

### ⚠️ After every HELIX update

Steam updates change HELIX's `BuildId`, which makes the plugin show up disabled
("modules built with a different engine version"). Fix = re-run the deploy:

```bat
python tools\deploy.py
```

(No rebuild needed unless HELIX jumps to a different engine version.)

### Troubleshooting

- **"The following modules are missing or built with a different engine version"** →
  BuildId mismatch. Run `python tools\deploy.py`, make sure MUSE is enabled in your
  `.uproject`, and fully restart HELIX.
- **Plugin checkbox is greyed / can't enable** → same cause; the deploy + a clean
  restart fixes it.
- **Status bar shows `MUSE off`** → enable **Native MCP** in Project Settings (step 6).
- **Port 3000 busy** → change **Native MCP Port** in Project Settings; update your
  agents' URLs to match.

## Connect an agent

Click the **MUSE** toolbar button to open the connect panel, or configure manually:

- **Claude Code:** `claude mcp add --transport http muse http://127.0.0.1:3000/mcp`
- **Cursor:** add `{ "muse": { "url": "http://127.0.0.1:3000/mcp" } }` to `~/.cursor/mcp.json`
- **Claude Desktop / Codex:** via `npx mcp-remote http://127.0.0.1:3000/mcp` (Node.js required)

## Credits

MUSE is built by Prysma Studio.
