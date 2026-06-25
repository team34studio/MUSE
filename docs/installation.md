# Installation

Setting MUSE up takes a few minutes, and you only do it once.

## What you need

- **Helix Studio** installed.
- **Unreal Engine 5.7** — free, from the Epic Games Launcher.
- **Visual Studio 2022** — the free Community edition is fine.
- An AI assistant that supports MCP, such as **Claude Code**, **Claude Desktop**, **Cursor**, or **Codex**.

## Step 1 — Build MUSE

Close Helix Studio, then run:

```
tools\build.bat
```

This prepares MUSE for your system. The first time takes about 10–20 minutes.

## Step 2 — Install it into Helix

```
python tools\deploy.py
```

This copies MUSE into Helix and links it to your version.

> Each time Helix updates, run this step again to re-link MUSE.

## Step 3 — Turn it on

1. Open Helix Studio with your project.
2. Open **Edit → Plugins**, find **MUSE**, and make sure it's enabled.
3. Open **Edit → Project Settings → Plugins → MUSE** and turn on **Enable Native MCP**.

You'll now see a **MUSE** button in the toolbar and a small **MUSE** status at the bottom of the editor.

## Step 4 — Connect your assistant

Click the **MUSE** button in the toolbar to open the connect panel, then press **Connect** next to your assistant.

That's it — your assistant can now work with you inside Helix.
