// McpAutomationBridge_HelixDocsHandlers.cpp
// Handler for the helix_docs tool — fetches docs.helixgame.com asynchronously
// (sitemap for list/search, the page itself for read) and completes the pending
// MCP request from the HTTP callback. No game-thread blocking.

#include "McpAutomationBridgeSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"

namespace HelixDocsInternal
{
	const TCHAR* kBaseUrl = TEXT("https://docs.helixgame.com/");

	void RemoveBlock(FString& S, const TCHAR* OpenTag, const TCHAR* CloseTag)
	{
		const int32 CloseLen = FCString::Strlen(CloseTag);
		while (true)
		{
			const int32 A = S.Find(OpenTag, ESearchCase::IgnoreCase);
			if (A == INDEX_NONE)
			{
				break;
			}
			const int32 B = S.Find(CloseTag, ESearchCase::IgnoreCase, ESearchDir::FromStart, A);
			if (B == INDEX_NONE)
			{
				S = S.Left(A);
				break;
			}
			S = S.Left(A) + S.Mid(B + CloseLen);
		}
	}

	FString StripHtml(const FString& Html)
	{
		FString S = Html;
		RemoveBlock(S, TEXT("<script"), TEXT("</script>"));
		RemoveBlock(S, TEXT("<style"), TEXT("</style>"));
		RemoveBlock(S, TEXT("<nav"), TEXT("</nav>"));
		RemoveBlock(S, TEXT("<header"), TEXT("</header>"));
		RemoveBlock(S, TEXT("<footer"), TEXT("</footer>"));

		// Strip remaining tags.
		FString Out;
		Out.Reserve(S.Len());
		bool bInTag = false;
		for (const TCHAR C : S)
		{
			if (C == '<') { bInTag = true; }
			else if (C == '>') { bInTag = false; }
			else if (!bInTag) { Out.AppendChar(C); }
		}

		Out.ReplaceInline(TEXT("&amp;"), TEXT("&"));
		Out.ReplaceInline(TEXT("&lt;"), TEXT("<"));
		Out.ReplaceInline(TEXT("&gt;"), TEXT(">"));
		Out.ReplaceInline(TEXT("&quot;"), TEXT("\""));
		Out.ReplaceInline(TEXT("&#39;"), TEXT("'"));
		Out.ReplaceInline(TEXT("&nbsp;"), TEXT(" "));

		// Collapse runs of blank lines and trim each line.
		TArray<FString> Lines;
		Out.ParseIntoArrayLines(Lines, false);
		FString Result;
		int32 Blank = 0;
		for (FString L : Lines)
		{
			L.TrimStartAndEndInline();
			if (L.IsEmpty())
			{
				if (++Blank > 1) { continue; }
			}
			else
			{
				Blank = 0;
			}
			Result += L;
			Result += TEXT("\n");
		}
		return Result.TrimStartAndEnd();
	}

	// Fire-and-forget async GET; OnDone runs on the game thread.
	void Fetch(const FString& Url, TFunction<void(bool /*ok*/, int32 /*code*/, const FString& /*body*/)> OnDone)
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetURL(Url);
		Req->SetVerb(TEXT("GET"));
		Req->SetHeader(TEXT("User-Agent"), TEXT("MUSE-HelixDocs"));
		Req->OnProcessRequestComplete().BindLambda(
			[OnDone](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnectedOk)
			{
				const bool bOk = bConnectedOk && Resp.IsValid();
				const int32 Code = bOk ? Resp->GetResponseCode() : 0;
				const FString Body = bOk ? Resp->GetContentAsString() : FString();
				OnDone(bOk, Code, Body);
			});
		Req->ProcessRequest();
	}
}

