#include "ShipHatchMonitorWidget.h"
#include "ShipHatchMonitorHost.h"
#include "ShipHatchOverlay.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextBlock.h"

void UShipHatchMonitorWidget::RestoreSiteVisualization()
{
    if(auto* PC=GetOwningPlayer())for(const auto& C:SiteHidden)PC->HiddenPrimitiveComponents.Remove(C);
    SiteHidden.Reset();if(SiteModel)SiteModel->DestroyComponent();SiteModel=nullptr;SiteShapeKey=0;SiteCabRevision=MAX_uint64;
}
void UShipHatchMonitorWidget::SetSiteVisualization(bool Enabled)
{
    if(bSiteVisualization==Enabled)return;
    bSiteVisualization=Enabled;
    if(Enabled){bSavedSiteColors=bSensorColors;SelectView(1);bSensorColors=true;}
    else {RestoreSiteVisualization();bSensorColors=bSavedSiteColors;}
    ApplyPreview();UpdateSegments();LastSensorBadgeUpdate=-1;UpdateSensorBadges();UpdateSiteVisualization();LastLayout=FVector2D::ZeroVector;Layout();
}
void UShipHatchMonitorWidget::UpdateSiteVisualization()
{
    auto* S=Simulation.Get();auto* PC=GetOwningPlayer();
    if(!bSiteVisualization||!S||!PC||!Host.IsValid()||!IsVisible()||!(bSplit||ActiveView<2))return;
    uint32 Key=GetTypeHash(S->BoomLength);for(double D:{S->BoomHeight,S->RailSpan,S->CabHeight,S->CabX,S->HoldDepth,S->OperatingDraft})Key=HashCombineFast(Key,GetTypeHash(D));Key=HashCombineFast(Key,GetTypeHash(S->bShowUnloader));
    if(SiteModel&&SiteShapeKey==Key&&SiteCabRevision==S->CabRevision())return;
    RestoreSiteVisualization();SiteShapeKey=Key;SiteCabRevision=S->CabRevision();
    Host->Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));SiteModel=NewObject<UShipHatchOverlay>(Host.Get());
    SiteModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);SiteModel->SetHiddenInSceneCapture(true);SiteModel->SetVisibleInRayTracing(false);SiteModel->SetCastShadow(false);SiteModel->SetCanEverAffectNavigation(false);SiteModel->bCalculateAccurateBounds=true;SiteModel->RegisterComponent();SiteModel->SetComponentTickEnabled(false);SiteModel->SetWorldTransform(FTransform::Identity);
    if(S->bShowUnloader)S->VisitUnloaderMeshes([&](UProceduralMeshComponent* Source)
    {
        if(!Source)return;
        if(!PC->HiddenPrimitiveComponents.Contains(Source)){PC->HiddenPrimitiveComponents.Add(Source);SiteHidden.Add(Source);}
        for(int32 I=0;I<Source->GetNumSections();++I)if(const auto* Section=Source->GetProcMeshSection(I))
        {
            FBatchedMesh Mesh;Mesh.Color=FColor(20,190,65,135);Mesh.DepthPriority=SDPG_World;
            Mesh.MeshVerts.Reserve(Section->ProcVertexBuffer.Num());for(const auto& V:Section->ProcVertexBuffer)Mesh.MeshVerts.Add(Source->GetComponentTransform().TransformPosition(FVector(V.Position)));
            Mesh.MeshIndices.Reserve(Section->ProcIndexBuffer.Num());for(uint32 Index:Section->ProcIndexBuffer)Mesh.MeshIndices.Add(int32(Index));SiteModel->BatchedMeshes.Add(MoveTemp(Mesh));
        }
    });
    SiteModel->UpdateBounds();SiteModel->MarkRenderStateDirty();
}
void UShipHatchMonitorWidget::SetFreeCamera(bool Enabled)
{
    if(bFreeCamera==Enabled||!Host.IsValid()||!Host->ViewCamera)return;
    bFreeCamera=Enabled;bOrbit=false;
    if(Enabled){FreeCameraPosition=Host->ViewCamera->GetActorLocation();FreeCameraRotation=Host->ViewCamera->GetActorRotation();bDigEnabled=false;SetKeyboardFocus();}
    else UpdateCamera();
    if(ModeLabel)ModeLabel->SetText(FText::FromString(Enabled?TEXT("자유 이동 · WASD 앞뒤/좌우 · QE 상하 · 좌드래그 회전 / 우드래그 팬 · 휠 전후 · 프리셋으로 종료"):TEXT("이동 모드 · 드래그 회전 · 휠 확대")));
}
FReply UShipHatchMonitorWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
    if(bFreeCamera)
    {
        const auto K=E.GetKey();const FRotationMatrix Basis(FreeCameraRotation);FVector Move=FVector::ZeroVector;
        if(K==EKeys::W)Move=Basis.GetUnitAxis(EAxis::X);if(K==EKeys::S)Move=-Basis.GetUnitAxis(EAxis::X);
        if(K==EKeys::D)Move=Basis.GetUnitAxis(EAxis::Y);if(K==EKeys::A)Move=-Basis.GetUnitAxis(EAxis::Y);
        if(K==EKeys::E)Move=FVector::UpVector;if(K==EKeys::Q)Move=-FVector::UpVector;
        if(!Move.IsZero()){FreeCameraPosition+=Move*(E.IsShiftDown()?200:50);UpdateCamera();return FReply::Handled();}
        if(K==EKeys::Escape){SetFreeCamera(false);return FReply::Handled();}
    }
    return Super::NativeOnKeyDown(G,E);
}
