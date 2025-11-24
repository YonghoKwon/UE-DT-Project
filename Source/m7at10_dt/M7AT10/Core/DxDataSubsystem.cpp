// Fill out your copyright notice in the Description page of Project Settings.


#include "DxDataSubsystem.h"

#include "m7at10_dt/M7AT10/Lib/YyJsonParser.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeStruct.h"

void UDxDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FString DataTablePath = TEXT("DataTable'/Game/M7AT10/Common/DataTables/DT_TransactionCode.DT_TransactionCode'");
	UDataTable* LoadedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));

	if (LoadedTable)
	{
		LoadedTable->ForeachRow<FTransactionCodeStruct>(TEXT("UDxDataSubsystem::Initialize"),
			[this](const FName& RowName, const FTransactionCodeStruct& Row)
			{
				if (Row.TransactionCodeMessageClass)
				{
					TObjectPtr<UTransactionCodeMessage> NewDoc = NewObject<UTransactionCodeMessage>(this, Row.TransactionCodeMessageClass);
					if (NewDoc && !NewDoc->TransactionCode.IsEmpty())
					{
						TransactionCodeMessageMap.Add(NewDoc->TransactionCode, NewDoc);
						UE_LOG(LogTemp, Log, TEXT("Loaded TransactionCode: %s"), *NewDoc->TransactionCode);
					}
				}
			});
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TransactionCodeDataTable failed to load from path: %s"), *DataTablePath);
	}
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
	int32 ProcessCount = 0;
	const int32 MaxProcessPerFrame = 50; // 프레임당 최대 처리 개수 제한

	// 큐에서 데이터를 꺼내고 제한 개수만큼만 처리
	while (ProcessCount < MaxProcessPerFrame && ApiDataQueue.Dequeue(Data))
	{
		UE_LOG(LogTemp, Log, TEXT("[API] Processing: %s"), *Data);
		// JSON 파싱 및 API 로직 처리
		ProcessCount++;
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
	int32 ProcessCount = 0;
	const int32 MaxProcessPerFrame = 50;

	while (ProcessCount < MaxProcessPerFrame && WebSocketDataQueue.Dequeue(Data))
	{
		UE_LOG(LogTemp, Log, TEXT("[WebSocket] Processing: %s"), *Data);
		// 실시간 데이터 로직 처리

		// 1. Parser 인스턴스 생성 (Stack 메모리)
		FYyJsonParser JsonParser;

		// 2. JSON 파싱 시도
		if (JsonParser.JsonParse(Data))
		{
			yyjson_val* Root = JsonParser.GetRoot();

			// 3. MESSAGE_ID 추출 (이것이 TransactionCode 역할을 함)
			// 예: "MESSAGE_ID": "KE2D1Z11"
			yyjson_val* MsgIdVal = JsonParser.JsonParseKeyword(Root, TEXT("MESSAGE_ID"));

			if (JsonParser.IsValid(MsgIdVal))
			{
				FString TrCode = JsonParser.GetString(MsgIdVal);

				// 4. 핸들러 찾기 및 데이터 전달
				if (UTransactionCodeMessage* MsgObj = FindTransactionCodeMessage(TrCode))
				{
					// yyjson 버전의 ProcessData 호출
					MsgObj->ProcessData(&JsonParser, Root);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Handler not found for MESSAGE_ID: %s"), *TrCode);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to find 'MESSAGE_ID' in JSON data: %s"), *Data);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON string via yyjson"));
		}

		ProcessCount++;
	}
}

UTransactionCodeMessage* UDxDataSubsystem::FindTransactionCodeMessage(const FString& TransactionCode)
{
	return TransactionCodeMessageMap.FindRef(TransactionCode);
}
