#include "Api/ApiMessage.h"


UWorld* UApiMessage::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
