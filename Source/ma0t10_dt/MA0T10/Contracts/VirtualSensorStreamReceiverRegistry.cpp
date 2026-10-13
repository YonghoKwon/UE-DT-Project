#include "VirtualSensorStreamReceiverRegistry.h"
#include "Misc/ScopeLock.h"

namespace
{
	FCriticalSection RegistryMutex;
	TSharedPtr<const IVirtualSensorStreamReceiverCodec, ESPMode::ThreadSafe> RegisteredCodec;
}

void VirtualSensorStreamReceiverRegistry::Register(const TSharedRef<const IVirtualSensorStreamReceiverCodec, ESPMode::ThreadSafe>& Codec)
{
	FScopeLock Lock(&RegistryMutex);
	check(!RegisteredCodec.IsValid());
	RegisteredCodec = Codec;
}

void VirtualSensorStreamReceiverRegistry::Unregister()
{
	FScopeLock Lock(&RegistryMutex);
	RegisteredCodec.Reset();
}

TSharedPtr<const IVirtualSensorStreamReceiverCodec, ESPMode::ThreadSafe> VirtualSensorStreamReceiverRegistry::Get()
{
	FScopeLock Lock(&RegistryMutex);
	return RegisteredCodec;
}
