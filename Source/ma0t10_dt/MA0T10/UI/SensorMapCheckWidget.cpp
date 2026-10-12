#include "SensorMapCheckWidget.h"
#include "VirtualSensorToolRig.h"
#include "VirtualSensorUiHostActor.h"
#include "SlabSimulationUiHostActor.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolWorkspaceStyle.h"
#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "Core/DTCoreSettings.h"
#include "Core/DxGameMode.h"
#include "Core/DxWidgetSubsystem.h"
#include "WebSocket/TransactionCodeStruct.h"
#include "Manager/DxLevelStruct.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/FactoryAgentScenarioTC.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorOutputPreflight.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabTrackReferenceActor.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

void USensorMapCheckWidget::RefreshChecks()
{
    auto* World=GetWorld();if(!World)return;
    const auto* Settings=GetDefault<UDTCoreSettings>();
    if(!bTablesLoaded){bTablesLoaded=true;TransactionTable=Settings->WebSocketDataTable.LoadSynchronous();LevelTable=Settings->LevelDataTable.LoadSynchronous();}
    TArray<FSensorMapCheckRow> NewRows;
    auto Add=[&](const TCHAR* Key,bool Ok,const FString& Status,const TCHAR* Remedy){NewRows.Add({Key,Status,Remedy,Ok});};
    int32 RigCount=0;AVirtualSensorToolRig* Rig=nullptr;
    for(TActorIterator<AVirtualSensorToolRig> It(World);It;++It)if(!It->IsActorBeingDestroyed()){Rig=*It;++RigCount;}
    Add(TEXT("센서 도구 설치"),RigCount==1&&Rig->IsSetupReady(),RigCount==1?Rig->SetupMessage:FString::Printf(TEXT("Rig %d개"),RigCount),TEXT("VirtualSensorToolRig를 맵에 하나만 배치하세요."));
    int32 Coordinators=0,MapUiHostCount=0;AVirtualSensorCoordinator* Coordinator=nullptr;
    for(TActorIterator<AVirtualSensorCoordinator> It(World);It;++It)if(!It->IsActorBeingDestroyed()){Coordinator=*It;++Coordinators;}
    for(TActorIterator<AVirtualSensorUiHostActor> It(World);It;++It)if(!It->IsActorBeingDestroyed())++MapUiHostCount;
    Add(TEXT("장비와 화면 구성"),Coordinators==1&&MapUiHostCount==1,FString::Printf(TEXT("Coordinator %d개 · 센서 UI %d개"),Coordinators,MapUiHostCount),TEXT("중복 구성을 제거하고 Rig의 연결 대상을 확인하세요."));
    TArray<ASlabActor*> Slabs;TSet<FString> Receivers;bool Routing=true,Tracks=true;
    for(TActorIterator<ASlabActor> It(World);It;++It)if(!It->IsActorBeingDestroyed())
    {Slabs.Add(*It);Tracks&=IsValid(It->TrackReference)&&It->TrackReference->GetWorld()==World;Routing&=!It->ReceiverId.IsEmpty()&&!Receivers.Contains(It->ReceiverId);Receivers.Add(It->ReceiverId);}
    const bool HasTarget=RigCount==1&&IsValid(Rig->TargetSlab);
    Add(TEXT("Slab 대상"),HasTarget,RigCount==1?Rig->GetSlabSetupMessage():FString::Printf(TEXT("Slab %d개"),Slabs.Num()),TEXT("SlabActor를 배치하세요. 여러 개면 Rig의 TargetSlab을 지정하세요."));
    Add(TEXT("TrackReference 연결"),!Slabs.IsEmpty()&&Tracks,Slabs.IsEmpty()?TEXT("Slab이 없습니다"):Tracks?TEXT("모든 Slab에 연결됨"):TEXT("연결되지 않은 Slab이 있습니다"),TEXT("SlabActor의 TrackReference에 같은 맵의 기준 Actor를 연결하세요."));
    Add(TEXT("ReceiverId"),!Slabs.IsEmpty()&&Routing,Slabs.IsEmpty()?TEXT("Slab이 없습니다"):Routing?TEXT("빈 값과 중복 없음"):TEXT("빈 값 또는 중복 있음"),TEXT("각 Slab의 ReceiverId를 서로 다르게 지정하세요. 전문의 대상 ID와 맞추세요."));
    bool HasTc=false;
    if(TransactionTable&&TransactionTable->GetRowStruct()==FTransactionCodeStruct::StaticStruct())TransactionTable->ForeachRow<FTransactionCodeStruct>(TEXT("MapCheck"),[&](const FName&,const auto& Row){HasTc|=Row.TransactionCodeMessageClass&&Row.TransactionCodeMessageClass->IsChildOf(UFactoryAgentScenarioTC::StaticClass());});
    Add(TEXT("TC DataTable · IFactory-agent"),HasTc,HasTc?TEXT("시나리오 처리 클래스 등록됨"):TEXT("시나리오 처리 클래스 누락"),TEXT("DTCore Settings의 WebSocketDataTable에 FactoryAgentScenarioTC 등록을 확인하세요."));
    const bool Topic=Settings->WebSocketTopics.Contains(TEXT("topic.scenario"));
    Add(TEXT("DTCore WebSocketTopics"),Topic,Topic?TEXT("topic.scenario 등록됨"):TEXT("topic.scenario 누락"),TEXT("Project Settings → DTCore Settings → WebSocketTopics에 topic.scenario를 추가하세요."));
    Add(TEXT("DTCore 업무 서버"),false,TEXT("현재 연결 확인 불가 · 공개 상태 조회 API 없음"),TEXT("업무 서버 실행과 DTCore 연결 로그를 확인하세요. 센서 출력 서버와 별개입니다."));
    const auto Output=Coordinators==1?UVirtualSensorOutputPreflight::InspectForPresentation(Coordinator,FVirtualSlabSensorOutputSelection(),{},EVirtualSensorPreflightPurpose::ManualStreamStart,false):FVirtualSensorOutputPreflightResult();
    Add(TEXT("센서 출력 서버"),Output.bTransportConnected,Output.bTransportConnected?TEXT("연결됨"):TEXT("연결 안 됨"),TEXT("도구 → 서버 연결 설정에서 출력 서버 연결을 확인하세요."));
    const bool GameMode=World->GetAuthGameMode()&&World->GetAuthGameMode()->IsA<ADxGameMode>();
    Add(TEXT("DTCore GameMode"),GameMode,GameMode?TEXT("DTCore 계열"):TEXT("DTCore 계열이 아닙니다"),TEXT("World Settings에서 프로젝트의 DTCore 계열 GameMode를 지정하세요. 자동 교체하지 않습니다."));
    FString Package=World->GetOutermost()->GetName();if(!World->StreamingLevelsPrefix.IsEmpty())Package=Package.Replace(*World->StreamingLevelsPrefix,TEXT(""));
    bool Listed=false;
    if(LevelTable&&LevelTable->GetRowStruct()==FDxLevelStruct::StaticStruct())LevelTable->ForeachRow<FDxLevelStruct>(TEXT("MapCheck"),[&](const FName&,const auto& Row){Listed|=Row.IsUse&&Row.Level.ToSoftObjectPath().GetLongPackageName()==Package;});
    Add(TEXT("DT_DxLevel · 패키징/레벨 전환"),Listed,Listed?TEXT("사용 행으로 등록됨"):TEXT("현재 맵 등록 없음"),TEXT("DTCore LevelDataTable의 사용 행에 맵을 등록하고 패키징의 cook 목록을 별도로 확인하세요."));
    for(int32 I=0;I<NewRows.Num();++I)
    {
        auto& Row=NewRows[I];
        if(Rows.IsValidIndex(I)&&Rows[I].Status==Row.Status&&Rows[I].bOk==Row.bOk){Row.LabelText=Rows[I].LabelText;Row.RemedyText=Rows[I].RemedyText;}
        else {Row.LabelText=FText::FromString(FString(Row.bOk?TEXT("✓ "):TEXT("⚠ "))+Row.Key+TEXT(" · ")+Row.Status);Row.RemedyText=FText::FromString(Row.Remedy);}
    }
    Rows=MoveTemp(NewRows);
}
TSharedRef<SWidget> USensorMapCheckWidget::RebuildWidget()
{
    RefreshChecks();auto List=SNew(SVerticalBox);
    for(int32 I=0;I<Rows.Num();++I)
    {
        List->AddSlot().AutoHeight().Padding(0,7)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([this](){return MapFont(14);}).AutoWrapText(true)
             .ColorAndOpacity_Lambda([this,I](){return Rows.IsValidIndex(I)&&Rows[I].bOk?FSensorOwnedPresentationStyle::Accent():FSensorOwnedPresentationStyle::Warning();})
             .Text_Lambda([this,I](){return Rows.IsValidIndex(I)?Rows[I].LabelText:FText::GetEmpty();})]
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font_Lambda([this](){return MapFont(12);}).AutoWrapText(true).ColorAndOpacity(FSensorOwnedPresentationStyle::Muted()).Text(Rows[I].RemedyText)]];
    }
    return SNew(SBorder).BorderImage(FSensorOwnedPresentationStyle::PanelBrush()).BorderBackgroundColor(FSensorOwnedPresentationStyle::Panel()).Padding(16)
        [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Font_Lambda([this](){return MapFont(18);}).Text(FText::FromString(TEXT("맵 점검")))]
         +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&FSensorOwnedPresentationStyle::Button()).Text(FText::FromString(TEXT("닫기"))).OnClicked_Lambda([this](){CloseWidget();return FReply::Handled();})]]
         +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(STextBlock).Font_Lambda([this](){return MapFont(12);}).AutoWrapText(true).Text(FText::FromString(TEXT("읽기 전용 · 설정과 맵을 자동 변경하지 않습니다.")))]
         +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[List]]];
}
FSlateFontInfo USensorMapCheckWidget::MapFont(int32 Size) const
{auto* W=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>():nullptr;return FSensorToolWorkspaceStyle::Font(Size,W?W->GetOwnedPanelFontScale():1.f);}
void USensorMapCheckWidget::NativeTick(const FGeometry& Geometry,float Delta)
{Super::NativeTick(Geometry,Delta);const double Now=FPlatformTime::Seconds();if(IsVisible()&&Now>=NextRefresh){NextRefresh=Now+1;RefreshChecks();}}
void USensorMapCheckWidget::NativeDestruct()
{if(auto* GI=GetGameInstance())if(auto* Dx=GI->GetSubsystem<UDxWidgetSubsystem>()){Dx->UnregisterExternalInputBlocker(this);Dx->UnregisterWidget(this);}Super::NativeDestruct();}
