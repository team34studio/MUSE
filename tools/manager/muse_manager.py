"""
MUSE Manager — one-click installer/updater for the MUSE plugin in HELIX Studio.

Works in two modes from the same code:
  * Developer mode  — run from the source tree (D:\\HELIX\\MUSE). If UE 5.7 is
    detected, the "Build" actions compile the plugin from source.
  * Prebuilt mode   — run as the packaged .exe. The plugin binaries are shipped
    alongside the exe, so end users only deploy + apply the BuildId workaround;
    no engine or source is required.

Actions:
  * Install   — build (if possible) + deploy + enable + BuildId workaround
  * Build     — compile only (hidden when no engine is available)
  * Deploy    — copy the plugin + BuildId workaround (no compile)
  * Update    — smart: fix the BuildId after a HELIX update, or push a newer build
  * Uninstall — remove the plugin and clean the files the workaround touched
"""

import json
import os
import queue
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

APP_NAME = "MUSE Manager"
PLUGIN_NAME = "MUSE"
PLUGIN_MODULE = "McpAutomationBridge"
MODULE_DLL = "UnrealEditor-McpAutomationBridge.dll"
MODULES_REL = os.path.join("Binaries", "Win64", "UnrealEditor.modules")

# ----------------------------------------------------------------------------
# Paths & environment detection
# ----------------------------------------------------------------------------

def app_dir():
    """Folder the script/exe lives in."""
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def bundle_dir():
    """Folder where bundled data (prebuilt plugin) is extracted/located."""
    if getattr(sys, "frozen", False):
        return getattr(sys, "_MEIPASS", app_dir())
    return app_dir()


def source_root():
    """The MUSE repo root if we're running inside the source tree, else None.

    Works both when run as a script (tools/manager/muse_manager.py -> root is two
    levels up) and when run as a frozen exe sitting in the tree (e.g. dist/), where
    __file__ points into a temp dir so we walk up from the exe location instead.
    """
    cands = []
    here = os.path.dirname(os.path.abspath(__file__))
    cands.append(os.path.dirname(os.path.dirname(here)))   # script: manager -> tools -> root
    if getattr(sys, "frozen", False):
        exe = os.path.dirname(sys.executable)
        # walk a few levels up from the exe (covers dist/, tools/manager/, root, ...)
        p = exe
        for _ in range(4):
            cands.append(p)
            p = os.path.dirname(p)
    for root in cands:
        if root and os.path.isfile(os.path.join(root, PLUGIN_NAME, f"{PLUGIN_NAME}.uplugin")):
            return root
    return None


def config_path():
    base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
    folder = os.path.join(base, "MUSE")
    os.makedirs(folder, exist_ok=True)
    return os.path.join(folder, "manager.json")


def load_config():
    try:
        with open(config_path(), "r", encoding="utf-8") as fh:
            return json.load(fh)
    except Exception:
        return {}


def save_config(cfg):
    try:
        with open(config_path(), "w", encoding="utf-8") as fh:
            json.dump(cfg, fh, indent=2)
    except Exception:
        pass


def steam_libraries():
    """All Steam library roots, parsed from libraryfolders.vdf (best effort)."""
    candidates = [
        r"C:\Program Files (x86)\Steam",
        r"C:\Program Files\Steam",
    ]
    libs = []
    for steam in candidates:
        vdf = os.path.join(steam, "steamapps", "libraryfolders.vdf")
        if not os.path.isfile(vdf):
            continue
        libs.append(steam)
        try:
            with open(vdf, "r", encoding="utf-8", errors="ignore") as fh:
                for line in fh:
                    line = line.strip()
                    # lines look like:  "path"   "D:\\SteamLibrary"
                    if '"path"' in line:
                        parts = line.split('"')
                        if len(parts) >= 4:
                            libs.append(parts[3].replace("\\\\", "\\"))
        except Exception:
            pass
    return libs


