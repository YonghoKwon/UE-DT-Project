// Fill out your copyright notice in the Description page of Project Settings.


#include "DxApiServiceTest.h"

#include "Core/DxApiSubsystem.h"

void UDxApiServiceTest::Initialize(UGameInstance* GameInstance)
{
	if (GameInstance)
	{
		ApiSubsystem = GameInstance->GetSubsystem<UDxApiSubsystem>();
	}
}

void UDxApiServiceTest::LevelInitApiCall()
{
	Step1_RequestOne();
}

void UDxApiServiceTest::Step1_RequestOne()
{
	if (!ApiSubsystem.IsValid()) return;

	// 콜백 연결
	FDxApiCallback OnComplete;
	OnComplete.BindDynamic(this, &UDxApiServiceTest::OnOneResponse);

	ApiSubsystem->DxHttpCall(TEXT("http://localhost:8080/activemq/test/api/response-test"), TEXT("GET"), TEXT(""), TMap<FString, FString>(), OnComplete);
	// ApiSubsystem->DxHttpCall(TEXT("http://localhost:8080/activemq/task/running-tasks"), TEXT("GET"), TEXT(""), TMap<FString, FString>(), OnComplete);
}

void UDxApiServiceTest::OnOneResponse(bool bSuccess, int32 Code, const FString& Content)
{
	if (bSuccess && Code == 200)
	{
		// 성공 -> 다음 단계 진행
		Step2_RequestTwo();
	}
	else
	{
		// 실패 -> 알림
		OnDataReady.Broadcast(false, TEXT("Step1 Failed"));
	}
}

void UDxApiServiceTest::Step2_RequestTwo()
{
	if (!ApiSubsystem.IsValid()) return;

	FDxApiCallback OnComplete;
	OnComplete.BindDynamic(this, &UDxApiServiceTest::OnTwoResponse);

	ApiSubsystem->DxHttpCall(TEXT("http://localhost:8080/activemq/task/running-tasks"), TEXT("GET"), TEXT(""), TMap<FString, FString>(), OnComplete);
}

void UDxApiServiceTest::OnTwoResponse(bool bSuccess, int32 Code, const FString& Content)
{
	if (bSuccess && Code == 200)
	{
		// 더 없으면 여기서 끝
		OnDataReady.Broadcast(true, TEXT("Success"));
	}
	else
	{
		OnDataReady.Broadcast(false, TEXT("Step2 Failed"));
	}
}

