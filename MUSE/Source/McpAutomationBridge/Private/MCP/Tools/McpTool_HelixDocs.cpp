// McpTool_HelixDocs.cpp — helix_docs tool definition (consult docs.helixgame.com)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_HelixDocs : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("helix_docs"); }

	FString GetDescription() const override
	{
		return TEXT("Consult the official Helix documentation (docs.helixgame.com). "
			"Use this to look up the Helix Lua/JS API, scripting events, the Character "
			"Creator, maps, blueprints and tutorials. Actions: "
			"'list' (browse every doc page path), "
			"'search' (find pages whose path matches a keyword), "
			"'read' (fetch a page as plain text by its path, e.g. "
			"'api/apiImport/classes/hcharacter/' or 'tutorials/Scripting/events/').");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("list"),
				TEXT("search"),
				TEXT("read")
			}, TEXT("What to do: list all pages, search by keyword, or read a page."))
			.String(TEXT("query"), TEXT("Keyword(s) to match against page paths (for 'search')."))
			.String(TEXT("path"), TEXT("Doc page path for 'read', e.g. 'tutorials/Scripting/events/'. A full https URL is also accepted."))
			.Required({ TEXT("action") })
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_HelixDocs);

// Anti-strip anchor. This file only contains a self-registering static, so under
// adaptive/standalone compilation the linker can dead-strip the whole object
// (and the registration never runs). Referencing this from the subsystem forces
// the linker to keep the translation unit.
void ForceLinkHelixDocsTool() {}
