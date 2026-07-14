#include "ma0t10_dt/MA0T10/Sensor/LidarUdpJsonLiveSourceComp.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

ULidarUdpJsonLiveSourceComp::ULidarUdpJsonLiveSourceComp()
{
    SourceId = TEXT("UdpJsonLiveLidarBridge");
    SourceKind = ERealSensorSourceKind::JsonLiveBridge;
    bAutoStartSource = false;
    bSendTransportByDefault = false;
}

bool ULidarUdpJsonLiveSourceComp::StartSource()
{
    if (ListenSocket)
    {
        SetSourceState(ERealSensorSourceConnectionState::Running, LastUdpMessage);
        return true;
    }

    FIPv4Address ParsedAddress;
    if (!FIPv4Address::Parse(BindAddress, ParsedAddress))
    {
        LastUdpMessage = FString::Printf(TEXT("Invalid UDP bind address: %s"), *BindAddress);
        SetSourceState(ERealSensorSourceConnectionState::Error, LastUdpMessage);
        return false;
    }

    const FIPv4Endpoint Endpoint(ParsedAddress, static_cast<uint16>(FMath::Clamp(BindPort, 0, 65535)));
    const FString SocketName = FString::Printf(TEXT("MA0T10_LidarUdpJsonLive_%s_%d"), *SourceId, BindPort);
    int32 ReceiveBufferSize = FMath::Clamp(MaxDatagramBytes, 1024, 1048576);

    ListenSocket = FUdpSocketBuilder(*SocketName)
        .AsNonBlocking()
        .AsReusable()
        .BoundToEndpoint(Endpoint)
        .WithReceiveBufferSize(ReceiveBufferSize);

    if (!ListenSocket)
    {
        LastUdpMessage = FString::Printf(TEXT("Failed to bind UDP JSON live source on %s:%d"), *BindAddress, BindPort);
        SetSourceState(ERealSensorSourceConnectionState::Error, LastUdpMessage);
        return false;
    }

    TSharedRef<FInternetAddr> BoundAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    ListenSocket->GetAddress(*BoundAddress);
    BoundPort = BoundAddress->GetPort();

    SocketReceiver = MakeUnique<FUdpSocketReceiver>(ListenSocket, FTimespan::FromMilliseconds(20), *SocketName);
    SocketReceiver->OnDataReceived().BindUObject(this, &ULidarUdpJsonLiveSourceComp::HandleReceivedData);
    SocketReceiver->Start();

    LastUdpMessage = FString::Printf(TEXT("UDP JSON live source listening on %s:%d"), *BindAddress, BoundPort);
    SetSourceState(ERealSensorSourceConnectionState::Running, LastUdpMessage);
    return true;
}

void ULidarUdpJsonLiveSourceComp::StopSource()
{
    CloseUdpSocket();
    SetSourceState(ERealSensorSourceConnectionState::Stopped, TEXT("UDP JSON live source stopped."));
}

bool ULidarUdpJsonLiveSourceComp::ProcessUdpPayloadJson(const FString& PayloadJson)
{
    const bool bAppended = AppendLivePayloadJson(PayloadJson);
    if (!bAppended)
    {
        LastUdpMessage = TEXT("UDP JSON live payload was rejected.");
        return false;
    }

    if (bAutoPushReceivedFrame)
    {
        return PushFrameOnce(bSendTransportForReceivedFrames);
    }

    LastUdpMessage = FString::Printf(TEXT("Buffered UDP JSON live payload. pendingLines=%d"), PendingLineCount);
    return true;
}

void ULidarUdpJsonLiveSourceComp::HandleReceivedData(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    if (!Data.IsValid() || Data->Num() <= 0)
    {
        return;
    }

    const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Data->GetData()), Data->Num());
    FString PayloadJson(Converter.Length(), Converter.Get());
    PayloadJson.TrimStartAndEndInline();

    TWeakObjectPtr<ULidarUdpJsonLiveSourceComp> WeakThis(this);
    const FString EndpointText = Endpoint.ToString();
    const int32 ByteCount = Data->Num();
    AsyncTask(ENamedThreads::GameThread, [WeakThis, PayloadJson, EndpointText, ByteCount]()
    {
        if (!WeakThis.IsValid())
        {
            return;
        }

        WeakThis->LastReceivedEndpoint = EndpointText;
        WeakThis->LastReceivedDatagramBytes = ByteCount;
        WeakThis->ProcessUdpPayloadJson(PayloadJson);
    });
}

void ULidarUdpJsonLiveSourceComp::CloseUdpSocket()
{
    if (SocketReceiver)
    {
        SocketReceiver->Stop();
        SocketReceiver.Reset();
    }

    if (ListenSocket)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
    BoundPort = 0;
}
