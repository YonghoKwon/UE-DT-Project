#include "WebSocket/TransactionCodeMessage.h"

// void UTransactionCodeMessage::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
// {
// }

UWorld* UTransactionCodeMessage::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
