#include "VirtualSensorPlacementPanelWidget.h"
#include "SensorToolWidgetDecl.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"
#include "Engine/World.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Rendering/DrawElements.h"

class SImuPlot final:public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SImuPlot){} SLATE_END_ARGS()
    void Construct(const FArguments&){}
    void Push(UVirtualImuComponent* Source)
    {if(Owner!=Source){Owner=Source;Values.Reset();Last=-1;}if(Source&&Source->HasSample()&&Last!=Source->GetLatestSample().Sequence){Last=Source->GetLatestSample().Sequence;Values.Add(Source->GetLatestSample().SpecificForce);if(Values.Num()>100)Values.RemoveAt(0);Invalidate(EInvalidateWidgetReason::Paint);}}
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(240,80);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& List,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const auto Size=G.GetLocalSize();const FLinearColor Colors[]={FLinearColor(1,.3,.3),FLinearColor(.3,1,.3),FLinearColor(.3,.6,1)};
        for(int32 Axis=0;Axis<3;++Axis){TArray<FVector2D> Points;for(int32 I=0;I<Values.Num();++I)Points.Add(FVector2D(Size.X*I/99,Size.Y*(.5-FMath::Clamp(Values[I][Axis]/40.,-.48,.48))));if(Points.Num()>1)FSlateDrawElement::MakeLines(List,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Colors[Axis],true,1.5f);}return Layer;
    }
