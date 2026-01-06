#include "Core/DxDataSubsystem.h"

#include "DTCore.h"
#include "Api/ApiMessage.h"
#include "Api/ApiStruct.h"
#include "Lib/YyJsonParser.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "WebSocket/TransactionCodeStruct.h"
#include "Async/Async.h"
#include "Core/DTCoreSettings.h"

struct FApiStruct;

UDxDataSubsystem::UDxDataSubsystem()
{
	const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();

	if (Settings->ApiDataTable.ToSoftObjectPath().IsValid())
	{
		ApiDataTable = Settings->ApiDataTable.LoadSynchronous();
	}

	if (Settings->WebSocketDataTable.ToSoftObjectPath().IsValid())
	{
		WebSocketDataTable = Settings->WebSocketDataTable.LoadSynchronous();
	}
}

void UDxDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (ApiDataTable)
	{
		ApiDataTable->ForeachRow<FApiStruct>(TEXT("UDxDataSubsystem::Initialize_Api"),
			[this](const FName& RowName, const FApiStruct& Row)
			{
				if (Row.ApiMessageClass)
				{
					// Resource와 Action이 비어있지 않은지 확인
					if (!Row.ApiResource.IsEmpty() && !Row.ApiAction.IsEmpty())
					{
						TObjectPtr<UApiMessage> NewHandler = NewObject<UApiMessage>(this, Row.ApiMessageClass);

						// Resource와 Action을 PascalCase로 변환하여 키 생성
						FString PascalResource = Row.ApiResource;
						FString PascalAction = Row.ApiAction;
						FString Key = FPaths::Combine(PascalResource, PascalAction);

						ApiMessageMap.Add(Key, NewHandler);
						UE_LOG(LogBase, Log, TEXT("Loaded Handler Key: %s"), *Key);
					}
				}
			});
	}
	else
	{
		UE_LOG(LogBase, Warning, TEXT("ApiStructDataTable failed to load"));
	}

	CachedHandlerApiMessageMap = MakeShared<TMap<FString, UApiMessage*>>();

	for (const auto& Pair : ApiMessageMap)
	{
		if (Pair.Value)
		{
			CachedHandlerApiMessageMap->Add(Pair.Key, Pair.Value);
		}
	}

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
						UE_LOG(LogBase, Log, TEXT("Loaded TransactionCode: %s"), *NewTc->TransactionCode);
					}
				}
			});
	}
	else
	{
		UE_LOG(LogBase, Warning, TEXT("TransactionCodeDataTable failed to load"));
	}

	CachedHandlerTransactionCodeMessageMap = MakeShared<TMap<FString, UTransactionCodeMessage*>>();

	for (const auto& Pair : TransactionCodeMessageMap)
	{
		if (Pair.Value)
		{
			CachedHandlerTransactionCodeMessageMap->Add(Pair.Key, Pair.Value);
		}
	}
}

void UDxDataSubsystem::Deinitialize()
{
	Super::Deinitialize();

	WebSocketHandlerInstanceCache.Empty();
	ApiHandlerInstanceCache.Empty();
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

    const double StartTime = FPlatformTime::Seconds();
    const double TimeBudget = 0.005;

    TArray<FString> BatchDataChunk;
    BatchDataChunk.Reserve(100);

    FString Data;
    while (!ApiDataQueue.IsEmpty())
    {
       if ((FPlatformTime::Seconds() - StartTime) > TimeBudget) break;
       if (ApiDataQueue.Dequeue(Data))
       {
          BatchDataChunk.Add(Data);
       }
    }

    if (BatchDataChunk.Num() > 0)
    {
        // 3. 백그라운드 처리
        AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, BatchDataChunk, SharedMap = CachedHandlerApiMessageMap]()
        {
            FYyJsonParser JsonParser;

            struct FApiResultItem {
                UApiMessage* Handler;
                TSharedPtr<FApiDataBase> Data;
            };
            TArray<FApiResultItem> BatchResults;
            BatchResults.Reserve(BatchDataChunk.Num());

            for (const FString& SingleData : BatchDataChunk)
            {
	            if (JsonParser.JsonParse(SingleData))
	            {
		            yyjson_val* Root = JsonParser.GetRoot();
		            yyjson_val* MetaNode = JsonParser.JsonParseKeyword(Root, TEXT("meta"));

		            if (JsonParser.IsValid(MetaNode))
		            {
			            FString Resource = JsonParser.
				            GetString(JsonParser.JsonParseKeyword(MetaNode, TEXT("resource")));
			            FString Action = JsonParser.GetString(JsonParser.JsonParseKeyword(MetaNode, TEXT("action")));

			            // Key (Resource_Action)
			            FString Key = FPaths::Combine(Resource, Action);

			            if (UApiMessage** HandlerPtr = SharedMap->Find(Key))
			            {
				            UApiMessage* Handler = *HandlerPtr;
				            if (IsValid(Handler))
				            {
					            // [중요] ParseToStruct 함수는 내부에서 NewObject 등을 쓰지 않는 순수 로직이어야 함
					            TSharedPtr<FApiDataBase> ParsedData = Handler->ParseToStruct(SingleData);

					            if (ParsedData.IsValid())
					            {
						            BatchResults.Add({Handler, ParsedData});
					            }
				            }
			            }
		            }
	            }
            }

            if (BatchResults.Num() > 0)
            {
	            AsyncTask(ENamedThreads::GameThread, [this, BatchResults]()
	            {
		            for (const auto& Item : BatchResults)
		            {
			            if (IsValid(Item.Handler))
			            {
				            // 최종 데이터 처리 실행
				            Item.Handler->ProcessStructData(Item.Data);
			            }
		            }
	            });
            }
        });
    }
}