bool UMcpAutomationBridgeSubsystem::HandleHelixDocsAction(
	const FString& RequestId, const FString& /*Action*/,
	const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
	using namespace HelixDocsInternal;

	FString Sub;
	if (Payload.IsValid()) { Payload->TryGetStringField(TEXT("action"), Sub); }
	Sub = Sub.ToLower();

	TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
	auto Complete = [WeakThis, RequestId, Socket]
		(bool bSuccess, const FString& Msg, const TSharedPtr<FJsonObject>& Result)
	{
		if (UMcpAutomationBridgeSubsystem* Self = WeakThis.Get())
		{
			Self->SendAutomationResponse(Socket, RequestId, bSuccess, Msg, Result,
				bSuccess ? FString() : TEXT("HELIX_DOCS_ERROR"));
		}
	};

	if (Sub == TEXT("read"))
	{
		FString Path;
		if (Payload.IsValid()) { Payload->TryGetStringField(TEXT("path"), Path); }
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty())
		{
			Complete(false, TEXT("'path' is required for action 'read'."), nullptr);
			return true;
		}
		FString Url = Path;
		if (!Url.StartsWith(TEXT("http")))
		{
			Path.RemoveFromStart(TEXT("/"));
			Url = FString(kBaseUrl) + Path;
		}
		Fetch(Url, [Complete, Url](bool bOk, int32 Code, const FString& Body)
		{
			if (!bOk || Code >= 400 || Body.IsEmpty())
			{
				Complete(false, FString::Printf(TEXT("Failed to fetch %s (HTTP %d)."), *Url, Code), nullptr);
				return;
			}
			FString Text = StripHtml(Body);
			const int32 MaxLen = 16000;
			bool bTruncated = false;
			if (Text.Len() > MaxLen)
			{
				Text = Text.Left(MaxLen) + TEXT("\n…[truncated — read a more specific page]");
				bTruncated = true;
			}
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("url"), Url);
			R->SetBoolField(TEXT("truncated"), bTruncated);
			R->SetStringField(TEXT("content"), Text);
			Complete(true, TEXT("ok"), R);
		});
		return true;
	}

	const bool bSearch = (Sub == TEXT("search"));
	if (Sub != TEXT("list") && !bSearch)
	{
		Complete(false, TEXT("Unknown action. Use 'list', 'search' or 'read'."), nullptr);
		return true;
	}

	FString Query;
	if (bSearch)
	{
		if (Payload.IsValid()) { Payload->TryGetStringField(TEXT("query"), Query); }
		Query = Query.ToLower().TrimStartAndEnd();
		if (Query.IsEmpty())
		{
			Complete(false, TEXT("'query' is required for action 'search'."), nullptr);
			return true;
		}
	}

	Fetch(FString(kBaseUrl) + TEXT("sitemap.xml"),
		[Complete, bSearch, Query](bool bOk, int32 Code, const FString& Body)
	{
		if (!bOk || Code >= 400 || Body.IsEmpty())
		{
			Complete(false, FString::Printf(TEXT("Could not fetch the docs sitemap (HTTP %d)."), Code), nullptr);
			return;
		}
		TArray<TSharedPtr<FJsonValue>> Pages;
		int32 Cursor = 0;
		while (true)
		{
			const int32 A = Body.Find(TEXT("<loc>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Cursor);
			if (A == INDEX_NONE) { break; }
			const int32 B = Body.Find(TEXT("</loc>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, A);
			if (B == INDEX_NONE) { break; }
			FString Loc = Body.Mid(A + 5, B - (A + 5)).TrimStartAndEnd();
			Cursor = B + 6;
			FString PathPart = Loc;
			PathPart.RemoveFromStart(TEXT("https://docs.helixgame.com/"));
			PathPart.RemoveFromStart(TEXT("http://docs.helixgame.com/"));
			if (PathPart.IsEmpty()) { continue; }
			if (bSearch && !PathPart.ToLower().Contains(Query)) { continue; }
			Pages.Add(MakeShared<FJsonValueString>(PathPart));
		}
		TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
		R->SetNumberField(TEXT("count"), Pages.Num());
		R->SetArrayField(TEXT("pages"), Pages);
		Complete(true, TEXT("ok"), R);
	});
	return true;
}
