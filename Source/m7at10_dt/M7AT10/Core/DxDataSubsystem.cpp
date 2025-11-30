// Fill out your copyright notice in the Description page of Project Settings.


#include "DxDataSubsystem.h"

#include "m7at10_dt/m7at10_dt.h"
#include "m7at10_dt/M7AT10/Api/ApiMessage.h"
#include "m7at10_dt/M7AT10/Api/ApiStruct.h"
#include "m7at10_dt/M7AT10/Lib/YyJsonParser.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeMessage.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeStruct.h"

struct FApiStruct;

void UDxDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// kebab-case를 PascalCase로 변환하는 헬퍼 람다
	auto ToPascalCase = [](const FString& KebabCaseString) -> FString
	{
		TArray<FString> Parts;
		KebabCaseString.ParseIntoArray(Parts, TEXT("-"), true);
		FString PascalCaseString;
		for (const FString& Part : Parts)
		{
			if (!Part.IsEmpty())
			{
				PascalCaseString += Part.Left(1).ToUpper() + Part.Mid(1).ToLower();
			}
		}
		return PascalCaseString;
	};

	const FString ApiTablePath = TEXT("DataTable'/Game/M7AT10/Common/DataTables/DT_Api.DT_Api'");
	UDataTable* LoadedApiTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *ApiTablePath));

	if (LoadedApiTable)
	{
		LoadedApiTable->ForeachRow<FApiStruct>(TEXT("UDxDataSubsystem::Initialize_Api"),
			[this, &ToPascalCase](const FName& RowName, const FApiStruct& Row)
			{
				if (Row.ApiMessageClass)
				{
					// Resource와 Action이 비어있지 않은지 확인
					if (!Row.ApiResource.IsEmpty() && !Row.ApiAction.IsEmpty())
					{
						TObjectPtr<UApiMessage> NewHandler = NewObject<UApiMessage>(this, Row.ApiMessageClass);

						// Resource와 Action을 PascalCase로 변환하여 키 생성
						FString PascalResource = ToPascalCase(Row.ApiResource);
						FString PascalAction = ToPascalCase(Row.ApiAction);
						FString Key = FPaths::Combine(PascalResource, PascalAction);

						ApiMessageMap.Add(Key, NewHandler);
						UE_LOG(LogM7AT10, Log, TEXT("Loaded Handler Key: %s"), *Key);
					}
				}
			});
	}
	else
	{
		UE_LOG(LogM7AT10, Warning, TEXT("ApiStructDataTable failed to load from path: %s"), *ApiTablePath);
	}

	const FString DataTablePath = TEXT("DataTable'/Game/M7AT10/Common/DataTables/DT_TransactionCode.DT_TransactionCode'");
	UDataTable* LoadedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));

	if (LoadedTable)
	{
		LoadedTable->ForeachRow<FTransactionCodeStruct>(TEXT("UDxDataSubsystem::Initialize_WebSocket"),
			[this](const FName& RowName, const FTransactionCodeStruct& Row)
			{
				if (Row.TransactionCodeMessageClass)
				{
					TObjectPtr<UTransactionCodeMessage> NewTc = NewObject<UTransactionCodeMessage>(this, Row.TransactionCodeMessageClass);
					if (NewTc && !NewTc->TransactionCode.IsEmpty())
					{
						TransactionCodeMessageMap.Add(NewTc->TransactionCode, NewTc);
						UE_LOG(LogM7AT10, Log, TEXT("Loaded TransactionCode: %s"), *NewTc->TransactionCode);
					}
				}
			});
	}
	else
	{
		UE_LOG(LogM7AT10, Warning, TEXT("TransactionCodeDataTable failed to load from path: %s"), *DataTablePath);
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
		UE_LOG(LogM7AT10, Log, TEXT("[API] Processing: %s"), *Data);
		// JSON 파싱 및 API 로직 처리

		// 1. Parser 인스턴스 생성
		FYyJsonParser JsonParser;

		// 2. Wrapper JSON 파싱 (meta->{resource, action...}, data가 포함된 JSON)
		if (JsonParser.JsonParse(Data))
		{
			yyjson_val* Root = JsonParser.GetRoot();

			// 3. 'meta' 객체 가져오기
			yyjson_val* MetaNode = JsonParser.JsonParseKeyword(Root, TEXT("meta"));

			if (JsonParser.IsValid(MetaNode))
			{
				// 4. 메타 데이터 추출
				FString Resource = JsonParser.GetString(JsonParser.JsonParseKeyword(MetaNode, TEXT("resource")));
				FString Action = JsonParser.GetString(JsonParser.JsonParseKeyword(MetaNode, TEXT("action")));

				if (!Resource.IsEmpty() && !Action.IsEmpty())
				{
					// 5. 핸들러 찾기
					if (UApiMessage* Handler = FindApiMessage(Resource, Action))
					{
						// 6. 데이터 추출
						yyjson_val* DataNode = JsonParser.JsonParseKeyword(Root, TEXT("data"));

						if (JsonParser.IsValid(DataNode))
						{
							// 7. 핸들러에게 실제 데이터 전달
							Handler->ProcessData(&JsonParser, DataNode);
						}
						else
						{
							// TODO: data가 없거나 null인 경우, 빈 노드라도 넘겨줄 수 있음
							UE_LOG(LogM7AT10, Log, TEXT("[API] data is empty for %s_%s"), *Resource, *Action);
							Handler->ProcessData(&JsonParser, nullptr);
						}
					}
					else
					{
						UE_LOG(LogM7AT10, Warning, TEXT("[API] No Handler found for resource: %s, action: %s"), *Resource, *Action);
					}
				}
				else
				{
					UE_LOG(LogM7AT10, Error, TEXT("[API] Wrapper JSON missing resource or action field"));
				}
			}
			else
			{
				UE_LOG(LogM7AT10, Error, TEXT("[API] Failed to find 'meta' object in JSON"));
			}
		}
		else
		{
			UE_LOG(LogM7AT10, Error, TEXT("[API] Failed to parse Wrapper JSON"));
		}

		ProcessCount++;
	}
}

