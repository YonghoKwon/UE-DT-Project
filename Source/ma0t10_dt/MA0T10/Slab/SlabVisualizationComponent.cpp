#include "SlabVisualizationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
namespace
{
void Isolate(UPrimitiveComponent* P)
{
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision); P->SetGenerateOverlapEvents(false);
	P->SetCastShadow(false); P->SetHiddenInSceneCapture(true); P->SetAffectDynamicIndirectLighting(false);
	P->SetAffectDistanceFieldLighting(false); P->SetAffectIndirectLightingWhileHidden(false);
	P->SetVisibleInRayTracing(false); P->bVisibleInReflectionCaptures=false; P->bVisibleInRealTimeSkyCaptures=false;
}
const FLinearColor Cyan(0.01,0.18,0.95),Grey(0.015,0.025,0.04),Yellow(1,0.26,0.015),Red(0.95,0.012,0.02),Green(0.0,0.42,0.28);
}
USlabVisualizationComponent::USlabVisualizationComponent()
{ PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false; PrimaryComponentTick.TickInterval=0.2f; }
void USlabVisualizationComponent::EnsureHelpers()
{
	if(bEnding||!Helpers.IsEmpty()||!GetOwner()||!GetOwner()->GetRootComponent()) return;
	UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/M_SlabAnalysisReadable.M_SlabAnalysisReadable"));
	if(!Material) Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	AppliedLineColors.Init(FLinearColor(-1,-1,-1,-1),MaxLineHelpers);
	for(int32 I=0;I<MaxLineHelpers;++I)
	{
		auto* M=NewObject<UStaticMeshComponent>(GetOwner(),*FString::Printf(TEXT("SlabDiagnosticLine%d"),I));
		M->SetStaticMesh(Cube); M->SetupAttachment(GetOwner()->GetRootComponent()); M->SetMobility(EComponentMobility::Movable);
		if(I>=11&&I<=13) M->SetAbsolute(true,true,true); // Track reference must not inherit Slab motion.
		Isolate(M);
		if(Material) M->SetMaterial(0,UMaterialInstanceDynamic::Create(Material,M));
		M->RegisterComponent(); M->SetVisibility(false); Helpers.Add(M);
	}
	auto* TextMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/M_SlabTextReadable.M_SlabTextReadable"));
	auto* Font=LoadObject<UFont>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/F_SlabDiagnostics.F_SlabDiagnostics"));
	for(int32 I=0;I<MaxTextHelpers;++I)
	{
		auto* T=NewObject<UTextRenderComponent>(GetOwner(),*FString::Printf(TEXT("SlabDiagnosticText%d"),I));
		T->SetupAttachment(GetOwner()->GetRootComponent()); T->SetMobility(EComponentMobility::Movable); Isolate(T);
		T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter); T->SetTextRenderColor(FColor::White);
		if(Font)T->SetFont(Font);if(TextMaterial)T->SetTextMaterial(TextMaterial);
		T->RegisterComponent(); T->SetVisibility(false); Labels.Add(T);
		auto* B=NewObject<UStaticMeshComponent>(GetOwner(),*FString::Printf(TEXT("SlabLabelBackground%d"),I));B->SetStaticMesh(Cube);B->SetupAttachment(GetOwner()->GetRootComponent());B->SetMobility(EComponentMobility::Movable);Isolate(B);
		if(Material){auto* MID=UMaterialInstanceDynamic::Create(Material,B);MID->SetVectorParameterValue(TEXT("Color"),FLinearColor(.003,.005,.009,1));B->SetMaterial(0,MID);}
		B->RegisterComponent();B->SetVisibility(false);LabelBackgrounds.Add(B);
	}
}
void USlabVisualizationComponent::UpdateGeometry(const FVector& S)
{
	if(bEnding) return; Size=S; EnsureHelpers(); DrawAnalysis();
}
void USlabVisualizationComponent::SetHelpersVisible(bool bVisible)
{ if(bEnding) return; bHelpersVisible=bVisible; DrawAnalysis(); UpdateLabels(); SetComponentTickEnabled(bHasAnalysis&&bVisible&&Display.HasAny()); }
void USlabVisualizationComponent::EndPlay(const EEndPlayReason::Type Reason)
{ bEnding=true; SetComponentTickEnabled(false); for(UStaticMeshComponent* H:Helpers) if(H) H->DestroyComponent(); Helpers.Reset(); for(UTextRenderComponent* T:Labels) if(T) T->DestroyComponent(); Labels.Reset();for(UStaticMeshComponent* B:LabelBackgrounds)if(B)B->DestroyComponent();LabelBackgrounds.Reset();Super::EndPlay(Reason); }
void USlabVisualizationComponent::ConfigureDisplay(const FSlabAnalysisDisplaySettings& Settings)
{ if(bEnding) return; Display=Settings;Display.Sanitize();DrawAnalysis();UpdateLabels();SetComponentTickEnabled(bHasAnalysis&&bHelpersVisible&&Display.HasAny()); }
void USlabVisualizationComponent::UpdateAnalysis(const FSlabScenarioRow&,const FSlabMetrics& Metrics,const FSlabSimulationStatus& Status,const FVector& S,const FTransform& Track,double LeftRailYcm,double RightRailYcm,bool bRailsValid,double ReferenceLengthCm)
{
	if(bEnding) return; Size=S; TrackTransform=Track; LastMetrics=Metrics; LeftRail=LeftRailYcm; RightRail=RightRailYcm; bRails=bRailsValid;
	FixedReferenceLength=ReferenceLengthCm>0&&FMath::IsFinite(ReferenceLengthCm)?ReferenceLengthCm:Size.X*1.2;
	if(MaterialId!=Status.MtlNo) MaterialId=Status.MtlNo;
	FrameNo=Status.FrameNo; Elapsed=Status.ElapsedSec; Duration=Status.DurationSec; Progress=Status.Progress; bHasAnalysis=true;
	EnsureHelpers();DrawAnalysis();SetComponentTickEnabled(bHelpersVisible&&Display.HasAny());
}
void USlabVisualizationComponent::SetLine(int32 I,const FVector& A,const FVector& B,float Thickness,FLinearColor Color,bool Visible)
{
	if(!Helpers.IsValidIndex(I)) return;
	auto* M=Helpers[I].Get(); M->SetVisibility(bHelpersVisible&&Visible); if(!bHelpersVisible||!Visible) return;
	const FVector Delta=B-A;
	M->SetWorldTransform(FTransform(Delta.Rotation(),(A+B)/2,FVector(FMath::Max(Thickness,static_cast<float>(Delta.Length()))/100,Thickness/100,Thickness/100)));
	if(AppliedLineColors[I]!=Color)if(auto* MID=Cast<UMaterialInstanceDynamic>(M->GetMaterial(0))){MID->SetVectorParameterValue(TEXT("Color"),Color);AppliedLineColors[I]=Color;}
}
float USlabVisualizationComponent::ResolveStrokeSize() const
{
	const auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
	if(!PC||!PC->PlayerCameraManager)return Display.LineWidthPixels;
	const auto& View=PC->PlayerCameraManager->GetCameraCacheView();int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<=0||H<=0){W=1280;H=720;}
	double Aspect=double(W)/H;if(View.bConstrainAspectRatio&&View.AspectRatio>0){Aspect=View.AspectRatio;H=FMath::Min(H,FMath::RoundToInt(W/Aspect));}
	const double Depth=FVector::DotProduct(GetOwner()->GetActorLocation()-View.Location,View.Rotation.Vector());
	return CalculateScreenWorldSize(Depth,View.FOV,H,Aspect,Display.LineWidthPixels,View.ProjectionMode==ECameraProjectionMode::Orthographic,View.OrthoWidth);
}
void USlabVisualizationComponent::DrawAnalysis()
{
	if(Helpers.Num()!=MaxLineHelpers||!GetOwner()) return;
	const FTransform Pose=GetOwner()->GetActorTransform();const float Stroke=ResolveStrokeSize();const double Z=Size.Z/2+FMath::Max(3.f,Stroke);
	FVector Corners[4]; const FVector Local[4]={FVector(-Size.X/2,-Size.Y/2,Z),FVector(Size.X/2,-Size.Y/2,Z),FVector(Size.X/2,Size.Y/2,Z),FVector(-Size.X/2,Size.Y/2,Z)};
	for(int32 I=0;I<4;++I) Corners[I]=Pose.TransformPositionNoScale(Local[I]);
	for(int32 I=0;I<4;++I) SetLine(I,Corners[I],Corners[(I+1)%4],Stroke,Cyan,Display.bOutline);
	SetLine(4,Pose.TransformPositionNoScale(FVector(-Size.X/2,0,Z+1)),Pose.TransformPositionNoScale(FVector(Size.X/2,0,Z+1)),Stroke,Cyan,Display.bCross);
	SetLine(5,Pose.TransformPositionNoScale(FVector(0,-Size.Y/2,Z+1)),Pose.TransformPositionNoScale(FVector(0,Size.Y/2,Z+1)),Stroke,Cyan,Display.bCross);
	const FVector Centre=Pose.TransformPositionNoScale(FVector(0,0,Z+3)); SetLine(6,Centre-FVector(0,0,Stroke*2),Centre+FVector(0,0,Stroke*2),Stroke*4,Red,Display.bCross);
	if(!bHasAnalysis) { for(int32 I=7;I<MaxLineHelpers;++I) Helpers[I]->SetVisibility(false); return; }
	const FTransform Reference(TrackTransform.GetRotation(),Pose.GetLocation(),FVector::OneVector);
	for(int32 I=0;I<4;++I) SetLine(7+I,Reference.TransformPositionNoScale(Local[I]+FVector(0,0,1)),Reference.TransformPositionNoScale(Local[(I+1)%4]+FVector(0,0,1)),Stroke*.7f,Grey,Display.bReferencePose);
	const double Half=FixedReferenceLength/2,CenterZ=Size.Z+7;
	const FVector Arrow=TrackTransform.TransformPositionNoScale(FVector(Half,0,CenterZ));
	SetLine(11,TrackTransform.TransformPositionNoScale(FVector(-Half,0,CenterZ)),Arrow,Stroke,Grey,Display.bCenterline);
	SetLine(12,Arrow,TrackTransform.TransformPositionNoScale(FVector(Half-Size.Y*.12,-Size.Y*.06,CenterZ)),Stroke,Grey,Display.bCenterline);
	SetLine(13,Arrow,TrackTransform.TransformPositionNoScale(FVector(Half-Size.Y*.12,Size.Y*.06,CenterZ)),Stroke,Grey,Display.bCenterline);
	const double Radius=FMath::Clamp(Size.Y*.7,30.0,400.0),Angle=FMath::DegreesToRadians(LastMetrics.LeftAngle);
	for(int32 I=0;I<12;++I)
	{
		auto P=[&](double T){return Reference.TransformPositionNoScale(FVector(Radius*FMath::Cos(Angle*T),Radius*FMath::Sin(Angle*T),Z+8));};
		SetLine(14+I,P(I/12.0),P((I+1)/12.0),Stroke*1.2f,Yellow,Display.bYaw&&FMath::Abs(Angle)>.0001);
	}
	int32 Left=0,Right=0; double Min=DBL_MAX,Max=-DBL_MAX;
	for(int32 I=0;I<4;++I) { const double Y=TrackTransform.InverseTransformPositionNoScale(Corners[I]).Y; if(Y<Min){Min=Y;Left=I;} if(Y>Max){Max=Y;Right=I;} }
	for(int32 Side=0;Side<2;++Side)
	{
		const FVector Start=Corners[Side?Right:Left]; FVector End=TrackTransform.InverseTransformPositionNoScale(Start); End.Y=Side?RightRail:LeftRail;
		const double Gap=Side?LastMetrics.MarginRightCm:LastMetrics.MarginLeftCm;
		SetLine(26+Side,Start,TrackTransform.TransformPositionNoScale(End),Stroke*1.3f,Gap<0?Red:Green,Display.bMargins&&bRails);
		const FVector Tick=TrackTransform.GetUnitAxis(EAxis::X)*Stroke*2.5f;const FVector Finish=TrackTransform.TransformPositionNoScale(End);
		SetLine(28+Side*2,Start-Tick,Start+Tick,Stroke,Gap<0?Red:Green,Display.bMargins&&bRails);
		SetLine(29+Side*2,Finish-Tick,Finish+Tick,Stroke,Gap<0?Red:Green,Display.bMargins&&bRails);
	}
}
void USlabVisualizationComponent::UpdateLabels()
{
	if(Labels.Num()!=MaxTextHelpers||!GetOwner()) return;
	if(!bHelpersVisible||!bHasAnalysis) { for(UTextRenderComponent* T:Labels) T->SetVisibility(false);for(UStaticMeshComponent* B:LabelBackgrounds)B->SetVisibility(false);return; }
	const FTransform Pose=GetOwner()->GetActorTransform(); const double Z=Size.Z/2+FMath::Max(15.0,Size.Y*.2);
	const FString ShortId=MaterialId.Len()>24?MaterialId.Left(21)+TEXT("..."):MaterialId;
	const FString Texts[5]={FString::Printf(TEXT("Yaw %+.2f deg"),LastMetrics.LeftAngle),bRails?FString::Printf(TEXT("L %+.1f cm%s"),LastMetrics.MarginLeftCm,LastMetrics.MarginLeftCm<0?TEXT(" [침범]"):TEXT("")):TEXT("L N/A"),bRails?FString::Printf(TEXT("R %+.1f cm%s"),LastMetrics.MarginRightCm,LastMetrics.MarginRightCm<0?TEXT(" [침범]"):TEXT("")):TEXT("R N/A"),FString::Printf(TEXT("%s | frame %lld\n%.1f / %.1f s  %.1f%%"),*ShortId,FrameNo,Elapsed,Duration,Progress*100),FMath::IsNearlyZero(LastMetrics.CenterOffsetCm,.01)?TEXT("Center aligned"):FString::Printf(TEXT("Center %+.1f cm"),LastMetrics.CenterOffsetCm)};
	const FVector Positions[5]={FVector(Size.Y*.65,0,Z),FVector(0,-Size.Y*.8,Z),FVector(0,Size.Y*.8,Z),FVector(-Size.X*.35,0,Z+20),FVector(Size.X*.3,0,Z)};
	const bool Visible[5]={Display.bYaw,Display.bMargins,Display.bMargins,Display.bStatus,Display.bCenterline};
	APlayerController* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
	FVector WorldPositions[5]; for(int32 I=0;I<5;++I) WorldPositions[I]=Pose.TransformPositionNoScale(Positions[I]);
	float TextSize=FMath::Clamp(static_cast<float>(Size.Y*.18),25.0f,45.0f);
	if(PC&&PC->PlayerCameraManager)
	{
		const auto& View=PC->PlayerCameraManager->GetCameraCacheView(); const FRotationMatrix ViewRotation(View.Rotation);
		const FVector Right=ViewRotation.GetUnitAxis(EAxis::Y),Up=ViewRotation.GetUnitAxis(EAxis::Z),Forward=ViewRotation.GetUnitAxis(EAxis::X);
		const FVector Centre=Pose.TransformPositionNoScale(FVector(0,0,Size.Z/2+3));
		const double Depth=FMath::Max(10.0,FVector::DotProduct(Centre-View.Location,Forward));
		int32 Width=0,Height=0; PC->GetViewportSize(Width,Height); if(Width<=0||Height<=0){Width=1280;Height=720;}
		double Aspect=double(Width)/Height;
		if(View.bConstrainAspectRatio&&View.AspectRatio>0) {Aspect=View.AspectRatio; Height=FMath::Min(Height,FMath::RoundToInt(Width/Aspect));}
		const bool bOrtho=View.ProjectionMode==ECameraProjectionMode::Orthographic;
		TextSize=CalculateScreenWorldSize(Depth,View.FOV,Height,Aspect,Display.TextHeightPixels,bOrtho,View.OrthoWidth);
		const FVector AxisX=Pose.GetUnitAxis(EAxis::X),AxisY=Pose.GetUnitAxis(EAxis::Y),AxisZ=Pose.GetUnitAxis(EAxis::Z);
		auto Extent=[&](const FVector& Axis){return .5*(FMath::Abs(FVector::DotProduct(AxisX,Axis))*Size.X+FMath::Abs(FVector::DotProduct(AxisY,Axis))*Size.Y+FMath::Abs(FVector::DotProduct(AxisZ,Axis))*Size.Z);};
		const double X=Extent(Right),Y=Extent(Up),Side=FVector::DotProduct(TrackTransform.GetUnitAxis(EAxis::Y),Right)<0?-1.0:1.0;
		// Screen-separated anchors, still at the physical Slab's view depth. L/R refer to Track sides.
		FVector2D Offsets[5]={FVector2D(0,Y+TextSize*1.2),FVector2D(-Side*(X+TextSize*4.0),0),FVector2D(Side*(X+TextSize*4.0),0),FVector2D(0,-Y-TextSize*2.2),FVector2D(0,Y+TextSize*2.7)};
		const double HalfViewX=bOrtho?View.OrthoWidth/2:Depth*FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(double(View.FOV),5.0,170.0))*.5);
		const double HalfViewY=HalfViewX/Aspect;
		for(int32 I=0;I<5;++I)
		{
			const double TextHalfWidth=TextSize*(I==3?8.0:4.0);
			Offsets[I].X=FMath::Clamp(Offsets[I].X,-FMath::Max(0.0,HalfViewX-TextHalfWidth),FMath::Max(0.0,HalfViewX-TextHalfWidth));
			Offsets[I].Y=FMath::Clamp(Offsets[I].Y,-FMath::Max(0.0,HalfViewY-TextSize*1.5),FMath::Max(0.0,HalfViewY-TextSize*1.5));
			WorldPositions[I]=Centre+Right*Offsets[I].X+Up*Offsets[I].Y;
		}
		// Presentation labels sit on a nearer view plane, not on the Slab/floor. Preserve the
		// screen anchor and glyph pixels by scaling all camera-relative coordinates equally.
		const double NearDepth=FMath::Max(50.0,double(bOrtho?View.OrthoNearClipPlane:View.GetFinalPerspectiveNearClipPlane())+20.0);
		const double MaximumPull=FMath::Max(0.0,Depth-NearDepth);
		double Pull=FMath::Min(MaximumPull,Extent(Forward)+TextSize*3.0);
		const FVector TrackUp=TrackTransform.GetUnitAxis(EAxis::Z);
		const double SafeHeight=TextSize*1.1+5.0,CameraHeight=FVector::DotProduct(View.Location-TrackTransform.GetLocation(),TrackUp);
		for(int32 I=0;I<5;++I) if(Visible[I])
		{
			const double HeightAboveFloor=FVector::DotProduct(WorldPositions[I]-TrackTransform.GetLocation(),TrackUp);
			if(HeightAboveFloor>=SafeHeight) continue;
			if(bOrtho)
			{
				const double RisePerCm=-FVector::DotProduct(Forward,TrackUp);
				if(RisePerCm>0.01) Pull=FMath::Max(Pull,(SafeHeight-HeightAboveFloor)/RisePerCm);
			}
			else if(CameraHeight>SafeHeight)
			{
				const double RequiredScale=(CameraHeight-SafeHeight)/(CameraHeight-HeightAboveFloor);
				Pull=FMath::Max(Pull,Depth*(1.0-FMath::Clamp(RequiredScale,0.0,1.0)));
			}
		}
		Pull=FMath::Clamp(Pull,0.0,MaximumPull); float LabelScale=1;
		for(FVector& Position:WorldPositions) Position=PullLabelTowardCamera(Position,View.Location,Forward,Depth,Pull,bOrtho,LabelScale);
		TextSize*=LabelScale;
	}
	for(int32 I=0;I<MaxTextHelpers;++I)
	{
		auto* T=Labels[I].Get();T->SetVisibility(Visible[I]);if(LabelBackgrounds.IsValidIndex(I))LabelBackgrounds[I]->SetVisibility(Visible[I]);if(!Visible[I])continue;
		if(T->Text.ToString()!=Texts[I])T->SetText(FText::FromString(Texts[I]));T->SetWorldSize(TextSize);T->SetWorldLocation(WorldPositions[I]);
		if(PC&&PC->PlayerCameraManager) T->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation()-T->GetComponentLocation()).Rotation());
		T->SetTextRenderColor((I==1&&LastMetrics.MarginLeftCm<0)||(I==2&&LastMetrics.MarginRightCm<0)?FColor::Red:FColor::White);
		if(LabelBackgrounds.IsValidIndex(I))
		{
			const FVector Bounds=T->GetTextLocalSize();
			LabelBackgrounds[I]->SetWorldTransform(FTransform(T->GetComponentQuat(),T->GetComponentLocation()-T->GetForwardVector()*FMath::Max(.5f,TextSize*.08f),FVector(.002,FMath::Max(TextSize*2.f,float(Bounds.Y))+TextSize*.6f,FMath::Max(TextSize,float(Bounds.Z))+TextSize*.35f)/FVector(1,100,100)));
		}
	}
}
float USlabVisualizationComponent::CalculateScreenWorldSize(double Depth,double Fov,int32 Height,double Aspect,float Pixels,bool Ortho,double OrthoWidth)
{
	if(!FMath::IsFinite(Depth)||!FMath::IsFinite(Fov)||!FMath::IsFinite(Aspect)||!FMath::IsFinite(Pixels)||Height<=0||Aspect<=0)return 4;
	const double Width=Ortho?(FMath::IsFinite(OrthoWidth)?FMath::Max(1.,OrthoWidth):1600.):2*FMath::Max(1.,Depth)*FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Fov,5.,170.))*.5);
	return float(FMath::Clamp(Pixels*Width/(Aspect*Height),.05,2000.));
}
float USlabVisualizationComponent::CalculateReadableTextSize(double ViewDepthCm,double HorizontalFovDegrees,int32 ViewportHeight,double AspectRatio,bool bOrthographic,double OrthoWidthCm)
{
	if(!FMath::IsFinite(ViewDepthCm)||!FMath::IsFinite(HorizontalFovDegrees)||!FMath::IsFinite(AspectRatio)||ViewportHeight<=0||AspectRatio<=0) return 30;
	const double Width=bOrthographic?(FMath::IsFinite(OrthoWidthCm)?FMath::Max(1.0,OrthoWidthCm):1600):
		2*FMath::Max(1.0,ViewDepthCm)*FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(HorizontalFovDegrees,5.0,170.0))*.5);
	return static_cast<float>(FMath::Clamp(18.0*Width/(AspectRatio*ViewportHeight),20.0,120.0));
}
FVector USlabVisualizationComponent::PullLabelTowardCamera(const FVector& Position,const FVector& CameraPosition,const FVector& CameraForward,double ViewDepthCm,double PullCm,bool bOrthographic,float& OutTextScale)
{
	OutTextScale=1;
	if(!FMath::IsFinite(ViewDepthCm)||!FMath::IsFinite(PullCm)||ViewDepthCm<=0||PullCm<=0) return Position;
	if(bOrthographic) return Position-CameraForward*PullCm;
	const double Scale=FMath::Clamp((ViewDepthCm-PullCm)/ViewDepthCm,0.001,1.0); OutTextScale=static_cast<float>(Scale);
	return CameraPosition+(Position-CameraPosition)*Scale;
}
void USlabVisualizationComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{ Super::TickComponent(Delta,TickType,TickFunction);DrawAnalysis();UpdateLabels(); }
