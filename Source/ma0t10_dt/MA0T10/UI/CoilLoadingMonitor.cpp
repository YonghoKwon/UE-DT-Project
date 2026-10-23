#include "CoilLoadingMonitor.h"
#include "CoilLoadingPresentation.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/SpinBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/ScrollBox.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "GameFramework/PlayerController.h"

namespace {using Style=FSensorOwnedPresentationStyle;}
void UCoilLoadingWarningButton::Bind(ACoilLoadingSimulation* S,const TArray<int32>& C){Sim=S;Indices=C;OnClicked.AddDynamic(this,&UCoilLoadingWarningButton::Highlight);}
void UCoilLoadingWarningButton::Highlight(){if(auto* S=Sim.Get())S->HighlightCoils(Indices);}
bool UCoilLoadingMonitor::DataExpanded() const{return DataRows&&DataRows->IsVisible();}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UCoilLoadingMonitor::NativeOnInitialized()
{
    Super::NativeOnInitialized();bPresistent=true;
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    for(int I=0;I<4;++I){auto* B=WidgetTree->ConstructWidget<UBorder>();B->SetBrush(*Style::PanelBrush());B->SetBrushColor(FLinearColor(.012,.018,.022,1));B->SetVisibility(ESlateVisibility::HitTestInvisible);auto* MaskSlot=Root->AddChildToCanvas(B);MaskSlot->SetZOrder(-1);SceneMasks.Add(MaskSlot);}
    auto* SceneInput=WidgetTree->ConstructWidget<UBorder>();SceneInput->SetBrushColor(FLinearColor::Transparent);SceneInputSlot=Root->AddChildToCanvas(SceneInput);
    const auto Label=[&](const FString& Text,int32 Size=13,bool Bold=false){auto* W=WidgetTree->ConstructWidget<UTextBlock>();W->SetText(FText::FromString(Text));W->SetFont(Style::Font(Size,1,Bold));W->SetColorAndOpacity(Style::Text());W->SetAutoWrapText(true);return W;};
    const auto Button=[&](const TCHAR* Text,FName Fn,bool Primary=false){auto* W=WidgetTree->ConstructWidget<UButton>();W->SetStyle(Primary?Style::PrimaryButton():Style::Button());auto* Caption=Label(Text);Caption->SetAutoWrapText(false);W->SetContent(Caption);FScriptDelegate D;D.BindUFunction(this,Fn);W->OnClicked.Add(D);return W;};
    const auto Card=[&](const TCHAR* Name){auto* B=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);B->SetBrush(*Style::PanelBrush());B->SetBrushColor(Style::Panel());B->SetPadding(FMargin(14));Slots.Add(Root->AddChildToCanvas(B));return B;};
    const auto Row=[&](UVerticalBox* Parent){auto* R=WidgetTree->ConstructWidget<UHorizontalBox>();Parent->AddChildToVerticalBox(R)->SetPadding(FMargin(0,4));return R;};
    const auto AddButton=[&](UHorizontalBox* R,const TCHAR* Text,FName Fn,bool Primary=false){auto* S=R->AddChildToHorizontalBox(Button(Text,Fn,Primary));S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));S->SetPadding(FMargin(2));};
    auto* Sidebar=Card(TEXT("CoilWorkflow"));auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Sidebar->SetContent(Scroll);auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);
    Body->AddChildToVerticalBox(Label(TEXT("제품 부두 · 코일 선적"),18,true));
    Body->AddChildToVerticalBox(Label(TEXT("최대 5단 · 하중 경고는 별도 확인\n코일 클릭 → 선택 제거 · 위 코일부터"),12));
    Heading=Label(TEXT("1단 · 1열 · 1번"),16,true);Body->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0,12));
    Badge=Label(TEXT("주의 · 해석 준비"),15,true);Body->AddChildToVerticalBox(Badge)->SetPadding(FMargin(0,8));
    auto* R=Row(Body);AddButton(R,TEXT("인양"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnPlace),true);AddButton(R,TEXT("취소"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnCancel));
    R=Row(Body);AddButton(R,TEXT("1개 배치"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnPlanOnly));AddButton(R,TEXT("단 채우기"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnTier));
    R=Row(Body);AddButton(R,TEXT("되돌리기"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnUndo));RemoveButton=Button(TEXT("선택 제거"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnRemove));R->AddChildToHorizontalBox(RemoveButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SelectionText=Label(TEXT("선택 없음 · 3D/단면/상면에서 클릭"),12);Body->AddChildToVerticalBox(SelectionText);
    Body->AddChildToVerticalBox(Label(TEXT("다음 배치 · 배치 미리보기\n코일 번호 / 내경 mm / 외경·폭 m / 질량 t"),12));
    TypeChoice=WidgetTree->ConstructWidget<UComboBoxString>();TypeChoice->ForegroundColor=Style::Text();TypeChoice->SetWidgetStyle(Style::ComboBox());TypeChoice->AddOption(TEXT("열연 HR · 무포장"));TypeChoice->AddOption(TEXT("냉연 CR · 포장"));TypeChoice->OnSelectionChanged.AddDynamic(this,&UCoilLoadingMonitor::OnTypeChanged);Body->AddChildToVerticalBox(TypeChoice);
    IdInput=WidgetTree->ConstructWidget<UEditableTextBox>();IdInput->WidgetStyle=Style::Input();IdInput->SetHintText(FText::FromString(TEXT("코일 번호")));IdInput->SetToolTipText(FText::FromString(TEXT("코일 식별 번호 · 내경 ID와 구별")));Body->AddChildToVerticalBox(IdInput);
    auto* Choices=Row(Body);InnerChoice=WidgetTree->ConstructWidget<UComboBoxString>();InnerChoice->ForegroundColor=Style::Text();InnerChoice->SetWidgetStyle(Style::ComboBox());for(const TCHAR* V:{TEXT("내경 508 mm"),TEXT("내경 610 mm"),TEXT("내경 762 mm")})InnerChoice->AddOption(V);Choices->AddChildToHorizontalBox(InnerChoice);
    GearChoice=WidgetTree->ConstructWidget<UComboBoxString>();GearChoice->ForegroundColor=Style::Text();GearChoice->SetWidgetStyle(Style::ComboBox());GearChoice->AddOption(TEXT("훅 + 보호 슬링"));GearChoice->AddOption(TEXT("C-hook"));Choices->AddChildToHorizontalBox(GearChoice);
    const auto Number=[&](UVerticalBox* Parent,const TCHAR* Title,double Value,double Min,double Max){auto* Pair=Row(Parent);Pair->AddChildToHorizontalBox(Label(Title,12))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));auto* W=WidgetTree->ConstructWidget<USpinBox>();W->SetWidgetStyle(Style::SpinBox());W->SetMinValue(Min);W->SetMaxValue(Max);W->SetValue(Value);W->SetMinDesiredWidth(100);W->SetDelta(.01);Pair->AddChildToHorizontalBox(W);Numbers.Add(W);};
    Number(Body,TEXT("순강재 외경(OD) [m]"),1.6,.05,10);Number(Body,TEXT("순강재 폭 [m]"),1.25,.05,10);Number(Body,TEXT("총 질량 [t] · 0 자동 계산"),0,0,500);Number(Body,TEXT("목재 접촉 μ · 포장값 확인 필요"),.3,0,2);
    Number(Body,TEXT("단 · 최대 5단"),1,1,100000);Number(Body,TEXT("열 · 선수미"),1,1,100000);Number(Body,TEXT("번호 · 횡방향"),1,1,100000);
    RowDirection=WidgetTree->ConstructWidget<UComboBoxString>();RowDirection->SetWidgetStyle(Style::ComboBox());RowDirection->ForegroundColor=Style::Text();RowDirection->AddOption(TEXT("자동 다음 열: 선수 → 선미"));RowDirection->AddOption(TEXT("자동 다음 열: 선미 → 선수"));RowDirection->SetSelectedIndex(0);RowDirection->OnSelectionChanged.AddDynamic(this,&UCoilLoadingMonitor::OnRowDirection);Body->AddChildToVerticalBox(RowDirection);
    R=Row(Body);AddButton(R,TEXT("사양·위치 적용"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnApply),true);AddButton(R,TEXT("설정 · 상세"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnDetails));
    Details=WidgetTree->ConstructWidget<UVerticalBox>();Body->AddChildToVerticalBox(Details);Details->SetVisibility(ESlateVisibility::Collapsed);
    R=Row(Details);AddButton(R,TEXT("크레인"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnCraneClose));AddButton(R,TEXT("코일"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnCoilClose));AddButton(R,TEXT("창고"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnShed));
    Number(Details,TEXT("탱크톱 균등분포하중(UDL) [t/m²]"),14.5,.1,100);Number(Details,TEXT("승인 유효 면적 [m²] · 0 미확인"),0,0,100);Number(Details,TEXT("국부 허용압 [t/m²] · 0 미확인"),0,0,1000);
    Number(Details,TEXT("풍속 [m/s] · 합성 진자"),0,0,50);Number(Details,TEXT("횡요 진폭 [°]"),5,0,40);Number(Details,TEXT("종요 진폭 [°]"),2,0,40);Number(Details,TEXT("운동 주기 [s]"),10,2,60);
    Number(Details,TEXT("쐐기 검증 내력 [kN] · 0 미확인"),0,0,10000);Number(Details,TEXT("선체 연결 래싱 내력 [kN]"),0,0,10000);Number(Details,TEXT("보정 지름 강성 [kN/m]"),0,0,10000000);Number(Details,TEXT("제조사 타원도(ovality) 한계 · 0 미확인"),0,0,.2);
    Number(Details,TEXT("훅 SWL [t]"),40,.1,500);Number(Details,TEXT("장구 자중 [t]"),2,0,100);Number(Details,TEXT("합성 경심 높이(KM) [m]"),9,0,100);Number(Details,TEXT("강 밀도 [t/m³]"),7.85,.1,30);Number(Details,TEXT("권취 충전율 η"),.97,.01,1);
    Number(Details,TEXT("장구 다리 WLL [t] · 0 미확인"),0,0,500);Number(Details,TEXT("슬링 다리 각도 [°] · 수직 기준"),30,0,85);
    Number(Details,TEXT("냉연 포장 방사 두께 [m] · 예제"),.0015,0,.05);Number(Details,TEXT("냉연 포장 한쪽 측면 [m] · 예제"),.002,0,.05);
    Number(Details,TEXT("강재/포장 간 접촉 μ"),.1,0,2);
    Number(Details,TEXT("구름 저항 모멘트 팔 [m] · 0 이상 원통"),0,0,.1);Number(Details,TEXT("MCT1cm [t·m/cm] · 0 미확인"),0,0,100000);
    Number(Details,TEXT("LCF · AP 기준 [m]"),0,0,300);Number(Details,TEXT("선체-부두 간격 [m]"),2,0,20);Number(Details,TEXT("부두 끝-안쪽 레일 [m]"),3,0,20);Number(Details,TEXT("레일 간격 [m]"),12,1,30);Number(Details,TEXT("부두 방향 · -1 우현 / +1 좌현"),-1,-1,1);
    Number(Details,TEXT("초기 기준 흘수 [m]"),6.2,.1,30);Number(Details,TEXT("흘수 1cm당 톤수(TPC) [t/cm] · 0 합성 추정"),0,0,200);Number(Details,TEXT("합성 수선면 계수 Cwp"),.85,.1,1);Number(Details,TEXT("물 밀도 [t/m³]"),1.025,.9,1.3);Number(Details,TEXT("허용 흘수 [m] · 0 미확인"),9.38,0,30);
    const auto Flag=[&](const TCHAR* Name,bool Checked){auto* C=WidgetTree->ConstructWidget<UCheckBox>();C->SetCheckedState(Checked?ECheckBoxState::Checked:ECheckBoxState::Unchecked);C->SetContent(Label(Name,12));Details->AddChildToVerticalBox(C);Flags.Add(C);};
    Flag(TEXT("받침목 · 3줄 200×50 mm"),true);Flag(TEXT("쐐기 있음 · 내력 입력 별도"),true);Flag(TEXT("래싱 있음 · 선체 연결 내력 별도"),false);Flag(TEXT("코일 구멍축 선수미 방향"),true);Flag(TEXT("젖은 강재 접촉 · μ0 시험"),false);Flag(TEXT("적재 지침서(Loading Manual) 입력 확인"),false);Flag(TEXT("냉연 포장 접촉 μ 인증값 확인"),false);Flag(TEXT("LCF 정수력 입력 확인"),false);Flag(TEXT("실선 TPC·기준 적재상태 확인"),false);
    Details->AddChildToVerticalBox(Label(TEXT("선박: N-class 참고 합성 배치\n174.10 m × 22.00 m · 상자형 화물창 4개\n작업 화물창/해치 24.80 m × 18.80 m\n화물창 높이 14.00 m · 균등분포하중(UDL) 14.5 t/m² · 국부 강도 미확인\n배수량 30000 t / 무게중심 높이(KG) 7 m / 경심 높이(KM) 9 m / 자유표면 0.2 m · 가상\nIMO CSS Annex6/13 · 연구 근거 문서 참조"),11));
    MotionChoice=WidgetTree->ConstructWidget<UComboBoxString>();MotionChoice->ForegroundColor=Style::Text();MotionChoice->SetWidgetStyle(Style::ComboBox());for(const TCHAR* V:{TEXT("사용자 횡요/종요(roll/pitch)"),TEXT("잔잔함 · 2°/1°/12s"),TEXT("보통 · 5°/2°/10s"),TEXT("강함 · 10°/5°/8s"),TEXT("CSS 하부 화물창(lower hold) 가속도")})MotionChoice->AddOption(V);MotionChoice->SetSelectedIndex(2);Details->AddChildToVerticalBox(MotionChoice);
    R=Row(Details);AddButton(R,TEXT("선박 운동 시험"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnMotion));AddButton(R,TEXT("센서 도구"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnTools));
    Details->AddChildToVerticalBox(Label(TEXT("불러올 계획/목록 경로 · 저장 후 자동 갱신"),12));FileInput=WidgetTree->ConstructWidget<UEditableTextBox>();FileInput->WidgetStyle=Style::Input();FileInput->SetText(FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CoilLoadingPlan.json"))));Details->AddChildToVerticalBox(FileInput);
    R=Row(Details);AddButton(R,TEXT("JSON 저장"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnSave));AddButton(R,TEXT("계획 읽기"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnLoad));
    R=Row(Details);AddButton(R,TEXT("목록 읽기"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnImport));
    R=Row(Details);AddButton(R,TEXT("슬롯 저장"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnSaveSlot));AddButton(R,TEXT("슬롯 읽기"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnLoadSlot));
    R=Row(Details);AddButton(R,TEXT("혼적 예시 · 경고 포함"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnMixedExample));
    auto* Header=Card(TEXT("CoilViewHeader"));auto* HB=WidgetTree->ConstructWidget<UVerticalBox>();Header->SetContent(HB);HB->AddChildToVerticalBox(Label(TEXT("제품 부두 코일 선적 · 사전 해석 / 실제 놓기 비교"),17,true));
    R=Row(HB);AddButton(R,TEXT("전경"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnOverview));AddButton(R,TEXT("홀드 내부"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnInterior));AddButton(R,TEXT("단면"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnSection));AddButton(R,TEXT("2D 상면"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnTop));AddButton(R,TEXT("점군"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnPoints));
    auto* FontButton=Button(*CoilLoadingPresentation::FontLabel(TextScale),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnFontSize));FontScaleLabel=Cast<UTextBlock>(FontButton->GetContent());FontButton->SetToolTipText(FText::FromString(TEXT("코일 선적 패널 글자만 100/150%로 전환합니다. 센서 도구는 하단의 센서 도구 글자 메뉴에서 별도로 변경하세요.")));R->AddChildToHorizontalBox(FontButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    // The same buttons must retain complete labels beside a wide tools sidebar.
    for(int32 I=0;I<R->GetChildrenCount();++I)if(auto* B=Cast<UButton>(R->GetChildAt(I)))if(auto* T=Cast<UTextBlock>(B->GetContent()))
    {T->SetAutoWrapText(true);T->SetJustification(ETextJustify::Center);if(auto* ContentSlot=Cast<UButtonSlot>(T->Slot))ContentSlot->SetHorizontalAlignment(HAlign_Fill);}
    Operation=Label(TEXT("매단 하중 아래 출입 금지 · 빨간 원은 표시용 작업구역"),12);HB->AddChildToVerticalBox(Operation);
    PlotCard=Card(TEXT("CoilPlanCard"));PlanView=WidgetTree->ConstructWidget<UCoilLoadingPlanView>();PlanView->Monitor=this;PlotCard->SetContent(PlanView);PlotCard->SetVisibility(ESlateVisibility::Collapsed);
    auto* Footer=Card(TEXT("CoilAnalysis"));auto* FB=WidgetTree->ConstructWidget<UVerticalBox>();Footer->SetContent(FB);Metrics=Label(TEXT("질량·하중·무게중심 높이(KG)·횡경사"),13,true);
    Comparison=Label(TEXT("해석 / Chaos / LiDAR 대기"),12);auto* WS=WidgetTree->ConstructWidget<UScrollBox>();FB->AddChildToVerticalBox(WS)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    // Reuse the existing analysis scroll container for the same metric/diagnostic widgets.
    // At 150% their wrapped text must not consume the entire fixed-height footer.
    auto* List=WidgetTree->ConstructWidget<UVerticalBox>();WS->AddChild(List);List->AddChildToVerticalBox(Metrics);List->AddChildToVerticalBox(Comparison)->SetPadding(FMargin(0,6));WarningRows=WidgetTree->ConstructWidget<UVerticalBox>();List->AddChildToVerticalBox(WarningRows);
    auto* DataButton=Button(TEXT("추가 확인 · 펼치기"),GET_FUNCTION_NAME_CHECKED(UCoilLoadingMonitor,OnDataNeeded));DataNeededLabel=Cast<UTextBlock>(DataButton->GetContent());DataNeededLabel->SetAutoWrapText(true);DataButton->SetToolTipText(FText::FromString(CoilLoadingPresentation::UnknownExplanation()));List->AddChildToVerticalBox(DataButton)->SetPadding(FMargin(0,8));DataRows=WidgetTree->ConstructWidget<UVerticalBox>();List->AddChildToVerticalBox(DataRows);DataRows->SetVisibility(ESlateVisibility::Collapsed);
    for(auto* C:{TypeChoice.Get(),InnerChoice.Get(),GearChoice.Get(),MotionChoice.Get(),RowDirection.Get()})C->OnGenerateWidgetEvent.BindDynamic(this,&UCoilLoadingMonitor::ComboEntry);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
void UCoilLoadingMonitor::Bind(ACoilLoadingMonitorHost* H,ACoilLoadingSimulation* S){Host=H;Sim=S;SyncInputs();Refresh();}
void UCoilLoadingMonitor::SyncInputs()
{
    auto* S=Sim.Get();if(!S||Numbers.Num()<40)return;TGuardValue<bool> Sync(bSync,true);IdInput->SetText(FText::FromString(S->NextSpec.CoilId));
    TypeChoice->SetSelectedIndex(int32(S->NextSpec.Type));InnerChoice->SetSelectedIndex(FMath::IsNearlyEqual(S->NextSpec.ID,.508)?0:FMath::IsNearlyEqual(S->NextSpec.ID,.610)?1:2);GearChoice->SetSelectedIndex(S->Settings.bCHook?1:0);RowDirection->SetSelectedIndex(S->bRowsForeToAft?0:1);
    const double V[]={S->NextSpec.OD,S->NextSpec.Width,S->NextSpec.MassOverride,S->NextSpec.Mu,double(S->NextTier),double(S->NextRow),double(S->NextColumn),S->Settings.UDLLimit,S->Settings.ApprovedPatchArea,S->Settings.LocalPressureLimit,S->Settings.WindMps,S->RollDegrees,S->PitchDegrees,S->MotionPeriod,S->NextChockKN,S->NextLashingKN,S->NextStiffness,S->NextOvalityLimit,S->Settings.HookSWL,S->Settings.GearMass,S->Settings.KM,S->NextSpec.Density,S->NextSpec.Fill,S->Settings.LegWLLTonnes,S->Settings.LegAngleDegrees,S->NextSpec.PackageRadial,S->NextSpec.PackageEnd,S->NextSpec.SteelMu,S->NextRollingResistanceM,S->Settings.MCT1cm,S->Settings.LCFFromAP,S->Settings.BerthGap,S->Settings.RailSetback,S->Settings.RailGauge,double(S->Settings.QuaySide),S->Settings.Draft,S->Settings.TPC,S->Settings.WaterplaneCoefficient,S->Settings.WaterDensity,S->Settings.SummerDraftLimit};
    for(int32 I=0;I<Numbers.Num();++I)Numbers[I]->SetValue(V[I]);const bool B[]={S->bNextDunnage,S->bNextChocks,S->bNextLashing,S->bNextForeAft,S->Settings.bWetSteel,S->Settings.bLoadingManual,S->NextSpec.bPackagedFrictionConfirmed,S->Settings.bLCFConfirmed,S->Settings.bTPCConfirmed};for(int32 I=0;I<Flags.Num();++I)Flags[I]->SetIsChecked(B[I]);
}
bool UCoilLoadingMonitor::ApplyInputs()
{
    auto* S=Sim.Get();if(!S||bSync)return false;if(S->Phase()!=ECoilLoadingPhase::Idle){S->Status=TEXT("작업 중 사양 변경 불가 · 먼저 취소하세요");return false;}
    if(FMath::RoundToInt(Numbers[4]->GetValue())>CoilLoading::MaximumTier){S->Status=TEXT("배치 거절: 최대 5단입니다. 입력한 단수를 줄이세요.");return false;}
    auto Spec=S->NextSpec;Spec.CoilId=IdInput->GetText().ToString();Spec.OD=Numbers[0]->GetValue();Spec.Width=Numbers[1]->GetValue();Spec.MassOverride=Numbers[2]->GetValue();Spec.Mu=Numbers[3]->GetValue();Spec.ID=InnerChoice->GetSelectedIndex()==0?.508:InnerChoice->GetSelectedIndex()==1?.610:.762;Spec.Density=Numbers[21]->GetValue();Spec.Fill=Numbers[22]->GetValue();FString E;
    Spec.Type=TypeChoice->GetSelectedIndex()==1?CoilLoading::ECoilType::ColdRolled:CoilLoading::ECoilType::HotRolled;Spec.PackageRadial=Numbers[25]->GetValue();Spec.PackageEnd=Numbers[26]->GetValue();Spec.SteelMu=Numbers[27]->GetValue();Spec.bPackagedFrictionConfirmed=Flags[6]->IsChecked();
    if(!Spec.Validate(E)){S->Status=E;return false;}S->NextSpec=Spec;S->NextTier=FMath::RoundToInt(Numbers[4]->GetValue());S->NextRow=FMath::RoundToInt(Numbers[5]->GetValue());S->NextColumn=FMath::RoundToInt(Numbers[6]->GetValue());
    S->Settings.UDLLimit=Numbers[7]->GetValue();S->Settings.ApprovedPatchArea=Numbers[8]->GetValue();S->Settings.LocalPressureLimit=Numbers[9]->GetValue();S->Settings.WindMps=Numbers[10]->GetValue();S->RollDegrees=Numbers[11]->GetValue();S->PitchDegrees=Numbers[12]->GetValue();S->MotionPeriod=Numbers[13]->GetValue();
    S->NextChockKN=Numbers[14]->GetValue();S->NextLashingKN=Numbers[15]->GetValue();S->NextStiffness=Numbers[16]->GetValue();S->NextOvalityLimit=Numbers[17]->GetValue();S->Settings.HookSWL=Numbers[18]->GetValue();S->Settings.GearMass=Numbers[19]->GetValue();S->Settings.KM=Numbers[20]->GetValue();S->Settings.LegWLLTonnes=Numbers[23]->GetValue();S->Settings.LegAngleDegrees=Numbers[24]->GetValue();
    S->NextRollingResistanceM=Numbers[28]->GetValue();S->Settings.MCT1cm=Numbers[29]->GetValue();S->Settings.LCFFromAP=Numbers[30]->GetValue();S->Settings.BerthGap=Numbers[31]->GetValue();S->Settings.RailSetback=Numbers[32]->GetValue();S->Settings.RailGauge=Numbers[33]->GetValue();S->Settings.QuaySide=FMath::RoundToInt(Numbers[34]->GetValue());S->Settings.bLCFConfirmed=Flags[7]->IsChecked();S->Settings.Draft=Numbers[35]->GetValue();S->Settings.TPC=Numbers[36]->GetValue();S->Settings.WaterplaneCoefficient=Numbers[37]->GetValue();S->Settings.WaterDensity=Numbers[38]->GetValue();S->Settings.SummerDraftLimit=Numbers[39]->GetValue();S->Settings.bTPCConfirmed=Flags[8]->IsChecked();
    S->bNextDunnage=Flags[0]->IsChecked();S->bNextChocks=Flags[1]->IsChecked();S->bNextLashing=Flags[2]->IsChecked();S->bNextForeAft=Flags[3]->IsChecked();S->Settings.bWetSteel=Flags[4]->IsChecked();S->Settings.bLoadingManual=Flags[5]->IsChecked();S->Settings.bCHook=GearChoice->GetSelectedIndex()==1;S->RefreshPreview();return true;
}
void UCoilLoadingMonitor::Refresh()
{
    auto* S=Sim.Get();if(!S||!IsVisible())return;if(LastPlanCount!=S->Plan().Num()){LastPlanCount=S->Plan().Num();SyncInputs();}const FVector2D FullSize=UWidgetLayoutLibrary::GetViewportSize(this)/UWidgetLayoutLibrary::GetViewportScale(this);if(FullSize.X<=0||FullSize.Y<=0)return;
    const FBox2D Area=CoilLoadingPresentation::AvailableArea(FullSize,Host.IsValid()?Host->GetToolsInset():FVector2D::ZeroVector);
    const FVector2D Size=Area.GetSize(),Offset=Area.Min;
    const double Left=FMath::Min(360.*TextScale,Size.X*.34),X=Left+24,Width=FMath::Max(100.,Size.X-X-12),FooterH=FMath::Clamp(Size.Y*.27,170.,260.);
    const double HeaderHeight=Slots.IsValidIndex(1)&&Slots[1]->Content?Slots[1]->Content->GetDesiredSize().Y:0.;
    const double Top=FMath::Max(160.*TextScale,HeaderHeight+24.);
    const FVector2D Positions[]={{12,12},{X,12},{X,Top},{X,Size.Y-FooterH-12}},Sizes[]={{Left,Size.Y-24},{Width,Top-24},{Width,Size.Y-FooterH-Top-24},{Width,FooterH}};
    for(int32 I=0;I<Slots.Num();++I){Slots[I]->SetPosition(Positions[I]+Offset);Slots[I]->SetSize(Sizes[I]);}
    const FVector2D MaskPositions[]={{0,0},{0,Size.Y-FooterH-24},{0,Top},{FullSize.X-12,Top}},MaskSizes[]={{FullSize.X,Top},{FullSize.X,FullSize.Y-Size.Y+FooterH+24},{X+Offset.X,Size.Y-FooterH-Top-24},{12,Size.Y-FooterH-Top-24}};
    for(int32 I=0;I<SceneMasks.Num();++I){SceneMasks[I]->SetPosition(MaskPositions[I]);SceneMasks[I]->SetSize(MaskSizes[I]);}
    ViewRect=FSlateRect(X+Offset.X,Top,FullSize.X-12,Size.Y-FooterH-24);SceneInputSlot->SetPosition({X+Offset.X,Top});SceneInputSlot->SetSize({Width,ViewRect.Bottom-ViewRect.Top});const int32 View=S->View();PlotCard->SetVisibility(View==2||View==3?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(Host.IsValid())Host->SetSceneRect(ViewRect,FullSize,View!=2&&View!=3);
    const int32 Selected=S->HighlightedCoils().Num()==1?S->HighlightedCoils()[0]:INDEX_NONE;RemoveButton->SetIsEnabled(S->Plan().IsValidIndex(Selected)&&S->Phase()==ECoilLoadingPhase::Idle);SelectionText->SetText(FText::FromString(S->Plan().IsValidIndex(Selected)?TEXT("선택: ")+S->Plan()[Selected].Spec.CoilId:TEXT("선택 없음 · 코일 클릭 후 제거")));
    if(!S->LastPlanSavePath.IsEmpty()&&S->LastPlanSavePath!=LastShownPlanPath){LastShownPlanPath=S->LastPlanSavePath;FileInput->SetText(FText::FromString(LastShownPlanPath));}
    Operation->SetText(FText::FromString(S->Status+FString::Printf(TEXT(" · 화물창 %.2f m × %.2f m / 개구 %.2f m × %.2f m"),S->Settings.HoldLength,S->Settings.HoldWidth,S->Settings.OpeningLength,S->Settings.OpeningWidth)));
    const auto& R=S->Analysis();const auto& P=S->Physics();const auto Runtime=S->Lidar?S->Lidar->GetSensorRuntimeStatus():FVirtualSensorRuntimeStatus();
    Comparison->SetText(FText::FromString(CoilLoadingPresentation::ComparisonSummary(P.State,P.MaxDisplacement,P.MaxRotationDegrees,Runtime.MeasuredAcquisitionRateHz,S->FittedCoilCount,S->bLidarCompared,S->LidarDeviationP95,S->LidarFitResidualP95)));
    Comparison->SetToolTipText(FText::FromString(FString::Printf(TEXT("측정 방식(backend): %s\n중심 편차 p95·표면 잔차는 LiDAR 코일 맞춤 비교 진단입니다. 점/방향 부족 시 비교 불가로 표시합니다."),*Runtime.ActiveAcquisitionBackend)));
    if(LastRevision==S->Revision())return;LastRevision=S->Revision();
    Heading->SetText(FText::FromString(FString::Printf(TEXT("%d단 · %d열 · %d번 / 배치 %d개"),S->NextTier,S->NextRow,S->NextColumn,S->Plan().Num())));
    const FString UDL=S->Settings.ApprovedPatchArea>0?FString::Printf(TEXT("%.1f%%"),R.MaxUtilization*100):TEXT("자료 필요");
    Metrics->SetText(FText::FromString(CoilLoadingPresentation::MetricSummary(R.TotalMass,S->NextSpec.GrossMass(),R.KG,R.GM,R.HeelDegrees,UDL,S->Hydrostatics().Draft,R.Hydro.Draft,R.Hydro.TonnesPerCm,R.Hydro.bConfirmed)));
    struct FGroup{FName Code;CoilLoading::ESeverity Level;bool Known;FString Reason;TArray<int32> Coils;double Value=0,Limit=0;};
    TArray<FGroup> Groups;
    for(const auto& I:R.Issues)
    {
        auto* Group=Groups.FindByPredicate([&](const FGroup& G){return G.Code==I.Code&&G.Known==I.bKnown;});
        if(!Group){FGroup G;G.Code=I.Code;G.Known=I.bKnown;G.Level=I.Severity;G.Reason=I.Reason;Groups.Add(MoveTemp(G));Group=&Groups.Last();}
        if(I.Coil>=0)Group->Coils.AddUnique(I.Coil);if(uint8(I.Severity)>uint8(Group->Level))Group->Level=I.Severity;
        const double Ratio=I.Limit>0?I.Value/I.Limit:FMath::Abs(I.Value),Old=Group->Limit>0?Group->Value/Group->Limit:FMath::Abs(Group->Value);
        if(Ratio>=Old){Group->Value=I.Value;Group->Limit=I.Limit;}
    }
    Groups.Sort([](const FGroup& A,const FGroup& B){if(A.Known!=B.Known)return A.Known;return uint8(A.Level)>uint8(B.Level);});
    DangerCount=CautionCount=DataNeededCount=0;CachedWarnings.Reset();WarningRows->ClearChildren();DataRows->ClearChildren();
    if(DataExpanded())
    {
        DataRows->AddChildToVerticalBox(MakeIssueText(CoilLoadingPresentation::UnknownExplanation(),Style::Muted()));
    }
    for(const auto& G:Groups)
    {
        if(!G.Known)++DataNeededCount;else if(G.Level==CoilLoading::ESeverity::Warning)++DangerCount;else if(G.Level==CoilLoading::ESeverity::Caution)++CautionCount;
        if(!G.Known&&!DataExpanded())continue;
        FString Text=FString::Printf(TEXT("%s · %s · %s"),!G.Known?TEXT("미확인"):G.Level==CoilLoading::ESeverity::Warning?TEXT("위험"):TEXT("주의"),*G.Reason,G.Coils.Num()?*FString::Printf(TEXT("%d개 코일%s"),G.Coils.Num(),G.Coils.Contains(S->Plan().Num())?TEXT(" (다음 포함)"):TEXT("")):TEXT("선박·공통"));
        if(!G.Known)Text+=FString(TEXT("\n확인 위치: "))+CoilLoadingPresentation::InputLocation(G.Code);
        if(G.Known&&(G.Value!=0||G.Limit!=0))Text+=FString::Printf(TEXT(" · 최악 %.3f / %.3f"),G.Value,G.Limit);
        auto* B=WidgetTree->ConstructWidget<UCoilLoadingWarningButton>();B->Bind(S,G.Coils);B->SetStyle(Style::QuietButton());B->SetContent(MakeIssueText(Text,!G.Known?Style::Muted():G.Level==CoilLoading::ESeverity::Warning?Style::Danger():Style::Warning()));
        (G.Known?WarningRows:DataRows)->AddChildToVerticalBox(B);if(G.Known)CachedWarnings+=Text+TEXT("\n");
    }
    Badge->SetText(FText::FromString(CoilLoadingPresentation::IssueSummary(DangerCount,CautionCount,DataNeededCount)));Badge->SetToolTipText(FText::FromString(CoilLoadingPresentation::UnknownExplanation()));Badge->SetColorAndOpacity(DangerCount?Style::Danger():CautionCount?Style::Warning():DataNeededCount?Style::Muted():Style::Success());
    DataNeededLabel->SetText(FText::FromString(CoilLoadingPresentation::AdditionalChecks(DataNeededCount,DataExpanded())));PlanView->InvalidateLayoutAndVolatility();
}
UTextBlock* UCoilLoadingMonitor::MakeIssueText(const FString& Text,FLinearColor Color)
{
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Text));Label->SetAutoWrapText(true);Label->SetFont(Style::Font(12,TextScale));Label->SetColorAndOpacity(Color);return Label;
}
void UCoilLoadingMonitor::OnApply(){ApplyInputs();Refresh();}
void UCoilLoadingMonitor::OnPlace(){if(!ApplyInputs())return;if(auto* S=Sim.Get())S->PlaceNext();SyncInputs();}
void UCoilLoadingMonitor::OnPlanOnly(){if(!ApplyInputs())return;if(auto* S=Sim.Get())S->PlaceNext(false);SyncInputs();}
void UCoilLoadingMonitor::OnTier(){if(!ApplyInputs())return;if(auto* S=Sim.Get())S->AddTier();SyncInputs();}
void UCoilLoadingMonitor::OnUndo(){if(auto* S=Sim.Get())S->Undo();SyncInputs();}
void UCoilLoadingMonitor::OnCancel(){if(auto* S=Sim.Get())S->CancelOperation();SyncInputs();}
void UCoilLoadingMonitor::OnMotion()
{
    if(!ApplyInputs())return;auto* S=Sim.Get();if(!S)return;const int32 P=MotionChoice->GetSelectedIndex();S->Settings.bUseCSS=P==4;
    if(P>=1&&P<=3){const double R[]={2,5,10},Pitch[]={1,2,5},Period[]={12,10,8};S->RollDegrees=R[P-1];S->PitchDegrees=Pitch[P-1];S->MotionPeriod=Period[P-1];}
    if(!S->Settings.bUseCSS){S->Settings.Ay=CoilLoading::Gravity*FMath::Sin(FMath::DegreesToRadians(S->RollDegrees));S->Settings.Ax=CoilLoading::Gravity*FMath::Sin(FMath::DegreesToRadians(S->PitchDegrees));}
    S->RefreshPreview();S->StartMotionTest();SyncInputs();
}
void UCoilLoadingMonitor::OnSave(){if(auto* S=Sim.Get())S->RequestPlanSave();}
void UCoilLoadingMonitor::OnLoad(){if(auto* S=Sim.Get())S->LoadPlan(FileInput->GetText().ToString());SyncInputs();}
void UCoilLoadingMonitor::OnSaveSlot(){if(auto* S=Sim.Get())S->SaveSlot();}
void UCoilLoadingMonitor::OnLoadSlot(){if(auto* S=Sim.Get())S->LoadSlot();SyncInputs();}
void UCoilLoadingMonitor::LoadMixedExample()
{
    if(auto* S=Sim.Get())if(S->Phase()==ECoilLoadingPhase::Idle)
    {S->NextSpec=CoilLoading::FSpec::Representative(CoilLoading::ECoilType::HotRolled);S->NextTier=1;S->NextRow=1;S->NextColumn=7;const FString Path=FPaths::ProjectDir()/TEXT("docs/examples/coil-loading-mixed.json");FileInput->SetText(FText::FromString(Path));S->LoadPlan(Path);}
}
void UCoilLoadingMonitor::OnMixedExample(){LoadMixedExample();}
void UCoilLoadingMonitor::OnImport(){auto* S=Sim.Get();if(!S)return;const FString Path=FileInput->GetText().ToString();if(IFileManager::Get().FileSize(*Path)>16*1024*1024){S->Status=TEXT("목록 파일16MiB 초과");return;}FString Text;if(FFileHelper::LoadFileToString(Text,*Path))S->ImportCoils(Text,Path.EndsWith(TEXT(".json")));else S->Status=TEXT("목록 파일 읽기 실패");SyncInputs();}
void UCoilLoadingMonitor::OnDetails(){Details->SetVisibility(Details->IsVisible()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);}
void UCoilLoadingMonitor::OnDataNeeded(){DataRows->SetVisibility(DataExpanded()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);LastRevision=MAX_uint64;Refresh();}
void UCoilLoadingMonitor::OnTypeChanged(FString Value,ESelectInfo::Type)
{
    auto* S=Sim.Get();if(!S||bSync)return;if(S->Phase()!=ECoilLoadingPhase::Idle){SyncInputs();return;}
    const auto Kind=Value.StartsWith(TEXT("냉연"))?CoilLoading::ECoilType::ColdRolled:CoilLoading::ECoilType::HotRolled;
    const FString Id=S->NextSpec.CoilId;S->NextSpec=CoilLoading::FSpec::Representative(Kind);S->NextSpec.CoilId=Id;S->RefreshPreview();SyncInputs();
}
void UCoilLoadingMonitor::SelectView(int32 V){if(Host.IsValid())Host->SetView(V);Refresh();}
void UCoilLoadingMonitor::OnOverview(){SelectView(0);}void UCoilLoadingMonitor::OnInterior(){SelectView(1);}void UCoilLoadingMonitor::OnSection(){SelectView(2);}void UCoilLoadingMonitor::OnTop(){SelectView(3);}void UCoilLoadingMonitor::OnPoints(){SelectView(4);}void UCoilLoadingMonitor::OnTools(){if(Host.IsValid())Host->ShowTools();}
void UCoilLoadingMonitor::OnCraneClose(){if(Host.IsValid())Host->Focus(0);}void UCoilLoadingMonitor::OnCoilClose(){if(Host.IsValid())Host->Focus(1);}void UCoilLoadingMonitor::OnShed(){if(Host.IsValid())Host->Focus(2);}
void UCoilLoadingMonitor::OnRemove(){if(auto* S=Sim.Get()){S->RemoveCoil(S->HighlightedCoils().Num()==1?S->HighlightedCoils()[0]:INDEX_NONE);SyncInputs();Refresh();}}
void UCoilLoadingMonitor::OnRowDirection(FString,ESelectInfo::Type){if(!bSync)if(auto* S=Sim.Get()){if(S->Phase()!=ECoilLoadingPhase::Idle){SyncInputs();return;}S->bRowsForeToAft=RowDirection->GetSelectedIndex()==0;S->RefreshPreview();}}
void UCoilLoadingMonitor::OnFontSize(){SetFontPercent(TextScale>1?100:150);}
void UCoilLoadingMonitor::SetFontPercent(int32 Percent)
{
    const float Scale=Percent==150?1.5f:1.f,Ratio=Scale/TextScale;TextScale=Scale;
    TArray<UWidget*> Widgets;WidgetTree->GetAllWidgets(Widgets);
    for(auto* W:Widgets)
    {
        if(auto* T=Cast<UTextBlock>(W)){auto Font=T->GetFont();Font.Size=FMath::RoundToInt(Font.Size*Ratio);T->SetFont(Font);}
        if(auto* Spin=Cast<USpinBox>(W))Spin->SetFont(Style::Font(13,Scale));
        if(auto* Edit=Cast<UEditableTextBox>(W)){Edit->WidgetStyle.TextStyle.SetFont(Style::Font(13,Scale));Edit->SynchronizeProperties();}
        if(auto* Combo=Cast<UComboBoxString>(W)){TGuardValue<bool> Sync(bSync,true);const auto Selected=Combo->GetSelectedOption();Combo->ClearSelection();Combo->SetSelectedOption(Selected);Combo->RefreshOptions();}
    }
    if(FontScaleLabel)FontScaleLabel->SetText(FText::FromString(CoilLoadingPresentation::FontLabel(TextScale)));
    LastRevision=MAX_uint64;Refresh();
}
UWidget* UCoilLoadingMonitor::ComboEntry(FString Value)
{
    ComboLabels.RemoveAll([](const auto& W){if(!W)return true;const auto Native=W->GetCachedWidget();return !Native.IsValid()||Native.GetSharedReferenceCount()<=2;});
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Value));Label->SetFont(Style::Font(13,TextScale));Label->SetColorAndOpacity(Style::Text());ComboLabels.Add(Label);return Label;
}
FReply UCoilLoadingMonitor::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    auto* S=Sim.Get();auto* PC=GetOwningPlayer();const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
    if(S&&PC&&E.GetEffectingButton()==EKeys::LeftMouseButton&&S->View()!=2&&S->View()!=3&&ViewRect.ContainsPoint(P))
    {
        int32 W,H;PC->GetViewportSize(W,H);const auto Size=G.GetLocalSize();FVector O,D;
        if(Size.X>0&&Size.Y>0&&PC->DeprojectScreenPositionToWorld(P.X*W/Size.X,P.Y*H/Size.Y,O,D))
        {const int32 Index=S->PickCoil(O,D);if(Index!=INDEX_NONE){S->HighlightCoils({Index});Refresh();return FReply::Handled();}}
    }
    return Super::NativeOnMouseButtonDown(G,E);
}
