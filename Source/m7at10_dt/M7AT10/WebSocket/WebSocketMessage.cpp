// Fill out your copyright notice in the Description page of Project Settings.


#include "WebSocketMessage.h"

#include "IStompMessage.h"

UWebSocketMessage::~UWebSocketMessage()
{
}

const TMap<FName, FString>& UWebSocketMessage::GetHeader() const
{
	return Headers;
}

FString UWebSocketMessage::GetBodyAsString() const
{
	return BodyString;
}

const TArray<uint8> UWebSocketMessage::GetRawBody() const
{
	return RawBody;
}

int32 UWebSocketMessage::GetRawBodyLength() const
{
	return RawBody.Num();
}

FString UWebSocketMessage::GetSubscriptionId() const
{
	return SubscriptionId;
}

FString UWebSocketMessage::GetDestination() const
{
	return Destination;
}

FString UWebSocketMessage::GetMessageId() const
{
	return MessageId;
}

FString UWebSocketMessage::GetAckId() const
{
	return AckId;
}

void UWebSocketMessage::InitializeFromStompMessage(const IStompMessage& Message)
{
	// 데이터 깊은 복사(Deep Copy)
	BodyString = Message.GetBodyAsString();

	// RawBody 복사
	const uint8* Data = (const uint8*)Message.GetRawBody();
	int32 Length = Message.GetRawBodyLength();
	if (Data && Length > 0)
	{
		RawBody.SetNumUninitialized(Length);
		FMemory::Memcpy(RawBody.GetData(), Data, Length);
	}
	else
	{
		RawBody.Empty();
	}

	Headers = Message.GetHeader();
	SubscriptionId = Message.GetSubscriptionId();
	Destination = Message.GetDestination();
	MessageId = Message.GetMessageId();
	AckId = Message.GetAckId();
}
