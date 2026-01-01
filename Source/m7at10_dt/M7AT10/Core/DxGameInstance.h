// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DxGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class M7AT10_DT_API UDxGameInstance : public UGameInstance
{
	GENERATED_BODY()

	// Function
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	static UDxGameInstance* GetInstance();

    static void WriteCustomLog(FString LogText);
private:
	// 오래된 로그 파일 삭제
	// TODO: Window 맞춤형, 재실행 되는 경우에 한하여 삭제. 계속 실행되는 경우 고려 필요
	void DeleteOldLogs(int32 RetentionDays);
protected:

	// Variable
public:
private:
	static UDxGameInstance* Instance;
protected:
};
