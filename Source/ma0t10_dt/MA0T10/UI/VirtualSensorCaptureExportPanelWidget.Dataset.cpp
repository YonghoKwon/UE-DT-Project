#include "VirtualSensorCaptureExportPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "SensorOwnedPresentationStyle.h"
#include "SensorToolUiText.h"
#include "ma0t10_dt/MA0T10/Scenario/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SWrapBox.h"

TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedDatasetSection()
{
    auto Body=SNew(SBox);
    auto Area=SNew(SExpandableArea).InitiallyCollapsed(true)
        .OnAreaExpansionChanged_Lambda([this,Body](bool Open)
        {bDatasetExpanded=Open;Body->SetContent(Open?BuildOwnedDatasetControls():SNullWidget::NullWidget);if(Open)RefreshDatasetPresentation();})
        .HeaderContent()[SNewSensorTool(STextBlock).AutoWrapText(true)
            .Text_Lambda([this,On=SensorToolUiText::OptionState(SensorToolUiText::Dataset(),true),Off=SensorToolUiText::OptionState(SensorToolUiText::Dataset(),false)](){return DatasetDraft.Enabled?On:Off;})]
        .BodyContent()[Body];
    DatasetArea=Area;return Area;
}
void UVirtualSensorCaptureExportPanelWidget::RefreshDatasetPresentation()
{
    if(!GetWorld())return;auto* Service=GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>();
    DatasetStatus=Service?Service->Message:FString();
    if(DatasetStatus.IsEmpty())DatasetStatus=DatasetDraft.Enabled?TEXT("대상을 선택하고 조건을 확인한 뒤 생성 시작을 누르세요."):TEXT("꺼짐 · 데이터셋 생성 사용을 켠 뒤 생성 시작을 누르세요.");
    FVirtualSensorEditableState L,C;
    const auto* Lidar=Cast<AVirtualLidarSensorActor>(DatasetLidar.Get());const auto* Camera=Cast<AVirtualCameraSensorActor>(DatasetCamera.Get());
    if(Lidar&&Camera&&Lidar->ReadEditableState(L)&&Camera->ReadEditableState(C))
    {
        const double Points=double(L.LidarHorizontalSamples)*L.LidarVerticalChannels*FMath::Max(1,L.MaxHitsPerRay);
        const double Pixels=double(C.CameraResolution.X)*C.CameraResolution.Y;
        DatasetEstimate=FString::Printf(TEXT("저장 위치: Saved/Datasets/%s_시각/ · %d장면 · 선택 Camera %d×%d, LiDAR 최대 %.0f점 · 비압축 환산 약 %.1f MiB (PNG/EXR 압축 후 용량은 장면에 따라 달라집니다)"),*DatasetDraft.Name,DatasetDraft.Scenes,C.CameraResolution.X,C.CameraResolution.Y,Points,DatasetDraft.Scenes*(Points*38+Pixels*(DatasetDraft.CameraTruth?13.:4.)+4096)/1048576.);
    }
    else DatasetEstimate=TEXT("LiDAR·Camera 대상을 선택하면 예상 용량을 표시합니다 · 저장 위치: Saved/Datasets");
}
TSharedRef<SWidget> UVirtualSensorCaptureExportPanelWidget::BuildOwnedDatasetControls()
{
    auto Column=SNew(SVerticalBox);
    auto Text=[this](const FString& S){return SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(S));};
    auto Service=[this](){return GetWorld()?GetWorld()->GetSubsystem<UVirtualDatasetSubsystem>():nullptr;};
    Column->AddSlot().AutoHeight().Padding(0,8)[Text(TEXT("직접 켜고 시작할 때만 파일을 만듭니다. 선택 대상의 측정·시나리오·출력은 먼저 중지하세요. 종료 시 원래 자세·설정을 복원합니다."))];
    auto Check=[this,Column,Text,Service](const FString& Label,bool* Value)
    {Column->AddSlot().AutoHeight().Padding(0,4)[SNew(SCheckBox).CheckBoxContentUsesAutoWidth(false).IsEnabled_Lambda([Service](){auto* S=Service();return !S||!S->IsActive();})
        .IsChecked_Lambda([Value](){return *Value?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .ToolTipText(FText::FromString(Label)).OnCheckStateChanged_Lambda([this,Value](ECheckBoxState V){*Value=V==ECheckBoxState::Checked;RefreshDatasetPresentation();})
        [SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([Value,On=SensorToolUiText::OptionState(FText::FromString(Label),true),Off=SensorToolUiText::OptionState(FText::FromString(Label),false)](){return *Value?On:Off;})]];};
    Check(TEXT("데이터셋 생성 사용"),&DatasetDraft.Enabled);
    Check(TEXT("카메라 정답 포함"),&DatasetDraft.CameraTruth);
    Check(TEXT("LiDAR 사실성 포함"),&DatasetDraft.LidarFidelity);
    auto Field=[this,Column,Text,Service](const FString& Label,const FString& Value,TFunction<void(const FString&)> Apply)
    {Column->AddSlot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label)]
        +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(SEditableTextBox).Text(FText::FromString(Value)).IsEnabled_Lambda([Service](){auto* S=Service();return !S||!S->IsActive();})
        .OnTextCommitted_Lambda([this,Apply](const FText& T,ETextCommit::Type){Apply(T.ToString());RefreshDatasetPresentation();})]];};
    Field(TEXT("이름"),DatasetDraft.Name,[this](const FString& S){DatasetDraft.Name=S;});
    Field(TEXT("장면 수 (1–10000)"),LexToString(DatasetDraft.Scenes),[this](const FString& S){LexTryParseString(DatasetDraft.Scenes,*S);});
    Field(TEXT("랜덤 시드"),LexToString(DatasetDraft.Seed),[this](const FString& S){LexTryParseString(DatasetDraft.Seed,*S);});
    Field(TEXT("안정화 프레임"),LexToString(DatasetDraft.SettleFrames),[this](const FString& S){LexTryParseString(DatasetDraft.SettleFrames,*S);});
    auto Target=[this,Column,Text,Service](const FString& Label,UClass* Class,TWeakObjectPtr<AActor>* Selected)
    {
        auto Caption=SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(Label+TEXT(": ")+(Selected->IsValid()?Selected->Get()->GetName():TEXT("선택하세요"))));
        Column->AddSlot().AutoHeight().Padding(0,4)[SNewSensorTool(SButton).IsEnabled_Lambda([Service](){auto* S=Service();return !S||!S->IsActive();})
            .ToolTipText_Lambda([Selected](){return FText::FromString(Selected->IsValid()?Selected->Get()->GetName():TEXT("누를 때마다 다음 대상을 선택합니다. 대상이 없으면 현재 맵의 구성을 확인하세요."));})
            .OnClicked_Lambda([this,Label,Class,Selected,Caption]()
            {
                TArray<AActor*> Items;for(TActorIterator<AActor> It(GetWorld(),Class);It;++It)Items.Add(*It);
                Items.Sort([](const AActor& A,const AActor& B){return A.GetName()<B.GetName();});
                const int32 Next=Items.IsEmpty()?INDEX_NONE:(Items.IndexOfByKey(Selected->Get())+1)%Items.Num();*Selected=Next==INDEX_NONE?nullptr:Items[Next];
                Caption->SetText(FText::FromString(Label+TEXT(": ")+(Selected->IsValid()?Selected->Get()->GetName():TEXT("대상이 없습니다"))));RefreshDatasetPresentation();return FReply::Handled();
            })[Caption]];
    };
    Column->AddSlot().AutoHeight().Padding(0,8)[Text(TEXT("대상 선택 · 버튼을 누를 때마다 다음 대상으로 바뀝니다. 조명은 조명 랜덤화에만 필요합니다."))];
    Target(TEXT("Slab 선택"),ASlabActor::StaticClass(),&DatasetSlab);
    Target(TEXT("LiDAR 선택"),AVirtualLidarSensorActor::StaticClass(),&DatasetLidar);
    Target(TEXT("Camera 선택"),AVirtualCameraSensorActor::StaticClass(),&DatasetCamera);
    Target(TEXT("조명 선택 · 이동 가능한 방향광"),ADirectionalLight::StaticClass(),&DatasetLight);
    auto Random=SNew(SVerticalBox);
    auto Range=[this,Random,Text,Service](const FString& Name,FVirtualDatasetRange* R)
    {
        FNumberFormattingOptions DisplayFormat;DisplayFormat.UseGrouping=false;
        Random->AddSlot().AutoHeight().Padding(0,6)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox).CheckBoxContentUsesAutoWidth(false).IsEnabled_Lambda([Service](){auto* S=Service();return !S||!S->IsActive();})
                .IsChecked_Lambda([R](){return R->Enabled?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([R](ECheckBoxState V){R->Enabled=V==ECheckBoxState::Checked;})[Text(Name)]]
            +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[SNewSensorTool(SEditableTextBox).ToolTipText(FText::FromString(Name+TEXT(" · 최솟값"))).Text(FText::AsNumber(R->Min,&DisplayFormat)).IsEnabled_Lambda([Service](){auto* S=Service();return !S||!S->IsActive();})
                .OnTextCommitted_Lambda([R](const FText& V,ETextCommit::Type){LexTryParseString(R->Min,*V.ToString());})]
            +SHorizontalBox::Slot().FillWidth(1)[SNewSensorTool(SEditableTextBox).ToolTipText(FText::FromString(Name+TEXT(" · 최댓값"))).Text(FText::AsNumber(R->Max,&DisplayFormat)).IsEnabled_Lambda([Service](){auto* S=Service();return !S||!S->IsActive();})
                .OnTextCommitted_Lambda([R](const FText& V,ETextCommit::Type){LexTryParseString(R->Max,*V.ToString());})]]];
    };
    Range(TEXT("Slab 수평 회전 (°)"),&DatasetDraft.Yaw);Range(TEXT("Slab 앞뒤 기울기 (°)"),&DatasetDraft.Pitch);Range(TEXT("Slab 좌우 기울기 (°)"),&DatasetDraft.Roll);
    Range(TEXT("Slab X 오프셋 (cm)"),&DatasetDraft.X);Range(TEXT("Slab Y 오프셋 (cm)"),&DatasetDraft.Y);Range(TEXT("Slab Z 오프셋 (cm)"),&DatasetDraft.Z);
    Range(TEXT("Slab 크기 배율"),&DatasetDraft.Size);Range(TEXT("표면 0 기본 / 1 고온 / 2 산화 / 3 냉각 / 4 무광"),&DatasetDraft.Surface);
    Range(TEXT("표면 거칠기 (0–1)"),&DatasetDraft.Roughness);Range(TEXT("산화도 (0–1)"),&DatasetDraft.Oxidation);
    Range(TEXT("조명 강도 (lux)"),&DatasetDraft.LightIntensity);Range(TEXT("조명 상하 각도 (°)"),&DatasetDraft.LightPitch);Range(TEXT("조명 수평 회전 (°)"),&DatasetDraft.LightYaw);
    Range(TEXT("센서 설치 위치 오차 · 각 축 (cm)"),&DatasetDraft.SensorPosition);Range(TEXT("센서 설치 각도 오차 · 각 축 (°)"),&DatasetDraft.SensorAngle);
    Column->AddSlot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(SensorToolUiText::DatasetRanges())].BodyContent()[Random]];
    Column->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(STextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping).Text_Lambda([this](){return FText::FromString(DatasetEstimate);})];
    auto Buttons=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8));
    auto Button=[this,Buttons](const FString& Name,TFunction<void()> Action)
    {Buttons->AddSlot()[SNewSensorTool(SButton).Text(FText::FromString(Name)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})];};
    Button(TEXT("생성 시작"),[this,Service](){if(auto* S=Service()){FString Error;if(!S->Start(DatasetDraft,Cast<ASlabActor>(DatasetSlab.Get()),Cast<AVirtualLidarSensorActor>(DatasetLidar.Get()),Cast<AVirtualCameraSensorActor>(DatasetCamera.Get()),Cast<ADirectionalLight>(DatasetLight.Get()),Error))S->Message=Error;RefreshDatasetPresentation();}});
    Button(TEXT("일시 중지"),[Service](){if(auto* S=Service())S->Pause();});
    Button(TEXT("재개"),[Service](){if(auto* S=Service()){FString Error;if(!S->Resume(Error))S->Message=Error;}});
    Button(TEXT("중단"),[Service](){if(auto* S=Service())S->Stop();});
    Column->InsertSlot(1).AutoHeight()[Buttons];
    Column->AddSlot().AutoHeight().Padding(0,8)[SNewSensorTool(SButton).Text(FText::FromString(TEXT("결과 폴더 열기"))).OnClicked_Lambda([Service](){if(auto* S=Service();S&&!S->Directory().IsEmpty())FPlatformProcess::ExploreFolder(*S->Directory());return FReply::Handled();})];
    Column->InsertSlot(2).AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping).ToolTipText_Lambda([this](){return FText::FromString(DatasetStatus);}).Text_Lambda([this](){return FText::FromString(DatasetStatus);})];return Column;
}
