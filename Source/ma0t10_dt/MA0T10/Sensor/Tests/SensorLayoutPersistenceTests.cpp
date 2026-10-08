#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"

class FLayoutPersistenceCheck : public IAutomationLatentCommand
{
public:
    FLayoutPersistenceCheck(FAutomationTestBase* InTest,UVirtualSensorLayoutSubsystem* InLayout,AVirtualSensorActorBase* InLevel,AVirtualSensorActorBase* InOwned,const FString& InRequest)
        : Test(InTest),Layout(InLayout),Level(InLevel),Owned(InOwned),Request(InRequest),Started(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        if(!Layout.IsValid()){Test->AddError(TEXT("layout world disappeared"));return true;}
        auto* L=Layout.Get();L->Tick(0);
        const auto Status=L->GetOperationStatus(Request);
        if(Status.State==EVirtualSensorLayoutOperation::Writing){if(FPlatformTime::Seconds()-Started<10)return false;Test->AddError(TEXT("layout writer timeout"));return true;}
        Test->TestTrue(TEXT("file saved only after writer completion"),Status.State==EVirtualSensorLayoutOperation::Saved);
        Test->TestTrue(TEXT("saved layout is discoverable"),L->GetSavedLayoutNames().Contains(TEXT("persistence fixture")));
        FString Error;const FString SensorId=Owned->GetSensorId();
        Level->SetActorLocation(FVector(999,999,999));L->DeleteSensor(Owned.Get(),Error);
        auto* Extra=L->CreateSensorAtPose(EVirtualSensorKind::Camera,FTransform(FVector(999,0,0)),Error,false);
        const FString Preview=L->PreviewLayoutRestore(TEXT("persistence fixture"));
        Test->TestTrue(TEXT("restore preview is readonly"),Level->GetActorLocation().Equals(FVector(999,999,999))&&IsValid(Extra));
        Test->TestTrue(TEXT("restore ticket ready"),L->GetOperationStatus(Preview).State==EVirtualSensorLayoutOperation::ReadyToApply);
        Test->TestTrue(TEXT("whole layout applied"),L->ApplyLayoutRestore(Preview,Error));
        Test->TestTrue(TEXT("preplaced identity and original pose preserved"),IsValid(Level.Get())&&Level->GetActorLocation().Equals(FVector(10,20,30),.001));
        Test->TestFalse(TEXT("surplus owned actor removed"),L->OwnsSensor(Extra));
        const auto Items=L->GetLayoutSensors();Test->TestEqual(TEXT("saved inventory restored"),Items.Num(),2);
        auto* Restored=L->Coordinator()->GetSensorActors().FindByPredicate([&](auto* A){return A&&A->GetSensorId()==SensorId;});
        Test->TestTrue(TEXT("saved SensorId retained on recreated runtime actor"),Restored!=nullptr);
        if(Restored)Test->TestTrue(TEXT("preview stopped flag restored"),!(*Restored)->IsSensorRunning());
        for(const auto& S:L->Coordinator()->StreamPublisherComponent->GetStreamStatuses())Test->TestFalse(TEXT("restore never starts Topic sending"),S.bEnabled);
        const FString Stale=L->PreviewLayoutRestore(TEXT("persistence fixture"));Level->SetActorLocation(FVector(25,0,0));
        Test->TestFalse(TEXT("pose change invalidates preview"),L->ApplyLayoutRestore(Stale,Error));
        Test->TestTrue(TEXT("rejection leaves pose intact"),Level->GetActorLocation().Equals(FVector(25,0,0)));
        const FString Busy=L->PreviewLayoutRestore(TEXT("persistence fixture"));
        L->Coordinator()->StreamPublisherComponent->StartStream(EVirtualSensorStreamKind::PointCloud,Level->GetSensorId());
        Test->TestFalse(TEXT("collection lock rechecked at apply"),L->ApplyLayoutRestore(Busy,Error));
        L->Coordinator()->StreamPublisherComponent->StopAllStreams(Level->GetSensorId());
        FString Json;FFileHelper::LoadFileToString(Json,*Status.Path);FVirtualSensorLayoutDocument D;
        FJsonObjectConverter::JsonObjectStringToUStruct(Json,&D);D.MapPackage=TEXT("/Game/WrongMap");
        FJsonObjectConverter::UStructToJsonObjectString(D,Json);FFileHelper::SaveStringToFile(Json,*Status.Path);
        const auto Wrong=L->GetOperationStatus(L->PreviewLayoutRestore(TEXT("persistence fixture")));
        Test->TestTrue(TEXT("other map rejected without partial changes"),Wrong.State==EVirtualSensorLayoutOperation::Failed&&Level->GetActorLocation().Equals(FVector(25,0,0)));
        FFileHelper::SaveStringToFile(TEXT("{broken"),*Status.Path);
        Test->TestTrue(TEXT("corrupt file rejected"),L->GetOperationStatus(L->PreviewLayoutRestore(TEXT("persistence fixture"))).State==EVirtualSensorLayoutOperation::Failed);
        return true;
    }
private:
    FAutomationTestBase* Test;TWeakObjectPtr<UVirtualSensorLayoutSubsystem> Layout;TWeakObjectPtr<AVirtualSensorActorBase> Level,Owned;FString Request;double Started;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorLayoutPersistenceTest,"MA0T10.SensorLayout.Persistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorLayoutPersistenceTest::RunTest(const FString&)
{
    auto* W=FAutomationEditorCommonUtils::CreateNewMap();auto* C=W->SpawnActor<AVirtualSensorCoordinator>();auto* L=W->GetSubsystem<UVirtualSensorLayoutSubsystem>();
    L->StorageRootOverrideForTests=FPaths::ProjectSavedDir()/TEXT("Reports/SensorPlacementWorkspace/layout-fixture")/FGuid::NewGuid().ToString();
    auto* Level=W->SpawnActor<AVirtualLidarSensorActor>();Level->SetActorLocation(FVector(10,20,30));Level->StopSensor();C->RegisterSensorActor(Level);
    FString Error;auto* Owned=L->CreateSensorAtPose(EVirtualSensorKind::Camera,FTransform(FVector(100,200,300)),Error,false);
    if(!Owned){AddError(Error);return false;}
    const FString Request=L->RequestSaveLayout(TEXT("persistence fixture"));
    TestTrue(TEXT("initial write asynchronous"),L->GetOperationStatus(Request).State==EVirtualSensorLayoutOperation::Writing);
    TestTrue(TEXT("single writer rejects overlapping request"),L->GetOperationStatus(L->RequestSaveLayout(TEXT("second"))).State==EVirtualSensorLayoutOperation::Failed);
    ADD_LATENT_AUTOMATION_COMMAND(FLayoutPersistenceCheck(this,L,Level,Owned,Request));return true;
}
#endif
