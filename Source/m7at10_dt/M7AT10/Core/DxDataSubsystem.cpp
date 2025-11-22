// Fill out your copyright notice in the Description page of Project Settings.


#include "DxDataSubsystem.h"

void UDxDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDxDataSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxDataSubsystem::Tick(float DeltaTime)
{
	// 각각의 처리 함수를 호출
	ProcessApiQueue();
	ProcessWebSocketQueue();
}

TStatId UDxDataSubsystem::GetStatId() const
{
	return TStatId();
}

void UDxDataSubsystem::EnqueueApiData(const FString& Data)
{
	ApiDataQueue.Enqueue(Data);
}

void UDxDataSubsystem::ProcessApiQueue()
{
	if (ApiDataQueue.IsEmpty()) return;

	FString Data;
	while (ApiDataQueue.Dequeue(Data))
	{
		UE_LOG(LogTemp, Log, TEXT("[API] Processing: %s"), *Data);
		// JSON 파싱 및 API 로직 처리
	}
}

void UDxDataSubsystem::EnqueueWebSocketData(const FString& Data)
{
	WebSocketDataQueue.Enqueue(Data);
}

void UDxDataSubsystem::ProcessWebSocketQueue()
{
	if (WebSocketDataQueue.IsEmpty()) return;

	FString Data;
	while (WebSocketDataQueue.Dequeue(Data))
	{
		UE_LOG(LogTemp, Log, TEXT("[WebSocket] Processing: %s"), *Data);
		// 실시간 데이터 로직 처리
	}
}
