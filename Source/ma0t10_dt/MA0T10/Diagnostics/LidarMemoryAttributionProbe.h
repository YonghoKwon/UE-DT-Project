#pragma once
#include "CoreMinimal.h"

class UWorld;
class FJsonObject;

// Opt-in observations only. Never calls DTCore hover/prune or changes its containers.
namespace LidarMemoryAttribution
{
bool IsEnabled();
void Begin(UWorld* World);
void Snapshot(UWorld* World,double Seconds,const TSharedRef<FJsonObject>& Row);
void End();
}
