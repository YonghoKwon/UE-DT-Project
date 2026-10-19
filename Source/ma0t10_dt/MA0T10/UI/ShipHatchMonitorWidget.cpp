#include "ShipHatchMonitorWidget.h"
#include "ShipHatchMonitorHost.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/InvalidationBox.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "String/LexFromString.h"
#include "GameFramework/PlayerController.h"
#include "Components/LineBatchComponent.h"

namespace {using Style=FSensorOwnedPresentationStyle;}
struct FShipHatchMonitorWork
{
    struct FResult
    {
        ShipHatch::FAnalysis Analysis;
        TArray<ShipHatch::FObservation> Observations;
        ShipHatch::FSettings Settings;
        FTransform HoldPose=FTransform::Identity;
        uint64 Revision=0;
        double Cell=1;
        int64 MLX=-1,Livox=-1;
    };
    TFuture<FResult> Pending;
    FResult Current;
    ShipHatch::FAnalysis Preview;
    uint64 PreviewRevision=MAX_uint64;
    double OpeningCoverage=0,FloorCoverage=0,VisibleFloorArea=0;
    TArray<ShipHatch::FObservation> ObservationScratch;
    FString ExportRequest;
};
void UShipHatchMonitorWidget::Bind(AShipHatchMonitorHost* H,AShipHatchSimulation* S)
{
    Host=H;Simulation=S;if(!S||!SeedInput)return;
    SyncSettings();SetCameraPreset(2);LastLayout=FVector2D::ZeroVector;Layout();
}
void UShipHatchMonitorWidget::SyncSettings()
{
    const auto* S=Simulation.Get();if(!S||!SeedInput)return;TGuardValue<bool> Sync(bSync,true);
    SeedInput->SetText(FText::FromString(LexToString(S->Seed)));MinFill->SetValue(S->FillMin);MaxFill->SetValue(S->FillMax);
    SensorHeight->SetValue(S->SensorAboveDeck);GridSize->SetValue(CellSize);DigRadius->SetValue(RadiusMeters);DigDepth->SetValue(DepthMeters);
    CargoChoice->SetSelectedIndex(S->bCoal?1:0);SensorChoice->SetSelectedIndex(!S->bMLX?2:!S->bLivox?(S->bNativeMLX?4:1):(S->bNativeMLX?3:0));
}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UShipHatchMonitorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();bPresistent=true;Work=MakeShared<FShipHatchMonitorWork>();
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto Label=[&](const FString& Value,int32 Size=Style::BodySize,bool Bold=false,FLinearColor Color=Style::Text())
    {auto* W=WidgetTree->ConstructWidget<UTextBlock>();W->SetText(FText::FromString(Value));W->SetFont(Style::Font(Size,1,Bold));W->SetColorAndOpacity(Color);W->SetAutoWrapText(false);TextBlocks.Add(W);BaseFontSizes.Add(Size);return W;};
    auto Card=[&](const TCHAR* Name,FLinearColor Color=Style::Panel())
    {auto* W=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);W->SetBrush(*Style::PanelBrush());W->SetBrushColor(Color);W->SetPadding(FMargin(Style::PanelPadding));return W;};
    auto Add=[&](UWidget* W){auto* LayoutSlot=Root->AddChildToCanvas(W);LayoutSlots.Add(LayoutSlot);return LayoutSlot;};
    auto Button=[&](const TCHAR* Value,FName Function,bool Primary=false)
    {auto* B=WidgetTree->ConstructWidget<UButton>();B->SetStyle(Primary?Style::PrimaryButton():Style::Button());B->SetContent(Label(Value));FScriptDelegate Delegate;Delegate.BindUFunction(this,Function);B->OnClicked.Add(Delegate);return B;};
    // Stable slots: input, header, metrics, 3D toolbar, top, heat, footer, point overlay, settings.
    auto* Input=Card(TEXT("HatchOrbitSurface"),FLinearColor::Transparent);Input->SetPadding(FMargin(0));Add(Input);
    auto* SensorCanvas=WidgetTree->ConstructWidget<UCanvasPanel>();Input->SetContent(SensorCanvas);SensorCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    SensorBadges={Button(TEXT("ML-X80"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnHighlightMLX)),Button(TEXT("Mid-360S"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnHighlightLivox))};
    for(auto B:SensorBadges)SensorCanvas->AddChildToCanvas(B);

    auto* Header=Card(TEXT("HatchHeader"));Add(Header);auto* Head=WidgetTree->ConstructWidget<UVerticalBox>();Header->SetContent(Head);
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Head->AddChildToVerticalBox(Row);
    Row->AddChildToHorizontalBox(Label(TEXT("해치 적재 모니터링"),Style::TitleSize,true))->SetPadding(FMargin(0,0,Style::SectionGap,0));
    StatusText=Label(TEXT("● 측정 준비 중"),Style::BodySize,false,Style::Success());StatusChip=Card(TEXT("HatchStatusChip"),Style::Selection());StatusChip->SetBrush(*Style::ChipBrush());StatusChip->SetPadding(FMargin(Style::Gap,4));StatusChip->SetContent(StatusText);Row->AddChildToHorizontalBox(StatusChip);Row->AddChildToHorizontalBox(Label(TEXT("")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Row->AddChildToHorizontalBox(Button(TEXT("3분할 보기"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnSplit)))->SetPadding(FMargin(0,0,Style::Gap,0));Row->AddChildToHorizontalBox(Button(TEXT("설정 · 상세"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnSettings)));
    auto* Sub=WidgetTree->ConstructWidget<UHorizontalBox>();Head->AddChildToVerticalBox(Sub)->SetPadding(FMargin(0,Style::Gap,0,0));
    ContextText=Label(TEXT("홀드 01 · 철광석 · ML-X + Livox"),Style::LabelSize,false,Style::Muted());Sub->AddChildToHorizontalBox(ContextText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UpdatedText=Label(TEXT("마지막 갱신 —"),Style::LabelSize,false,Style::Muted());Sub->AddChildToHorizontalBox(UpdatedText);
    auto* Metrics=WidgetTree->ConstructWidget<UHorizontalBox>();Add(Metrics);
    const TCHAR* Labels[]={TEXT("남은 적재율"),TEXT("남은 부피 · 모의"),TEXT("최고 높이"),TEXT("평균 높이"),TEXT("최저 높이"),TEXT("측정 불가 비율")};
    for(int32 I=0;I<6;++I)
    {auto* C=Card(*FString::Printf(TEXT("Metric%d"),I),Style::Section());auto* B=WidgetTree->ConstructWidget<UVerticalBox>();C->SetContent(B);MetricValues.Add(Label(TEXT("—"),Style::TitleSize+Style::Gap,true,I==0?Style::Accent():Style::Text()));B->AddChildToVerticalBox(MetricValues.Last());B->AddChildToVerticalBox(Label(Labels[I],Style::LabelSize,false,Style::Muted()));auto* LayoutSlot=Metrics->AddChildToHorizontalBox(C);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?Style::Gap:0,0,0,0));}
    auto* Toolbar=Card(TEXT("Hatch3dToolbar"));Add(Toolbar);auto* ToolsBox=WidgetTree->ConstructWidget<UVerticalBox>();Toolbar->SetContent(ToolsBox);
    auto* Modes=WidgetTree->ConstructWidget<UHorizontalBox>();ToolsBox->AddChildToVerticalBox(Modes);
    ViewButtons.Add(Button(TEXT("일반 3D"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnRender)));ViewButtons.Add(Button(TEXT("점군"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnPoints)));for(auto B:ViewButtons)Modes->AddChildToHorizontalBox(B)->SetPadding(FMargin(0,0,Style::Gap,0));
    DigToggle=Button(TEXT("퍼내기 켜기"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnDigToggle));Modes->AddChildToHorizontalBox(DigToggle);
    Modes->AddChildToHorizontalBox(Label(TEXT("")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Modes->AddChildToHorizontalBox(Button(TEXT("최대화"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnMax3d)));
    auto* Cameras=WidgetTree->ConstructWidget<UHorizontalBox>();ToolsBox->AddChildToVerticalBox(Cameras)->SetPadding(FMargin(0,Style::Gap,0,0));
    Cameras->AddChildToHorizontalBox(Button(TEXT("위에서 보기"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnTop)))->SetPadding(FMargin(0,0,Style::Gap,0));Cameras->AddChildToHorizontalBox(Button(TEXT("비스듬히 보기"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnOblique)))->SetPadding(FMargin(0,0,Style::Gap,0));Cameras->AddChildToHorizontalBox(Button(TEXT("시점 초기화"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnReset)));
    UndoButton=Button(TEXT("되돌리기"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnUndoDig));RestoreButton=Button(TEXT("원래대로"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnRestoreCargo));Cameras->AddChildToHorizontalBox(UndoButton)->SetPadding(FMargin(Style::Gap,0));Cameras->AddChildToHorizontalBox(RestoreButton);
    auto* Top=Card(TEXT("HatchTopCard"));Add(Top);auto* TopBox=WidgetTree->ConstructWidget<UVerticalBox>();Top->SetContent(TopBox);
    auto* TopHead=WidgetTree->ConstructWidget<UHorizontalBox>();TopBox->AddChildToVerticalBox(TopHead);TopHead->AddChildToHorizontalBox(Label(TEXT("2D 상면"),Style::TitleSize,true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    FillBadge=Label(TEXT("— %"),Style::TitleSize,true,Style::Accent());TopHead->AddChildToHorizontalBox(FillBadge)->SetPadding(FMargin(0,0,Style::Gap,0));TopHead->AddChildToHorizontalBox(Button(TEXT("최대화"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnMaxTop)));
    TopView=WidgetTree->ConstructWidget<UShipHatchAnalysisView>();TopView->Monitor=this;TopCache=WidgetTree->ConstructWidget<UInvalidationBox>();TopCache->SetCanCache(true);TopCache->SetContent(TopView);TopBox->AddChildToVerticalBox(TopCache)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));TopBox->AddChildToVerticalBox(Label(TEXT("파란 테두리  개구부   ·   회색  선박 구조 가림"),Style::LabelSize,false,Style::Muted()));
    auto* Heat=Card(TEXT("HatchHeatCard"));Add(Heat);auto* HeatBox=WidgetTree->ConstructWidget<UVerticalBox>();Heat->SetContent(HeatBox);
    auto* HeatHead=WidgetTree->ConstructWidget<UHorizontalBox>();HeatBox->AddChildToVerticalBox(HeatHead);HeatHead->AddChildToHorizontalBox(Label(TEXT("높이 히트맵"),Style::TitleSize,true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));HeatHead->AddChildToHorizontalBox(Button(TEXT("최대화"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnMaxHeat)));
    HeatView=WidgetTree->ConstructWidget<UShipHatchAnalysisView>();HeatView->Monitor=this;HeatView->bHeatmap=true;HeatCache=WidgetTree->ConstructWidget<UInvalidationBox>();HeatCache->SetCanCache(true);HeatCache->SetContent(HeatView);HeatBox->AddChildToVerticalBox(HeatCache)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));HeatBox->AddChildToVerticalBox(Label(TEXT("회색  측정 불가   ·   칸 위에 마우스를 올려 높이 확인"),Style::LabelSize,false,Style::Muted()));
    auto* Footer=Card(TEXT("HatchFooter"));Footer->SetPadding(FMargin(Style::Gap));Add(Footer);DigAmounts=Label(TEXT("퍼낸 양  이번 0.00 m³ · 누적 0.00 m³  |  전체 홀드 기준 · 회색은 측정 불가"),Style::LabelSize,false,Style::Muted());Footer->SetContent(DigAmounts);
    PointPanel=Card(TEXT("HatchPointOverlay"));Add(PointPanel);auto* PointScroll=WidgetTree->ConstructWidget<UScrollBox>();PointPanel->SetContent(PointScroll);auto* Points=WidgetTree->ConstructWidget<UVerticalBox>();PointScroll->AddChild(Points);Points->AddChildToVerticalBox(Label(TEXT("점군 표시"),Style::BodySize,true));
    auto* Colors=WidgetTree->ConstructWidget<UHorizontalBox>();Points->AddChildToVerticalBox(Colors)->SetPadding(FMargin(0,Style::Gap,0,Style::Gap));
    ColorButtons={Button(TEXT("높이"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnHeight)),Button(TEXT("분류"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnClass)),Button(TEXT("반사"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnReflection)),Button(TEXT("거리"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnDistance)),Button(TEXT("단색"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnMono))};for(auto B:ColorButtons)Colors->AddChildToHorizontalBox(B)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    PointSizeLabel=Label(TEXT("점 크기  3.0"),Style::LabelSize);Points->AddChildToVerticalBox(PointSizeLabel);SizeSlider=WidgetTree->ConstructWidget<USlider>();SizeSlider->SetMinValue(.25);SizeSlider->SetMaxValue(12);SizeSlider->SetValue(3);SizeSlider->SetSliderBarColor(Style::Border());SizeSlider->SetSliderHandleColor(Style::Accent());Points->AddChildToVerticalBox(SizeSlider);
    DensityLabel=Label(TEXT("표시 밀도  50%"),Style::LabelSize);Points->AddChildToVerticalBox(DensityLabel);DensitySlider=WidgetTree->ConstructWidget<USlider>();DensitySlider->SetMinValue(1);DensitySlider->SetMaxValue(100);DensitySlider->SetValue(50);DensitySlider->SetSliderBarColor(Style::Border());DensitySlider->SetSliderHandleColor(Style::Accent());Points->AddChildToVerticalBox(DensitySlider);
    auto* Palette=WidgetTree->ConstructWidget<UShipHatchAnalysisView>();Palette->Monitor=this;Palette->bLegend=true;PaletteCache=WidgetTree->ConstructWidget<UInvalidationBox>();PaletteCache->SetCanCache(true);PaletteCache->SetContent(Palette);Points->AddChildToVerticalBox(PaletteCache);LegendText=Label(TEXT(""),Style::LabelSize,false,Style::Muted());LegendText->SetAutoWrapText(true);Points->AddChildToVerticalBox(LegendText)->SetPadding(FMargin(0,Style::Gap,0,0));
    SettingsPanel=Card(TEXT("HatchSettings"));Add(SettingsPanel);auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();SettingsPanel->SetContent(Scroll);auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Box);Box->AddChildToVerticalBox(Label(TEXT("측정 설정"),Style::TitleSize,true));
    auto Choice=[&](const TCHAR* Name,TArray<FString> Values)
    {Box->AddChildToVerticalBox(Label(Name,Style::LabelSize,false,Style::Muted()))->SetPadding(FMargin(0,Style::Gap,0,4));auto* W=WidgetTree->ConstructWidget<UComboBoxString>();for(const auto& V:Values)W->AddOption(V);W->SetSelectedIndex(0);W->ForegroundColor=Style::Text();W->SetWidgetStyle(Style::ComboBox());Box->AddChildToVerticalBox(W);return W;};
    CargoChoice=Choice(TEXT("원료"),{TEXT("철광석"),TEXT("석탄")});SensorChoice=Choice(TEXT("측정 장비"),{TEXT("ML-X(80) Integration 200 + Mid-360S"),TEXT("ML-X(80) Integration 200"),TEXT("Livox Mid-360S"),TEXT("ML-X(80) Native + Mid-360S"),TEXT("ML-X(80) Native")});
    ScanToggle=Button(TEXT("광선 보기"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnScanToggle));Box->AddChildToVerticalBox(ScanToggle)->SetPadding(FMargin(0,Style::Gap));
    auto* ScanHelp=Label(TEXT("실제 검출 광선 일부 · 최대 96개 · 5Hz\n센서 이름을 클릭하면 강조 · 배치 모형 확대"),Style::LabelSize,false,Style::Muted());ScanHelp->SetAutoWrapText(true);Box->AddChildToVerticalBox(ScanHelp);
    Box->AddChildToVerticalBox(Button(TEXT("적재 상태 바꾸기(랜덤)"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnRandom),true))->SetPadding(FMargin(0,Style::SectionGap,0,Style::Gap));Box->AddChildToVerticalBox(Label(TEXT("시드 · 직접 입력 후 적용"),Style::LabelSize,false,Style::Muted()));SeedInput=WidgetTree->ConstructWidget<UEditableTextBox>();SeedInput->WidgetStyle=Style::Input();Box->AddChildToVerticalBox(SeedInput);
    auto Spin=[&](const TCHAR* Name,float Value,float Min,float Max,float Delta)
    {Box->AddChildToVerticalBox(Label(Name,Style::LabelSize,false,Style::Muted()))->SetPadding(FMargin(0,Style::Gap,0,4));auto* W=WidgetTree->ConstructWidget<USpinBox>();W->SetWidgetStyle(Style::SpinBox());W->SetMinValue(Min);W->SetMaxValue(Max);W->SetMinSliderValue(Min);W->SetMaxSliderValue(Max);W->SetDelta(Delta);W->SetValue(Value);Box->AddChildToVerticalBox(W);return W;};
    const auto* S=Simulation.Get();
    MinFill=Spin(TEXT("최소 적재율 (%)"),S?S->FillMin:ShipHatch::FSettings().FillMin,0,95,1);MaxFill=Spin(TEXT("최대 적재율 (%)"),S?S->FillMax:ShipHatch::FSettings().FillMax,0,95,1);SensorHeight=Spin(TEXT("센서 높이 기준 (m)"),S?S->SensorAboveDeck:ShipHatch::FSettings().SensorAboveDeck,2.1,30,.5);Box->AddChildToVerticalBox(Button(TEXT("시드·범위·높이 적용"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnApply)))->SetPadding(FMargin(0,Style::Gap));GridSize=Spin(TEXT("분석 격자 목표 크기 (m)"),CellSize,.25,5,.25);FontChoice=Choice(TEXT("글자 크기"),{TEXT("100%"),TEXT("150%")});
    Box->AddChildToVerticalBox(Label(TEXT("퍼내기 설정"),Style::TitleSize,true))->SetPadding(FMargin(0,Style::SectionGap,0,0));Box->AddChildToVerticalBox(Label(TEXT("원료 짧게 클릭 · 드래그 회전"),Style::LabelSize,false,Style::Muted()));
    DigRadius=Spin(TEXT("퍼내기 반경 (m)"),RadiusMeters,.5,5,.5);DigDepth=Spin(TEXT("퍼내기 깊이 (m)"),DepthMeters,.1,3,.1);
    Box->AddChildToVerticalBox(Label(TEXT("되돌리기 최근 16회 · 새 적재 시 초기화"),Style::LabelSize,false,Style::Muted()));
    DigRadius->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnDigRadius);DigDepth->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnDigDepth);
    Box->AddChildToVerticalBox(Button(TEXT("센서 도구 · PCD 전송 / 파일 저장"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnTools)))->SetPadding(FMargin(0,Style::SectionGap,0,Style::Gap));Box->AddChildToVerticalBox(Button(TEXT("분석 JSON 저장 · 현재 측정값"),GET_FUNCTION_NAME_CHECKED(UShipHatchMonitorWidget,OnExport)));StatsText=Label(TEXT(""),Style::LabelSize,false,Style::Muted());StatsText->SetAutoWrapText(true);Box->AddChildToVerticalBox(StatsText)->SetPadding(FMargin(0,Style::Gap));
    CargoChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnCargo);SensorChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnSensor);FontChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnFont);GridSize->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnGrid);SizeSlider->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnSizeSlider);DensitySlider->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnDensitySlider);SetFontPercent(100);UpdateSegments();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
