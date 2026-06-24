#include "HelixConnectPanel.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"

#if WITH_EDITOR
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Styling/AppStyle.h"
#include "McpAutomationBridgeSettings.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "SWebBrowser.h"
#endif

namespace HelixPanelInternal
{
	const TCHAR* kServerKey = TEXT("muse");

	struct FHelixAgent
	{
		FString Key;
		FString Name;
		FString Note;
		FString Format;   // "json" | "toml"
		FString Path;     // config file
		FString MapKey;   // "mcpServers" | "mcp_servers"
	};

	FString HomeDir()
	{
		// Win64 plugin — USERPROFILE is always set.
		return FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	}

	FString AppDataDir()
	{
		return FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	}

	TArray<FHelixAgent> GetAgents()
	{
		const FString Home = HomeDir();
		const FString AppData = AppDataDir();
		TArray<FHelixAgent> A;
		A.Add({ TEXT("claude-code"), TEXT("Claude Code"),
			TEXT("Anthropic CLI/IDE. Adds an HTTP MCP server to ~/.claude.json (user scope)."),
			TEXT("json"), FPaths::Combine(Home, TEXT(".claude.json")), TEXT("mcpServers") });
		A.Add({ TEXT("claude-desktop"), TEXT("Claude Desktop"),
			TEXT("Desktop app (stdio only) — bridged via 'npx mcp-remote'. Node.js required."),
			TEXT("json"), FPaths::Combine(AppData, TEXT("Claude"), TEXT("claude_desktop_config.json")), TEXT("mcpServers") });
		A.Add({ TEXT("cursor"), TEXT("Cursor"),
			TEXT("Registers globally so any Cursor workspace can see it."),
			TEXT("json"), FPaths::Combine(Home, TEXT(".cursor"), TEXT("mcp.json")), TEXT("mcpServers") });
		A.Add({ TEXT("codex"), TEXT("OpenAI Codex CLI"),
			TEXT("Adds an [mcp_servers.helix-mcp] section to ~/.codex/config.toml (via mcp-remote)."),
			TEXT("toml"), FPaths::Combine(Home, TEXT(".codex"), TEXT("config.toml")), TEXT("mcp_servers") });
		return A;
	}

