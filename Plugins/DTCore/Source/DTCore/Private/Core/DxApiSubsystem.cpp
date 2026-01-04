#include "Core/DxApiSubsystem.h"

#include "DTCore.h"
#include "Core/DxDataSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

void UDxApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	HttpModule = &FHttpModule::Get();
}

void UDxApiSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDxApiSubsystem::DxHttpCall(const FString& FullUrl, const FString& Verb, const FString& ContentString, const TMap<FString, FString>& Headers, FDxApiCallback Callback)
{
	if (!HttpModule) return;

	TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
	Request->SetURL(FullUrl);
	// Method 설정 (GET, POST, PUT, DELETE 등)
	Request->SetVerb(Verb);
	// 헤더 설정
	for (const auto& Header : Headers)
	{
		Request->SetHeader(Header.Key, Header.Value);
	}
	// Body 설정 (POST/PUT 등의 경우)
	if (!ContentString.IsEmpty())
	{
		Request->SetContentAsString(ContentString);
	}

	Request->OnProcessRequestComplete().BindUObject(this, &UDxApiSubsystem::InternalOnResponseReceived, Callback);

	Request->ProcessRequest();
}

void UDxApiSubsystem::DxRequestApi(const FString& Server, const FString& RequestApiType, const FString& MethodType)
{
	// 예시: 기본 헤더 설정
	TMap<FString, FString> DefaultHeaders;
	DefaultHeaders.Add(TEXT("Content-Type"), TEXT("application/json"));

	// URL 조합 (예: Server + / + RequestApiType) - 실제 로직에 맞게 수정 필요
	FString FullUrl = FPaths::Combine(Server, RequestApiType);

	// 콜백이 없는 단순 호출 (빈 델리게이트 전달)
	DxHttpCall(FullUrl, MethodType, TEXT(""), DefaultHeaders, FDxApiCallback());
}

void UDxApiSubsystem::DxRequestApiWithParameter(const FString& Server, const FString& RequestApiType, const FString& MethodType, const TArray<FString>& Parameters)
{
	// 안전성 체크: 파라미터가 비어있으면 중단
	if (Parameters.Num() == 0)
	{
		UE_LOG(LogBase, Warning, TEXT("DxRequestApiWithParameter: Parameters array is empty."));
		return;
	}

	TMap<FString, FString> DefaultHeaders;
	DefaultHeaders.Add(TEXT("Content-Type"), TEXT("application/json"));

	// URL 조합 로직 예시
	FString FullUrl = FPaths::Combine(Server, RequestApiType, Parameters[0]);

	DxHttpCall(FullUrl, MethodType, TEXT(""), DefaultHeaders, FDxApiCallback());
}

void UDxApiSubsystem::InternalOnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FDxApiCallback Callback)
{
	int32 ResponseCode = 0;
	FString Content = TEXT("");
	bool bIsOk = false;

	if (bWasSuccessful && Response.IsValid())
	{
		ResponseCode = Response->GetResponseCode();
		Content = *Response->GetContentAsString();

		// HTTP 코드가 200번대(성공)인지 확인
		if (EHttpResponseCodes::IsOk(ResponseCode))
		{
			UE_LOG(LogBase, Log, TEXT("Response [%d]: %s"), ResponseCode, *Content);
			bIsOk = true;

			// DxDataSubsystem에 데이터 저장
			TObjectPtr<UGameInstance> GameInstance = GetGameInstance();
			if (GameInstance)
			{
				TObjectPtr<UDxDataSubsystem> DataSubsystem = GameInstance->GetSubsystem<UDxDataSubsystem>();
				if (DataSubsystem)
				{
					DataSubsystem->EnqueueApiData(Content);
				}
			}
		}
		else
		{
			UE_LOG(LogBase, Warning, TEXT("Response Error [%d]: %s"), ResponseCode, *Content);
		}
	}
	else
	{
		UE_LOG(LogBase, Error, TEXT("Connection Failed or No Response"));
	}

	Callback.ExecuteIfBound(bIsOk, ResponseCode, Content);
}

