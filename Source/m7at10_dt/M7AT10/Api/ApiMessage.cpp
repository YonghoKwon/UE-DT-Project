// Fill out your copyright notice in the Description page of Project Settings.


#include "ApiMessage.h"


UWorld* UApiMessage::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