def detect_helix():
    """Find the HELIX Studio install folder (the one containing Engine\\)."""
    for lib in steam_libraries():
        path = os.path.join(lib, "steamapps", "common", "HELIX Studio")
        if os.path.isdir(os.path.join(path, "Engine")):
            return path
    # common manual location seen in the wild
    fallback = r"D:\SteamLibrary\steamapps\common\HELIX Studio"
    if os.path.isdir(os.path.join(fallback, "Engine")):
        return fallback
    return ""


def detect_engine():
    """Find a vanilla UE 5.7 install usable for building (dev only)."""
    for path in (r"E:\UE\UE_5.7", r"C:\Program Files\Epic Games\UE_5.7"):
        if os.path.isfile(os.path.join(path, "Engine", "Build", "BatchFiles", "Build.bat")):
            return path
    return ""


def detect_project():
    """Best-effort default project that should enable MUSE."""
    for path in (r"D:\HELIX\Projects\CC2\CC2.uproject",):
        if os.path.isfile(path):
            return path
    return ""


def discover_projects(cfg):
    """All known .uproject files: ones the user added + ones found by scanning."""
    found = {}
    for p in cfg.get("projects", []):
        if os.path.isfile(p):
            found[os.path.normpath(p)] = True
    roots = [r"D:\HELIX\Projects"]
    dp = cfg.get("project") or detect_project()
    if dp:
        roots.append(os.path.dirname(os.path.dirname(dp)))
    for root in roots:
        if not os.path.isdir(root):
            continue
        try:
            for name in os.listdir(root):
                sub = os.path.join(root, name)
                if not os.path.isdir(sub):
                    continue
                for f in os.listdir(sub):
                    if f.lower().endswith(".uproject"):
                        found[os.path.normpath(os.path.join(sub, f))] = True
        except OSError:
            pass
    return sorted(found.keys(), key=lambda p: os.path.basename(p).lower())


