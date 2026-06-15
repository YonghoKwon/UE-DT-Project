#include "m7at10_dt/M7AT10/Sensor/LidarHttpJsonLiveSourceComp.h"

#include "Containers/StringConv.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

ULidarHttpJsonLiveSourceComp::ULidarHttpJsonLiveSourceComp()
{
    SourceId = TEXT("HttpJsonLiveLidarBridge");
    SourceKind = ERealSensorSourceKind::JsonLiveBridge;
    bAutoStartSource = false;
    bSendTransportByDefault = false;
    LastHttpMessage = TEXT("HTTP JSON live source is stopped.");
}

bool ULidarHttpJsonLiveSourceComp::StartSource()
{
    if (RouteHandle.IsValid())
    {
        SetSourceState(ERealSensorSourceConnectionState::Running, LastHttpMessage);
        return true;
    }

    const int32 ClampedPort = FMath::Clamp(ListenPort, 1, 65535);
    const FString NormalizedPath = NormalizeRoutePath();
    TSharedPtr<IHttpRouter> Router = FHttpServerModule::Get().GetHttpRouter(static_cast<uint32>(ClampedPort), true);
    if (!Router.IsValid())
    {
        LastHttpMessage = FString::Printf(TEXT("Failed to create HTTP JSON live router on port %d."), ClampedPort);
        SetSourceState(ERealSensorSourceConnectionState::Error, LastHttpMessage);
        return false;
    }

    TWeakObjectPtr<ULidarHttpJsonLiveSourceComp> WeakThis(this);
    FHttpRouteHandle NewRouteHandle = Router->BindRoute(
        FHttpPath(NormalizedPath),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakThis](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
        {
            if (!WeakThis.IsValid())
            {
                OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::ServerError, TEXT("source_destroyed"), TEXT("HTTP JSON live source was destroyed.")));
                return true;
            }
            return WeakThis->HandleHttpRequest(Request, OnComplete);
        });

    if (!NewRouteHandle.IsValid())
    {
        LastHttpMessage = FString::Printf(TEXT("Failed to bind HTTP JSON live route %s on port %d."), *NormalizedPath, ClampedPort);
        SetSourceState(ERealSensorSourceConnectionState::Error, LastHttpMessage);
        return false;
    }

    HttpRouter = Router;
    RouteHandle = NewRouteHandle;
    FHttpServerModule::Get().StartAllListeners();

    LastHttpMessage = FString::Printf(TEXT("HTTP JSON live source listening on port %d route %s."), ClampedPort, *NormalizedPath);
    SetSourceState(ERealSensorSourceConnectionState::Running, LastHttpMessage);
    return true;
}

void ULidarHttpJsonLiveSourceComp::StopSource()
{
    CloseHttpRoute();
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("HTTP JSON live source stopped."));
}

bool ULidarHttpJsonLiveSourceComp::ProcessHttpPayloadJson(const FString& PayloadJson)
{
    const bool bAppended = AppendLivePayloadJson(PayloadJson);
    if (!bAppended)
    {
        LastHttpMessage = TEXT("HTTP JSON live payload was rejected.");
        LastResponseCode = 400;
        return false;
    }

    if (bAutoPushReceivedFrame)
    {
        const bool bPushed = PushFrameOnce(bSendTransportForReceivedFrames);
        LastResponseCode = bPushed ? 202 : 422;
        LastHttpMessage = bPushed
            ? FString::Printf(TEXT("Accepted HTTP JSON live payload. points=%lld"), static_cast<long long>(LastSourcePointCount))
            : TEXT("HTTP JSON live payload parsed but frame push failed.");
        return bPushed;
    }

    LastResponseCode = 202;
    LastHttpMessage = FString::Printf(TEXT("Buffered HTTP JSON live payload. pendingLines=%d"), PendingLineCount);
    return true;
}

bool ULidarHttpJsonLiveSourceComp::HandleHttpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
    LastReceivedRequestBytes = Request.Body.Num();
    if (LastReceivedRequestBytes <= 0)
    {
        LastResponseCode = 400;
        LastHttpMessage = TEXT("HTTP JSON live request body is empty.");
        OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("empty_body"), LastHttpMessage));
        return true;
    }

    if (LastReceivedRequestBytes > FMath::Max(1024, MaxRequestBytes))
    {
        LastResponseCode = 413;
        LastHttpMessage = FString::Printf(TEXT("HTTP JSON live request exceeded MaxRequestBytes. bytes=%d max=%d"), LastReceivedRequestBytes, MaxRequestBytes);
        OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::RequestTooLarge, TEXT("body_too_large"), LastHttpMessage));
        return true;
    }

    const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
    FString PayloadJson(Converter.Length(), Converter.Get());
    PayloadJson.TrimStartAndEndInline();

    const bool bAccepted = ProcessHttpPayloadJson(PayloadJson);
    const FString ResponseJson = FString::Printf(
        TEXT("{\"accepted\":%s,\"sourceId\":\"%s\",\"frameId\":%lld,\"pointCount\":%lld,\"message\":\"%s\"}"),
        bAccepted ? TEXT("true") : TEXT("false"),
        *SourceId.ReplaceCharWithEscapedChar(),
        static_cast<long long>(LastSourceFrameId),
        static_cast<long long>(LastSourcePointCount),
        *LastHttpMessage.ReplaceCharWithEscapedChar());

    TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
    Response->Code = bAccepted ? EHttpServerResponseCodes::Accepted : EHttpServerResponseCodes::BadRequest;
    OnComplete(MoveTemp(Response));
    return true;
}

void ULidarHttpJsonLiveSourceComp::CloseHttpRoute()
{
    if (HttpRouter.IsValid() && RouteHandle.IsValid())
    {
        HttpRouter->UnbindRoute(RouteHandle);
    }
    RouteHandle.Reset();
    HttpRouter.Reset();
}

FString ULidarHttpJsonLiveSourceComp::NormalizeRoutePath() const
{
    FString Path = RoutePath;
    Path.TrimStartAndEndInline();
    if (Path.IsEmpty())
    {
        Path = TEXT("/m7at10/lidar/live");
    }
    if (!Path.StartsWith(TEXT("/")))
    {
        Path = TEXT("/") + Path;
    }
    return Path;
}
