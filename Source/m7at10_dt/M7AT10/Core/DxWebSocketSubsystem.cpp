// Fill out your copyright notice in the Description page of Project Settings.


#include "DxWebSocketSubsystem.h"

#include "DxDataSubsystem.h"
#include "IStompClient.h"
#include "StompModule.h"
#include "m7at10_dt/m7at10_dt.h"
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
	StompClient.Reset();
}

void UDxWebSocketSubsystem::ConnectWebSocket()
{
	// 이미 연결된 클라이언트가 있다면 정리
	if (StompClient.IsValid())
	{
		DisconnectStompClient(LoginInfo);
		StompClient.Reset();
	}

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
		UE_LOG(LogM7AT10, Log, TEXT("Connect STOMP WebSocket"));
		StompClient->Connect(LoginInfo);
	}
}

void UDxWebSocketSubsystem::DisconnectStompClient(const TMap<FName, FString>& Header)
{
	if (StompClient.IsValid())
	{
		UE_LOG(LogM7AT10, Log, TEXT("Disconnect STOMP WebSocket"));
		StompClient->Disconnect(LoginInfo);
	}
}

void UDxWebSocketSubsystem::ReceivedMessage(UWebSocketMessage* Message)
{
	// FStompBuffer Body = Message->GetRawBody();
	const FString BodyString = Message->GetBodyAsString();

	UE_LOG(LogM7AT10, Log, TEXT("Received Message Body: %s"), *BodyString);

	// DxDataSubsystem에 데이터 저장
	TObjectPtr<UGameInstance> GameInstance = GetGameInstance();
	if (GameInstance && !BodyString.IsEmpty())
	{
		TObjectPtr<UDxDataSubsystem> DataSubsystem = GameInstance->GetSubsystem<UDxDataSubsystem>();
		if (DataSubsystem)
		{
			DataSubsystem->EnqueueWebSocketData(BodyString);
		}
	}
}

FString UDxWebSocketSubsystem::Subscribe(const FString& Destination, const FSTOMPSubscriptionEvent& EventCallback,
	const FSTOMPRequestCompleted& CompletionCallback)
{
	// 1. Weak Pointer 생성 (this가 파괴되었는지 체크하기 위함)
	TWeakObjectPtr<UDxWebSocketSubsystem> WeakThis(this);

	return StompClient->Subscribe(Destination,
		FStompSubscriptionEvent::CreateLambda([WeakThis, EventCallback](const IStompMessage& Message)->void
		{
			// 2. Weak Pointer가 유효한지 확인 (게임이 종료되었거나 서브시스템이 죽었으면 실행 X)
			TObjectPtr<UDxWebSocketSubsystem> StrongThis = WeakThis.Get();
			if (!StrongThis)
			{
				return;
			}

			// 3. StrongThis를 사용하여 안전하게 객체 생성
			TObjectPtr<UWebSocketMessage> Msg = NewObject<UWebSocketMessage>(StrongThis);
			Msg->InitializeFromStompMessage(Message);
			EventCallback.ExecuteIfBound(Msg);
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
	UE_LOG(LogM7AT10, Error, TEXT("ConnectionError STOMP WebSocket"));
}

void UDxWebSocketSubsystem::HandleOnError(const FString& Error)
{
	UE_LOG(LogM7AT10, Error, TEXT("Error STOMP WebSocket"));
}

void UDxWebSocketSubsystem::HandleOnClosed(const FString& Reason)
{
	UE_LOG(LogM7AT10, Error, TEXT("Closed STOMP WebSocket"));
}
