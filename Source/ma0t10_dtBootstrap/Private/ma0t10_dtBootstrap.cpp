#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

namespace
{
	constexpr double Mlx80WebSocketServiceIntervalSeconds = 0.001;
}

class FMa0t10DtBootstrapModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// UE 5.3's libwebsockets manager reads this value when the WebSockets
		// module starts. PostConfigInit runs before DTCore/STOMP can load that
		// module, so a 1 MiB binary PCD is serviced across enough writable
		// callbacks for ML-X(80)'s 20 Hz stream without modifying user config.
		if (GConfig)
		{
			GConfig->SetDouble(
				TEXT("WebSockets.LibWebSockets"),
				TEXT("ThreadTargetFrameTimeInSeconds"),
				Mlx80WebSocketServiceIntervalSeconds,
				GEngineIni);
		}
	}
};

IMPLEMENT_MODULE(FMa0t10DtBootstrapModule, ma0t10_dtBootstrap);