def project_muse_on(uproject):
    """Effective state. The engine plugin is EnabledByDefault, so MUSE is ON in a
    project unless that project explicitly turns it off."""
    try:
        with open(uproject, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        for p in data.get("Plugins", []):
            if p.get("Name") == PLUGIN_NAME:
                return bool(p.get("Enabled", True))
        return True  # no entry -> follows the engine default (on)
    except Exception:
        return True


def set_project_enabled(uproject, enabled):
    """Force MUSE on/off for one project. Returns True if the file changed.

    On  -> drop any explicit entry so the project follows the engine default (on).
    Off -> write an explicit {"Name": "MUSE", "Enabled": false}.
    """
    with open(uproject, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    plugins = data.get("Plugins", [])
    entry = next((p for p in plugins if p.get("Name") == PLUGIN_NAME), None)
    changed = False
    if enabled:
        if entry is not None:
            plugins = [p for p in plugins if p.get("Name") != PLUGIN_NAME]
            changed = True
    else:
        if entry is None:
            plugins.append({"Name": PLUGIN_NAME, "Enabled": False})
            changed = True
        elif entry.get("Enabled", True) is not False:
            entry["Enabled"] = False
            changed = True
    if not changed:
        return False
    if plugins:
        data["Plugins"] = plugins
    else:
        data.pop("Plugins", None)
    with open(uproject, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent="\t")
    return True


# ----------------------------------------------------------------------------
# Plugin payload: assembled from a source skeleton + a freshly built binary
# ----------------------------------------------------------------------------

def plugin_skeleton():
    """The plugin source folder (Source/Config/.uplugin) — source tree or bundle."""
    cands = []
    root = source_root()
    if root:
        cands.append(os.path.join(root, PLUGIN_NAME))
    cands.append(os.path.join(bundle_dir(), PLUGIN_NAME))   # prebuilt next to exe
    for c in cands:
        if os.path.isfile(os.path.join(c, f"{PLUGIN_NAME}.uplugin")):
            return c
    return None


def find_binaries_dir():
    """The Binaries/Win64 folder holding the freshly built module DLL (newest wins)."""
    cands = []
    root = source_root()
    if root:
        cands.append(os.path.join(root, ".dev", "HostProject", "Binaries", "Win64"))  # build-fast.bat
        cands.append(os.path.join(root, PLUGIN_NAME, "Binaries", "Win64"))             # standalone
        cands.append(os.path.join(root, "build", PLUGIN_NAME, "Binaries", "Win64"))    # RunUAT
        cands.append(os.path.join(root, "build", PLUGIN_NAME, "HostProject", "Plugins",
                                  PLUGIN_NAME, "Binaries", "Win64"))
    cands.append(os.path.join(bundle_dir(), PLUGIN_NAME, "Binaries", "Win64"))          # prebuilt
    found = []
    for c in cands:
        dll = os.path.join(c, MODULE_DLL)
        if os.path.isfile(dll):
            found.append((os.path.getmtime(dll), c))
    if not found:
        return None
    found.sort(reverse=True)
    return found[0][1]


def read_buildid(modules_file):
    with open(modules_file, "r", encoding="utf-8") as fh:
        return json.load(fh).get("BuildId")


def engine_buildid(helix):
    return read_buildid(os.path.join(helix, "Engine", "Binaries", "Win64", "UnrealEditor.modules"))


def installed_dir(helix):
    return os.path.join(helix, "Engine", "Plugins", PLUGIN_NAME)


def legacy_dir(helix):
    return os.path.join(helix, "Engine", "Plugins", "McpAutomationBridge")


# ----------------------------------------------------------------------------
# Core operations (run on a worker thread; `log` is a thread-safe callback)
# ----------------------------------------------------------------------------

class OpError(Exception):
    pass


def op_build(ctx, log):
    """Compile the plugin from source (developer mode only)."""
    root = source_root()
    if not root:
        raise OpError("Build needs the MUSE source tree — not available in this copy.")
    if not ctx.get("engine"):
        raise OpError("Build needs UE 5.7. Set the engine path or use a prebuilt copy.")
    script = os.path.join(root, "tools", "build-fast.bat")
    if not os.path.isfile(script):
        raise OpError(f"Build script not found: {script}")
    log(f"Building from source (first run is slow, later runs are incremental)...")
    rc = run_stream([script], log, cwd=os.path.join(root, "tools"),
                    env_extra={"ENGINE": ctx["engine"]})
    if rc != 0:
        raise OpError(f"Build failed (exit {rc}). See the log above.")
    log("[OK] Build finished.")


def op_deploy(ctx, log):
    """Assemble the plugin into HELIX (source skeleton + built binary) + BuildId workaround."""
    helix = ctx["helix"]
    skeleton = plugin_skeleton()
    bindir = find_binaries_dir()
    if not skeleton:
        raise OpError("Plugin source folder not found.")
    if not bindir:
        raise OpError("No built module binary found. Build first, "
                      "or use a copy that ships prebuilt binaries.")
    log(f"Skeleton: {skeleton}")
    log(f"Binaries: {bindir}")
    target = installed_dir(helix)

    for old in (target, legacy_dir(helix)):
        if os.path.isdir(old):
            shutil.rmtree(old, ignore_errors=True)
            log(f"Removed old: {old}")

    # 1. Source skeleton (no build artifacts).
    shutil.copytree(skeleton, target,
                    ignore=shutil.ignore_patterns("Binaries", "Intermediate", "HostProject"))

    # 2. Built module binary (and its .pdb if present).
    out_bin = os.path.join(target, "Binaries", "Win64")
    os.makedirs(out_bin, exist_ok=True)
    shutil.copy2(os.path.join(bindir, MODULE_DLL), os.path.join(out_bin, MODULE_DLL))
    pdb = MODULE_DLL.replace(".dll", ".pdb")
    if os.path.isfile(os.path.join(bindir, pdb)):
        shutil.copy2(os.path.join(bindir, pdb), os.path.join(out_bin, pdb))

    # 3. Clean module manifest for just this plugin, with HELIX's BuildId.
    want = engine_buildid(helix)
    manifest = {"BuildId": want, "Modules": {PLUGIN_MODULE: MODULE_DLL}}
    with open(os.path.join(out_bin, "UnrealEditor.modules"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent="\t")
    log(f"BuildId -> {want}")
    log("[OK] Deployed.")


def op_enable_project(ctx, log):
    """Ensure the project .uproject enables MUSE (optional; engine default also enables it)."""
    proj = ctx.get("project")
    if not proj or not os.path.isfile(proj):
        log("No project set — skipping project enable (engine-level default still enables MUSE).")
        return
    with open(proj, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    plugins = data.setdefault("Plugins", [])
    if any(p.get("Name") == PLUGIN_NAME for p in plugins):
        log(f"Project already enables MUSE: {proj}")
        return
    plugins.append({"Name": PLUGIN_NAME, "Enabled": True})
    with open(proj, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent="\t")
    log(f"Enabled MUSE in project: {proj}")


def op_fix_buildid(ctx, log):
    """Re-swap the BuildId of the already-installed plugin to match the engine."""
    helix = ctx["helix"]
    modules = os.path.join(installed_dir(helix), MODULES_REL)
    if not os.path.isfile(modules):
        raise OpError("MUSE is not installed yet — use Install first.")
    want = engine_buildid(helix)
    have = read_buildid(modules)
    if have == want:
        log(f"BuildId already matches ({want}). Nothing to fix.")
        return False
    with open(modules, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    data["BuildId"] = want
    with open(modules, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=1)
    log(f"Fixed BuildId {have} -> {want}")
    return True


def op_install(ctx, log):
    if source_root() and ctx.get("engine"):
        op_build(ctx, log)
    else:
        log("No engine/source — installing the prebuilt plugin.")
    op_deploy(ctx, log)
    op_enable_project(ctx, log)
    log("\nDone. Restart HELIX Studio. MCP server: http://127.0.0.1:3000/mcp")


def op_update(ctx, log):
    """Smart: if only the BuildId drifted (after a HELIX update) fix it fast;
    otherwise push the newest local/bundled build."""
    helix = ctx["helix"]
    target_modules = os.path.join(installed_dir(helix), MODULES_REL)
    if not os.path.isfile(target_modules):
        log("MUSE not installed — running a full install.")
        op_install(ctx, log)
        return

    # Is there a newer build available than what's deployed?
    bindir = find_binaries_dir()
    newer = False
    if bindir:
        src_dll = os.path.join(bindir, MODULE_DLL)
        dst_dll = os.path.join(installed_dir(helix), "Binaries", "Win64", MODULE_DLL)
        if os.path.isfile(src_dll) and os.path.isfile(dst_dll):
            newer = os.path.getmtime(src_dll) > os.path.getmtime(dst_dll) + 1

    if newer:
        log("A newer build is available — redeploying.")
        op_install(ctx, log)
        return

    log("No new version — checking the BuildId (HELIX-update fix).")
    fixed = op_fix_buildid(ctx, log)
    if fixed:
        log("\nDone. Restart HELIX Studio to re-enable MUSE.")
    else:
        log("\nMUSE is already up to date and matches the current HELIX build.")


def op_uninstall(ctx, log):
    helix = ctx["helix"]
    removed = False
    for d in (installed_dir(helix), legacy_dir(helix)):
        if os.path.isdir(d):
            shutil.rmtree(d, ignore_errors=True)
            log(f"Removed plugin: {d}")
            removed = True
    # Clean the project entry we added, so HELIX doesn't warn about a missing plugin.
    proj = ctx.get("project")
    if proj and os.path.isfile(proj):
        try:
            with open(proj, "r", encoding="utf-8") as fh:
                data = json.load(fh)
            plugins = data.get("Plugins", [])
            kept = [p for p in plugins if p.get("Name") != PLUGIN_NAME]
            if len(kept) != len(plugins):
                if kept:
                    data["Plugins"] = kept
                else:
                    data.pop("Plugins", None)
                with open(proj, "w", encoding="utf-8") as fh:
                    json.dump(data, fh, indent="\t")
                log(f"Removed MUSE entry from project: {proj}")
                removed = True
        except Exception as e:
            log(f"Could not clean project file: {e}")
    if not removed:
        log("Nothing to remove — MUSE was not installed.")
    else:
        log("\nDone. MUSE has been removed. Restart HELIX Studio.")


# ----------------------------------------------------------------------------
# Subprocess streaming
# ----------------------------------------------------------------------------

def run_stream(cmd, log, cwd=None, env_extra=None):
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)
    flags = 0
    if os.name == "nt":
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    proc = subprocess.Popen(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, bufsize=1,
                            creationflags=flags)
    for line in proc.stdout:
        log(line.rstrip("\n"))
    proc.wait()
    return proc.returncode


# ----------------------------------------------------------------------------
# GUI
# ----------------------------------------------------------------------------

BG = "#111111"
PANEL = "#181a21"
FG = "#e7e9ee"
MUTED = "#8b90a0"
ACCENT = "#6c5ce7"
OK = "#39d98a"
WARN = "#ffb454"
ERR = "#ff6b6b"


class ManagerApp:
    def __init__(self, root):
        self.root = root
        self.cfg = load_config()
        self.q = queue.Queue()
        self.busy = False

        root.title(APP_NAME)
        root.configure(bg=BG)
        root.geometry("720x560")
        root.minsize(660, 500)

        self.helix = tk.StringVar(value=self.cfg.get("helix") or detect_helix())
        self.engine = tk.StringVar(value=self.cfg.get("engine") or detect_engine())
        self.project = tk.StringVar(value=self.cfg.get("project") or detect_project())

        self._build_ui()
        self._refresh_status()
        self._refresh_projects()
        self.root.after(80, self._drain_log)

    # ---- layout -------------------------------------------------------------
    def _apply_style(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Treeview", background="#10121a", fieldbackground="#10121a",
                        foreground=FG, rowheight=26, borderwidth=0)
        style.configure("Treeview.Heading", background="#1d2029", foreground=MUTED,
                        relief="flat", font=("Segoe UI", 9))
        style.map("Treeview", background=[("selected", ACCENT)],
                  foreground=[("selected", "#ffffff")])
        style.configure("TNotebook", background=BG, borderwidth=0)
        style.configure("TNotebook.Tab", background="#1d2029", foreground=MUTED,
                        padding=(18, 8), font=("Segoe UI Semibold", 9))
        style.map("TNotebook.Tab", background=[("selected", PANEL)],
                  foreground=[("selected", FG)])

    def _build_ui(self):
        self._apply_style()

        header = tk.Frame(self.root, bg=BG)
        header.pack(fill="x", padx=20, pady=(18, 4))
        tk.Label(header, text="MUSE", bg=BG, fg=FG,
                 font=("Segoe UI Semibold", 22)).pack(side="left")
        tk.Label(header, text="  Manager", bg=BG, fg=ACCENT,
                 font=("Segoe UI", 22)).pack(side="left")
        self.status_lbl = tk.Label(header, text="", bg=BG, fg=MUTED,
                                    font=("Segoe UI", 10))
        self.status_lbl.pack(side="right")

        nb = ttk.Notebook(self.root)
        nb.pack(fill="both", expand=True, padx=12, pady=(4, 12))
        tab_setup = tk.Frame(nb, bg=BG)
        tab_projects = tk.Frame(nb, bg=BG)
        nb.add(tab_setup, text="  Setup  ")
        nb.add(tab_projects, text="  Projects  ")
        nb.bind("<<NotebookTabChanged>>", lambda e: self._refresh_projects())

        self._build_setup(tab_setup)
        self._build_projects(tab_projects)

    def _build_setup(self, parent):
        # paths panel
        paths = tk.Frame(parent, bg=PANEL)
        paths.pack(fill="x", padx=8, pady=8)
        self._path_row(paths, "HELIX Studio", self.helix, self._pick_helix, 0)
        self._path_row(paths, "UE 5.7 engine (build, optional)", self.engine, self._pick_engine, 1)
        self._path_row(paths, "Default project (optional)", self.project, self._pick_project, 2)
        paths.grid_columnconfigure(1, weight=1)

        # actions
        actions = tk.Frame(parent, bg=BG)
        actions.pack(fill="x", padx=8, pady=(6, 4))
        self.btn_install = self._action(actions, "Install", self.on_install, primary=True)
        self.btn_update = self._action(actions, "Update", self.on_update)
        self.btn_build = self._action(actions, "Build", self.on_build)
        self.btn_deploy = self._action(actions, "Deploy", self.on_deploy)
        self.btn_uninstall = self._action(actions, "Uninstall", self.on_uninstall, danger=True)

        # log
        logwrap = tk.Frame(parent, bg=PANEL)
        logwrap.pack(fill="both", expand=True, padx=8, pady=(8, 8))
        self.log = tk.Text(logwrap, bg="#0b0c10", fg=FG, insertbackground=FG,
                           relief="flat", wrap="word", font=("Consolas", 9),
                           padx=12, pady=10)
        self.log.pack(side="left", fill="both", expand=True)
        sb = ttk.Scrollbar(logwrap, command=self.log.yview)
        sb.pack(side="right", fill="y")
        self.log.configure(yscrollcommand=sb.set, state="disabled")
        for tag, col in (("ok", OK), ("warn", WARN), ("err", ERR), ("muted", MUTED)):
            self.log.tag_configure(tag, foreground=col)

    def _build_projects(self, parent):
        top = tk.Frame(parent, bg=BG)
        top.pack(fill="x", padx=8, pady=(12, 2))
        tk.Label(top, text="Your HELIX projects", bg=BG, fg=MUTED,
                 font=("Segoe UI", 10)).pack(side="left")
        tk.Button(top, text="Refresh", command=self._refresh_projects, bg="#262a36",
                  fg=FG, relief="flat", padx=10, cursor="hand2",
                  activebackground="#323848", activeforeground=FG).pack(side="right")
        tk.Button(top, text="Add…", command=self._add_project, bg="#262a36",
                  fg=FG, relief="flat", padx=10, cursor="hand2",
                  activebackground="#323848", activeforeground=FG).pack(side="right", padx=(0, 8))

        tk.Label(parent, text="MUSE is on in every project by default. Use this to turn it "
                 "off where you don't want it (or back on).", bg=BG, fg=MUTED,
                 font=("Segoe UI", 8), anchor="w", justify="left").pack(
                     fill="x", padx=8, pady=(0, 6))

        wrap = tk.Frame(parent, bg=PANEL)
        wrap.pack(fill="both", expand=True, padx=8, pady=6)
        self.tree = ttk.Treeview(wrap, columns=("status", "path"), show="tree headings")
        self.tree.heading("#0", text="Project")
        self.tree.heading("status", text="MUSE")
        self.tree.heading("path", text="Path")
        self.tree.column("#0", width=150, stretch=False)
        self.tree.column("status", width=90, anchor="center", stretch=False)
        self.tree.column("path", width=380)
        self.tree.tag_configure("on", foreground=OK)
        self.tree.tag_configure("off", foreground=MUTED)
        self.tree.pack(side="left", fill="both", expand=True)
        sb = ttk.Scrollbar(wrap, command=self.tree.yview)
        sb.pack(side="right", fill="y")
        self.tree.configure(yscrollcommand=sb.set)
        self._tree_paths = {}

        btns = tk.Frame(parent, bg=BG)
        btns.pack(fill="x", padx=8, pady=(4, 12))
        self._action(btns, "Enable here", self._enable_selected, primary=True)
        self._action(btns, "Disable here", self._disable_selected)
        self._action(btns, "Enable in all", self._enable_all)
        self._action(btns, "Disable in all", self._disable_all)

    def _path_row(self, parent, label, var, cmd, row):
        tk.Label(parent, text=label, bg=PANEL, fg=MUTED, font=("Segoe UI", 9),
                 width=28, anchor="w").grid(row=row, column=0, sticky="w", padx=(12, 6), pady=6)
        tk.Entry(parent, textvariable=var, bg="#10121a", fg=FG, relief="flat",
                 insertbackground=FG, font=("Consolas", 9)).grid(
                     row=row, column=1, sticky="ew", pady=6)
        tk.Button(parent, text="Browse", command=cmd, bg="#262a36", fg=FG,
                  relief="flat", padx=10, cursor="hand2",
                  activebackground="#323848", activeforeground=FG).grid(
                      row=row, column=2, padx=(6, 12), pady=6)

    def _action(self, parent, text, cmd, primary=False, danger=False):
        bg = ACCENT if primary else ("#3a2330" if danger else "#262a36")
        fg = "#ffffff" if primary else (ERR if danger else FG)
        b = tk.Button(parent, text=text, command=cmd, bg=bg, fg=fg, relief="flat",
                      padx=16, pady=10, cursor="hand2", font=("Segoe UI Semibold", 10),
                      activebackground=bg, activeforeground=fg)
        b.pack(side="left", padx=(0, 8))
        return b

    # ---- pickers ------------------------------------------------------------
    def _pick_dir(self, var, title):
        d = filedialog.askdirectory(title=title, initialdir=var.get() or os.path.expanduser("~"))
        if d:
            var.set(os.path.normpath(d))
            self._save()
            self._refresh_status()

    def _pick_helix(self):
        self._pick_dir(self.helix, "Select the HELIX Studio folder (contains Engine)")

    def _pick_engine(self):
        self._pick_dir(self.engine, "Select the UE 5.7 engine root")

    def _pick_project(self):
        f = filedialog.askopenfilename(title="Select a .uproject", filetypes=[("Unreal project", "*.uproject")],
                                       initialdir=os.path.dirname(self.project.get()) if self.project.get() else os.path.expanduser("~"))
        if f:
            self.project.set(os.path.normpath(f))
            self._save()
            self._refresh_status()

    def _save(self):
        self.cfg.update(helix=self.helix.get(), engine=self.engine.get(), project=self.project.get())
        save_config(self.cfg)

    # ---- status -------------------------------------------------------------
    def _refresh_status(self):
        # show/hide Build depending on engine + source availability
        can_build = bool(source_root()) and bool(self.engine.get()) and \
            os.path.isfile(os.path.join(self.engine.get(), "Engine", "Build", "BatchFiles", "Build.bat"))
        self.btn_build.pack_forget()
        if can_build:
            self.btn_build.pack(side="left", padx=(0, 8), after=self.btn_update)

        helix = self.helix.get()
        if not helix or not os.path.isdir(os.path.join(helix, "Engine")):
            self._set_status("HELIX not found — set the path", WARN)
            return
        modules = os.path.join(installed_dir(helix), MODULES_REL)
        if not os.path.isfile(modules):
            self._set_status("MUSE not installed", MUTED)
            return
        try:
            match = read_buildid(modules) == engine_buildid(helix)
        except Exception:
            match = False
        if match:
            self._set_status("MUSE installed — OK", OK)
        else:
            self._set_status("MUSE installed — BuildId outdated (run Update)", WARN)

    def _set_status(self, text, color):
        self.status_lbl.configure(text="● " + text, fg=color)

    # ---- logging ------------------------------------------------------------
    def log_line(self, msg):
        self.q.put(msg)

    def _drain_log(self):
        try:
            while True:
                msg = self.q.get_nowait()
                tag = ""
                low = msg.lower()
                if msg.startswith("[OK]") or "done." in low:
                    tag = "ok"
                elif "[fail]" in low or "error" in low or "failed" in low:
                    tag = "err"
                elif msg.startswith("●") or "skipping" in low or "no " in low[:4]:
                    tag = "muted"
                self.log.configure(state="normal")
                self.log.insert("end", msg + "\n", tag)
                self.log.see("end")
                self.log.configure(state="disabled")
        except queue.Empty:
            pass
        self.root.after(80, self._drain_log)

    # ---- run actions on a worker thread ------------------------------------
    def _ctx(self):
        return {"helix": self.helix.get(), "engine": self.engine.get(),
                "project": self.project.get()}

    def _guard(self):
        if self.busy:
            return False
        helix = self.helix.get()
        if not helix or not os.path.isdir(os.path.join(helix, "Engine")):
            messagebox.showwarning(APP_NAME, "Set a valid HELIX Studio folder first.")
            return False
        return True

    def _run(self, fn, title):
        if not self._guard():
            return
        self._save()
        self.busy = True
        self._set_buttons("disabled")
        self.log.configure(state="normal")
        self.log.insert("end", f"\n=== {title} ===\n", "muted")
        self.log.configure(state="disabled")

        def worker():
            try:
                fn(self._ctx(), self.log_line)
            except OpError as e:
                self.log_line(f"[FAIL] {e}")
            except Exception as e:
                self.log_line(f"[FAIL] Unexpected error: {e}")
            finally:
                self.root.after(0, self._done)

        threading.Thread(target=worker, daemon=True).start()

    def _done(self):
        self.busy = False
        self._set_buttons("normal")
        self._refresh_status()

    def _set_buttons(self, state):
        for b in (self.btn_install, self.btn_update, self.btn_build,
                  self.btn_deploy, self.btn_uninstall):
            b.configure(state=state)

    # ---- button handlers ----------------------------------------------------
    def on_install(self):
        self._run(op_install, "Install")

    def on_update(self):
        self._run(op_update, "Update")

    def on_build(self):
        self._run(op_build, "Build")

    def on_deploy(self):
        self._run(op_deploy, "Deploy")

    def on_uninstall(self):
        if messagebox.askyesno(APP_NAME, "Remove MUSE from HELIX Studio?"):
            self._run(op_uninstall, "Uninstall")

    # ---- projects tab -------------------------------------------------------
    def _refresh_projects(self):
        if not hasattr(self, "tree"):
            return
        self.tree.delete(*self.tree.get_children())
        self._tree_paths = {}
        for p in discover_projects(self.cfg):
            on = project_muse_on(p)
            iid = self.tree.insert(
                "", "end", text=os.path.splitext(os.path.basename(p))[0],
                values=("On" if on else "Off", p),
                tags=("on" if on else "off",))
            self._tree_paths[iid] = p

    def _add_project(self):
        f = filedialog.askopenfilename(title="Select a .uproject",
                                       filetypes=[("Unreal project", "*.uproject")])
        if not f:
            return
        f = os.path.normpath(f)
        lst = self.cfg.setdefault("projects", [])
        if f not in lst:
            lst.append(f)
            save_config(self.cfg)
        self._refresh_projects()

    def _selected_paths(self):
        return [self._tree_paths[i] for i in self.tree.selection() if i in self._tree_paths]

    def _apply_project(self, paths, enabled):
        if not paths:
            messagebox.showinfo(APP_NAME, "Select a project in the list first.")
            return
        verb = "On" if enabled else "Off"
        changed = 0
        for p in paths:
            try:
                if set_project_enabled(p, enabled):
                    changed += 1
                    self.log_line(f"MUSE -> {verb} in {os.path.basename(p)}")
            except Exception as e:
                self.log_line(f"[FAIL] {os.path.basename(p)}: {e}")
        if changed == 0:
            self.log_line(f"No change — already {verb.lower()}.")
        self._refresh_projects()

    def _enable_selected(self):
        self._apply_project(self._selected_paths(), True)

    def _disable_selected(self):
        self._apply_project(self._selected_paths(), False)

    def _enable_all(self):
        if messagebox.askyesno(APP_NAME, "Turn MUSE ON in ALL listed projects?"):
            self._apply_project(list(self._tree_paths.values()), True)

    def _disable_all(self):
        if messagebox.askyesno(APP_NAME, "Turn MUSE OFF in ALL listed projects?"):
            self._apply_project(list(self._tree_paths.values()), False)


def main():
    root = tk.Tk()
    ManagerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