	// ── JSON helpers ──────────────────────────────────────────────────────
	TSharedPtr<FJsonObject> LoadJson(const FString& Path)
	{
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *Path) && !Text.TrimStartAndEnd().IsEmpty())
		{
			TSharedPtr<FJsonObject> Obj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
			if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			{
				return Obj;
			}
		}
		return MakeShared<FJsonObject>();
	}

	bool SaveJson(const FString& Path, const TSharedPtr<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Out, *Path);
	}

	TSharedPtr<FJsonObject> MakeEntry(const FString& AgentKey, const FString& Url)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		if (AgentKey == TEXT("claude-code"))
		{
			Entry->SetStringField(TEXT("type"), TEXT("http"));
			Entry->SetStringField(TEXT("url"), Url);
		}
		else if (AgentKey == TEXT("cursor"))
		{
			Entry->SetStringField(TEXT("url"), Url);
		}
		else // claude-desktop (stdio via mcp-remote)
		{
			Entry->SetStringField(TEXT("command"), TEXT("npx"));
			TArray<TSharedPtr<FJsonValue>> Args;
			Args.Add(MakeShared<FJsonValueString>(TEXT("mcp-remote")));
			Args.Add(MakeShared<FJsonValueString>(Url));
			Entry->SetArrayField(TEXT("args"), Args);
		}
		return Entry;
	}

	bool JsonIsRegistered(const FHelixAgent& Ag)
	{
		TSharedPtr<FJsonObject> Root = LoadJson(Ag.Path);
		const TSharedPtr<FJsonObject>* Servers;
		return Root->TryGetObjectField(Ag.MapKey, Servers) && (*Servers)->HasField(kServerKey);
	}

	void JsonRegister(const FHelixAgent& Ag, const FString& Url)
	{
		TSharedPtr<FJsonObject> Root = LoadJson(Ag.Path);
		const TSharedPtr<FJsonObject>* ExistingServers;
		TSharedPtr<FJsonObject> Servers = Root->TryGetObjectField(Ag.MapKey, ExistingServers)
			? *ExistingServers : MakeShared<FJsonObject>();
		Servers->SetObjectField(kServerKey, MakeEntry(Ag.Key, Url));
		Root->SetObjectField(Ag.MapKey, Servers);
		SaveJson(Ag.Path, Root);
	}

	void JsonUnregister(const FHelixAgent& Ag)
	{
		TSharedPtr<FJsonObject> Root = LoadJson(Ag.Path);
		const TSharedPtr<FJsonObject>* Servers;
		if (Root->TryGetObjectField(Ag.MapKey, Servers))
		{
			(*Servers)->RemoveField(kServerKey);
			SaveJson(Ag.Path, Root);
		}
	}

	// ── TOML helpers (Codex) ──────────────────────────────────────────────
	FString TomlHeader(const FHelixAgent& Ag)
	{
		return FString::Printf(TEXT("[%s.%s]"), *Ag.MapKey, kServerKey);
	}

	bool TomlIsRegistered(const FHelixAgent& Ag)
	{
		FString Text;
		return FFileHelper::LoadFileToString(Text, *Ag.Path) && Text.Contains(TomlHeader(Ag));
	}

	void TomlUnregister(const FHelixAgent& Ag)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Ag.Path))
		{
			return;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		const FString Header = TomlHeader(Ag);
		TArray<FString> Out;
		bool bSkip = false;
		for (const FString& Line : Lines)
		{
			const FString T = Line.TrimStartAndEnd();
			if (T == Header) { bSkip = true; continue; }
			if (bSkip)
			{
				if (T.StartsWith(TEXT("[")) && T.EndsWith(TEXT("]"))) { bSkip = false; }
				else { continue; }
			}
			Out.Add(Line);
		}
		FFileHelper::SaveStringToFile(FString::Join(Out, TEXT("\n")), *Ag.Path);
	}

	void TomlRegister(const FHelixAgent& Ag, const FString& Url)
	{
		TomlUnregister(Ag);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Ag.Path), true);
		FString Text;
		FFileHelper::LoadFileToString(Text, *Ag.Path);
		Text += FString::Printf(
			TEXT("\n%s\ncommand = \"npx\"\nargs = [\"mcp-remote\", \"%s\"]\n"),
			*TomlHeader(Ag), *Url);
		FFileHelper::SaveStringToFile(Text, *Ag.Path);
	}

	// ── dispatch ──────────────────────────────────────────────────────────
	bool IsRegistered(const FHelixAgent& Ag)
	{
		return Ag.Format == TEXT("toml") ? TomlIsRegistered(Ag) : JsonIsRegistered(Ag);
	}

	void Register(const FHelixAgent& Ag, const FString& Url)
	{
		if (Ag.Format == TEXT("toml")) { TomlRegister(Ag, Url); } else { JsonRegister(Ag, Url); }
	}

	void Unregister(const FHelixAgent& Ag)
	{
		if (Ag.Format == TEXT("toml")) { TomlUnregister(Ag); } else { JsonUnregister(Ag); }
	}

	FString JsonEscape(const FString& In)
	{
		FString S = In;
		S.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		S.ReplaceInline(TEXT("\""), TEXT("\\\""));
		return S;
	}

	FString BuildStatusJson(int32 McpPort, const FString& Url)
	{
		TArray<FString> Rows;
		for (const FHelixAgent& Ag : GetAgents())
		{
			Rows.Add(FString::Printf(
				TEXT("{\"key\":\"%s\",\"name\":\"%s\",\"registered\":%s,\"path\":\"%s\",\"note\":\"%s\"}"),
				*JsonEscape(Ag.Key), *JsonEscape(Ag.Name), IsRegistered(Ag) ? TEXT("true") : TEXT("false"),
				*JsonEscape(Ag.Path), *JsonEscape(Ag.Note)));
		}
		return FString::Printf(TEXT("{\"port\":%d,\"url\":\"%s\",\"server\":\"%s\",\"agents\":[%s]}"),
			McpPort, *JsonEscape(Url), kServerKey, *FString::Join(Rows, TEXT(",")));
	}

	FString BodyAgentKey(const FString& Body)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
		{
			return Obj->GetStringField(TEXT("agent"));
		}
		return FString();
	}

#if WITH_EDITOR
	const FName MusePanelTabId(TEXT("MUSEConnectPanel"));

	TSharedRef<SDockTab> SpawnMusePanelTab(const FSpawnTabArgs&)
	{
		int32 Port = 3000;
		if (const UMcpAutomationBridgeSettings* S = GetDefault<UMcpAutomationBridgeSettings>())
		{
			Port = S->NativeMCPPort;
		}
		const FString PanelUrl = FString::Printf(TEXT("http://127.0.0.1:%d/panel"), Port);
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SWebBrowser)
				.InitialURL(PanelUrl)
				.ShowControls(false)
			];
	}

	void RegisterMusePanelTabSpawner()
	{
		static bool bRegistered = false;
		if (bRegistered)
		{
			return;
		}
		bRegistered = true;
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			MusePanelTabId, FOnSpawnTab::CreateStatic(&SpawnMusePanelTab))
			.SetDisplayName(FText::FromString(TEXT("MUSE")))
			.SetTooltipText(FText::FromString(TEXT("MUSE connect panel — by Prysma Studio")))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
	}

	void OpenHelixPanel()
	{
		// Open the connect panel as a dockable tab inside the editor.
		RegisterMusePanelTabSpawner();
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(MusePanelTabId));
	}
