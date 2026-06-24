#pragma once

#include "CoreMinimal.h"

// Helix MCP — in-editor connect panel.
//
// Served by the native MCP HTTP server (reuses its socket). A toolbar button
// opens http://127.0.0.1:<port>/panel in the browser; the panel lets the user
// one-click-register AI agents (Claude Code / Claude Desktop / Cursor / Codex)
// against the native streamable-HTTP endpoint http://127.0.0.1:<port>/mcp.

/** Full HTML for the connect panel (port is substituted in). */
FString HelixPanel_GetHtml(int32 McpPort);

/** Handle a /panel/api/* request. Fills OutCode + OutJson (application/json). */
void HelixPanel_HandleApi(const FString& Path, const FString& Body, int32 McpPort,
                          int32& OutCode, FString& OutJson);

/** Register the "Helix MCP" toolbar button (call once, on the game thread). */
void HelixPanel_RegisterToolbar();