void UShipHatchMonitorWidget::NativeDestruct(){RestorePointVisibility();Work.Reset();Super::NativeDestruct();}
void UShipHatchMonitorWidget::RestorePointVisibility()
{
    if(auto* PC=GetOwningPlayer())for(const auto& Part:StalePointHides)PC->HiddenPrimitiveComponents.RemoveSingle(Part);
    StalePointHides.Reset();
}
void UShipHatchMonitorWidget::SetSplitView(bool Enabled){bOrbit=false;if(Enabled&&ActiveView>=2)SelectView(LastSceneView);bSplit=Enabled;LastLayout=FVector2D::ZeroVector;Layout();}
void UShipHatchMonitorWidget::SelectView(int32 V)
{
    bOrbit=false;ActiveView=FMath::Clamp(V,0,3);if(ActiveView>=2)bSplit=false;else LastSceneView=ActiveView;
    if(auto* H=Host.Get())if(H->Coordinator)H->Coordinator->SetPointCloudOnlyMode(ActiveView==1);
    ApplyPreview();UpdateSegments();LastLayout=FVector2D::ZeroVector;Layout();
}
void UShipHatchMonitorWidget::SelectColor(int32 C){ActiveColor=FMath::Clamp(C,0,4);ApplyPreview();UpdateSegments();}
void UShipHatchMonitorWidget::UpdateSegments()
{
    InvalidatePlots();
    for(int32 I=0;I<ViewButtons.Num();++I)ViewButtons[I]->SetStyle(I==ActiveView?Style::PrimaryButton():Style::Button());for(int32 I=0;I<ColorButtons.Num();++I)ColorButtons[I]->SetStyle(I==ActiveColor?Style::PrimaryButton():Style::Button());
    if(LegendText)
    {
        const FString Legends[]={
            TEXT("원료 높이 · 홀드 바닥 기준\n구조물은 분류 색으로 표시"),
            TEXT("검출된 점의 종류"),
            TEXT("반사 강도  0 → 1 · 보라 / 초록 / 노랑"),
            TEXT("거리  가까움 → 멂 · 센서별 검출 범위"),
            TEXT("단색 · 검출된 점")};
        LegendText->SetText(FText::FromString(Legends[ActiveColor]));
    }
}
void UShipHatchMonitorWidget::InvalidatePlots()
{
    if(TopView)TopView->InvalidateLayoutAndVolatility();
    if(HeatView)HeatView->InvalidateLayoutAndVolatility();
    if(PaletteCache&&PaletteCache->GetContent())PaletteCache->GetContent()->InvalidateLayoutAndVolatility();
}
void UShipHatchMonitorWidget::Layout()
{
    if(LayoutSlots.Num()!=9)return;FVector2D Size=GetCachedGeometry().GetLocalSize();if(Size.X<100)Size=UWidgetLayoutLibrary::GetViewportSize(this)/UWidgetLayoutLibrary::GetViewportScale(this);if(Size.Equals(LastLayout,.1))return;LastLayout=Size;for(auto Value:MetricValues)Value->SetFont(Style::Font(Size.X<1500?Style::TitleSize:Style::TitleSize+Style::Gap,FontScale,true));
    const double Gap=Style::Gap,P=Style::PanelPadding,S=FontScale,Header=92*S,Metrics=88*S,Footer=30*S,DataTop=P+Header+Gap+Metrics+Gap,DataHeight=FMath::Max(120.,Size.Y-DataTop-P-Footer-Gap);
    const double Width=Size.X-2*P,Left=bSplit?(Width-Gap)*.62:Width,Right=Width-Left-Gap,RightX=P+Left+Gap;
    auto Set=[&](int32 I,double X,double Y,double W,double H,bool Visible=true){auto* LayoutSlot=LayoutSlots[I].Get();LayoutSlot->SetPosition(FVector2D(X,Y));LayoutSlot->SetSize(FVector2D(W,H));LayoutSlot->Content->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    Set(1,P,P,Width,Header);Set(2,P,P+Header+Gap,Width,Metrics);const bool Scene=bSplit||ActiveView<2;
    Set(0,P,DataTop,Left,DataHeight,Scene);Set(3,P,DataTop,Left,116*S,Scene);SceneRect=FSlateRect(P,DataTop+116*S,P+Left,DataTop+DataHeight);
    Set(4,bSplit?RightX:P,DataTop,bSplit?Right:Width,bSplit?(DataHeight-Gap)*.5:DataHeight,bSplit||ActiveView==2);Set(5,bSplit?RightX:P,bSplit?DataTop+(DataHeight+Gap)*.5:DataTop,bSplit?Right:Width,bSplit?(DataHeight-Gap)*.5:DataHeight,bSplit||ActiveView==3);
    Set(6,P,Size.Y-P-Footer,Width,Footer);Set(7,P+Gap,DataTop+116*S+Gap,FMath::Min(Left-2*Gap,360.*S),FMath::Min(280*S,SceneRect.Bottom-SceneRect.Top-2*Gap),Scene&&ActiveView==1);Set(8,Size.X-P-FMath::Min(Width,Style::SidebarWidth*S),P+Header+Gap,FMath::Min(Width,Style::SidebarWidth*S),Size.Y-P-(P+Header+Gap),bSettings);
    if(auto* H=Host.Get())H->SetSceneRect(SceneRect,Size,Scene);
    UpdateCamera();
}
void UShipHatchMonitorWidget::SetFontPercent(int32 Percent)
{
    InvalidatePlots();
    bOrbit=false;FontScale=Percent==150?1.5f:1.f;for(int32 I=0;I<TextBlocks.Num();++I){auto F=TextBlocks[I]->GetFont();F.Size=FMath::RoundToInt(BaseFontSizes[I]*FontScale);TextBlocks[I]->SetFont(F);}for(auto* Spin:{MinFill.Get(),MaxFill.Get(),SensorHeight.Get(),GridSize.Get(),DigRadius.Get(),DigDepth.Get()})if(Spin)Spin->SetFont(Style::Font(Style::BodySize,FontScale));
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    if(SeedInput){SeedInput->WidgetStyle.SetTextStyle(FTextBlockStyle().SetFont(Style::Font(Style::BodySize,FontScale)));SeedInput->SynchronizeProperties();}TGuardValue<bool> Sync(bSync,true);for(auto* Combo:{CargoChoice.Get(),SensorChoice.Get(),FontChoice.Get()})if(Combo){const int32 Index=Combo->GetSelectedIndex();Combo->Font=Style::Font(Style::BodySize,FontScale);Combo->ClearSelection();Combo->SetSelectedIndex(Index);Combo->RefreshOptions();}if(FontChoice)FontChoice->SetSelectedIndex(Percent==150?1:0);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    LastLayout=FVector2D::ZeroVector;Layout();
}
void UShipHatchMonitorWidget::Refresh()
{
    auto* S=Simulation.Get();if(!S||!IsVisible()||!StatusText)return;Layout();if(bWasGenerating!=!S->HasFreshMeasurement()){bWasGenerating=!S->HasFreshMeasurement();ApplyPreview();InvalidatePlots();}
    UpdateDigControls();UpdateSensorBadges();
    if(LastRevision!=S->Revision())
    {LastRevision=S->Revision();LastMLX=-1;LastLivox=-1;SeedInput->SetText(FText::FromString(LexToString(S->Seed)));{TGuardValue<bool> Sync(bSync,true);CargoChoice->SetSelectedIndex(S->bCoal?1:0);SensorChoice->SetSelectedIndex(!S->bMLX?2:!S->bLivox?(S->bNativeMLX?4:1):(S->bNativeMLX?3:0));}RefreshEditPreview();S->Coverage(Work->OpeningCoverage,Work->FloorCoverage,Work->VisibleFloorArea);ApplyPreview();}
    const bool MLXRunning=S->bMLX&&S->MLX&&S->MLX->IsSensorRunning(),LivoxRunning=S->bLivox&&S->Livox&&S->Livox->IsSensorRunning();
    const float Rate=S->bMLX?(MLXRunning?S->MLX->ScanComponent->GetRuntimeStatus().MeasuredAcquisitionRateHz:0):(LivoxRunning?S->Livox->ScanComponent->GetRuntimeStatus().MeasuredAcquisitionRateHz:0);
    CachedStatus=S->HasFreshMeasurement()?FString::Printf(TEXT("● 로컬 측정 중 · %.1f Hz"),Rate):TEXT("● 새 측정 준비 중");
    if(!S->IsGenerating()&&!MLXRunning&&!LivoxRunning)CachedStatus=TEXT("● 측정 정지");
    else if(S->HasFreshMeasurement()&&S->bMLX&&S->bLivox)CachedStatus+=FString::Printf(TEXT(" / %.1f Hz"),LivoxRunning?S->Livox->ScanComponent->GetRuntimeStatus().MeasuredAcquisitionRateHz:0);
    StatusChip->SetBrushColor(S->HasFreshMeasurement()?Style::Selection():Style::WarningSurface());StatusText->SetColorAndOpacity(!MLXRunning&&!LivoxRunning&&!S->IsGenerating()?Style::Muted():S->HasFreshMeasurement()?Style::Success():Style::Warning());StatusText->SetText(FText::FromString(CachedStatus));ContextText->SetText(FText::FromString(FString::Printf(TEXT("홀드 01   /   %s   /   %s"),S->bCoal?TEXT("석탄"):TEXT("철광석"),S->bMLX&&S->bLivox?TEXT("ML-X + Livox"):S->bMLX?TEXT("ML-X"):TEXT("Livox"))));
    if(bSettings)StatsText->SetText(FText::FromString(FString::Printf(TEXT("%s\n시드 %d · 경사 상한 %.1f°\n밀도 가정 %.0f kg/m³ · 홀드 %.1f × %.1f × %.1f m\n중앙 배치 · ML-X80 높이 %.1f m / Mid-360S %.1f m\n시야 범위: 개구부 %.1f%% · 빈 홀드 바닥 %.1f%% (%.0f m²)\n실측 영역 %.0f m³ · 가림·미관측 추정 %.0f m³\n전체 부피·적재율은 모의 원료 / 홀드 전체 기준\n%s\nPCD 기본 선택 · 분석 저장은 실제 측정값만"),*S->Status,S->Seed,S->Surface().Repose,S->Surface().Density,S->HoldWidth,S->HoldLength,S->HoldDepth,S->EffectiveMLXHeight(),S->SensorLocalOrigin(false).Z-S->HoldDepth,Work->OpeningCoverage,Work->FloorCoverage,Work->VisibleFloorArea,Work->Current.Analysis.Volume,FMath::Max(0.,S->Surface().Volume-Work->Current.Analysis.Volume),IsAnalysisPreview()?TEXT("변경 미리보기 · 실제 측정 대기"):TEXT("실제 측정 표시"))));
    if(!S->HasFreshMeasurement())return;RefreshAnalysis();if(Work&&!Work->ExportRequest.IsEmpty()){const auto R=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(Work->ExportRequest);if(R.IsTerminal()){S->Status=R.Message;Work->ExportRequest.Reset();}}
}
const ShipHatch::FAnalysis* UShipHatchMonitorWidget::GetAnalysis() const{return Work&&Simulation.IsValid()&&Simulation->HasFreshMeasurement()&&Work->Current.Revision==Simulation->Revision()&&Work->Current.Cell==CellSize?&Work->Current.Analysis:nullptr;}
const ShipHatch::FSettings* UShipHatchMonitorWidget::GetAnalysisSettings() const{return Work&&Work->Current.Analysis.Cells.Num()?&Work->Current.Settings:nullptr;}
bool UShipHatchMonitorWidget::IsAnalysisPreview() const{return Work&&Simulation.IsValid()&&Work->Current.Revision!=Simulation->Revision();}
const ShipHatch::FAnalysis* UShipHatchMonitorWidget::GetDisplayAnalysis() const
{if(!Work)return nullptr;if(IsAnalysisPreview()&&Work->PreviewRevision==Simulation->Revision())return &Work->Preview;return Work->Current.Analysis.Cells.Num()?&Work->Current.Analysis:nullptr;}
void UShipHatchMonitorWidget::RefreshEditPreview()
{
    if(!Work||!Simulation.IsValid())return;const auto& S=Simulation->Surface();
    MetricValues[0]->SetText(FText::FromString(FString::Printf(TEXT("%.1f %%"),S.Fill)));MetricValues[1]->SetText(FText::FromString(FString::Printf(TEXT("%.0f m³"),S.Volume)));FillBadge->SetText(MetricValues[0]->GetText());
    Work->Preview=Work->Current.Analysis;Work->PreviewRevision=Simulation->Revision();auto& A=Work->Preview;
    // Display-only correction of previously observed cells. No new observations,
    // JSON export, immutable frame mutation or fabricated coverage is introduced.
    A.Min=DBL_MAX;A.Max=A.Mean=A.Volume=0;
    for(int32 Y=0;Y<A.NY;++Y)for(int32 X=0;X<A.NX;++X)
    {auto& C=A.Cells[Y*A.NX+X];if(C.Count){const double H=S.Height((X+.5)*A.DX-S.Settings.Length*.5,(Y+.5)*A.DY-S.Settings.Width*.5);C.Sum=H*C.Count;C.Highest=H;}}
    for(const auto& C:A.Cells)if(C.Count){A.Min=FMath::Min(A.Min,C.Mean());A.Max=FMath::Max(A.Max,C.Mean());A.Mean+=C.Mean();A.Volume+=C.Mean()*A.DX*A.DY;}
    if(A.Observed)A.Mean/=A.Observed;else A.Min=0;
    const double Heights[]={A.Max,A.Mean,A.Min};for(int32 I=0;I<3;++I)MetricValues[I+2]->SetText(FText::FromString(FString::Printf(TEXT("%.2f m"),Heights[I])));
    UpdatedText->SetText(FText::FromString(TEXT("변경 미리보기 · 실제 측정 대기")));InvalidatePlots();
}

void UShipHatchMonitorWidget::OnHighlightMLX(){if(auto* S=Simulation.Get())S->HighlightSensor(0);UpdateSensorBadges();}
void UShipHatchMonitorWidget::OnHighlightLivox(){if(auto* S=Simulation.Get())S->HighlightSensor(1);UpdateSensorBadges();}
void UShipHatchMonitorWidget::UpdateSensorBadges()
{
    auto* S=Simulation.Get();auto* PC=GetOwningPlayer();if(!S||!PC||SensorBadges.Num()!=2||LayoutSlots.Num()!=9)return;
    int32 W,H;PC->GetViewportSize(W,H);if(W<=0||H<=0)return;const FVector2D Size=GetCachedGeometry().GetLocalSize();if(Size.X<1)return;
    FVector2D P[2];for(int32 I=0;I<2;++I){PC->ProjectWorldLocationToScreen(S->GetActorTransform().TransformPosition(S->SensorLocalOrigin(I==0)*100),P[I]);P[I]=P[I]*Size/FVector2D(W,H);}
    const bool Overlap=(P[0]-P[1]).Size()<160*FontScale;
    for(int32 I=0;I<2;++I)
    {
        FVector2D Pos=P[I]+FVector2D(24*FontScale,(-36+(Overlap?I*42:0))*FontScale);
        Pos.X=FMath::Clamp(Pos.X,double(SceneRect.Left+8),FMath::Max(double(SceneRect.Left+8),double(SceneRect.Right-136*FontScale)));
        Pos.Y=FMath::Clamp(Pos.Y,double(SceneRect.Top+8),FMath::Max(double(SceneRect.Top+8),double(SceneRect.Bottom-40*FontScale)));
        if(ActiveView==1&&Pos.X<SceneRect.Left+370*FontScale&&Pos.Y<SceneRect.Top+290*FontScale){Pos.X=SceneRect.Right-136*FontScale;Pos.Y=SceneRect.Top+(8+I*42)*FontScale;}
        auto* BadgeSlot=CastChecked<UCanvasPanelSlot>(SensorBadges[I]->Slot);BadgeSlot->SetPosition(Pos-LayoutSlots[0]->GetPosition());BadgeSlot->SetSize(FVector2D(128,32)*FontScale);
        const bool Enabled=I==0?S->bMLX:S->bLivox,Highlight=S->HighlightedSensor()==I||(S->HighlightedSensor()<0&&Enabled);
        SensorBadges[I]->SetStyle(Highlight?Style::PrimaryButton():Style::Button());
    }
}

void UShipHatchMonitorWidget::OnScanToggle()
{if(auto* S=Simulation.Get()){S->SetScanDisplay(!S->IsScanDisplayEnabled());ScanToggle->SetStyle(S->IsScanDisplayEnabled()?Style::PrimaryButton():Style::Button());CastChecked<UTextBlock>(ScanToggle->GetContent())->SetText(FText::FromString(S->IsScanDisplayEnabled()?TEXT("광선 끄기"):TEXT("광선 보기")));}}
FText UShipHatchMonitorWidget::CellTooltip(int32 I) const
{const auto* A=GetDisplayAnalysis();if(!A||!A->Cells.IsValidIndex(I))return FText::GetEmpty();const auto& C=A->Cells[I];return FText::FromString(C.Count?FString::Printf(TEXT("격자 %d, %d · 평균 높이 %.2f m · 검출 %d점"),I%A->NX+1,I/A->NX+1,C.Mean(),C.Count):TEXT("측정 불가 · 검출점 없음"));}
void UShipHatchMonitorWidget::RefreshAnalysis()
{
    auto* S=Simulation.Get();if(!S||!Work)return;
    if(Work->Pending.IsValid())
    {if(!Work->Pending.IsReady())return;auto R=Work->Pending.Get();Work->ObservationScratch=MoveTemp(R.Observations);if(R.Revision==S->Revision()&&R.Cell==CellSize)
     {Work->Current=MoveTemp(R);const auto& A=Work->Current.Analysis;++AnalysisBuilds;const FString Values[]={FString::Printf(TEXT("%.1f %%"),S->Surface().Fill),FString::Printf(TEXT("%.0f m³"),S->Surface().Volume),FString::Printf(TEXT("%.2f m"),A.Max),FString::Printf(TEXT("%.2f m"),A.Mean),FString::Printf(TEXT("%.2f m"),A.Min),FString::Printf(TEXT("%.1f %%"),(1-A.Coverage)*100)};for(int32 I=0;I<MetricValues.Num();++I)MetricValues[I]->SetText(FText::FromString(Values[I]));FillBadge->SetText(FText::FromString(Values[0]));UpdatedText->SetText(FText::FromString(TEXT("마지막 갱신  ")+FDateTime::Now().ToString(TEXT("%H:%M:%S"))));InvalidatePlots();}}
    const int64 M=S->bMLX&&S->MLX?S->MLX->ScanComponent->GetRuntimeStatus().FrameId:-1,L=S->bLivox&&S->Livox?S->Livox->ScanComponent->GetRuntimeStatus().FrameId:-1;if(M==LastMLX&&L==LastLivox)return;LastMLX=M;LastLivox=L;
    auto Points=MoveTemp(Work->ObservationScratch);S->CollectObservations(Points);const auto Settings=S->Surface().Settings;const auto Pose=S->GetActorTransform();const double Cell=CellSize;const uint64 Revision=S->Revision();
    Work->Pending=Async(EAsyncExecution::ThreadPool,[Points=MoveTemp(Points),Settings,Pose,Cell,Revision,M,L]() mutable{FShipHatchMonitorWork::FResult R;R.Revision=Revision;R.Cell=Cell;R.MLX=M;R.Livox=L;R.Settings=Settings;R.HoldPose=Pose;R.Analysis=ShipHatch::Analyze(Settings,Cell,Points);R.Observations=MoveTemp(Points);return R;});
}
void UShipHatchMonitorWidget::OnSettings(){bOrbit=false;bSettings=!bSettings;LastLayout=FVector2D::ZeroVector;Layout();}
void UShipHatchMonitorWidget::OnDigToggle(){bOrbit=false;bDigEnabled=!bDigEnabled;UpdateDigControls();}
void UShipHatchMonitorWidget::OnUndoDig(){bOrbit=false;if(Simulation.IsValid()&&Simulation->UndoDig())Refresh();}
void UShipHatchMonitorWidget::OnRestoreCargo(){bOrbit=false;if(Simulation.IsValid()&&Simulation->RestoreCargo())Refresh();}
void UShipHatchMonitorWidget::UpdateDigControls()
{
    const auto* S=Simulation.Get();if(!S||!DigToggle)return;
    if(LastDigRevision==S->Revision()&&bLastDigEnabled==bDigEnabled&&bLastDigGenerating==S->IsGenerating())return;
    LastDigRevision=S->Revision();bLastDigEnabled=bDigEnabled;bLastDigGenerating=S->IsGenerating();
    DigToggle->SetStyle(bDigEnabled?Style::PrimaryButton():Style::Button());CastChecked<UTextBlock>(DigToggle->GetContent())->SetText(FText::FromString(bDigEnabled?TEXT("퍼내기 켜짐"):TEXT("퍼내기 켜기")));
    DigToggle->SetIsEnabled(!S->IsGenerating());UndoButton->SetIsEnabled(!S->IsGenerating()&&S->UndoCount()>0);RestoreButton->SetIsEnabled(!S->IsGenerating()&&S->TotalRemovedVolume()>0);
    DigAmounts->SetText(FText::FromString(FString::Printf(TEXT("퍼낸 양  이번 %.2f m³ · 누적 %.2f m³  |  전체 홀드 기준 · 회색은 측정 불가"),S->LastRemovedVolume(),S->TotalRemovedVolume())));
}
void UShipHatchMonitorWidget::OnSizeSlider(float V){PointSizeLabel->SetText(FText::FromString(FString::Printf(TEXT("점 크기  %.1f"),V)));ApplyPreview();}
void UShipHatchMonitorWidget::OnDensitySlider(float V){DensityLabel->SetText(FText::FromString(FString::Printf(TEXT("표시 밀도  %.0f%%"),100./FMath::Max(1,FMath::RoundToInt(100./V)))));ApplyPreview();}
void UShipHatchMonitorWidget::SetCameraPreset(int32 P){OrbitYaw=P==0?90:48;OrbitPitch=P==0?-89:-62;OrbitDistance=7800;UpdateCamera();}
void UShipHatchMonitorWidget::UpdateCamera()
{
    auto* H=Host.Get();auto* S=Simulation.Get();if(!H||!H->ViewCamera||!S)return;
    const FVector Target=S->GetActorTransform().TransformPosition(FVector(0,0,S->HoldDepth*70));
    const FRotator Rotation(OrbitPitch,OrbitYaw,0);const FRotationMatrix Basis(Rotation);
    const double Aspect=FMath::Max(.2,double(SceneRect.Right-SceneRect.Left)/FMath::Max(1.f,SceneRect.Bottom-SceneRect.Top));
    const double TanHorizontal=FMath::Tan(FMath::DegreesToRadians(29.)),TanVertical=TanHorizontal/Aspect;
    double FitDistance=1;
    auto FitCorner=[&](FVector LocalMeters)
    {
        const FVector Delta=S->GetActorTransform().TransformPosition(LocalMeters*100)-Target;
        const double Forward=FVector::DotProduct(Delta,Basis.GetUnitAxis(EAxis::X));
        FitDistance=FMath::Max(FitDistance,FMath::Abs(FVector::DotProduct(Delta,Basis.GetUnitAxis(EAxis::Y)))/(TanHorizontal*.9)-Forward);
        FitDistance=FMath::Max(FitDistance,FMath::Abs(FVector::DotProduct(Delta,Basis.GetUnitAxis(EAxis::Z)))/(TanVertical*.9)-Forward);
    };
    for(int32 X:{-1,1})for(int32 Y:{-1,1})
    {
        FitCorner(FVector(X*(S->HoldLength+.6)*.5,Y*(S->HoldWidth+.6)*.5,-.3));
        FitCorner(FVector(X*(S->HoldLength+8)*.5,Y*(S->HoldWidth+8)*.5,S->HoldDepth+.3));
        FitCorner(FVector(X*(S->OpeningLength+.6)*.5,Y*(S->OpeningWidth+.6)*.5,S->HoldDepth+S->CoamingHeight));
        FitCorner(FVector(X*.7,Y*(S->OpeningWidth*.5+1.4),S->HoldDepth+S->EffectiveMLXHeight()+3.5));
    }
    H->ViewCamera->SetActorLocationAndRotation(Target-Rotation.Vector()*(FitDistance*OrbitDistance/7800.),Rotation);
    UpdateSensorBadges();
    H->ViewCamera->GetCameraComponent()->SetFieldOfView(58);H->ViewCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
}
bool UShipHatchMonitorWidget::SceneInputAt(const FGeometry& G,FVector2D P) const
{
    if(!IsVisible()||!(bSplit||ActiveView<2)||!SceneRect.ContainsPoint(G.AbsoluteToLocal(P)))return false;
    if(bSettings&&SettingsPanel->GetCachedGeometry().IsUnderLocation(P))return false;
    for(auto B:SensorBadges)if(B&&B->GetCachedGeometry().IsUnderLocation(P))return false;
    return ActiveView!=1||!PointPanel->GetCachedGeometry().IsUnderLocation(P);
}
bool UShipHatchMonitorWidget::DigAt(const FGeometry& G,FVector2D P)
{
    if(!Simulation.IsValid()||!SceneInputAt(G,P))return false;
    auto* PC=GetOwningPlayer();if(!PC)return false;int32 W=0,H=0;PC->GetViewportSize(W,H);
    const auto Size=G.GetLocalSize(),Local=G.AbsoluteToLocal(P);if(Size.X<=0||Size.Y<=0)return false;
    FVector O,D;if(!PC->DeprojectScreenPositionToWorld(Local.X*W/Size.X,Local.Y*H/Size.Y,O,D))return false;
    if(!bDigEnabled){Simulation->SelectSensorAt(O,D);return false;}
    if(!Simulation->Dig(O,D,RadiusMeters,DepthMeters))return false;Refresh();return true;
}
FReply UShipHatchMonitorWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{if(SceneInputAt(G,E.GetScreenSpacePosition())&&E.GetEffectingButton()==EKeys::LeftMouseButton){bOrbit=true;bDragged=false;PointerDown=E.GetScreenSpacePosition();PointerDownTime=FPlatformTime::Seconds();PointerRevision=Simulation.IsValid()?Simulation->Revision():0;return FReply::Handled().CaptureMouse(TakeWidget());}return Super::NativeOnMouseButtonDown(G,E);}
FReply UShipHatchMonitorWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{if(E.GetEffectingButton()==EKeys::LeftMouseButton&&(bOrbit||TakeWidget()->HasMouseCapture())){const bool Candidate=bOrbit;bOrbit=false;if(Candidate&&!bDragged&&(E.GetScreenSpacePosition()-PointerDown).Size()<=6&&FPlatformTime::Seconds()-PointerDownTime<=.35&&Simulation.IsValid()&&PointerRevision==Simulation->Revision())DigAt(G,E.GetScreenSpacePosition());return FReply::Handled().ReleaseMouseCapture();}return Super::NativeOnMouseButtonUp(G,E);}
FReply UShipHatchMonitorWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{if(bOrbit){bDragged|=(E.GetScreenSpacePosition()-PointerDown).Size()>6;if(bDragged){OrbitYaw+=E.GetCursorDelta().X*.25;OrbitPitch=FMath::Clamp(OrbitPitch-E.GetCursorDelta().Y*.25,-89.,-15.);UpdateCamera();}return FReply::Handled();}return Super::NativeOnMouseMove(G,E);}
FReply UShipHatchMonitorWidget::NativeOnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{if(SceneInputAt(G,E.GetScreenSpacePosition())){bOrbit=false;OrbitDistance=FMath::Clamp(OrbitDistance*FMath::Pow(.88,E.GetWheelDelta()),2500.,18000.);UpdateCamera();return FReply::Handled().ReleaseMouseCapture();}return Super::NativeOnMouseWheel(G,E);}
void UShipHatchMonitorWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& E){bOrbit=false;Super::NativeOnMouseCaptureLost(E);}

