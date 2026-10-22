#include "CoilTestReportPaths.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilReportPathAliasTest,"MA0T10.CoilLoading.ReportPaths.Aliases",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilReportPathAliasTest::RunTest(const FString&)
{
    TestEqual(TEXT("default report root"),CoilTestReportPaths::Resolve(TEXT("")),FPaths::ProjectSavedDir()/TEXT("Reports/COIL"));
    TestEqual(TEXT("legacy quoted path remains supported"),CoilTestReportPaths::Resolve(TEXT("-CoilPhotoReport=\"C:/reports/legacy photos\"")),FString(TEXT("C:/reports/legacy photos")));
    TestEqual(TEXT("new option"),CoilTestReportPaths::Resolve(TEXT("-CoilReport=C:/reports/new")),FString(TEXT("C:/reports/new")));
    TestEqual(TEXT("new option takes priority regardless of argument order"),CoilTestReportPaths::Resolve(TEXT("-CoilPhotoReport=C:/old -CoilReport=C:/new")),FString(TEXT("C:/new")));
    TestEqual(TEXT("legacy option cannot override new option"),CoilTestReportPaths::Resolve(TEXT("-CoilReport=C:/new -CoilPhotoReport=C:/old")),FString(TEXT("C:/new")));
    return true;
}
#endif
