// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DxWebSocketSubsystem.generated.h"

class IStompClient;

DECLARE_DYNAMIC_DELEGATE_TwoParams(FSTOMPRequestCompleted, bool, bSuccess, FString, Error);
DECLARE_DYNAMIC_DELEGATE_OneParam(FSTOMPSubscriptionEvent, class UWebSocketMessage*, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSTOMPConnectedEvent, FString, ProtocolVersion, FString, SessionId, FString, ServerString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSTOMPConnectionErrorEvent, FString, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSTOMPErrorEvent, FString, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSTOMPCloseEvent, FString, Reason);
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

	UFUNCTION(Category = "DxWebSocket")
	void ConnectWebSocket();
	UFUNCTION(Category = "DxWebSocket")
	void ConnectStompClient(const TMap<FName, FString>& Header);
	UFUNCTION(Category = "DxWebSocket")
	void DisconnectStompClient(const TMap<FName, FString>& Header);
	UFUNCTION(Category = "DxWebSocket")
	void ReceivedMessage(UWebSocketMessage* Message);
	UFUNCTION(Category = "DxWebSocket")
	FString Subscribe(const FString& Destination, const FSTOMPSubscriptionEvent& EventCallback, const FSTOMPRequestCompleted& CompletionCallback);
	UFUNCTION(Category = "DxWebSocket")
	void Unsubscribe(const FString& Subscription, const FSTOMPRequestCompleted& CompletionCallback);
private:
	UFUNCTION(Category = "DxWebSocket")
	void HandleOnConnected(const FString& ProtocolVersion, const FString& SessionId, const FString& ServerString);
	UFUNCTION(Category = "DxWebSocket")
	void HandleOnConnectionError(const FString& Error);
	UFUNCTION(Category = "DxWebSocket")
	void HandleOnError(const FString& Error);
	UFUNCTION(Category = "DxWebSocket")
	void HandleOnClosed(const FString&  Reason);
protected:

	// Variable
public:
	UPROPERTY(BlueprintReadOnly, Category = "DxWebSocket")
	FSTOMPSubscriptionEvent ReceivedMessageEvent;
	UPROPERTY(BlueprintReadOnly, Category = "DxWebSocket")
	FSTOMPRequestCompleted CompletedMessageEvent;
	UPROPERTY(BlueprintReadOnly, Category = "DxWebSocket")
	FSTOMPConnectedEvent OnConnected;
private:
protected:
	TSharedPtr<IStompClient> StompClient;
	TMap<FName, FString> LoginInfo;
};
