// Fill out your copyright notice in the Description page of Project Settings.


#include "TransactionCodeMessage.h"

// void UTransactionCodeMessage::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
// {
// }

UWorld* UTransactionCodeMessage::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
