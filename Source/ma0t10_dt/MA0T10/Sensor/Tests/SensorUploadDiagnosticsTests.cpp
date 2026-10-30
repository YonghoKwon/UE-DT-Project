#include "SensorUploadDiagnostics.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorUploadLedgerTest,"MA0T10.Memory.UploadLedger",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorUploadLedgerTest::RunTest(const FString&)
{
    using namespace SensorUploadDiagnostics;auto Ledger=MakeUnique<FAccumulator>();
    Ledger->Copy(ESite::PreviewPosition,10,128,256);Ledger->Copy(ESite::PreviewColor,10,32,32);Ledger->Copy(ESite::PreviewPosition,10,128,256);
    Ledger->Copy(ESite::NativeRays,10,4096,8192);Ledger->Copy(ESite::WorldPreviewInstances,10,1024,1024);
    Ledger->Allocation(ESite::PreviewPosition,0,256);Ledger->Allocation(ESite::PreviewPosition,256,512);Ledger->Allocation(ESite::PreviewPosition,512,128);Ledger->Skip(ESite::PreviewColor);
    auto S=Ledger->Snapshot();TestEqual(TEXT("same-frame RHI streams sum, no native/GT double count"),S.PeakFrameBytes[uint32(EDomain::RhiRequest)],uint64(288));
    TestEqual(TEXT("same-frame calls sum"),S.PeakFrameCalls[uint32(EDomain::RhiRequest)],uint64(3));TestEqual(TEXT("native mapped writes separate"),S.PeakFrameBytes[uint32(EDomain::DirectMappedWrite)],uint64(4096));
    TestEqual(TEXT("engine inputs separate"),S.PeakFrameBytes[uint32(EDomain::EngineManagedInput)],uint64(1024));
    const auto& V=S.Sites[uint32(ESite::PreviewPosition)];TestEqual(TEXT("creation"),V.Creations,uint64(1));TestEqual(TEXT("grow"),V.Growths,uint64(1));TestEqual(TEXT("shrink"),V.Shrinks,uint64(1));TestEqual(TEXT("capacity high water"),V.MaxCapacity,uint64(512));
    TestEqual(TEXT("same-site frame sum"),V.PeakFrameBytes,uint64(256));TestEqual(TEXT("skips are not copies"),S.Sites[uint32(ESite::PreviewColor)].CopyCalls,uint64(1));
    Ledger->Copy(ESite::PreviewPosition,266,16,16);S=Ledger->Snapshot();TestEqual(TEXT("ring collision does not accumulate an old frame"),S.PeakFrameBytes[uint32(EDomain::RhiRequest)],uint64(288));
    Ledger->Reset();Ledger->Copy(ESite::PreviewColor,10,1,1);S=Ledger->Snapshot();TestEqual(TEXT("window reset clears frame tags/totals"),S.PeakFrameBytes[uint32(EDomain::RhiRequest)],uint64(1));TestEqual(TEXT("reset clears native peak"),S.PeakFrameBytes[uint32(EDomain::DirectMappedWrite)],uint64(0));
    return true;
}
#endif