void UDxDataSubsystem::EnqueueWebSocketData(const FString& Data)
{
	WebSocketDataQueue.Enqueue(Data);
}

UApiMessage* UDxDataSubsystem::FindApiMessage(const FString& Resource, const FString& Action)
{
	// kebab-case를 PascalCase로 변환하는 헬퍼 람다
	auto ToPascalCase = [](const FString& KebabCaseString) -> FString
	{
		TArray<FString> Parts;
		KebabCaseString.ParseIntoArray(Parts, TEXT("-"), true);
		FString PascalCaseString;
		for (const FString& Part : Parts)
		{
			if (!Part.IsEmpty())
			{
				PascalCaseString += Part.Left(1).ToUpper() + Part.Mid(1).ToLower();
			}
		}
		return PascalCaseString;
	};

	// Resource와 Action을 PascalCase로 변환
	FString PascalResource = ToPascalCase(Resource);
	FString PascalAction = ToPascalCase(Action);

	FString Key = FPaths::Combine(PascalResource, PascalAction);
	return ApiMessageMap.FindRef(Key);
}

void UDxDataSubsystem::ProcessWebSocketQueue()
{
	if (WebSocketDataQueue.IsEmpty()) return;

	FString Data;
	int32 ProcessCount = 0;
	const int32 MaxProcessPerFrame = 50;

	while (ProcessCount < MaxProcessPerFrame && WebSocketDataQueue.Dequeue(Data))
	{
		UE_LOG(LogM7AT10, Log, TEXT("[WebSocket] Processing: %s"), *Data);
		// 실시간 데이터 로직 처리

		// 1. Parser 인스턴스 생성 (Stack 메모리)
		FYyJsonParser JsonParser;

		// 2. JSON 파싱
		if (JsonParser.JsonParse(Data))
		{
			yyjson_val* Root = JsonParser.GetRoot();

			// 3. TC_ID
			yyjson_val* MsgIdVal = JsonParser.JsonParseKeyword(Root, TEXT("MESSAGE_ID"));

			if (JsonParser.IsValid(MsgIdVal))
			{
				FString TrCode = JsonParser.GetString(MsgIdVal);

				// 4. 핸들러 찾기 및 데이터 전달
				if (UTransactionCodeMessage* MsgObj = FindTransactionCodeMessage(TrCode))
				{
					// ProcessData 호출
					MsgObj->ProcessData(&JsonParser, Root);
				}
				else
				{
					UE_LOG(LogM7AT10, Warning, TEXT("Handler not found for MESSAGE_ID: %s"), *TrCode);
				}
			}
			else
			{
				UE_LOG(LogM7AT10, Warning, TEXT("Failed to find 'MESSAGE_ID' in JSON data: %s"), *Data);
			}
		}
		else
		{
			UE_LOG(LogM7AT10, Error, TEXT("Failed to parse JSON string via yyjson"));
		}

		ProcessCount++;
	}
}

UTransactionCodeMessage* UDxDataSubsystem::FindTransactionCodeMessage(const FString& TransactionCode)
{
	return TransactionCodeMessageMap.FindRef(TransactionCode);
}

void UDxDataSubsystem::RegisterCraneDataSyncComp(const FString& CraneId, UCraneDataSyncComp* Comp)
{
	if (CraneId.IsEmpty() || !Comp)
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[DxDataSubsystem] Invalid CraneId or Comp for registration"));
		return;
	}

	if (CraneDataSyncCompMap.Contains(CraneId))
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[DxDataSubsystem] CraneId '%s' already registered. Overwriting."), *CraneId);
	}

	CraneDataSyncCompMap.Add(CraneId, Comp);
	UE_LOG(LogM7AT10, Log, TEXT("[DxDataSubsystem] Registered CraneDataSyncComp for CraneId: %s"), *CraneId);
}

void UDxDataSubsystem::UnregisterCraneDataSyncComp(const FString& CraneId)
{
	if (CraneDataSyncCompMap.Remove(CraneId) > 0)
	{
		UE_LOG(LogM7AT10, Log, TEXT("[DxDataSubsystem] Unregistered CraneDataSyncComp for CraneId: %s"), *CraneId);
	}
}

UCraneDataSyncComp* UDxDataSubsystem::FindCraneDataSyncComp(const FString& CraneId)
{
	return CraneDataSyncCompMap.FindRef(CraneId);
}