private:
    TWeakObjectPtr<UVirtualImuComponent> Owner;TArray<FVector> Values;int64 Last=-1;
};
bool UVirtualSensorPlacementPanelWidget::AttachSelectedImu()
{EndPlacement();auto* A=Selected();if(!A||!A->CanEditSensorConfiguration(Status))return false;const bool Ok=UVirtualImuComponent::Attach(A)!=nullptr;Status=Ok?TEXT("IMU 부착 완료 · 기록과 전송은 따로 켜세요. 세션 종료 시 해제됩니다."):TEXT("IMU를 부착할 수 없습니다.");return Ok;}
void UVirtualSensorPlacementPanelWidget::RefreshImu()
{
    if(!ImuCsvRequest.IsEmpty())
    {const auto R=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(ImuCsvRequest);Status=R.Message;if(R.IsTerminal()){if(!R.FileResults.IsEmpty())Status+=TEXT(" · ")+R.FileResults[0].AbsolutePath;ImuCsvRequest.Reset();}}
    if(!ImuExpanded)return;
    auto* A=Selected();auto* C=A?A->FindComponentByClass<UVirtualImuComponent>():nullptr;
    if(ImuPlot)ImuPlot->Push(C);
    if(!C||!C->HasSample()){ImuValues=FText::FromString(C?TEXT("IMU 준비 중"):TEXT("IMU 없음 · 선택 장비에 부착하세요"));return;}
    const auto S=C->GetLatestSample();ImuValues=FText::FromString(FString::Printf(TEXT("비력 m/s²  %.3f  %.3f  %.3f\n각속도 rad/s  %.4f  %.4f  %.4f\n자세 xyzw  %.3f  %.3f  %.3f  %.3f\n표본 %lld · 기록 한도 초과 %lld\n그래프: X 빨강 · Y 초록 · Z 파랑 (±20 m/s², 최근 20초)"),S.SpecificForce.X,S.SpecificForce.Y,S.SpecificForce.Z,S.AngularVelocity.X,S.AngularVelocity.Y,S.AngularVelocity.Z,S.Orientation.X,S.Orientation.Y,S.Orientation.Z,S.Orientation.W,C->SampleCount,C->Recorder?C->Recorder->GetImuOverflowCount(C->GetImuId()):0));
}
TSharedRef<SWidget> UVirtualSensorPlacementPanelWidget::BuildImuSection()
{
    auto Current=[this](){auto* A=Selected();return A?A->FindComponentByClass<UVirtualImuComponent>():nullptr;};
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[SNewSensorTool(SButton).Text(FText::FromString(TEXT("선택 장비에 IMU 부착"))).IsEnabled_Lambda([Current](){return !Current();}).OnClicked_Lambda([this](){AttachSelectedImu();return FReply::Handled();})];
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("세션 전용 · FLU(전방/좌측/위) · 정지 시 +Z ≈ 9.81 · 자세는 시뮬레이터 정답")))];
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("표본 주파수 (Hz)")))];
    Box->AddSlot().AutoHeight()[SNew(SSpinBox<double>).MinValue(100).MaxValue(400).Delta(10).Value_Lambda([Current](){auto* C=Current();return C?C->GetConfig().RateHz:200.;}).IsEnabled_Lambda([Current](){return Current()!=nullptr;}).OnValueCommitted_Lambda([this,Current](double V,ETextCommit::Type){if(auto* C=Current()){auto Config=C->GetConfig();Config.RateHz=V;C->Configure(Config,Status);}})];
    Box->AddSlot().AutoHeight()[SNew(SCheckBox).IsEnabled_Lambda([Current](){return Current()!=nullptr;}).IsChecked_Lambda([Current](){auto* C=Current();return C&&C->GetConfig().Noise?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this,Current](ECheckBoxState V){if(auto* C=Current()){auto Config=C->GetConfig();Config.Noise=V==ECheckBoxState::Checked;Config.Profile=Config.Noise?EVirtualImuProfile::IndustrialMems:EVirtualImuProfile::Ideal;C->Configure(Config,Status);}})[SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("산업용 MEMS 가정 노이즈")))]];
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("노이즈 seed")))];
    Box->AddSlot().AutoHeight()[SNew(SSpinBox<int32>).MinValue(0).MaxValue(1000000).Value_Lambda([Current](){auto* C=Current();return C?C->GetConfig().Seed:1337;}).IsEnabled_Lambda([Current](){return Current()!=nullptr;}).OnValueCommitted_Lambda([this,Current](int32 V,ETextCommit::Type){if(auto* C=Current()){auto Config=C->GetConfig();Config.Seed=V;C->Configure(Config,Status);}})];
    auto Check=[&](FString Label,bool Record){Box->AddSlot().AutoHeight()[SNew(SCheckBox).IsEnabled_Lambda([Current](){return Current()!=nullptr;}).IsChecked_Lambda([Current,Record](){auto* C=Current();return C&&(Record?C->IsRecording():C->IsPublishing())?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([Current,Record](ECheckBoxState V){if(auto* C=Current()){if(Record)C->SetRecording(V==ECheckBoxState::Checked);else C->SetPublishing(V==ECheckBoxState::Checked);}})[SNewSensorTool(STextBlock).Text(FText::FromString(Label))]];};
    Check(TEXT("IMU 기록 (최대 24,000 표본)"),true);Check(TEXT("IMU 전송 · 별도 topic"),false);
    Box->AddSlot().AutoHeight()[SNewSensorTool(SButton).Text(FText::FromString(TEXT("기록한 IMU CSV 저장"))).IsEnabled_Lambda([Current](){return Current()!=nullptr;}).OnClicked_Lambda([this,Current](){if(auto* C=Current())ImuCsvRequest=C->SaveCsv();return FReply::Handled();})];
    Box->AddSlot().AutoHeight()[SNewSensorTool(STextBlock).AutoWrapText(true).Text_Lambda([this](){return ImuValues;})];
    Box->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(80)[SAssignNew(ImuPlot,SImuPlot)]];
    return SNew(SExpandableArea).InitiallyCollapsed(true).OnAreaExpansionChanged_Lambda([this](bool V){ImuExpanded=V;}).HeaderContent()[SNewSensorTool(STextBlock).Text(FText::FromString(TEXT("IMU · 부착·설정·미리보기")))].BodyContent()[Box];
}
