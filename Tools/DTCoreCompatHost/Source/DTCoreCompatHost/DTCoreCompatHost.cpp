#include "Modules/ModuleManager.h"
#include "Core/DxDataType.h"
#include "DTCore.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, DTCoreCompatHost, "DTCoreCompatHost");

// 소비 프로젝트의 확장 타입은 플러그인 enum을 변경하지 않는다.
struct FCompatExtendedData final : FDxDataBase
{
    int32 GetType() const override { return 1001; }
};

// Shipping에서 로그 매크로도 실제 C++ 컴파일 대상으로 만든다.
void CompileShippingLogContract(UWorld* World)
{
    DX_LOG(World, TEXT("DTCore compatibility host"));
}