#endif
} // namespace HelixPanelInternal

using namespace HelixPanelInternal;

// ── Public API ─────────────────────────────────────────────────────────────
void HelixPanel_HandleApi(const FString& Path, const FString& Body, int32 McpPort,
                          int32& OutCode, FString& OutJson)
{
	OutCode = 200;
	const FString Url = FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), McpPort);

	if (Path.EndsWith(TEXT("/status")))
	{
		OutJson = BuildStatusJson(McpPort, Url);
		return;
	}
	if (Path.EndsWith(TEXT("/connect-all")))
	{
		for (const FHelixAgent& Ag : GetAgents()) { Register(Ag, Url); }
		OutJson = TEXT("{\"ok\":true}");
		return;
	}

	const FString Key = BodyAgentKey(Body);
	const TArray<FHelixAgent> Agents = GetAgents();
	const FHelixAgent* Found = Agents.FindByPredicate([&](const FHelixAgent& A){ return A.Key == Key; });
	if (!Found)
	{
		OutCode = 400;
		OutJson = TEXT("{\"ok\":false,\"error\":\"unknown agent\"}");
		return;
	}
	if (Path.EndsWith(TEXT("/register")))   { Register(*Found, Url);  OutJson = TEXT("{\"ok\":true}"); return; }
	if (Path.EndsWith(TEXT("/unregister"))) { Unregister(*Found);     OutJson = TEXT("{\"ok\":true}"); return; }

	OutCode = 404;
	OutJson = TEXT("{\"ok\":false,\"error\":\"not found\"}");
}

void HelixPanel_RegisterToolbar()
{
#if WITH_EDITOR
	// Register the dockable-tab spawner so the panel can live inside the editor.
	RegisterMusePanelTabSpawner();

	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}
	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.User"));
	if (!Toolbar)
	{
		return;
	}
	FToolMenuSection& Section = Toolbar->FindOrAddSection(TEXT("HelixMCP"));
	// Use a labelled widget so "MUSE" is always visible (a plain toolbar button
	// with no icon renders as empty/hard to find).
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget(
		TEXT("MUSEPanelButton"),
		SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.ToolTipText(FText::FromString(TEXT("Open the MUSE connect panel — by Prysma Studio")))
		.VAlign(VAlign_Center)
		.OnClicked(FOnClicked::CreateLambda([]() { OpenHelixPanel(); return FReply::Handled(); }))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.f, 0.f, 7.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("MUSE")))
			]
		],
		FText::GetEmpty(),
		true,   // bNoIndent
		false   // bSearchable
	);
	Section.AddEntry(Entry);
#endif
}

