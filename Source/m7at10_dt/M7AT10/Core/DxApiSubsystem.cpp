// Fill out your copyright notice in the Description page of Project Settings.


#include "DxApiSubsystem.h"

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

// void UDxApiSubsystem::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
// {
// 	if (Response)
// 	{
// 		const FString Result = *Response->GetContentAsString();
// 		UE_LOG(LogTemp, Log, TEXT("Response: %s"), *Result);
// 		// DxDataSubsystem의 큐에 저장
// 	}
// 	else
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("Response Error"));
// 	}
// }

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
	FString FullUrl = FString::Printf(TEXT("%s/%s"), *Server, *RequestApiType);

	// 콜백이 없는 단순 호출 (빈 델리게이트 전달)
	DxHttpCall(FullUrl, MethodType, TEXT(""), DefaultHeaders, FDxApiCallback());
}

void UDxApiSubsystem::DxRequestApiWithParameter(const FString& Server, const FString& RequestApiType, const FString& MethodType, const TArray<FString>& Parameters)
{
	// 안전성 체크: 파라미터가 비어있으면 중단
	if (Parameters.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DxRequestApiWithParameter: Parameters array is empty."));
		return;
	}

	TMap<FString, FString> DefaultHeaders;
	DefaultHeaders.Add(TEXT("Content-Type"), TEXT("application/json"));

	// URL 조합 로직 예시
	FString FullUrl = FString::Printf(TEXT("%s/%s/%s"), *Server, *RequestApiType, *Parameters[0]);

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
			UE_LOG(LogTemp, Log, TEXT("Response [%d]: %s"), ResponseCode, *Content);
			bIsOk = true;
			// 필요한 경우 여기서 DxDataSubsystem 큐에 저장 로직 수행
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Response Error [%d]: %s"), ResponseCode, *Content);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Connection Failed or No Response"));
	}

	// 요청한 곳으로 결과 전달
	Callback.ExecuteIfBound(bIsOk, ResponseCode, Content);
}

