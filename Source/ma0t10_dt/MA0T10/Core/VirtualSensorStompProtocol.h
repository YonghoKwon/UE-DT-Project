#pragma once

#include "CoreMinimal.h"

/** A decoded STOMP 1.2 frame. Body remains binary-safe. */
struct MA0T10_DT_API FVirtualSensorStompFrame
{
	FString Command;
	TMap<FString, FString> Headers;
	TArray<uint8> Body;
};

/** Incremental binary-safe STOMP parser used by the background TCP transport. */
class MA0T10_DT_API FVirtualSensorStompParser
{
public:
	bool Append(const uint8* Data, int32 NumBytes, TArray<FVirtualSensorStompFrame>& OutFrames, FString& OutError);
	void Reset();

	static FString EscapeHeader(const FString& Value);
	static FString UnescapeHeader(const FString& Value);

private:
	bool TryParseOne(FVirtualSensorStompFrame& OutFrame, int32& OutConsumedBytes, FString& OutError) const;
	TArray<uint8> Buffer;
};