FString HelixPanel_GetHtml(int32 McpPort)
{
	FString Html = TEXT(R"HELIXPANEL(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>MUSE — by Prysma Studio</title>
<style>
:root{--bg:#080808;--surface:#161616;--surface2:#0e0e0e;--line:#1b212b;--text:#eef3f8;
--muted:#7e8896;--faint:#535d6b;--accent:#1ce8e0;--accent-ink:#021416;--danger:#ff5d73;
--mono:ui-monospace,Consolas,monospace;--sans:"Segoe UI",system-ui,sans-serif;}
*{box-sizing:border-box;}body{margin:0;background:radial-gradient(1200px 480px at 50% -260px,#0bd6d61a,transparent 70%),var(--bg);
color:var(--text);font-family:var(--sans);font-size:14px;}
.wrap{max-width:680px;margin:0 auto;padding:38px 24px 64px;}
.brand{font-weight:800;font-size:26px;letter-spacing:.14em;text-transform:uppercase;}
.brand b{color:var(--accent);}.tag{font-family:var(--mono);font-size:10.5px;letter-spacing:.22em;
color:var(--accent);border:1px solid var(--accent);border-radius:5px;padding:2px 7px;margin-left:8px;}
.lede{color:var(--muted);margin:8px 0 28px;}
.card{background:linear-gradient(180deg,var(--surface2),var(--surface));border:1px solid var(--line);
border-radius:14px;padding:16px 18px;margin-bottom:14px;}
.status{display:flex;align-items:center;gap:14px;}
.dot{width:9px;height:9px;border-radius:50%;background:var(--accent);box-shadow:0 0 0 4px #1ce8e022;}
.addr{font-family:var(--mono);font-size:12px;color:var(--muted);margin-top:2px;}
.label{display:flex;justify-content:space-between;align-items:center;margin:24px 2px 10px;}
.label h2{font-size:11px;letter-spacing:.18em;text-transform:uppercase;color:var(--faint);margin:0;}
button{font:inherit;font-size:12.5px;color:var(--text);background:transparent;border:1px solid var(--line);
border-radius:9px;padding:7px 13px;cursor:pointer;transition:.14s;}
button:hover{border-color:#2c3543;background:#131922;}
button.cta{color:var(--accent-ink);background:var(--accent);border-color:var(--accent);font-weight:700;}
button.danger:hover{border-color:var(--danger);color:var(--danger);}
button:disabled{opacity:.4;cursor:default;}
.agent{display:flex;align-items:center;gap:13px;padding:14px 4px;border-top:1px solid #161b23;}
.agent:first-child{border-top:0;}.agent .info{flex:1;min-width:0;}
.agent .name{font-weight:600;display:flex;align-items:center;gap:9px;}
.agent .name i{width:7px;height:7px;border-radius:50%;background:#41495a;display:inline-block;}
.agent .name i.on{background:var(--accent);box-shadow:0 0 8px var(--accent);}
.agent .note{color:var(--muted);font-size:12px;margin-top:3px;}
.agent .path{color:var(--faint);font-size:11px;font-family:var(--mono);margin-top:3px;
white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.act{display:flex;gap:7px;}
.foot{color:var(--faint);font-size:11.5px;margin-top:22px;line-height:1.6;}
#flash{position:fixed;left:50%;bottom:22px;transform:translateX(-50%);background:var(--surface);
border:1px solid var(--line);padding:9px 16px;border-radius:9px;opacity:0;transition:.2s;}
#flash.show{opacity:1;}
</style></head><body><div class="wrap">
<div><span class="brand">MUSE</span><span class="tag">by Prysma Studio</span></div>
<p class="lede">Modular Unreal Studio Engine — connect your AI assistant to the native in-editor MCP server.</p>
<div class="card status"><span class="dot"></span><div>
<div style="font-weight:600">Native MCP server running</div>
<div class="addr" id="addr">127.0.0.1:__PORT__/mcp</div></div></div>
<div class="label"><h2>Agents</h2><button id="all" class="cta">Connect all</button></div>
<div class="card" id="agents"></div>
<p class="foot" id="foot"></p></div><div id="flash"></div>
<script>
const $=i=>document.getElementById(i);
function flash(m){const f=$("flash");f.textContent=m;f.classList.add("show");
clearTimeout(f._t);f._t=setTimeout(()=>f.classList.remove("show"),1800);}
async function api(p,b){const o=b?{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(b)}:{};
return (await fetch("/panel/api/"+p,o)).json();}
function render(s){$("addr").textContent="127.0.0.1:"+s.port+"/mcp";
const box=$("agents");box.innerHTML="";
for(const a of s.agents){const el=document.createElement("div");el.className="agent";
el.innerHTML=`<div class="info"><div class="name"><i class="${a.registered?'on':''}"></i>${a.name}</div>
<div class="note">${a.note||""}</div><div class="path">${a.path}</div></div>
<div class="act"><button data-a="${a.key}" data-x="${a.registered?'unregister':'register'}"
class="${a.registered?'danger':'cta'}">${a.registered?'Remove':'Connect'}</button></div>`;
box.appendChild(el);}
$("foot").innerHTML="Claude Desktop & Codex use <code>npx mcp-remote</code> (Node.js required). "+
"Make sure 'Enable Native MCP' is on in Project Settings &rsaquo; Plugins &rsaquo; MUSE. "+
"<br>MUSE — Modular Unreal Studio Engine · by Prysma Studio";}
async function refresh(){try{render(await api("status"))}catch(e){}}
document.addEventListener("click",async e=>{const b=e.target.closest("button[data-a]");if(!b)return;
b.disabled=true;await api(b.dataset.x,{agent:b.dataset.a});
flash(b.dataset.x=="register"?"Connected":"Removed");refresh();});
$("all").onclick=async()=>{await api("connect-all",{});flash("All agents connected");refresh();};
refresh();setInterval(refresh,3000);
</script></body></html>)HELIXPANEL");
	Html.ReplaceInline(TEXT("__PORT__"), *FString::FromInt(McpPort));
	return Html;
}
