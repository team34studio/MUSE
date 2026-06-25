# Installation

The easiest way to set up MUSE is the **MUSE Manager** app. It does everything for you in one click.

## What you need

- **Helix Studio** installed.
- An AI assistant that supports MCP, such as **Claude Code**, **Claude Desktop**, **Cursor**, or **Codex**.

That's all for the easy path. (Building MUSE yourself is optional and only for developers — see the bottom of this page.)

## The easy way — MUSE Manager

1. Download **MUSE Manager** and open it.
2. It finds your Helix Studio automatically. If it doesn't, click **Browse** and point it to your Helix folder.
3. Click **Install**.
4. Restart Helix Studio.

That's it. MUSE is now inside Helix.

### After a Helix update

When Helix updates, MUSE may turn itself off. Just open **MUSE Manager** and click **Update** — it relinks MUSE to the new version in a second. No reinstall needed.

### The buttons

- **Install** — set MUSE up (or refresh it to the latest).
- **Update** — fix MUSE after a Helix update, or move to a newer MUSE version.
- **Deploy** — copy MUSE into Helix again without rebuilding.
- **Uninstall** — remove MUSE cleanly and undo every change it made.

## Turn it on and connect

1. Open Helix Studio with your project.
2. You'll see a **MUSE** button in the toolbar and a small **MUSE** status at the bottom of the editor.
3. Click the **MUSE** button to open the connect panel, then press **Connect** next to your assistant.

Your assistant can now work with you inside Helix.

---

## For developers — build it yourself

If you have the source code and want to build MUSE from scratch:

- **First build (clean):** `tools\build.bat` — full build, takes about 10–20 minutes.
- **While developing (fast):** `tools\build-fast.bat` — recompiles only the files you changed, so it's quick after the first run.
- **Install the build:** `python tools\deploy.py` — copies MUSE into Helix and links it to your version.

You need **Unreal Engine 5.7** and **Visual Studio 2022** (free Community edition) for building. MUSE Manager picks up your local build automatically, so you can also use its buttons during development.
