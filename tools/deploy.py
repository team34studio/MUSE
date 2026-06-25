"""
Deploy the built MUSE plugin into HELIX Studio and swap its BuildId so the
custom build will load it.

The plugin is named MUSE; its C++ module/DLL stays McpAutomationBridge (internal).
Run AFTER build.bat succeeds:
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
BUILD_OUT = os.path.join(ROOT, "build", PLUGIN_NAME)
TARGET = os.path.join(HELIX_ENGINE, "Plugins", PLUGIN_NAME)
# Older deploy folder name — removed so two plugins don't provide the same module.
LEGACY_TARGET = os.path.join(HELIX_ENGINE, "Plugins", "McpAutomationBridge")
helix_modules = os.path.join(HELIX_ENGINE, "Binaries", "Win64", "UnrealEditor.modules")


def find_plugin_dir():
    """The built plugin dir (containing Binaries/Win64/UnrealEditor.modules).

    Checks both the incremental build output (build-fast.bat writes into the
    plugin source dir) and the clean packaged output (build.bat -> build/MUSE),
    and picks whichever was built most recently.
    """
    candidates = (
        os.path.join(ROOT, PLUGIN_NAME),                                  # build-fast.bat (incremental)
        BUILD_OUT,                                                        # build.bat (clean package)
        os.path.join(BUILD_OUT, "HostProject", "Plugins", PLUGIN_NAME),   # older RunUAT layout
    )
    found = []
    for c in candidates:
        modules = os.path.join(c, "Binaries", "Win64", "UnrealEditor.modules")
        if os.path.isfile(modules):
            found.append((os.path.getmtime(modules), c))
    if not found:
        return None
    found.sort(reverse=True)  # newest .modules first
    return found[0][1]


def main():
    src = find_plugin_dir()
    if not src:
        sys.exit(f"No built plugin with binaries under {BUILD_OUT}\n"
                 "Run build.bat and make sure it finished with [OK].")

    with open(helix_modules, "r", encoding="utf-8") as fh:
        helix_buildid = json.load(fh)["BuildId"]
    print("HELIX BuildId:", helix_buildid)
    print("Plugin source:", src)

    for old in (TARGET, LEGACY_TARGET):
        if os.path.isdir(old):
            shutil.rmtree(old)
            print("Removed old plugin folder:", old)

    shutil.copytree(src, TARGET,
                    ignore=shutil.ignore_patterns("HostProject", "Intermediate"))
    print("Deployed ->", TARGET)

    plugin_modules = os.path.join(TARGET, "Binaries", "Win64", "UnrealEditor.modules")
    if not os.path.isfile(plugin_modules):
        sys.exit(f"Module manifest not found: {plugin_modules}")
    with open(plugin_modules, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    old = data.get("BuildId")
    data["BuildId"] = helix_buildid
    with open(plugin_modules, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=1)
    print(f"Swapped BuildId {old} -> {helix_buildid}")

    print("\nDone. Ensure your project enables  { \"Name\": \"MUSE\", \"Enabled\": true }")
    print("then restart HELIX. Native MCP server: http://127.0.0.1:3000/mcp")


if __name__ == "__main__":
    main()
