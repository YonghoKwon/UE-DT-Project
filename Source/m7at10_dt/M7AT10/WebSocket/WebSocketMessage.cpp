// Fill out your copyright notice in the Description page of Project Settings.


#include "WebSocketMessage.h"

#include "IStompMessage.h"

UWebSocketMessage::~UWebSocketMessage()
{
}

const TMap<FName, FString>& UWebSocketMessage::GetHeader() const
{
	return MyMessage->GetHeader();
}

FString UWebSocketMessage::GetBodyAsString() const
{
	return MyMessage->GetBodyAsString();
}

const TArray<uint8> UWebSocketMessage::GetRawBody() const
{
	return TArray<uint8>(MyMessage->GetRawBody(), MyMessage->GetRawBodyLength());
}

int32 UWebSocketMessage::GetRawBodyLength() const
{
	return MyMessage->GetRawBodyLength();
}

FString UWebSocketMessage::GetSubscriptionId() const
{
	return MyMessage->GetSubscriptionId();
}

FString UWebSocketMessage::GetDestination() const
{
	return MyMessage->GetDestination();
}

FString UWebSocketMessage::GetMessageId() const
{
	return MyMessage->GetMessageId();
}

FString UWebSocketMessage::GetAckId() const
{
	return MyMessage->GetAckId();
}
