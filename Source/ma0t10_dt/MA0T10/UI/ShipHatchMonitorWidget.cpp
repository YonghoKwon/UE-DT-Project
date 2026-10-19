#include "ShipHatchMonitorWidget.h"
#include "ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "String/LexFromString.h"

struct FShipHatchMonitorWork
{
    struct FResult
    {
        ShipHatch::FAnalysis Analysis;
        ShipHatch::FSettings Settings;
        FTransform HoldPose=FTransform::Identity;
        TArray<FColor> Pixels;
        uint64 Revision=0;
        int32 View=0;
        double Cell=1;
        int64 MLX=-1,Livox=-1;
    };
    TFuture<FResult> Pending;
    FResult Current;
    FString ExportRequest;
};
void UShipHatchMonitorWidget::Bind(AShipHatchMonitorHost* H,AShipHatchSimulation* S)
{
    Host=H;Simulation=S;if(!S||!SeedInput)return;
    TGuardValue<bool> Sync(bSync,true);SeedInput->SetText(FText::FromString(LexToString(S->Seed)));
    MinFill->SetValue(S->FillMin);MaxFill->SetValue(S->FillMax);SensorHeight->SetValue(S->SensorAboveDeck);CargoChoice->SetSelectedIndex(S->bCoal?1:0);
}
void UShipHatchMonitorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();bPresistent=true;Work=MakeShared<FShipHatchMonitorWork>();
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto Text=[&](UVerticalBox* Box,const FString& Value,int32 Size=15)
    {auto* W=WidgetTree->ConstructWidget<UTextBlock>();W->SetText(FText::FromString(Value));W->SetAutoWrapText(true);
     auto Font=W->GetFont();Font.Size=Size;W->SetFont(Font);TextBlocks.Add(W);auto* Slot=Box->AddChildToVerticalBox(W);Slot->SetPadding(FMargin(0,5));return W;};
    auto* Controls=WidgetTree->ConstructWidget<UBorder>();Controls->SetBrushColor(FLinearColor(.018,.034,.054,.97));Controls->SetPadding(FMargin(20));
    ControlsSlot=Root->AddChildToCanvas(Controls);ControlsSlot->SetAnchors(FAnchors(1,0,1,1));ControlsSlot->SetAlignment(FVector2D(1,0));
    ControlsSlot->SetOffsets(FMargin(-20,20,510,20));
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Controls->SetContent(Scroll);
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Box);
    Text(Box,TEXT("해치 · 카고 홀드"),26);
    Text(Box,TEXT("선박 원료 측정 시뮬레이션 · HATCH-01"),13);
    auto PlainButton=[&](const TCHAR* Label)
    {auto* B=WidgetTree->ConstructWidget<UButton>();auto* W=WidgetTree->ConstructWidget<UTextBlock>();W->SetText(FText::FromString(Label));W->SetColorAndOpacity(FLinearColor(.01,.02,.03));auto F=W->GetFont();F.Size=16;W->SetFont(F);TextBlocks.Add(W);B->SetContent(W);Box->AddChildToVerticalBox(B)->SetPadding(FMargin(0,5));return B;};
    PlainButton(TEXT("적재 상태 바꾸기(랜덤)"))->OnClicked.AddDynamic(this,&UShipHatchMonitorWidget::OnRandom);
    StatusText=Text(Box,TEXT("측정 준비 중"));StatsText=Text(Box,TEXT(""));
    auto Choice=[&](const TCHAR* Label,TArray<FString> Values)
    {Text(Box,Label);auto* W=WidgetTree->ConstructWidget<UComboBoxString>();for(const auto& V:Values)W->AddOption(V);W->SetSelectedIndex(0);Box->AddChildToVerticalBox(W)->SetPadding(FMargin(0,2,0,5));return W;};
    ViewChoice=Choice(TEXT("보기"),{TEXT("일반 3D 보기"),TEXT("3D 점군 보기"),TEXT("2D 상면"),TEXT("높이 히트맵")});
    ColorChoice=Choice(TEXT("점 색"),{TEXT("높이 · jet"),TEXT("분류"),TEXT("반사 강도"),TEXT("거리"),TEXT("단색")});
    CargoChoice=Choice(TEXT("원료"),{TEXT("철광석"),TEXT("석탄")});
    SensorChoice=Choice(TEXT("측정 장비 · 해치 위 중앙"),{TEXT("ML-X(80) Integration 200 + Mid-360S"),TEXT("ML-X(80) Integration 200"),TEXT("Livox Mid-360S"),TEXT("ML-X(80) Native + Mid-360S"),TEXT("ML-X(80) Native")});
    auto Spin=[&](const TCHAR* Label,float Value,float Min,float Max,float Delta)
    {Text(Box,Label);auto* W=WidgetTree->ConstructWidget<USpinBox>();W->SetForegroundColor(FSlateColor(FLinearColor(.01,.02,.03)));W->SetMinValue(Min);W->SetMaxValue(Max);W->SetMinSliderValue(Min);W->SetMaxSliderValue(Max);W->SetDelta(Delta);W->SetValue(Value);Box->AddChildToVerticalBox(W);return W;};
    Text(Box,TEXT("현재 시드 · 직접 입력 후 적용"));SeedInput=WidgetTree->ConstructWidget<UEditableTextBox>();Box->AddChildToVerticalBox(SeedInput);
    auto* S=Simulation.Get();
    MinFill=Spin(TEXT("최소 적재율 (%)"),S?S->FillMin:35,0,95,1);
    MaxFill=Spin(TEXT("최대 적재율 (%)"),S?S->FillMax:65,0,95,1);
    SensorHeight=Spin(TEXT("갑판 위 센서 높이 (m)"),S?S->SensorAboveDeck:8,2.1,30,.5);
    GridSize=Spin(TEXT("분석 격자 목표 크기 (m)"),1,.25,5,.25);
    PointSize=Spin(TEXT("점 크기"),3,.25,12,.25);
    PointStride=Spin(TEXT("점 표시 간격"),2,1,100,1);
    PlainButton(TEXT("시드·범위·높이 적용"))->OnClicked.AddDynamic(this,&UShipHatchMonitorWidget::OnApply);
    PlainButton(TEXT("센서 도구 · PCD 전송 / 파일 저장"))->OnClicked.AddDynamic(this,&UShipHatchMonitorWidget::OnTools);
    PlainButton(TEXT("분석 JSON 저장 · 현재 측정값"))->OnClicked.AddDynamic(this,&UShipHatchMonitorWidget::OnExport);
    FontChoice=Choice(TEXT("글자 크기"),{TEXT("100%"),TEXT("150%")});
    Text(Box,TEXT("홀드 42.3 × 25.8 × 22.5 m\n개구부 23 × 20 m · 코밍 2 m\nPCD 기본 선택 · 분석 JSON은 버튼으로만 저장\n분류: 원료=주황 / 코밍=초록 / 홀드=파랑 / 바깥=빨강"),13);
    auto* Left=WidgetTree->ConstructWidget<UBorder>();Left->SetBrushColor(FLinearColor(.012,.023,.04,.98));Left->SetPadding(FMargin(24));
    AnalysisSlot=Root->AddChildToCanvas(Left);AnalysisSlot->SetAnchors(FAnchors(0,0,1,1));AnalysisSlot->SetOffsets(FMargin(24,70,550,50));
    auto* LeftBox=WidgetTree->ConstructWidget<UVerticalBox>();Left->SetContent(LeftBox);
    Text(LeftBox,TEXT("원료 높이 · 홀드 바닥 기준"),24);
    AnalysisImage=WidgetTree->ConstructWidget<UImage>();LeftBox->AddChildToVerticalBox(AnalysisImage)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LegendText=Text(LeftBox,TEXT("높이 0 m ━ 파랑 → 청록 → 노랑 → 빨강 ━ 22.5 m\n회색 격자: 측정 불가 · 흰 테두리: 해치 개구부\n수치는 관측 격자만 적분한 부분 부피입니다. 전체 홀드 부피를 외삽하지 않습니다."),16);
    Left->SetVisibility(ESlateVisibility::Collapsed);
    ViewChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnView);
    ColorChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnColor);
    CargoChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnCargo);
    SensorChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnSensor);
    FontChoice->OnSelectionChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnFont);
    GridSize->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnGrid);
    PointSize->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnPointSize);
    PointStride->OnValueChanged.AddDynamic(this,&UShipHatchMonitorWidget::OnPointStride);
    if(S){SeedInput->SetText(FText::FromString(LexToString(S->Seed)));bSync=true;CargoChoice->SetSelectedIndex(S->bCoal?1:0);bSync=false;}
    for(auto T:TextBlocks)BaseFontSizes.Add(T->GetFont().Size);
    SetFontPercent(100);
}
void UShipHatchMonitorWidget::NativeDestruct(){Work.Reset();Super::NativeDestruct();}
void UShipHatchMonitorWidget::SelectView(int32 V)
{
    ActiveView=FMath::Clamp(V,0,3);
    if(StatsText)StatsText->SetVisibility(ActiveView>=2?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
    if(ViewChoice&&ViewChoice->GetSelectedIndex()!=ActiveView){bSync=true;ViewChoice->SetSelectedIndex(ActiveView);bSync=false;}
    if(AnalysisImage)if(auto* Parent=AnalysisImage->GetParent())if(auto* Border=Parent->GetParent())Border->SetVisibility(ActiveView>=2?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
    if(auto* H=Host.Get())if(H->Coordinator)H->Coordinator->SetPointCloudOnlyMode(ActiveView==1);
    ApplyPreview();LastMLX=-1;LastLivox=-1;
}
void UShipHatchMonitorWidget::SelectColor(int32 C)
{
    ActiveColor=FMath::Clamp(C,0,4);
    if(ColorChoice&&ColorChoice->GetSelectedIndex()!=ActiveColor){bSync=true;ColorChoice->SetSelectedIndex(ActiveColor);bSync=false;}
    ApplyPreview();
}
void UShipHatchMonitorWidget::ApplyPreview()
{
    auto* S=Simulation.Get();if(!S)return;
    for(auto* A:{S->MLX.Get(),S->Livox.Get()})if(A)
    {
        auto* V=A->VisualizationComponent.Get();auto Settings=V->GetVisualizationSettings();
        const ELidarColorMode Modes[]={ELidarColorMode::RelativeHeight,ELidarColorMode::SemanticLabel,ELidarColorMode::ReflectionIntensity,ELidarColorMode::DistanceTurbo,ELidarColorMode::HitMask};
        Settings.ColorMode=Modes[ActiveColor];Settings.bAutoHeightRange=false;Settings.HeightReference=ELidarHeightReference::WorldZ;
        Settings.HeightMinMeters=S->GetActorLocation().Z*.01;Settings.HeightMaxMeters=Settings.HeightMinMeters+S->HoldDepth;
        Settings.PointSize=PointSize?PointSize->GetValue():3;
        V->SetColoredCpuPointPreview(true);V->SetHeightJetPalette(ActiveColor==0,TEXT("HatchCargo"));V->SetVisualizationSettings(Settings);
        A->ScanComponent->SetPreviewPolicy(FMath::RoundToInt(PointStride?PointStride->GetValue():2),12000,true);
        V->SetWorldPointCloudEnabled(ActiveView==1&&S->HasFreshMeasurement()&&((A==S->MLX&&S->bMLX)||(A==S->Livox&&S->bLivox)));
    }
}
void UShipHatchMonitorWidget::Refresh()
{
    auto* S=Simulation.Get();if(!S||!IsVisible()||!StatusText)return;
    if(bWasGenerating!=!S->HasFreshMeasurement()){bWasGenerating=!S->HasFreshMeasurement();ApplyPreview();}
    if(LastRevision!=S->Revision())
    {
        LastRevision=S->Revision();LastMLX=-1;LastLivox=-1;SeedInput->SetText(FText::FromString(LexToString(S->Seed)));
        {TGuardValue<bool> Sync(bSync,true);CargoChoice->SetSelectedIndex(S->bCoal?1:0);SensorChoice->SetSelectedIndex(!S->bMLX?2:!S->bLivox?(S->bNativeMLX?4:1):(S->bNativeMLX?3:0));}
        StatsText->SetText(FText::FromString(TEXT("새 측정 분석 대기")));AnalysisImage->SetRenderOpacity(0);ApplyPreview();
    }
    const auto& Surface=S->Surface();
    CachedStatus=FString::Printf(TEXT("%s\n시드 %d · %s · 설정 적재율 %.1f%%\n봉우리 %d · 경사 상한 %.1f° · 밀도 가정 %.0f kg/m³\nML-X %d점 / Mid-360S %d점"),
        *S->Status,S->Seed,S->bCoal?TEXT("석탄"):TEXT("철광석"),Surface.Fill,Surface.Peaks,Surface.Repose,Surface.Density,
        S->bMLX&&S->MLX?S->MLX->ScanComponent->GetLastHitPointCount():0,S->bLivox&&S->Livox?S->Livox->ScanComponent->GetLastHitPointCount():0);
    StatusText->SetText(FText::FromString(CachedStatus));
    if(!S->HasFreshMeasurement()){StatsText->SetText(FText::FromString(TEXT("새 형상 측정 대기")));return;}
    if(ActiveView>=2)RefreshAnalysis();
    if(Work&&!Work->ExportRequest.IsEmpty())
    {const auto R=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(Work->ExportRequest);if(R.IsTerminal()){S->Status=R.Message;Work->ExportRequest.Reset();}}
}
void UShipHatchMonitorWidget::RefreshAnalysis()
{
    auto* S=Simulation.Get();if(!S||!Work)return;
    if(Work->Pending.IsValid())
    {
        if(!Work->Pending.IsReady())return;
        auto Result=Work->Pending.Get();
        if(Result.Revision==S->Revision()&&Result.View==ActiveView&&Result.Cell==CellSize)
        {
            Work->Current=MoveTemp(Result);const auto& R=Work->Current;const auto& A=R.Analysis;++AnalysisBuilds;AnalysisImage->SetRenderOpacity(1);
            if(!AnalysisTexture){AnalysisTexture=UTexture2D::CreateTransient(768,512,PF_B8G8R8A8);AnalysisTexture->SRGB=true;AnalysisTexture->UpdateResource();AnalysisImage->SetBrushFromTexture(AnalysisTexture);}
            if(AnalysisTexture->GetResource())
            {auto* Data=new uint8[R.Pixels.Num()*4];FMemory::Memcpy(Data,R.Pixels.GetData(),R.Pixels.Num()*4);
             auto* Region=new FUpdateTextureRegion2D(0,0,0,0,768,512);AnalysisTexture->UpdateTextureRegions(0,1,Region,768*4,4,Data,[Region](uint8* P,const FUpdateTextureRegion2D*){delete[] P;delete Region;});}
            StatsText->SetText(FText::FromString(FString::Printf(TEXT("관측 격자 %d / %d · 범위 %.1f%%\n높이 최소 %.2f / 최고 %.2f / 평균 %.2f m\n측정 부분 추정 부피 %.1f m³\n부분 부피 / 전체 홀드 용량 %.1f%%\n전체 부피·적재율: 미관측 영역으로 계산 불가"),A.Observed,A.Cells.Num(),A.Coverage*100,A.Min,A.Max,A.Mean,A.Volume,A.FillLowerBound)));
        }
    }
    const int64 M=S->bMLX&&S->MLX?S->MLX->ScanComponent->GetRuntimeStatus().FrameId:-1;
    const int64 L=S->bLivox&&S->Livox?S->Livox->ScanComponent->GetRuntimeStatus().FrameId:-1;
    if(M==LastMLX&&L==LastLivox)return;LastMLX=M;LastLivox=L;
    TArray<ShipHatch::FObservation> Points;S->CollectObservations(Points);
    const auto Settings=S->Surface().Settings;const auto HoldPose=S->GetActorTransform();const double Cell=CellSize;const uint64 Revision=S->Revision();const int32 View=ActiveView;
    Work->Pending=Async(EAsyncExecution::ThreadPool,[Points=MoveTemp(Points),Settings,HoldPose,Cell,Revision,View,M,L]() mutable
    {
        FShipHatchMonitorWork::FResult R;R.Revision=Revision;R.View=View;R.Cell=Cell;R.MLX=M;R.Livox=L;R.Settings=Settings;R.HoldPose=HoldPose;
        R.Analysis=ShipHatch::Analyze(Settings,Cell,Points);R.Pixels.Init(FColor(14,22,34),768*512);
        const auto& A=R.Analysis;
        for(int32 Y=16;Y<496;++Y)for(int32 X=16;X<752;++X)
        {
            double U=double(X-16)/736,V=double(Y-16)/480;
            int32 CX=FMath::Min(int32(U*A.NY),A.NY-1),CY=FMath::Min(int32(V*A.NX),A.NX-1);
            if(A.NX==0||A.NY==0)continue;const auto& C=A.Cells[CX*A.NX+CY];
            FColor Color=C.Count?(View==3?ShipHatch::Jet(C.Mean()/Settings.Depth):(Settings.Cargo==ShipHatch::ECargo::Coal?FColor(28,32,39):FColor(150,67,43))):FColor(48,55,65);
            if(View==3&&(FMath::Frac(U*A.NY)<.025||FMath::Frac(V*A.NX)<.025))Color=FColor(22,28,38);
            const double WM=(U-.5)*Settings.Width,LM=(V-.5)*Settings.Length;
            if((FMath::Abs(FMath::Abs(WM)-Settings.OpeningWidth*.5)<.09&&FMath::Abs(LM)<=Settings.OpeningLength*.5)||
               (FMath::Abs(FMath::Abs(LM)-Settings.OpeningLength*.5)<.09&&FMath::Abs(WM)<=Settings.OpeningWidth*.5))Color=FColor::White;
            R.Pixels[Y*768+X]=Color;
        }
        // A physical meter legend, separate from missing cells.
        if(View==3)for(int32 Y=502;Y<510;++Y)for(int32 X=16;X<752;++X)R.Pixels[Y*768+X]=ShipHatch::Jet(double(X-16)/736);
        return R;
    });
}
void UShipHatchMonitorWidget::SetFontPercent(int32 Percent)
{
    const float Scale=Percent==150?1.5f:1.f;
    for(int32 I=0;I<TextBlocks.Num();++I)if(auto T=TextBlocks[I]){auto F=T->GetFont();F.Size=FMath::RoundToInt((BaseFontSizes.IsValidIndex(I)?BaseFontSizes[I]:15)*Scale);T->SetFont(F);}
    for(auto* Spin:{MinFill.Get(),MaxFill.Get(),SensorHeight.Get(),GridSize.Get(),PointSize.Get(),PointStride.Get()})if(Spin)
    {auto F=Spin->GetFont();F.Size=FMath::RoundToInt(16*Scale);Spin->SetFont(F);}
    // UE5.3 has no public font setter for these UMG controls; change only this map-owned instance.
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    if(SeedInput){SeedInput->WidgetStyle.TextStyle.Font.Size=FMath::RoundToInt(16*Scale);SeedInput->WidgetStyle.SetForegroundColor(FSlateColor(FLinearColor(.01,.02,.03)));SeedInput->WidgetStyle.SetFocusedForegroundColor(FSlateColor(FLinearColor(.01,.02,.03)));SeedInput->SynchronizeProperties();}
    TGuardValue<bool> Sync(bSync,true);
    for(auto* Combo:{ViewChoice.Get(),ColorChoice.Get(),CargoChoice.Get(),SensorChoice.Get(),FontChoice.Get()})if(Combo)
    {const int32 Index=Combo->GetSelectedIndex();Combo->Font.Size=FMath::RoundToInt(16*Scale);Combo->ClearSelection();Combo->SetSelectedIndex(Index);Combo->RefreshOptions();}
    if(FontChoice)FontChoice->SetSelectedIndex(Percent==150?1:0);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    if(ControlsSlot)ControlsSlot->SetOffsets(FMargin(-20,20,510*Scale,20));
    if(AnalysisSlot)AnalysisSlot->SetOffsets(FMargin(24,70,550*Scale,50));
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
    auto* S=Simulation.Get();if(!S||!Work||ActiveView<2||S->IsGenerating()||Work->Current.Revision!=S->Revision()||!Work->Current.Analysis.Observed)
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
void UShipHatchMonitorWidget::OnView(FString Value,ESelectInfo::Type){if(!bSync)SelectView(ViewChoice->GetSelectedIndex());}
void UShipHatchMonitorWidget::OnColor(FString Value,ESelectInfo::Type){if(!bSync)SelectColor(ColorChoice->GetSelectedIndex());}
void UShipHatchMonitorWidget::OnCargo(FString Value,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){Simulation->bCoal=CargoChoice->GetSelectedIndex()==1;Simulation->Regenerate(false);}}
void UShipHatchMonitorWidget::OnSensor(FString Value,ESelectInfo::Type)
{if(!bSync&&Simulation.IsValid()){int32 I=SensorChoice->GetSelectedIndex();Simulation->SetSensorSelection(I!=2,I==0||I==2||I==3,I>=3);ApplyPreview();}}
void UShipHatchMonitorWidget::OnFont(FString Value,ESelectInfo::Type){if(!bSync)SetFontPercent(FontChoice->GetSelectedIndex()?150:100);}
void UShipHatchMonitorWidget::OnGrid(float Value){CellSize=Value;LastMLX=-1;LastLivox=-1;}
void UShipHatchMonitorWidget::OnPointSize(float){ApplyPreview();}
void UShipHatchMonitorWidget::OnPointStride(float){ApplyPreview();}
