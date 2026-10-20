#include "ShipHatchMonitorWidget.h"
#include "ShipHatchMonitorHost.h"
#include "ShipHatchOverlay.h"
#include "SensorOwnedPresentationStyle.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

using HatchStyle=FSensorOwnedPresentationStyle;
void UShipHatchMonitorWidget::SetHatchPresentationVisible(bool Visible)
{if(Overlay)Overlay->SetVisibility(Visible);if(!Visible){FeedbackUntil=0;bOverlayDirty=true;}}
UWidget* UShipHatchMonitorWidget::ComboEntry(FString Value)
{
    ComboLabels.RemoveAll([](const auto& W){return !W.IsValid();});
    auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(Value));Text->SetColorAndOpacity(HatchStyle::Text());Text->SetFont(HatchStyle::Font(HatchStyle::BodySize,FontScale));ComboLabels.Add(Text);return Text;
}
void UShipHatchMonitorWidget::SetMapHeat(bool Heat)
{
    bMapHeat=Heat;if(MapSwitcher)MapSwitcher->SetActiveWidgetIndex(Heat?1:0);
    if(MapTopButton)MapTopButton->SetStyle(!Heat?HatchStyle::PrimaryButton():HatchStyle::Button());
    if(MapHeatButton)MapHeatButton->SetStyle(Heat?HatchStyle::PrimaryButton():HatchStyle::Button());InvalidatePlots();
}
double UShipHatchMonitorWidget::SectionOffset() const
{
    const auto* S=GetSurface();return S?(SectionNormalized-.5)*(bSectionLength?S->Settings.Width:S->Settings.Length):0;
}
const ShipHatch::FSurface* UShipHatchMonitorWidget::GetSurface() const{return Simulation.IsValid()?&Simulation->Surface():nullptr;}
void UShipHatchMonitorWidget::SetSection(bool AlongLength,double Normalized)
{
    if(!FMath::IsFinite(Normalized))return;bSectionLength=AlongLength;SectionNormalized=FMath::Clamp(Normalized,0.,1.);
    if(SectionSlider)SectionSlider->SetValue(SectionNormalized);UpdateSection();
}
bool UShipHatchMonitorWidget::CameraInHold(FVector& O,FVector& D,double& Fov) const
{
    if(!Host.IsValid()||!Host->ViewCamera||!Simulation.IsValid())return false;
    const auto Pose=Simulation->GetActorTransform();O=Pose.InverseTransformPosition(Host->ViewCamera->GetActorLocation())*.01;
    D=Pose.InverseTransformVectorNoScale(Host->ViewCamera->GetActorForwardVector()).GetSafeNormal();Fov=Host->ViewCamera->GetCameraComponent()->FieldOfView;return true;
}
void UShipHatchMonitorWidget::FocusAt(FVector2D XY)
{
    if(bDigEnabled||XY.ContainsNaN()||!Simulation.IsValid())return;
    Focus=FVector2D(FMath::Clamp(XY.X,-Simulation->HoldLength*.5,Simulation->HoldLength*.5),FMath::Clamp(XY.Y,-Simulation->HoldWidth*.5,Simulation->HoldWidth*.5));UpdateCamera();
}
void UShipHatchMonitorWidget::UpdateOverlay()
{
    const auto* S=GetSurface();if(!S||S->Heights.IsEmpty()||!Host.IsValid())return;
    const bool Feedback=FeedbackUntil>FPlatformTime::Seconds();
    if(!bOverlayDirty&&!Feedback&&!bHadFeedback)return;bOverlayDirty=false;bHadFeedback=Feedback;
    const double Now=FPlatformTime::Seconds();if(Now-LastOverlayUpdate<.2){bOverlayDirty=true;return;}LastOverlayUpdate=Now;
    if(!Overlay)
    {
        Host->Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));
        Overlay=NewObject<UShipHatchOverlay>(Host.Get());Overlay->SetCollisionEnabled(ECollisionEnabled::NoCollision);Overlay->SetHiddenInSceneCapture(true);Overlay->SetCastShadow(false);Overlay->SetCanEverAffectNavigation(false);Overlay->bCalculateAccurateBounds=true;Overlay->RegisterComponent();Overlay->SetComponentTickEnabled(false);
    }
    auto& Lines=Overlay->BatchedLines;Lines.Reset(40);auto& Meshes=Overlay->BatchedMeshes;if(Meshes.IsEmpty())Meshes.AddDefaulted();
    auto& Plane=Meshes[0];Plane.MeshVerts.Reset(4);Plane.MeshIndices.Reset(6);Plane.Color=FColor(242,194,107,28);Plane.DepthPriority=SDPG_World;
    const double Half=(bSectionLength?S->Settings.Length:S->Settings.Width)*.5,Offset=SectionOffset();
    auto World=[&](double P,double H){return Simulation->GetActorTransform().TransformPosition((bSectionLength?FVector(P,Offset,H):FVector(Offset,P,H))*100);};
    for(auto P:{FVector2D(-Half,0),FVector2D(Half,0),FVector2D(Half,S->Settings.Depth+S->Settings.Coaming),FVector2D(-Half,S->Settings.Depth+S->Settings.Coaming)})Plane.MeshVerts.Add(World(P.X,P.Y));
    Plane.MeshIndices.Append({0,1,2,0,2,3});
    for(int32 I=0;I<4;++I)Lines.Emplace(Plane.MeshVerts[I],Plane.MeshVerts[(I+1)%4],HatchStyle::Warning(),0.f,1.f,SDPG_World);
    if(Feedback)
    {
        const double T=FMath::Clamp((FeedbackUntil-FPlatformTime::Seconds())/1.2,0.,1.);const FVector Center=Simulation->GetActorTransform().TransformPosition(FVector(FeedbackCenter.X,FeedbackCenter.Y,S->Height(FeedbackCenter.X,FeedbackCenter.Y)+.1)*100);
        const double Radius=RadiusMeters*100*(1.1-.1*T);const FLinearColor Color(1,.7,.1,T);
        for(int32 I=0;I<32;++I){const double A=2*PI*I/32,B=2*PI*(I+1)/32;Lines.Emplace(Center+FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius,Center+FVector(FMath::Cos(B),FMath::Sin(B),0)*Radius,Color,0.f,2.f,SDPG_World);}
        Lines.Emplace(Center+FVector(0,0,150*T),Center,Color,0.f,3.f,SDPG_World);
        // Six 5Hz phases over 1.2s: descend, close two jaws, then lift.
        const double Phase=1-T,Lift=Phase<.35?1-Phase/.35:Phase>.65?(Phase-.65)/.35:0;
        const double Open=Phase<.35?1:Phase<.65?1-(Phase-.35)/.3:0;
        const FVector Bucket=Center+FVector(0,0,50+Lift*350);const double JawHalf=RadiusMeters*70;
        for(int Sign:{-1,1})
        {
            const FVector A=Bucket+FVector(-JawHalf,Sign*(20+Open*JawHalf),60),B=Bucket+FVector(JawHalf,Sign*(20+Open*JawHalf),60);
            const FVector C=Bucket+FVector(-JawHalf,Sign*Open*JawHalf*.4,-30),D=Bucket+FVector(JawHalf,Sign*Open*JawHalf*.4,-30);
            for(auto Pair:{TPair<FVector,FVector>(A,B),TPair<FVector,FVector>(B,D),TPair<FVector,FVector>(D,C),TPair<FVector,FVector>(C,A),TPair<FVector,FVector>(A,Bucket+FVector(0,0,100)),TPair<FVector,FVector>(B,Bucket+FVector(0,0,100))})Lines.Emplace(Pair.Key,Pair.Value,FLinearColor(1,.7,.1),0.f,3.f,SDPG_World);
        }
    }
    Overlay->UpdateBounds();Overlay->MarkRenderStateDirty();
}
void UShipHatchMonitorWidget::ApplyLighting()
{
    if(!Simulation.IsValid()||!AzimuthSlider||!ElevationSlider||!IntensitySlider)return;
    Simulation->SetLighting(AzimuthSlider->GetValue(),ElevationSlider->GetValue(),IntensitySlider->GetValue(),Simulation->bFillLight);
    UpdateLightingLabel();
}
void UShipHatchMonitorWidget::UpdateLightingLabel()
{
    const double Now=FPlatformTime::Seconds();if(!bSettings||!Simulation.IsValid()||Now-LastLightingLabel<.2)return;LastLightingLabel=Now;
    LightLabel->SetText(FText::FromString(FString::Printf(TEXT("방위 %.0f° · 고도 %.0f° · 세기 %.1f"),Simulation->LightAzimuth,Simulation->LightElevation,Simulation->LightIntensity)));
    FillToggle->SetStyle(Simulation->bFillLight?HatchStyle::PrimaryButton():HatchStyle::Button());CastChecked<UTextBlock>(FillToggle->GetContent())->SetText(FText::FromString(Simulation->bFillLight?TEXT("홀드 보조광 켜짐"):TEXT("홀드 보조광 꺼짐")));
}
void UShipHatchMonitorWidget::UpdateSectionLabel()
{
    const double Now=FPlatformTime::Seconds();if(!SectionLabel||!bSectionLabelPending||!(bSplit||ActiveView==4)||Now-LastSectionLabel<.2)return;
    LastSectionLabel=Now;bSectionLabelPending=false;SectionLabel->SetText(FText::FromString(FString::Printf(TEXT("%s %+.1f m"),bSectionLength?TEXT("Y"):TEXT("X"),SectionOffset())));
}
void UShipHatchMonitorWidget::OnNoon(){AzimuthSlider->SetValue(-25);ElevationSlider->SetValue(80);IntensitySlider->SetValue(7);Simulation->bFillLight=true;ApplyLighting();}
void UShipHatchMonitorWidget::OnEvening(){AzimuthSlider->SetValue(120);ElevationSlider->SetValue(18);IntensitySlider->SetValue(3);Simulation->bFillLight=false;ApplyLighting();}
void UShipHatchMonitorWidget::OnWorkLight(){AzimuthSlider->SetValue(-60);ElevationSlider->SetValue(50);IntensitySlider->SetValue(2);Simulation->bFillLight=true;ApplyLighting();}
void UShipHatchMonitorWidget::OnFill(){if(Simulation.IsValid()){Simulation->bFillLight=!Simulation->bFillLight;ApplyLighting();}}
void UShipHatchMonitorWidget::OnHighlightRight(){if(Simulation.IsValid())Simulation->HighlightSensor(2);UpdateSensorBadges();}
void UShipHatchMonitorWidget::OnFieldA(FString,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){const int32 V=FieldChoices[0]->GetSelectedIndex();Simulation->SetFieldSensor(0,V>0,V==3?2:V==2?1:0);ApplyPreview();}}
void UShipHatchMonitorWidget::OnFieldB(FString,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){const int32 V=FieldChoices[1]->GetSelectedIndex();Simulation->SetFieldSensor(1,V>0,V==3?2:V==2?1:0);ApplyPreview();}}
void UShipHatchMonitorWidget::OnFieldC(FString,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){const int32 V=FieldChoices[2]->GetSelectedIndex();Simulation->SetFieldSensor(2,V>0,V==3?2:V==2?1:0);ApplyPreview();}}
void UShipHatchMonitorWidget::OnUnloaderToggle(){if(Simulation.IsValid())Simulation->ShowUnloader(!Simulation->bShowUnloader);}
void UShipHatchMonitorWidget::OnUnloaderApply()
{
    if(!Simulation.IsValid()||Simulation->IsGenerating())return;
    auto* S=Simulation.Get();S->BoomLength=UnloaderInputs[0]->GetValue();S->BoomHeight=UnloaderInputs[1]->GetValue();S->CabHeight=UnloaderInputs[2]->GetValue();S->CabX=UnloaderInputs[3]->GetValue();S->CabY=UnloaderInputs[4]->GetValue();S->bFieldLayout=true;S->RefreshFieldLayout();UpdateCamera();
}
void UShipHatchMonitorWidget::OnSensorColors(){bSensorColors=!bSensorColors;ApplyPreview();UpdateSegments();for(int32 I=0;I<3;++I)if(auto* S=Simulation->SensorAt(I))S->VisualizationComponent->RefreshPointPreview();}
void UShipHatchMonitorWidget::OnCoverage(){bCoverageOverlay=!bCoverageOverlay;UpdateSection();InvalidatePlots();}
void UShipHatchMonitorWidget::SetGrabAspect(float V){if(Simulation.IsValid())Simulation->GrabAspect=FMath::Clamp(double(V),.5,2.);}
void UShipHatchMonitorWidget::OnSensorPose()
{
    auto* S=Simulation.Get();if(!S||S->IsGenerating()||SensorPoseInputs.Num()!=15)return;
    for(int32 I=0;I<3;++I){S->SensorOffsets[I]=FVector(SensorPoseInputs[I*5]->GetValue(),SensorPoseInputs[I*5+1]->GetValue(),SensorPoseInputs[I*5+2]->GetValue()-(I==0?S->BoomHeight:S->CabHeight));S->SensorTiltTrim[I]=SensorPoseInputs[I*5+3]->GetValue();S->SensorYaw[I]=SensorPoseInputs[I*5+4]->GetValue();}
    S->bFieldLayout=true;S->RefreshFieldLayout();UpdateCamera();
}
