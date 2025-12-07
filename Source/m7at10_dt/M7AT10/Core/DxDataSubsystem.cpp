// Fill out your copyright notice in the Description page of Project Settings.


#include "DxDataSubsystem.h"

#include "m7at10_dt/m7at10_dt.h"
#include "m7at10_dt/M7AT10/Api/ApiMessage.h"
#include "m7at10_dt/M7AT10/Api/ApiStruct.h"
#include "m7at10_dt/M7AT10/Lib/YyJsonParser.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeMessage.h"
#include "m7at10_dt/M7AT10/WebSocket/TransactionCodeStruct.h"
#include "Async/Async.h"

struct FApiStruct;

UDxDataSubsystem::UDxDataSubsystem()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> ApiDataTableFinder(TEXT("/Game/M7AT10/Common/DataTables/DT_Api.DT_Api"));

	if (ApiDataTableFinder.Succeeded())
	{
		ApiDataTable = ApiDataTableFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> WebSocketDataTableFinder(TEXT("/Game/M7AT10/Common/DataTables/DT_TransactionCode.DT_TransactionCode"));

	if (WebSocketDataTableFinder.Succeeded())
	{
		WebSocketDataTable = WebSocketDataTableFinder.Object;
	}
}

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

	// const FString ApiTablePath = TEXT("DataTable'/Game/M7AT10/Common/DataTables/DT_Api.DT_Api'");
	// UDataTable* LoadedApiTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *ApiTablePath));

	if (ApiDataTable)
	{
		ApiDataTable->ForeachRow<FApiStruct>(TEXT("UDxDataSubsystem::Initialize_Api"),
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
		UE_LOG(LogM7AT10, Warning, TEXT("ApiStructDataTable failed to load"));
	}

	// const FString DataTablePath = TEXT("DataTable'/Game/M7AT10/Common/DataTables/DT_TransactionCode.DT_TransactionCode'");
	// UDataTable* LoadedTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));

	if (WebSocketDataTable)
	{
		WebSocketDataTable->ForeachRow<FTransactionCodeStruct>(TEXT("UDxDataSubsystem::Initialize_WebSocket"),
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
		UE_LOG(LogM7AT10, Warning, TEXT("TransactionCodeDataTable failed to load"));
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

	double StartTime = FPlatformTime::Seconds();
	const double TimeBudget = 0.005; // 5ms (0.005초) 동안만 처리

	// 1. Parser 인스턴스 생성
	FYyJsonParser JsonParser;

	FString Data;
	while (!ApiDataQueue.IsEmpty())
	{
		// 시간이 다 되었는지 체크
		if ((FPlatformTime::Seconds() - StartTime) > TimeBudget)
		{
			break; // 다음 프레임에 계속 처리
		}

		if (ApiDataQueue.Dequeue(Data))
		{
			UE_LOG(LogM7AT10, Log, TEXT("[API] Processing: %s"), *Data);
			// JSON 파싱 및 API 로직 처리

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
		}
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
	// if (WebSocketDataQueue.IsEmpty()) return;
	//
	// const double StartTime = FPlatformTime::Seconds();
	// const double TimeBudget = 0.005; // 5ms (0.005초) 동안만 처리
	//
	// TSharedPtr<TMap<FString, TSubclassOf<UTransactionCodeMessage>>> SharedHandlerMap =
	// 		MakeShared<TMap<FString, TSubclassOf<UTransactionCodeMessage>>>();
	//
	// for (const auto& Pair : TransactionCodeMessageMap)
	// {
	// 	if (Pair.Value) SharedHandlerMap->Add(Pair.Key, Pair.Value->GetClass());
	// }
	//
	// FString Data;
	// while (!WebSocketDataQueue.IsEmpty())
	// {
	// 	// 시간이 다 되었는지 체크
	// 	if ((FPlatformTime::Seconds() - StartTime) > TimeBudget)
	// 	{
	// 		break; // 다음 프레임에 계속 처리
	// 	}
	//
	// 	if (WebSocketDataQueue.Dequeue(Data))
	// 	{
	// 		// 디버그용. 처리할 때마다 카운트 증가 및 화면 표시
	// 		TotalProcessedCount++;
	//
	// 		if (TotalProcessedCount % 1000 == 0) // 100개 단위로만 로그 찍기 (로그창 보호)
	// 		{
	// 			GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::Green,
	// 				FString::Printf(TEXT("Processed: %d / Queue Remaining: %d"),
	// 				TotalProcessedCount, WebSocketDataQueue.IsEmpty() ? 0 : 1));
	// 		}
	//
	// 		// [백그라운드] : yyjson 파싱, 메시지 ID 찾기
	// 		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, Data, SharedHandlerMap]()
	// 		{
	// 			FYyJsonParser JsonParser;
	// 			if (JsonParser.JsonParse(Data))
	// 			{
	// 				yyjson_val* Root = JsonParser.GetRoot();
	// 				yyjson_val* MsgIdVal = JsonParser.JsonParseKeyword(Root, TEXT("MESSAGE_ID"));
	//
	// 				if (JsonParser.IsValid(MsgIdVal))
	// 				{
	// 					FString TrCode = JsonParser.GetString(MsgIdVal);
	//
	// 					if (const TSubclassOf<UTransactionCodeMessage>* HandlerClassPtr = SharedHandlerMap->Find(TrCode))
	// 					{
	// 						UTransactionCodeMessage* DefaultHandler = GetMutableDefault<UTransactionCodeMessage>(
	// 							*HandlerClassPtr);
	//
	// 						if (DefaultHandler)
	// 						{
	// 							TSharedPtr<FTransactionCodeDataBase> ParsedData = DefaultHandler->ParseToStruct(Data);
	//
	// 							if (ParsedData.IsValid())
	// 							{
	// 								// [메인 스레드] : NewObject 생성, ProcessStructData 실행
	// 								AsyncTask(ENamedThreads::GameThread, [this, ParsedData, HandlerClass = *HandlerClassPtr]()
	// 								{
	// 									UTransactionCodeMessage* MsgObj = NewObject<UTransactionCodeMessage>(this, HandlerClass);
	// 									MsgObj->ProcessStructData(ParsedData);
	// 								});
	// 							}
	// 						}
	// 					}
	// 				}
	// 			}
	// 		});
	// 	}
	// }

	if (WebSocketDataQueue.IsEmpty()) return;

    const double StartTime = FPlatformTime::Seconds();
    const double TimeBudget = 0.005;

    // 1. 핸들러 맵 복사
    TSharedPtr<TMap<FString, TSubclassOf<UTransactionCodeMessage>>> SharedHandlerMap =
        MakeShared<TMap<FString, TSubclassOf<UTransactionCodeMessage>>>();

    for (const auto& Pair : TransactionCodeMessageMap)
    {
       if (Pair.Value) SharedHandlerMap->Add(Pair.Key, Pair.Value->GetClass());
    }

    // 2. 입력 데이터 묶음 (Batch Input)
    TArray<FString> BatchDataChunk;
    BatchDataChunk.Reserve(2000);

    FString Data;
    while (!WebSocketDataQueue.IsEmpty())
    {
       if ((FPlatformTime::Seconds() - StartTime) > TimeBudget) break;
       if (WebSocketDataQueue.Dequeue(Data))
       {
          BatchDataChunk.Add(Data);
       }
    }

	// 3. 백그라운드로 묶음 전송
	if (BatchDataChunk.Num() > 0)
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, BatchDataChunk, SharedHandlerMap]()
		{
			FYyJsonParser JsonParser;

			// [중요 1] 결과를 담을 '바구니'를 만듭니다. (중간 저장소)
			struct FResultItem
			{
				TSubclassOf<UTransactionCodeMessage> HandlerClass;
				TSharedPtr<FTransactionCodeDataBase> Data;
			};
			TArray<FResultItem> BatchResults;
			BatchResults.Reserve(BatchDataChunk.Num());

			// --- 반복문 시작 ---
			for (const FString& SingleData : BatchDataChunk)
			{
				if (JsonParser.JsonParse(SingleData))
				{
					yyjson_val* Root = JsonParser.GetRoot();
					yyjson_val* MsgIdVal = JsonParser.JsonParseKeyword(Root, TEXT("MESSAGE_ID"));

					if (JsonParser.IsValid(MsgIdVal))
					{
						FString TrCode = JsonParser.GetString(MsgIdVal);

						if (const TSubclassOf<UTransactionCodeMessage>* HandlerClassPtr = SharedHandlerMap->
							Find(TrCode))
						{
							// [최적화] NewObject 대신 CDO(기본객체) 사용
							UTransactionCodeMessage* DefaultHandler = GetMutableDefault<UTransactionCodeMessage>(
								*HandlerClassPtr);

							if (DefaultHandler)
							{
								TSharedPtr<FTransactionCodeDataBase> ParsedData = DefaultHandler->ParseToStruct(SingleData);

								if (ParsedData.IsValid())
								{
									// [중요 2] 바로 보내지 말고 바구니에 담기만 함! (AsyncTask 호출 X)
									BatchResults.Add({*HandlerClassPtr, ParsedData});
								}
							}
						}
					}
				}
			}
			// --- 반복문 종료 ---

			// [중요 3] 바구니가 찼으면 '한 번만' 메인 스레드로 보냄 (2000번 -> 1번)
			if (BatchResults.Num() > 0)
			{
				AsyncTask(ENamedThreads::GameThread, [this, BatchResults]()
				{
					// 여기서 메인 스레드 루프를 돕니다.
					for (const auto& Item : BatchResults)
					{
						// [최적화] NewObject 대신 CDO 재사용 (단순 데이터 적용일 경우 훨씬 빠름)
						UTransactionCodeMessage* Handler = GetMutableDefault<
							UTransactionCodeMessage>(Item.HandlerClass);
						if (Handler)
						{
							Handler->ProcessStructData(Item.Data);
						}

						// 카운터 증가
						TotalProcessedCount.Increment();
					}
				});
			}
		});
	}

    // 화면 디버깅
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Green,
            FString::Printf(TEXT("Total Processed: %d"), TotalProcessedCount.GetValue()));
    }

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H:%M:%S.%s"));
	UE_LOG(LogM7AT10, Log, TEXT("Received Message at: %s"), *Timestamp);
	UE_LOG(LogM7AT10, Log, TEXT("Total Processed: %d"), TotalProcessedCount.GetValue());
}

UTransactionCodeMessage* UDxDataSubsystem::FindTransactionCodeMessage(const FString& TransactionCode)
{
	return TransactionCodeMessageMap.FindRef(TransactionCode);
}