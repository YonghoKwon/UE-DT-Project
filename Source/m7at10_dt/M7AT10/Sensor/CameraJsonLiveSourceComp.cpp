#include "m7at10_dt/M7AT10/Sensor/CameraJsonLiveSourceComp.h"

#include "GameFramework/Actor.h"
#include "m7at10_dt/M7AT10/Camera/VirtualCameraComp.h"

UCameraJsonLiveSourceComp::UCameraJsonLiveSourceComp()
{
    PrimaryComponentTick.bCanEverTick = false;
    SourceKind = ERealSensorSourceKind::JsonLiveBridge;
    SourceId = TEXT("CameraJsonLiveBridge");
    LastSourceMessage = TEXT("Camera JSON live bridge is ready for virtual-camera.v1 payloads.");
}

bool UCameraJsonLiveSourceComp::StartSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Running, TEXT("Camera JSON live bridge ready."));
    return true;
}

void UCameraJsonLiveSourceComp::StopSource()
{
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("Camera JSON live bridge stopped."));
}

bool UCameraJsonLiveSourceComp::AppendLivePayloadJson(const FString& PayloadJson)
{
    FString Trimmed = PayloadJson;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty())
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Camera JSON live payload is empty."));
        return false;
    }

    BufferedPayloadJson = Trimmed;
    bHasBufferedPayload = true;
    LastPayloadLength = BufferedPayloadJson.Len();
    LastSourceMessage = FString::Printf(TEXT("Buffered camera JSON live payload. bytes=%d"), LastPayloadLength);
    return true;
}

void UCameraJsonLiveSourceComp::ClearBufferedFrame()
{
    BufferedPayloadJson.Reset();
    bHasBufferedPayload = false;
    LastPayloadLength = 0;
    LastSourceMessage = TEXT("Camera JSON live buffer cleared.");
}

bool UCameraJsonLiveSourceComp::PushFrameOnce(bool bSendTransport)
{
    UVirtualCameraComp* CameraComp = ResolveTargetCamera();
    if (!CameraComp)
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Target camera is not set."));
        return false;
    }
    if (!bHasBufferedPayload || BufferedPayloadJson.IsEmpty())
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Camera JSON live buffer is empty."));
        return false;
    }

    if (!CameraComp->InjectExternalJsonPayload(BufferedPayloadJson, bSendTransport))
    {
        SetSourceState(ERealSensorSourceConnectionState::Error, TEXT("Camera JSON live payload was rejected by target camera."));
        return false;
    }

    MarkFramePushed(1, FString::Printf(TEXT("Pushed camera JSON live frame. bytes=%d sendTransport=%s"),
        LastPayloadLength,
        bSendTransport ? TEXT("true") : TEXT("false")));
    if (bClearBufferAfterPush)
    {
        ClearBufferedFrame();
    }
    return true;
}

UVirtualCameraComp* UCameraJsonLiveSourceComp::ResolveTargetCamera() const
{
    if (TargetCamera)
    {
        return TargetCamera;
    }
    return GetOwner() ? GetOwner()->FindComponentByClass<UVirtualCameraComp>() : nullptr;
}
