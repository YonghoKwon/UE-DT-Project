// Fill out your copyright notice in the Description page of Project Settings.


#include "CraneInfoGetByBittInOracle.h"

UCraneInfoGetByBittInOracle::UCraneInfoGetByBittInOracle()
{
	ResourceAndAction = "CraneInfoGetByBittInOracle";
}

void UCraneInfoGetByBittInOracle::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
{
	Super::ProcessData(JsonParser, RootNode);
}