void UShipHatchMonitorWidget::ApplyPreview()
{
    auto* S=Simulation.Get();if(!S)return;
    for(auto* A:{S->MLX.Get(),S->Livox.Get()})if(A)
    {
        auto* V=A->VisualizationComponent.Get();const auto Previous=V->GetVisualizationSettings();auto Settings=Previous;
        const ELidarColorMode Modes[]={ELidarColorMode::RelativeHeight,ELidarColorMode::SemanticLabel,ELidarColorMode::ReflectionIntensity,ELidarColorMode::DistanceTurbo,ELidarColorMode::HitMask};
        Settings.ColorMode=Modes[ActiveColor];Settings.bAutoHeightRange=false;Settings.HeightReference=ELidarHeightReference::WorldZ;
        Settings.HeightMinMeters=S->GetActorLocation().Z*.01;Settings.HeightMaxMeters=Settings.HeightMinMeters+S->HoldDepth;
        Settings.PointSize=SizeSlider?SizeSlider->GetValue():3;
        const bool Selected=(A==S->MLX&&S->bMLX)||(A==S->Livox&&S->bLivox);
        Settings.bShowWorldPointCloud=ActiveView==1&&Selected;
        V->SetColoredCpuPointPreview(true);V->SetHeightJetPalette(ActiveColor==0,TEXT("HatchCargo"));
        // Retain the populated preview during acquisition. The map-local display
        // adapter corrects old cargo heights immediately without changing frames.
        if(!FVirtualLidarVisualizationSettings::StaticStruct()->CompareScriptStruct(&Previous,&Settings,PPF_None))V->SetVisualizationSettings(Settings);
        const int32 Stride=FMath::RoundToInt(DensitySlider?100.f/DensitySlider->GetValue():2);const auto Policy=A->ScanComponent->GetPreviewPolicyState();
        if(Policy.PointStride!=Stride||Policy.MaxPoints!=12000||!Policy.bHitOnly)A->ScanComponent->SetPreviewPolicy(Stride,12000,true);
    }
}
void UShipHatchMonitorWidget::OnApply()
{
    auto* S=Simulation.Get();if(!S)return;
    int32 NewSeed;if(!LexTryParseString(NewSeed,*SeedInput->GetText().ToString())){S->Status=TEXT("시드는 32비트 정수로 입력하세요.");return;}
    auto Candidate=S->Settings();Candidate.Seed=NewSeed;Candidate.FillMin=MinFill->GetValue();Candidate.FillMax=MaxFill->GetValue();Candidate.SensorAboveDeck=SensorHeight->GetValue();
    if(!Candidate.Validate(S->Status))return;
    S->Seed=NewSeed;S->FillMin=MinFill->GetValue();S->FillMax=MaxFill->GetValue();S->SensorAboveDeck=SensorHeight->GetValue();S->Regenerate(false);
}
void UShipHatchMonitorWidget::OnRandom()
{
    auto* S=Simulation.Get();if(!S)return;
    auto Candidate=S->Settings();Candidate.FillMin=MinFill->GetValue();Candidate.FillMax=MaxFill->GetValue();Candidate.SensorAboveDeck=SensorHeight->GetValue();
    if(!Candidate.Validate(S->Status))return;
    S->FillMin=Candidate.FillMin;S->FillMax=Candidate.FillMax;S->SensorAboveDeck=Candidate.SensorAboveDeck;S->Regenerate(true);
}
void UShipHatchMonitorWidget::OnTools(){if(Host.IsValid())Host->ShowTools();}
void UShipHatchMonitorWidget::OnExport()
{
    auto* S=Simulation.Get();if(!S||!Work||S->IsGenerating()||Work->Current.Revision!=S->Revision()||!Work->Current.Analysis.Observed)
    {if(S)S->Status=TEXT("2D 또는 히트맵에서 새 측정을 기다린 뒤 저장하세요.");return;}
    auto Root=MakeShared<FJsonObject>();const auto& A=Work->Current.Analysis;const auto& Settings=Work->Current.Settings;
    Root->SetStringField(TEXT("schema"),TEXT("ma0t10.hatch-analysis.v1"));Root->SetNumberField(TEXT("seed"),Settings.Seed);
    Root->SetStringField(TEXT("cargo"),Settings.Cargo==ShipHatch::ECargo::Coal?TEXT("coal"):TEXT("iron-ore"));Root->SetStringField(TEXT("volume_scope"),TEXT("observed-cells-only-no-extrapolation"));
    Root->SetStringField(TEXT("analysis_export_utc"),FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("coordinates"),TEXT("hold local meters: X length, Y breadth, Z above floor; pose uses UE world meters and xyzw quaternion"));
    Root->SetNumberField(TEXT("nx"),A.NX);Root->SetNumberField(TEXT("ny"),A.NY);
    Root->SetNumberField(TEXT("hold_length_m"),Settings.Length);Root->SetNumberField(TEXT("hold_width_m"),Settings.Width);Root->SetNumberField(TEXT("hold_depth_m"),Settings.Depth);
    Root->SetNumberField(TEXT("opening_length_m"),Settings.OpeningLength);Root->SetNumberField(TEXT("opening_width_m"),Settings.OpeningWidth);
    Root->SetNumberField(TEXT("minimum_height_m"),A.Min);Root->SetNumberField(TEXT("maximum_height_m"),A.Max);Root->SetNumberField(TEXT("mean_height_m"),A.Mean);
    auto Numbers=[](std::initializer_list<double> Values){TArray<TSharedPtr<FJsonValue>> Out;for(double V:Values)Out.Add(MakeShared<FJsonValueNumber>(V));return Out;};
    const auto Pose=Work->Current.HoldPose;const auto P=Pose.GetLocation()*.01;const auto Q=Pose.GetRotation();const auto Scale=Pose.GetScale3D();
    Root->SetArrayField(TEXT("hold_origin_world_ue_m"),Numbers({P.X,P.Y,P.Z}));Root->SetArrayField(TEXT("hold_rotation_xyzw"),Numbers({Q.X,Q.Y,Q.Z,Q.W}));Root->SetArrayField(TEXT("hold_scale"),Numbers({Scale.X,Scale.Y,Scale.Z}));
    Root->SetNumberField(TEXT("coverage"),A.Coverage);Root->SetNumberField(TEXT("observed_volume_m3"),A.Volume);
    Root->SetNumberField(TEXT("partial_capacity_percent"),A.FillLowerBound);Root->SetNumberField(TEXT("dx_m"),A.DX);Root->SetNumberField(TEXT("dy_m"),A.DY);
    Root->SetStringField(TEXT("mlx_frame_id"),LexToString(Work->Current.MLX));Root->SetStringField(TEXT("livox_frame_id"),LexToString(Work->Current.Livox));
    TArray<TSharedPtr<FJsonValue>> Cells;for(const auto& C:A.Cells){if(C.Count)Cells.Add(MakeShared<FJsonValueNumber>(C.Mean()));else Cells.Add(MakeShared<FJsonValueNull>());}Root->SetArrayField(TEXT("height_m_row_major"),Cells);
    FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    Work->ExportRequest=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->RequestAnalysisJson(S,Json,TEXT("ShipHatch"));
}
void UShipHatchMonitorWidget::OnView(FString Value,ESelectInfo::Type){if(!bSync&&ViewChoice)SelectView(ViewChoice->GetSelectedIndex());}
void UShipHatchMonitorWidget::OnColor(FString Value,ESelectInfo::Type){if(!bSync&&ColorChoice)SelectColor(ColorChoice->GetSelectedIndex());}
void UShipHatchMonitorWidget::OnCargo(FString Value,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){Simulation->bCoal=CargoChoice->GetSelectedIndex()==1;Simulation->Regenerate(false);}}
void UShipHatchMonitorWidget::OnSensor(FString Value,ESelectInfo::Type)
{if(!bSync&&Simulation.IsValid()){int32 I=SensorChoice->GetSelectedIndex();Simulation->SetSensorSelection(I!=2,I==0||I==2||I==3,I>=3);ApplyPreview();}}
void UShipHatchMonitorWidget::OnFont(FString Value,ESelectInfo::Type){if(!bSync)SetFontPercent(FontChoice->GetSelectedIndex()?150:100);}
void UShipHatchMonitorWidget::OnGrid(float Value){CellSize=Value;LastMLX=-1;LastLivox=-1;}
void UShipHatchMonitorWidget::OnPointSize(float){ApplyPreview();}
void UShipHatchMonitorWidget::OnPointStride(float){ApplyPreview();}
