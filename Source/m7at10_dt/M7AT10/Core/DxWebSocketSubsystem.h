// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IStompClient.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxWebSocketSubsystem.generated.h"

class IStompClient;

DECLARE_DYNAMIC_DELEGATE_TwoParams(FSTOMPRequestCompleted, bool, bSuccess, FString, Error);
DECLARE_DYNAMIC_DELEGATE_OneParam(FSTOMPSubscriptionEvnet, class UWebSocketMessage*, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSTOMPConnectedEvent, FString, ProtocolVersion, FString, SeesionId, FString, ServerString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSTOMPConnectionErrorEvent, FString, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSTOMPErrorEvent, FString, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSTOMPCloseEvnet, FString, Reason);
/**
 * 
 */
UCLASS()
class M7AT10_DT_API UDxWebSocketSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// Function
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION()
	void ConnectWebSocket();
	UFUNCTION()
	void ConnectStompClient(const TMap<FName, FString>& Header);
	UFUNCTION()
	void DisconnectStompClient(const TMap<FName, FString>& Header);
	UFUNCTION()
	void ReceivedMessage(UWebSocketMessage* Message);
	UFUNCTION()
	FString Subscribe(const FString& Destination, const FSTOMPSubscriptionEvnet& EventCallback, const FSTOMPRequestCompleted& CompletionCallback);
	UFUNCTION()
	void Unsubscribe(const FString& Subscription, const FSTOMPRequestCompleted& CompletionCallback);
private:
	UFUNCTION()
	void HandleOnConnected(const FString& ProtocolVersion, const FString& SessionId, const FString& ServerString);
	UFUNCTION()
	void HandleOnConnectionError(const FString& Error);
	UFUNCTION()
	void HandleOnError(const FString& Error);
	UFUNCTION()
	void HandleOnClosed(const FString&  Reason);
protected:

	// Variable
public:
	UPROPERTY()
	FSTOMPSubscriptionEvnet ReceivedMessageEvent;
	UPROPERTY()
	FSTOMPRequestCompleted CompletedMessageEvent;
	UPROPERTY()
	FSTOMPConnectedEvent OnConnected;
private:
protected:
	TSharedPtr<IStompClient> StompClient;
	TMap<FName, FString> LoginInfo;
};