void UDxDataSubsystem::EnqueueWebSocketData(const FString& Data)
{
	WebSocketDataQueue.Enqueue(Data);
}

void UDxDataSubsystem::ProcessWebSocketQueue()
{
	if (WebSocketDataQueue.IsEmpty()) return;

    const double StartTime = FPlatformTime::Seconds();
    const double TimeBudget = 0.005;

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

	if (BatchDataChunk.Num() > 0)
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, BatchDataChunk, SharedMap = CachedHandlerTransactionCodeMessageMap]()
		{
			FYyJsonParser JsonParser;

			struct FResultItem
			{
				UTransactionCodeMessage* Handler;
				TSharedPtr<FTransactionCodeDataBase> Data;
			};
			TArray<FResultItem> BatchResults;
			BatchResults.Reserve(BatchDataChunk.Num());

			for (const FString& SingleData : BatchDataChunk)
			{
				if (JsonParser.JsonParse(SingleData))
				{
					yyjson_val* Root = JsonParser.GetRoot();
					yyjson_val* MsgIdVal = JsonParser.JsonParseKeyword(Root, TEXT("MESSAGE_ID"));

					if (JsonParser.IsValid(MsgIdVal))
					{
						FString TrCode = JsonParser.GetString(MsgIdVal);

						if (UTransactionCodeMessage** HandlerPtr = SharedMap->Find(TrCode))
						{
							UTransactionCodeMessage* Handler = *HandlerPtr;
							if (IsValid(Handler))
							{
								// [중요] ParseToStruct 함수는 내부에서 NewObject 등을 쓰지 않는 순수 로직이어야 함
								TSharedPtr<FTransactionCodeDataBase> ParsedData = Handler->ParseToStruct(SingleData);

								if (ParsedData.IsValid())
								{
									BatchResults.Add({ Handler, ParsedData });
								}
							}
						}
					}
				}
			}

			if (BatchResults.Num() > 0)
			{
				AsyncTask(ENamedThreads::GameThread, [this, BatchResults]()
				{
					// 메인 스레드
					for (const auto& Item : BatchResults)
					{
						if (IsValid(Item.Handler))
						{
							// 최종 데이터 처리 실행
							Item.Handler->ProcessStructData(Item.Data);
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
	UE_LOG(LogBase, Log, TEXT("Received Message at: %s"), *Timestamp);
	UE_LOG(LogBase, Log, TEXT("Total Processed: %d"), TotalProcessedCount.GetValue());
}

UApiMessage* UDxDataSubsystem::GetOrLoadApiHandler(UClass* HandlerClass)
{
	if (!HandlerClass) return nullptr;

	if (UApiMessage** FoundHandler = ApiHandlerInstanceCache.Find(HandlerClass))
	{
		UApiMessage* Handler = *FoundHandler;
		if (IsValid(Handler) && !Handler->IsUnreachable())
		{
			return Handler;
		}
		else
		{
			ApiHandlerInstanceCache.Remove(HandlerClass);
		}
	}

	UApiMessage* NewHandler = NewObject<UApiMessage>(this, HandlerClass);
	if (NewHandler)
	{
		ApiHandlerInstanceCache.Add(HandlerClass, NewHandler);
	}
	return NewHandler;
}

UTransactionCodeMessage* UDxDataSubsystem::GetOrLoadTransactionHandler(UClass* HandlerClass)
{
	if (!HandlerClass) return nullptr;

	if (UTransactionCodeMessage** FoundHandler = WebSocketHandlerInstanceCache.Find(HandlerClass))
	{
		UTransactionCodeMessage* Handler = *FoundHandler;

		if (IsValid(Handler) && !Handler->IsUnreachable())
		{
			return Handler;
		}
		else
		{
			WebSocketHandlerInstanceCache.Remove(HandlerClass);
		}
	}

	UTransactionCodeMessage* NewHandler = NewObject<UTransactionCodeMessage>(this, HandlerClass);
	if (NewHandler)
	{
		WebSocketHandlerInstanceCache.Add(HandlerClass, NewHandler);
	}

	return NewHandler;
}
