"""
Deploy the built MUSE plugin into HELIX Studio and swap its BuildId so the
custom build will load it.

The plugin is named MUSE; its C++ module/DLL stays McpAutomationBridge (internal).

The plugin is assembled from two places:
  * the canonical source skeleton (this repo's MUSE/ folder: Source, Config, .uplugin)
  * the freshly built module binary (UnrealEditor-McpAutomationBridge.dll)

The incremental build (build-fast.bat) puts the binary in the dev host project's
Binaries; the clean build (build.bat / RunUAT) puts a full plugin under build/.
This script finds whichever binary is newest and assembles a clean plugin.

Run AFTER a build succeeds:
    python deploy.py
"""

import json
import os
import shutil
import sys

HELIX_ENGINE = r"D:\SteamLibrary\steamapps\common\HELIX Studio\Engine"
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

PLUGIN_NAME = "MUSE"
PLUGIN_MODULE = "McpAutomationBridge"
MODULE_DLL = "UnrealEditor-McpAutomationBridge.dll"

SKELETON = os.path.join(ROOT, PLUGIN_NAME)          # Source / Config / .uplugin live here
BUILD_OUT = os.path.join(ROOT, "build", PLUGIN_NAME)
TARGET = os.path.join(HELIX_ENGINE, "Plugins", PLUGIN_NAME)
# Older deploy folder name — removed so two plugins don't provide the same module.
LEGACY_TARGET = os.path.join(HELIX_ENGINE, "Plugins", "McpAutomationBridge")
HELIX_MODULES = os.path.join(HELIX_ENGINE, "Binaries", "Win64", "UnrealEditor.modules")


def find_binaries_dir():
    """The Binaries/Win64 folder holding the freshly built module DLL (newest wins)."""
    cands = [
        os.path.join(ROOT, ".dev", "HostProject", "Binaries", "Win64"),   # build-fast.bat
        os.path.join(SKELETON, "Binaries", "Win64"),                      # standalone (if any)
        os.path.join(BUILD_OUT, "Binaries", "Win64"),                     # RunUAT
        os.path.join(BUILD_OUT, "HostProject", "Plugins", PLUGIN_NAME, "Binaries", "Win64"),
    ]
    found = []
    for c in cands:
        dll = os.path.join(c, MODULE_DLL)
        if os.path.isfile(dll):
            found.append((os.path.getmtime(dll), c))
    if not found:
        return None
    found.sort(reverse=True)
    return found[0][1]


def main():
    if not os.path.isfile(os.path.join(SKELETON, f"{PLUGIN_NAME}.uplugin")):
        sys.exit(f"Plugin source not found at {SKELETON}")
    bindir = find_binaries_dir()
    if not bindir:
        sys.exit("No built module binary found. Run build.bat or build-fast.bat first.")

    with open(HELIX_MODULES, "r", encoding="utf-8") as fh:
        helix_buildid = json.load(fh)["BuildId"]
    print("HELIX BuildId:", helix_buildid)
    print("Skeleton     :", SKELETON)
    print("Binaries     :", bindir)

    for old in (TARGET, LEGACY_TARGET):
        if os.path.isdir(old):
            shutil.rmtree(old)
            print("Removed old plugin folder:", old)

    # 1. Copy the source skeleton (no build artifacts).
    shutil.copytree(SKELETON, TARGET,
                    ignore=shutil.ignore_patterns("Binaries", "Intermediate", "HostProject"))

    # 2. Copy the built module binary (and its .pdb if present).
    out_bin = os.path.join(TARGET, "Binaries", "Win64")
    os.makedirs(out_bin, exist_ok=True)
    shutil.copy2(os.path.join(bindir, MODULE_DLL), os.path.join(out_bin, MODULE_DLL))
    pdb = MODULE_DLL.replace(".dll", ".pdb")
    if os.path.isfile(os.path.join(bindir, pdb)):
        shutil.copy2(os.path.join(bindir, pdb), os.path.join(out_bin, pdb))

    # 3. Write a clean module manifest for just this plugin, with HELIX's BuildId.
    manifest = {"BuildId": helix_buildid, "Modules": {PLUGIN_MODULE: MODULE_DLL}}
    with open(os.path.join(out_bin, "UnrealEditor.modules"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent="\t")
    print("Deployed ->", TARGET)
    print(f"Wrote module manifest (BuildId {helix_buildid})")

    print("\nDone. Ensure your project enables  { \"Name\": \"MUSE\", \"Enabled\": true }")
    print("then restart HELIX. Native MCP server: http://127.0.0.1:3000/mcp")


if __name__ == "__main__":
    main()
