#include "SlabVisualizationComponent.h"
#include "SlabLabelLayout.h"
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
void USlabVisualizationComponent::ClearAnalysis()
{
	if(bEnding)return;
	bHasAnalysis=false;LastMetrics=FSlabMetrics();MaterialId.Empty();FrameNo=INDEX_NONE;Elapsed=Duration=0;Progress=0;
	SetComponentTickEnabled(false);DrawAnalysis();UpdateLabels();
}
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
	EnsureHelpers();SetComponentTickEnabled(bHelpersVisible&&Display.HasAny());
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
void USlabVisualizationComponent::SetDetailedLabels(bool bVisible)
{
    if(bEnding||bDetailedLabels==bVisible)return;
    bDetailedLabels=bVisible;UpdateLabels();
}
void USlabVisualizationComponent::SetLabelExclusionRects(const UObject* Owner,const TArray<FBox2D>& Rects)
{
    if(bEnding)return;
    ExclusionOwner=Owner;LabelExclusions=Rects;
}
void USlabVisualizationComponent::UpdateLabels()
{
    if(Labels.Num()!=MaxTextHelpers||!GetOwner())return;
    auto Hide=[&](){for(int32 I=0;I<5;++I){Labels[I]->SetVisibility(false);LabelBackgrounds[I]->SetVisibility(false);Helpers[32+I]->SetVisibility(false);}};
    if(!bHelpersVisible||!bHasAnalysis){Hide();return;}
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    if(!PC||!PC->PlayerCameraManager){Hide();return;}
    const auto& View=PC->PlayerCameraManager->GetCameraCacheView();
    const FRotationMatrix Rotation(View.Rotation);const FVector Forward=Rotation.GetUnitAxis(EAxis::X),Up=Rotation.GetUnitAxis(EAxis::Z),Right=Rotation.GetUnitAxis(EAxis::Y);
    const FTransform Pose=GetOwner()->GetActorTransform();const FVector Center=Pose.TransformPositionNoScale(FVector(0,0,Size.Z/2+3));
    const double Depth=FVector::DotProduct(Center-View.Location,Forward);
    const bool Ortho=View.ProjectionMode==ECameraProjectionMode::Orthographic;
    const float Fade=SlabLabelLayout::DistanceFade(Ortho?View.OrthoWidth:Depth);
    int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
    FVector2D CenterPixel;
    if(Width<=0||Height<=0||Depth<=10||Fade<.05f||!PC->ProjectWorldLocationToScreen(Center,CenterPixel)||
        CenterPixel.X<0||CenterPixel.Y<0||CenterPixel.X>Width||CenterPixel.Y>Height){Hide();return;}
    if(!ExclusionOwner.IsValid())LabelExclusions.Reset();
    double Aspect=double(Width)/Height;int32 ViewHeight=Height;
    if(View.bConstrainAspectRatio&&View.AspectRatio>0){Aspect=View.AspectRatio;ViewHeight=FMath::Min(Height,FMath::RoundToInt(Width/Aspect));}
    const float Pixels=FMath::Clamp(Display.TextHeightPixels*.68f,14.f,28.f)*(.75f+.25f*Fade);
    const float WorldSize=CalculateScreenWorldSize(Depth,View.FOV,ViewHeight,Aspect,Pixels,Ortho,View.OrthoWidth);
    FBox2D SlabRect(ForceInit);
    double ExtentForward=0;
    for(int32 X=-1;X<=1;X+=2)for(int32 Y=-1;Y<=1;Y+=2)for(int32 Z=-1;Z<=1;Z+=2)
    {
        const FVector Point=Pose.TransformPositionNoScale(FVector(X*Size.X/2,Y*Size.Y/2,Z*Size.Z/2));FVector2D Screen;
        if(PC->ProjectWorldLocationToScreen(Point,Screen))SlabRect+=Screen;
        ExtentForward=FMath::Max(ExtentForward,FMath::Abs(FVector::DotProduct(Point-Center,Forward)));
    }
    if(!SlabRect.bIsValid){Hide();return;}
    const float Side=FVector::DotProduct(TrackTransform.GetUnitAxis(EAxis::Y),Right)<0?-1.f:1.f;
    const FVector2D Anchors[5]={{CenterPixel.X,SlabRect.Min.Y-Pixels-8},
        {Side>0?SlabRect.Min.X-Pixels*3:SlabRect.Max.X+Pixels*3,CenterPixel.Y},
        {Side>0?SlabRect.Max.X+Pixels*3:SlabRect.Min.X-Pixels*3,CenterPixel.Y},
        {CenterPixel.X,SlabRect.Max.Y+Pixels+10},{CenterPixel.X,SlabRect.Min.Y-Pixels*3-16}};
    const bool Enabled[5]={Display.bYaw,bDetailedLabels&&Display.bMargins,bDetailedLabels&&Display.bMargins,Display.bStatus,bDetailedLabels&&Display.bCenterline};
    FSlabLabelCandidate Candidates[5];
    for(int32 I=0;I<5;++I)
    {
        if(!Enabled[I])continue; // Hidden detail strings are not formatted.
        uint32 Key=I==0?GetTypeHash(FMath::RoundToInt(LastMetrics.LeftAngle*100)):
            I==1?HashCombine(GetTypeHash(FMath::RoundToInt(LastMetrics.MarginLeftCm*10)),GetTypeHash(bRails)):
            I==2?HashCombine(GetTypeHash(FMath::RoundToInt(LastMetrics.MarginRightCm*10)),GetTypeHash(bRails)):
            I==4?GetTypeHash(FMath::RoundToInt(LastMetrics.CenterOffsetCm*10)):
            HashCombine(GetTypeHash(MaterialId),GetTypeHash(FMath::RoundToInt(Progress*100)));
        if(I==3&&bDetailedLabels)Key=HashCombine(Key,HashCombine(GetTypeHash(FrameNo),HashCombine(GetTypeHash(FMath::RoundToInt(Elapsed*10)),GetTypeHash(FMath::RoundToInt(Duration*10)))));
        Key=HashCombine(Key,GetTypeHash(bDetailedLabels));
        if(CachedLabelTexts[I].IsEmpty()||LabelTextKeys[I]!=Key)
        {
            LabelTextKeys[I]=Key;const FString ShortId=MaterialId.Len()>24?MaterialId.Left(21)+TEXT("..."):MaterialId;
            if(I==0)CachedLabelTexts[I]=FString::Printf(TEXT("Yaw %+.2f deg"),LastMetrics.LeftAngle);
            else if(I==1||I==2){const double Gap=I==1?LastMetrics.MarginLeftCm:LastMetrics.MarginRightCm;CachedLabelTexts[I]=bRails?FString::Printf(TEXT("%s %+.1f cm%s"),I==1?TEXT("L"):TEXT("R"),Gap,Gap<0?TEXT(" [침범]"):TEXT("")):FString(I==1?TEXT("L N/A"):TEXT("R N/A"));}
            else if(I==4)CachedLabelTexts[I]=FMath::IsNearlyZero(LastMetrics.CenterOffsetCm,.01)?TEXT("Center aligned"):FString::Printf(TEXT("Center %+.1f cm"),LastMetrics.CenterOffsetCm);
            else CachedLabelTexts[I]=bDetailedLabels?FString::Printf(TEXT("%s | frame %lld\n%.1f / %.1f s  %.0f%%"),*ShortId,FrameNo,Elapsed,Duration,Progress*100):FString::Printf(TEXT("%s | %.0f%%"),*ShortId,Progress*100);
            Labels[I]->SetText(FText::FromString(CachedLabelTexts[I]));
        }
        Labels[I]->SetWorldSize(WorldSize);const FVector Bounds=Labels[I]->GetTextLocalSize();
        Candidates[I].Anchor=Anchors[I];Candidates[I].bEnabled=true;
        Candidates[I].HalfSize=FVector2D(FMath::Max(2.f*Pixels,float(Bounds.Y/WorldSize*Pixels))*.5f+6,FMath::Max(Pixels,float(Bounds.Z/WorldSize*Pixels))*.5f+4);
    }
    // Material/progress and tilt precede detailed gap/center labels; an intrusion gap
    // precedes the other side. L/R always describe Track sides, independent of roll.
    const int32 Priority[5]={3,0,LastMetrics.MarginRightCm<0&&LastMetrics.MarginLeftCm>=0?2:1,LastMetrics.MarginRightCm<0&&LastMetrics.MarginLeftCm>=0?1:2,4};
    FSlabLabelPlacement Placement[5];SlabLabelLayout::Place(Candidates,Priority,FBox2D(FVector2D(12,12),FVector2D(Width-12,Height-12)),LabelExclusions,Placement);
    const double Near=FMath::Max(50.,double(Ortho?View.OrthoNearClipPlane:View.GetFinalPerspectiveNearClipPlane())+20.);
    const double PlaneDepth=FMath::Max(Near,Depth-ExtentForward-WorldSize*3);
    const float TextSize=CalculateScreenWorldSize(PlaneDepth,View.FOV,ViewHeight,Aspect,Pixels,Ortho,View.OrthoWidth);
    auto WorldForPixel=[&](const FVector2D& P)
    {
        FVector Origin,Ray;PC->DeprojectScreenPositionToWorld(P.X,P.Y,Origin,Ray);
        return Origin+Ray*((PlaneDepth-FVector::DotProduct(Origin-View.Location,Forward))/FMath::Max(.001,FVector::DotProduct(Ray,Forward)));
    };
    const FQuat Facing=FRotationMatrix::MakeFromXZ(-Forward,Up).ToQuat();
    for(int32 I=0;I<5;++I)
    {
        const bool Visible=Placement[I].bVisible;auto* T=Labels[I].Get();auto* B=LabelBackgrounds[I].Get();
        T->SetVisibility(Visible);B->SetVisibility(Visible);Helpers[32+I]->SetVisibility(false);if(!Visible)continue;
        const FVector Location=WorldForPixel(Placement[I].Rect.GetCenter());T->SetWorldSize(TextSize);T->SetWorldLocation(Location);T->SetWorldRotation(Facing);
        const bool Intrusion=(I==1&&LastMetrics.MarginLeftCm<0)||(I==2&&LastMetrics.MarginRightCm<0);
        const FLinearColor Color=Intrusion?FLinearColor(1,.08,.04):FLinearColor::White;
        T->SetTextRenderColor((Color*(.35f+.65f*Fade)).ToFColor(true));
        const FVector Bounds=T->GetTextLocalSize();
        B->SetWorldTransform(FTransform(Facing,Location+Forward*FMath::Max(.5f,TextSize*.08f),FVector(.002,(FMath::Max(TextSize*2.f,float(Bounds.Y))+TextSize*.6f)/100,(FMath::Max(TextSize,float(Bounds.Z))+TextSize*.35f)/100)));
        if(FVector2D::Distance(Candidates[I].Anchor,Placement[I].Rect.GetCenter())>Pixels)
            SetLine(32+I,WorldForPixel(Candidates[I].Anchor),Location,CalculateScreenWorldSize(PlaneDepth,View.FOV,ViewHeight,Aspect,.8f,Ortho,View.OrthoWidth),FLinearColor(.12,.16,.20)*Fade,true);
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
