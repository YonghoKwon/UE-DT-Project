// Fill out your copyright notice in the Description page of Project Settings.


#include "DxWebSocketSubsystem.h"

#include "IStompClient.h"
#include "StompModule.h"
#include "m7at10_dt/M7AT10/WebSocket/WebSocketMessage.h"

void UDxWebSocketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	this->ConnectWebSocket();
}

void UDxWebSocketSubsystem::Deinitialize()
{
	Super::Deinitialize();

	this->DisconnectStompClient(LoginInfo);
}

void UDxWebSocketSubsystem::ConnectWebSocket()
{
	IModuleInterface* StompInterface = FModuleManager::Get().LoadModule("Stomp");
	if (StompInterface)
	{
		StompInterface->StartupModule();
	}
	FStompModule* StompModule = &FStompModule::Get();

	StompClient = StompModule->CreateClient("ws://localhost:61616");
	LoginInfo.Add("login","artemis");
	LoginInfo.Add("passcode","artemis");

	this->ConnectStompClient(LoginInfo);

	StompClient->OnConnected().AddUObject(this, &UDxWebSocketSubsystem::HandleOnConnected);
	StompClient->OnConnectionError().AddUObject(this, &UDxWebSocketSubsystem::HandleOnConnectionError);
	StompClient->OnError().AddUObject(this, &UDxWebSocketSubsystem::HandleOnError);
	StompClient->OnClosed().AddUObject(this, &UDxWebSocketSubsystem::HandleOnClosed);
}

void UDxWebSocketSubsystem::ConnectStompClient(const TMap<FName, FString>& Header)
{
	if (StompClient.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("Connect STOMP WebSocket"));
		StompClient->Connect(LoginInfo);
	}
}

void UDxWebSocketSubsystem::DisconnectStompClient(const TMap<FName, FString>& Header)
{
	if (StompClient.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("Disconnect STOMP WebSocket"));
		StompClient->Disconnect(LoginInfo);
	}
}

void UDxWebSocketSubsystem::ReceivedMessage(UWebSocketMessage* Message)
{
	FStompBuffer Body = Message->GetRawBody();

	// Body Log (임시)
	FString BodyString;
	if (Body.Num() > 0)
	{
		// 마지막에 0(Null)을 추가한 임시 버퍼 생성
		TArray<uint8> TempBody = Body;
		TempBody.Add(0);

		// UTF8_TO_TCHAR 매크로 사용 (이제 Null이 보장되므로 안전)
		BodyString = UTF8_TO_TCHAR(reinterpret_cast<const char*>(TempBody.GetData()));
	}

	UE_LOG(LogTemp, Log, TEXT("Received Message Body: %s"), *BodyString);
}

FString UDxWebSocketSubsystem::Subscribe(const FString& Destination, const FSTOMPSubscriptionEvnet& EventCallback,
	const FSTOMPRequestCompleted& CompletionCallback)
{
	return StompClient->Subscribe(Destination,
		FStompSubscriptionEvent::CreateLambda([this, EventCallback](const IStompMessage& Message)->void
		{
			UWebSocketMessage* Msg = NewObject<UWebSocketMessage>(this);
			Msg->MyMessage = &Message;
			EventCallback.ExecuteIfBound(Msg);
			Msg->ConditionalBeginDestroy();
		}),
		FStompRequestCompleted::CreateLambda([CompletionCallback](bool bSuccess, const FString& Error)->void
		{
			CompletionCallback.ExecuteIfBound(bSuccess, Error);
		})
		);
}

void UDxWebSocketSubsystem::Unsubscribe(const FString& Subscription, const FSTOMPRequestCompleted& CompletionCallback)
{
	StompClient->Unsubscribe(Subscription,
		FStompRequestCompleted::CreateLambda([CompletionCallback](bool bSuccess, const FString& Error)->void
		{
			CompletionCallback.ExecuteIfBound(bSuccess, Error);
		})
		);
}

void UDxWebSocketSubsystem::HandleOnConnected(const FString& ProtocolVersion, const FString& SessionId,
                                              const FString& ServerString)
{
	ReceivedMessageEvent.BindDynamic(this, &UDxWebSocketSubsystem::ReceivedMessage);
	Subscribe("topic.cep.output.0", ReceivedMessageEvent, CompletedMessageEvent);
}

void UDxWebSocketSubsystem::HandleOnConnectionError(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("ConnectionError STOMP WebSocket"));
}

void UDxWebSocketSubsystem::HandleOnError(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("Error STOMP WebSocket"));
}

void UDxWebSocketSubsystem::HandleOnClosed(const FString& Reason)
{
	UE_LOG(LogTemp, Error, TEXT("Closed STOMP WebSocket"));
}
