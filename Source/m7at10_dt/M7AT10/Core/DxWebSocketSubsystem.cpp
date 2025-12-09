// Fill out your copyright notice in the Description page of Project Settings.


#include "DxWebSocketSubsystem.h"

#include "DxDataSubsystem.h"
#include "IStompClient.h"
#include "IStompMessage.h"
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

	// UE_LOG(LogM7AT10, Log, TEXT("Received Message Body: %s"), *BodyString);

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
	// // 1. Weak Pointer 생성 (this가 파괴되었는지 체크하기 위함)
	// TWeakObjectPtr<UDxWebSocketSubsystem> WeakThis(this);
	//
	// return StompClient->Subscribe(Destination,
	// 	FStompSubscriptionEvent::CreateLambda([WeakThis, EventCallback](const IStompMessage& Message)->void
	// 	{
	// 		// 2. Weak Pointer가 유효한지 확인 (게임이 종료되었거나 서브시스템이 죽었으면 실행 X)
	// 		TObjectPtr<UDxWebSocketSubsystem> StrongThis = WeakThis.Get();
	// 		if (!StrongThis)
	// 		{
	// 			return;
	// 		}
	//
	// 		// 3. StrongThis를 사용하여 안전하게 객체 생성
	// 		TObjectPtr<UWebSocketMessage> Msg = NewObject<UWebSocketMessage>(StrongThis);
	// 		Msg->InitializeFromStompMessage(Message);
	// 		EventCallback.ExecuteIfBound(Msg);
	// 	}),
	// 	FStompRequestCompleted::CreateLambda([CompletionCallback](bool bSuccess, const FString& Error)->void
	// 	{
	// 		CompletionCallback.ExecuteIfBound(bSuccess, Error);
	// 	})
	// 	);

	TWeakObjectPtr<UDxWebSocketSubsystem> WeakThis(this);

	return StompClient->Subscribe(Destination,
	FStompSubscriptionEvent::CreateLambda([WeakThis, EventCallback](const IStompMessage& Message) -> void
	{
		// 1. [백그라운드 스레드] 필요한 데이터를 미리 '값 복사(Copy)' 해둡니다.
		// 원본 Message가 사라지기 전에 데이터를 빼내는 과정입니다.
		FString CopiedBody = Message.GetBodyAsString();
		TMap<FName, FString> CopiedHeaders = Message.GetHeader(); // 헤더도 필요하다면 복사
		FString CopiedDest = Message.GetDestination();
		FString CopiedMsgId = Message.GetMessageId();

		// 2. [데이터 전달] 복사해둔 변수들(Copied...)을 캡처해서 넘깁니다.
		// 여기서 Message 자체는 절대 넘기면 안 됩니다.
		AsyncTask(ENamedThreads::GameThread, [WeakThis, EventCallback, CopiedBody, CopiedHeaders, CopiedDest, CopiedMsgId]()
		{
			// 3. [게임 스레드] 안전하게 UObject 생성
			TObjectPtr<UDxWebSocketSubsystem> StrongThis = WeakThis.Get();
			if (!StrongThis) return;

			UWebSocketMessage* Msg = NewObject<UWebSocketMessage>(StrongThis);

			// 4. 복사해둔 데이터를 UObject에 넣어줍니다.
			// (UWebSocketMessage.h에 변수들이 public으로 선언되어 있으므로 직접 대입 가능)
			Msg->BodyString = CopiedBody;
			Msg->Headers = CopiedHeaders;
			Msg->Destination = CopiedDest;
			Msg->MessageId = CopiedMsgId;

			// RawBody가 필요하다면 위 1번 단계에서 TArray<uint8>로 복사해서 넘겨야 합니다.

			// 5. 델리게이트 실행
			EventCallback.ExecuteIfBound(Msg);
		});
	}),
	FStompRequestCompleted::CreateLambda([CompletionCallback](bool bSuccess, const FString& Error) -> void
	{
		AsyncTask(ENamedThreads::GameThread, [CompletionCallback, bSuccess, Error]()
		{
			CompletionCallback.ExecuteIfBound(bSuccess, Error);
		});
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
