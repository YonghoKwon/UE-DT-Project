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
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"

using HatchStyle=FSensorOwnedPresentationStyle;
void UShipHatchMonitorWidget::SetHatchPresentationVisible(bool Visible)
{if(Overlay)Overlay->SetVisibility(Visible);if(!Visible){FeedbackUntil=DischargeUntil=0;RestoreFeedbackGrab();bOverlayDirty=true;}}
UWidget* UShipHatchMonitorWidget::ComboEntry(FString Value)
{
    ComboLabels.RemoveAll([](const auto& W)
    {
        if(!W)return true;
        const auto Native=W->GetCachedWidget();
        // UTextBlock::MyTextBlock owns one reference; this lookup adds one.
        // Any additional reference belongs to a live Slate row/content owner.
        return !Native.IsValid()||Native.GetSharedReferenceCount()<=2;
    });
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
void UShipHatchMonitorWidget::RestoreFeedbackGrab()
{if(auto* PC=GetOwningPlayer())if(bFeedbackOwnsGrabHide)PC->HiddenPrimitiveComponents.RemoveSingle(FeedbackGrabHidden);FeedbackGrabHidden.Reset();bFeedbackOwnsGrabHide=false;if(Simulation.IsValid())Simulation->ResetGrabPose();}
void UShipHatchMonitorWidget::UpdateOverlay()
{
    const auto* S=GetSurface();if(!S||S->Heights.IsEmpty()||!Host.IsValid())return;
    const double Clock=FPlatformTime::Seconds();
    if(!Simulation->IsGrabVisible()){FeedbackUntil=DischargeUntil=0;}
    if(Simulation->Revision()!=FeedbackRevision){FeedbackUntil=DischargeUntil=0;}
    const bool Feedback=FeedbackUntil>Clock,Carry=DischargeUntil>Clock,Running=Feedback||Carry;
    if(!Running)RestoreFeedbackGrab();
    if(!IsVisible()||!(bSplit||ActiveView<2))return;
    if(!bOverlayDirty&&!Running&&!bHadFeedback)return;bOverlayDirty=false;bHadFeedback=Running;
    const double Now=FPlatformTime::Seconds();if(Now-LastOverlayUpdate<.2){bOverlayDirty=true;return;}LastOverlayUpdate=Now;
    if(!Overlay)
    {
        Host->Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));
        Overlay=NewObject<UShipHatchOverlay>(Host.Get());Overlay->SetCollisionEnabled(ECollisionEnabled::NoCollision);Overlay->SetHiddenInSceneCapture(true);Overlay->SetCastShadow(false);Overlay->SetCanEverAffectNavigation(false);Overlay->bCalculateAccurateBounds=true;Overlay->RegisterComponent();Overlay->SetComponentTickEnabled(false);
    }
    auto& Lines=Overlay->BatchedLines;Lines.Reset(40);auto& Meshes=Overlay->BatchedMeshes;Meshes.SetNum(4);
    for(int I=1;I<4;++I){Meshes[I].MeshVerts.Reset();Meshes[I].MeshIndices.Reset();Meshes[I].DepthPriority=SDPG_World;}
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
    }
    if(Running)
    {
        auto& Mist=Meshes[2];auto& Stream=Meshes[3];Mist.Color=FColor(100,150,165,12);Stream.Color=Simulation->bCoal?FColor(35,38,40,190):FColor(105,75,52,190);
        auto W=[&](FVector P){return Simulation->GetActorTransform().TransformPosition(P*100);};
        auto Quad=[&](FBatchedMesh& Mesh,FVector A,FVector B,FVector C,FVector D){const int N=Mesh.MeshVerts.Num();for(auto P:{A,B,C,D})Mesh.MeshVerts.Add(W(P));Mesh.MeshIndices.Append({N,N+1,N+2,N,N+2,N+3,N,N+2,N+1,N,N+3,N+2});};
        const double Post=FMath::Clamp(4.-(DischargeUntil-Now),0.,4.);const double HopperY=Simulation->SeaRailY()-Simulation->RailSpan*.5,HopperTop=Simulation->QuayZ()+15,SafeZ=FMath::Max(HopperTop+4,Simulation->HoldDepth+9);
        FVector Bucket(FeedbackCenter.X,FeedbackCenter.Y,S->Height(FeedbackCenter.X,FeedbackCenter.Y)+4.1);double Open=0;
        if(Feedback){const double Phase=1-FMath::Clamp((FeedbackUntil-Now)/1.2,0.,1.),Lift=Phase<.35?1-Phase/.35:Phase>.65?(Phase-.65)/.35:0;Bucket.Z=S->Height(Bucket.X,Bucket.Y)+.6+Lift*3.5;Open=Phase<.35?1:Phase<.65?1-(Phase-.35)/.3:0;}
        else if(Post<1)Bucket.Z=FMath::Lerp(Bucket.Z,SafeZ,Post);
        else {const double Move=FMath::Clamp((Post-1)/1.2,0.,1.);Bucket.X=FMath::Lerp(Bucket.X,0.,Move);Bucket.Y=FMath::Lerp(Bucket.Y,HopperY,Move);Bucket.Z=SafeZ;Open=FMath::Clamp((Post-2.2)/.5,0.,1.);}
        Simulation->SetGrabPose(Bucket,Open);
        if(!Feedback&&Post>2.2)
        {
            for(int I=0;I<4;++I){const double X=(I-1.5)*.35,Y=HopperY+FMath::Sin(Post*12+I)*.2;Quad(Stream,FVector(X-.15,Y,Bucket.Z+.8),FVector(X+.15,Y,Bucket.Z+.8),FVector(X+.3,Y,HopperTop-4),FVector(X-.3,Y,HopperTop-4));}
            for(int I=0;I<8;++I){const double X=(I-3.5)*.85,Y=HopperY+FMath::Sin(I*2.)*.8,H=.6+FMath::Sin(Post*5+I)*.2;Quad(Mist,FVector(X-.25,Y,HopperTop-.5),FVector(X+.25,Y,HopperTop-.5),FVector(X+.4,Y,HopperTop+H),FVector(X-.4,Y,HopperTop+H));}
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
void UShipHatchMonitorWidget::OnFieldA(FString,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){const int32 V=FieldChoices[0]->GetSelectedIndex();Simulation->SetFieldSensor(0,V>0,FMath::Max(0,V-1));ApplyPreview();}}
void UShipHatchMonitorWidget::OnFieldB(FString,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){const int32 V=FieldChoices[1]->GetSelectedIndex();Simulation->SetFieldSensor(1,V>0,FMath::Max(0,V-1));ApplyPreview();}}
void UShipHatchMonitorWidget::OnFieldC(FString,ESelectInfo::Type){if(!bSync&&Simulation.IsValid()){const int32 V=FieldChoices[2]->GetSelectedIndex();Simulation->SetFieldSensor(2,V>0,FMath::Max(0,V-1));ApplyPreview();}}
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

void UShipHatchMonitorWidget::OnGrabDisplayToggle()
{
 if(!Simulation.IsValid())return;FeedbackUntil=DischargeUntil=0;RestoreFeedbackGrab();Simulation->SetGrabVisible(!Simulation->IsGrabVisible());bOverlayDirty=true;LastOverlayUpdate=0;UpdateOverlay();Refresh();
}
