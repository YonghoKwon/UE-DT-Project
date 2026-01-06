#include "Core/DxApiSubsystem.h"

#include "DTCore.h"
#include "Core/DxDataSubsystem.h"
#include "HttpModule.h"
#include "Core/DTCoreSettings.h"
#include "Interfaces/IHttpResponse.h"

UDxApiSubsystem::UDxApiSubsystem()
{
	const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();

	if (Settings->ApiDataTable.ToSoftObjectPath().IsValid())
	{
		DT_Api = Settings->ApiDataTable.LoadSynchronous();
	}
}

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

void UDxApiSubsystem::DxRequestApi(const FName& RowName, FDxApiCallback Callback)
{
	// DT_Api 데이터테이블이 유효한지 체크
	if (!DT_Api)
	{
		UE_LOG(LogBase, Error, TEXT("DxRequestApi: DT_Api is not set."));
		Callback.ExecuteIfBound(false, 0, TEXT("DT_Api is not set"));
		return;
	}

	// 데이터테이블에서 Row 검색
	FApiStruct* ApiData = DT_Api->FindRow<FApiStruct>(RowName, TEXT("DxRequestApi"));
	if (!ApiData)
	{
		UE_LOG(LogBase, Error, TEXT("DxRequestApi: Row '%s' not found in DT_Api."), *RowName.ToString());
		Callback.ExecuteIfBound(false, 0, FString::Printf(TEXT("Row '%s' not found in DT_Api."), *RowName.ToString()));
		return;
	}

	// 예시: 기본 헤더 설정
	TMap<FString, FString> DefaultHeaders;
	DefaultHeaders.Add(TEXT("Content-Type"), TEXT("application/json"));

	// ApiType에 따라 Server URL 결정
	FString ServerUrl = GetServerUrl(ApiData->ApiType);
	// HTTP Method
	FString MethodType = GetHttpStr(ApiData->ApiMethod);

	// URL 조합 (예: Server + / + RequestApiType) - 실제 로직에 맞게 수정 필요
	FString FullUrl = ServerUrl + ApiData->ApiUrl;

	// 콜백이 없는 단순 호출 (빈 델리게이트 전달)
	DxHttpCall(FullUrl, MethodType, TEXT(""), DefaultHeaders, FDxApiCallback());
}

void UDxApiSubsystem::DxRequestApiWithParameter(const FName& RowName, FDxApiCallback Callback,
	const TArray<FString>& Parameters)
{
	// DT_Api 데이터테이블이 유효한지 체크
	if (!DT_Api)
	{
		UE_LOG(LogBase, Error, TEXT("DxRequestApi: DT_Api is not set."));
		Callback.ExecuteIfBound(false, 0, TEXT("DT_Api is not set"));
		return;
	}

	// 데이터테이블에서 Row 검색
	FApiStruct* ApiData = DT_Api->FindRow<FApiStruct>(RowName, TEXT("DxRequestApi"));
	if (!ApiData)
	{
		UE_LOG(LogBase, Error, TEXT("DxRequestApi: Row '%s' not found in DT_Api."), *RowName.ToString());
		Callback.ExecuteIfBound(false, 0, FString::Printf(TEXT("Row '%s' not found in DT_Api."), *RowName.ToString()));
		return;
	}

	// ApiType에 따라 Server URL 결정
	FString ServerUrl = GetServerUrl(ApiData->ApiType);
	// HTTP Method
	FString MethodType = GetHttpStr(ApiData->ApiMethod);

	// URL 조합 (예: Server + / + RequestApiType) - 실제 로직에 맞게 수정 필요
	FString FullUrl = ServerUrl + ApiData->ApiUrl;

	// Path Parameter가 있으면 URL에 추가
	for (const FString& PathParm : Parameters)
	{
		if (!PathParm.IsEmpty())
		{
			FullUrl += TEXT("/") + PathParm;
		}
	}

	// 헤더 설정
	TMap<FString, FString> DefaultHeaders;
	DefaultHeaders.Add(TEXT("Content-Type"), TEXT("application/json"));

	// Body 설정
	// FString ContentString = ApiData->Body;

	UE_LOG(LogBase, Log, TEXT("DxRequestApiWithParameter: Calling API - %s, %s"), *MethodType, *FullUrl);

	// HTTP 요청 실행
	DxHttpCall(FullUrl, MethodType, TEXT(""), DefaultHeaders, Callback);
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

FString UDxApiSubsystem::GetServerUrl(EApiType ApiType)
{
	switch (ApiType)
	{
	case EApiType::Local:
		return TEXT("http://localhost:8090");
	case EApiType::Test:
		return TEXT("http://172.28.79.32:8090"); // 개발계 (dt3test)
	case EApiType::Prod:
		return TEXT("");
	default:
		return TEXT("http://localhost:8090");
	}
}
FString UDxApiSubsystem::GetHttpStr(EApiMethod ApiMethod)
{
	// HTTP Method 변환
	const UEnum* ApiMethodEnum = StaticEnum<EApiMethod>();
	return ApiMethodEnum->GetDisplayNameTextByValue(static_cast<int64>(ApiMethod)).ToString();
}

