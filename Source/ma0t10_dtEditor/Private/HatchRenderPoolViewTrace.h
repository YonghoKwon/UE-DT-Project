#pragma once
#include "CoreMinimal.h"
class ISceneViewExtension;
TSharedPtr<ISceneViewExtension,ESPMode::ThreadSafe> CreateHatchRenderPoolViewTrace();
