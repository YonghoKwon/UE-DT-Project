#include "ma0t10_dt/MA0T10/Core/VirtualSensorStompProtocol.h"

namespace
{
int32 FindByte(const TArray<uint8>& Buffer, uint8 Needle, int32 Start)
{
	for (int32 Index = FMath::Max(0, Start); Index < Buffer.Num(); ++Index)
	{
		if (Buffer[Index] == Needle) return Index;
	}
	return INDEX_NONE;
}

FString Utf8Slice(const TArray<uint8>& Buffer, int32 Start, int32 Length)
{
	if (Length <= 0) return FString();
	FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Buffer.GetData() + Start), Length);
	return FString(Converted.Length(), Converted.Get());
}
}

bool FVirtualSensorStompParser::Append(
	const uint8* Data,
	int32 NumBytes,
	TArray<FVirtualSensorStompFrame>& OutFrames,
	FString& OutError)
{
	OutError.Reset();
	if (NumBytes < 0 || (!Data && NumBytes > 0))
	{
		OutError = TEXT("Invalid STOMP input buffer.");
		return false;
	}
	if (NumBytes > 0) Buffer.Append(Data, NumBytes);
	if (Buffer.Num() > 64 * 1024 * 1024)
	{
		OutError = TEXT("STOMP receive buffer exceeded 64 MiB.");
		Reset();
		return false;
	}

	while (!Buffer.IsEmpty())
	{
		// STOMP heartbeats are bare LF/CRLF bytes between frames.
		while (!Buffer.IsEmpty() && (Buffer[0] == '\n' || Buffer[0] == '\r'))
		{
			Buffer.RemoveAt(0, 1, false);
		}
		if (Buffer.IsEmpty()) break;

		FVirtualSensorStompFrame Frame;
		int32 Consumed = 0;
		if (!TryParseOne(Frame, Consumed, OutError))
		{
			return OutError.IsEmpty();
		}
		if (Consumed <= 0) break;
		Buffer.RemoveAt(0, Consumed, false);
		OutFrames.Add(MoveTemp(Frame));
	}
	return true;
}

void FVirtualSensorStompParser::Reset()
{
	Buffer.Reset();
}

bool FVirtualSensorStompParser::TryParseOne(
	FVirtualSensorStompFrame& OutFrame,
	int32& OutConsumedBytes,
	FString& OutError) const
{
	OutConsumedBytes = 0;
	const int32 CommandEnd = FindByte(Buffer, '\n', 0);
	if (CommandEnd == INDEX_NONE) return false;
	int32 CommandLength = CommandEnd;
	if (CommandLength > 0 && Buffer[CommandLength - 1] == '\r') --CommandLength;
	OutFrame.Command = Utf8Slice(Buffer, 0, CommandLength);
	if (OutFrame.Command.IsEmpty())
	{
		OutError = TEXT("STOMP command is empty.");
		return false;
	}

	int32 Cursor = CommandEnd + 1;
	while (true)
	{
		const int32 LineEnd = FindByte(Buffer, '\n', Cursor);
		if (LineEnd == INDEX_NONE) return false;
		int32 LineLength = LineEnd - Cursor;
		if (LineLength > 0 && Buffer[LineEnd - 1] == '\r') --LineLength;
		if (LineLength == 0)
		{
			Cursor = LineEnd + 1;
			break;
		}
		const FString Line = Utf8Slice(Buffer, Cursor, LineLength);
		int32 Colon = INDEX_NONE;
		if (!Line.FindChar(TEXT(':'), Colon) || Colon <= 0)
		{
			OutError = FString::Printf(TEXT("Malformed STOMP header: %s"), *Line.Left(128));
			return false;
		}
		const FString Key = UnescapeHeader(Line.Left(Colon));
		const FString Value = UnescapeHeader(Line.Mid(Colon + 1));
		if (!OutFrame.Headers.Contains(Key)) OutFrame.Headers.Add(Key, Value);
		Cursor = LineEnd + 1;
	}

	int64 ContentLength = -1;
	if (const FString* LengthText = OutFrame.Headers.Find(TEXT("content-length")))
	{
		if (!LexTryParseString(ContentLength, **LengthText) || ContentLength < 0 || ContentLength > MAX_int32)
		{
			OutError = TEXT("Invalid STOMP content-length.");
			return false;
		}
	}

	int32 BodyLength = 0;
	int32 Terminator = INDEX_NONE;
	if (ContentLength >= 0)
	{
		BodyLength = static_cast<int32>(ContentLength);
		Terminator = Cursor + BodyLength;
		if (Buffer.Num() <= Terminator) return false;
		if (Buffer[Terminator] != 0)
		{
			OutError = TEXT("STOMP content-length body is not NUL terminated.");
			return false;
		}
	}
	else
	{
		Terminator = FindByte(Buffer, 0, Cursor);
		if (Terminator == INDEX_NONE) return false;
		BodyLength = Terminator - Cursor;
	}

	OutFrame.Body.Append(Buffer.GetData() + Cursor, BodyLength);
	OutConsumedBytes = Terminator + 1;
	return true;
}

FString FVirtualSensorStompParser::EscapeHeader(const FString& Value)
{
	FString Result;
	Result.Reserve(Value.Len());
	for (const TCHAR Char : Value)
	{
		switch (Char)
		{
		case TEXT('\\'): Result += TEXT("\\\\"); break;
		case TEXT('\r'): Result += TEXT("\\r"); break;
		case TEXT('\n'): Result += TEXT("\\n"); break;
		case TEXT(':'): Result += TEXT("\\c"); break;
		default: Result.AppendChar(Char); break;
		}
	}
	return Result;
}

FString FVirtualSensorStompParser::UnescapeHeader(const FString& Value)
{
	FString Result;
	Result.Reserve(Value.Len());
	for (int32 Index = 0; Index < Value.Len(); ++Index)
	{
		if (Value[Index] != TEXT('\\') || Index + 1 >= Value.Len())
		{
			Result.AppendChar(Value[Index]);
			continue;
		}
		const TCHAR Escaped = Value[++Index];
		switch (Escaped)
		{
		case TEXT('n'): Result.AppendChar(TEXT('\n')); break;
		case TEXT('r'): Result.AppendChar(TEXT('\r')); break;
		case TEXT('c'): Result.AppendChar(TEXT(':')); break;
		case TEXT('\\'): Result.AppendChar(TEXT('\\')); break;
		default: Result.AppendChar(Escaped); break;
		}
	}
	return Result;
}

