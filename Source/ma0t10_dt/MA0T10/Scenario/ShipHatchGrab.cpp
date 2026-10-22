#include "ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"

namespace
{
struct FGrabMesh
{
 TArray<FVector> V;TArray<int32> T;TArray<FLinearColor> C;
 void Quad(FVector A,FVector B,FVector D,FVector E,FLinearColor Color)
 {const int N=V.Num();for(auto P:{A,B,D,E}){V.Add(P*100);C.Add(Color);}T.Append({N,N+2,N+1,N,N+3,N+2});}
 void Rod(FVector A,FVector B,double Radius,FLinearColor Color,int Segments=8)
 {
  const FVector Z=(B-A).GetSafeNormal(),X=FVector::CrossProduct(FMath::Abs(Z.Z)>.95?FVector::RightVector:FVector::UpVector,Z).GetSafeNormal(),Y=FVector::CrossProduct(Z,X);const int Base=V.Num();
  for(int Side=0;Side<2;++Side)for(int I=0;I<Segments;++I){const double Angle=2*PI*I/Segments;V.Add(((Side?B:A)+(X*FMath::Cos(Angle)+Y*FMath::Sin(Angle))*Radius)*100);C.Add(Color);}
  for(int I=0;I<Segments;++I){const int J=(I+1)%Segments;T.Append({Base+I,Base+J,Base+Segments+J,Base+I,Base+Segments+J,Base+Segments+I});}
  for(int I=1;I<Segments-1;++I)T.Append({Base,Base+I+1,Base+I,Base+Segments,Base+Segments+I,Base+Segments+I+1});
 }
 void BeveledBox(FVector P,FVector Size,double Bevel,FLinearColor Color)
 {
  const double X=Size.X*.5,Y=Size.Y*.5,B=FMath::Min(Bevel,FMath::Min(X,Y)*.5);TArray<FVector> Outline={FVector(-X+B,-Y,0),FVector(X-B,-Y,0),FVector(X,-Y+B,0),FVector(X,Y-B,0),FVector(X-B,Y,0),FVector(-X+B,Y,0),FVector(-X,Y-B,0),FVector(-X,-Y+B,0)};
  for(int I=0;I<8;++I){const int J=(I+1)%8;Quad(P+Outline[I]-FVector(0,0,Size.Z*.5),P+Outline[J]-FVector(0,0,Size.Z*.5),P+Outline[J]+FVector(0,0,Size.Z*.5),P+Outline[I]+FVector(0,0,Size.Z*.5),I%2?Color*1.15f:Color);}
  for(int I=1;I<7;++I){Quad(P+Outline[0]+FVector(0,0,Size.Z*.5),P+Outline[I]+FVector(0,0,Size.Z*.5),P+Outline[I+1]+FVector(0,0,Size.Z*.5),P+Outline[0]+FVector(0,0,Size.Z*.5),Color*1.1f);Quad(P+Outline[0]-FVector(0,0,Size.Z*.5),P+Outline[I+1]-FVector(0,0,Size.Z*.5),P+Outline[I]-FVector(0,0,Size.Z*.5),P+Outline[0]-FVector(0,0,Size.Z*.5),Color*.8f);}
 }
};
}
void AShipHatchSimulation::SetGrabVisible(bool Enabled)
{
 if(bShowGrab==Enabled)return;bShowGrab=Enabled;bGrabPoseOverride=false;
 LastGeometryEdit=FPlatformTime::Seconds();
 if(!bGenerating){ConfigureSensors();++SurfaceRevision;OnSurfaceChanged.Broadcast();}
}
bool AShipHatchSimulation::IsGrabPoseSafe(FVector Lip) const
{
 if(Lip.ContainsNaN()||Lip.Y<SeaRailY()-RailSpan+2||Lip.Y>BoomTipY()-2||FMath::Abs(Lip.X)>OpeningLength*.5-2.1||Lip.Z<0||Lip.Z+5.8>BoomSoffitZ())return false;
 // Four rope columns and the bucket's full opening envelope must clear the
 // independent cab (2.4 x 2.8m) by at least 1m. Separate trolley lanes never cross.
 const double DX=FMath::Abs(Lip.X-CabX),DY=FMath::Abs(Lip.Y-CabY);
 if(DX<1.2+.65+1&&DY<1.4+.65+1)return false;
 if(Lip.Z+5.8>HoldDepth+CabHeight+.35-1&&DX<1.2+2.1+1&&DY<1.4+2+1)return false;
 return true;
}
void AShipHatchSimulation::SetGrabPose(FVector Lip,double Open){TrySetGrabPose(Lip,Open);}
bool AShipHatchSimulation::TrySetGrabPose(FVector Lip,double Open)
{
 if(!bShowGrab||!UnloaderPresentation||bCabMoving||!FMath::IsFinite(Open)||!IsGrabPoseSafe(Lip))return false;
 bGrabPoseOverride=true;GrabLipPose=Lip;GrabOpenFraction=FMath::Clamp(Open,0.,1.);BuildGrabModel();return true;
}
void AShipHatchSimulation::ResetGrabPose()
{if(!bGrabPoseOverride)return;bGrabPoseOverride=false;if(bShowGrab)BuildGrabModel();}
void AShipHatchSimulation::BuildGrabModel()
{
 if(!UnloaderPresentation)return;
 if(GrabPresentation&&!bShowGrab){GrabPresentation->SetVisibility(false);GrabPresentation->SetCollisionEnabled(ECollisionEnabled::NoCollision);return;}
 const double Started=FPlatformTime::Seconds();
 if(!GrabPresentation)
 {
  GrabPresentation=NewObject<UProceduralMeshComponent>(UnloaderPresentation,TEXT("PhysicalClamshell"));GrabPresentation->SetupAttachment(UnloaderPresentation->Mesh);GrabPresentation->bUseComplexAsSimpleCollision=true;GrabPresentation->SetCollisionProfileName(TEXT("BlockAll"));GrabPresentation->SetCastShadow(false);GrabPresentation->SetCanEverAffectNavigation(false);GrabPresentation->ComponentTags.Add(TEXT("ShipHatchGrab"));GrabPresentation->RegisterComponent();
 }
 if(bShowGrab)GrabPresentation->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 // Quarter-ellipse two-shell bucket:4.2x4.0x1.55m gross semi-ellipse
 // envelope20.45m3 (~20m3 class). Crosshead/4 constant3.3m tie rods are
 // representative kinematics, not an individual manufacturer's shop drawing.
 constexpr double HalfWidth=2.1,HalfLength=2.,Depth=1.55,ArmLength=3.3;constexpr int ArcSteps=12;
 const double H=HoldDepth+BoomHeight+1.2;
 const FVector Lip=bGrabPoseOverride?GrabLipPose:FVector(0,4,FMath::Min(H-6,Current.Height(0,4)+.2));
 const FVector Hinge=Lip+FVector(0,0,Depth);const double Angle=FMath::DegreesToRadians(65.)*(bGrabPoseOverride?GrabOpenFraction:0.);
 const FLinearColor Steel(.14,.16,.17),Inside(.065,.075,.08),Edge(.38,.40,.39),Dark(.035,.045,.045),Dirt(.10,.065,.035);
 FGrabMesh Mesh;
 auto Jaw=[&](int Sign,double A,double Y,double Inset=0.){const FVector Local(Sign*(HalfWidth-Inset)*FMath::Cos(A),Y,-(Depth-Inset)*FMath::Sin(A));const double S=FMath::Sin(-Sign*Angle),C=FMath::Cos(Angle);return Hinge+FVector(Local.X*C+Local.Z*S,Local.Y,-Local.X*S+Local.Z*C);};
 for(int Sign:{-1,1})
 {
  for(int I=0;I<ArcSteps;++I)
  {
   const double A=PI*.5*I/ArcSteps,B=PI*.5*(I+1)/ArcSteps;
   const auto Color=I>8?Dirt:Steel*(I%3==0?1.12f:1.f);
   if(Sign>0)Mesh.Quad(Jaw(Sign,A,-HalfLength),Jaw(Sign,A,HalfLength),Jaw(Sign,B,HalfLength),Jaw(Sign,B,-HalfLength),Color);else Mesh.Quad(Jaw(Sign,A,HalfLength),Jaw(Sign,A,-HalfLength),Jaw(Sign,B,-HalfLength),Jaw(Sign,B,HalfLength),Color);
   if(Sign>0)Mesh.Quad(Jaw(Sign,B,-HalfLength+.06,.07),Jaw(Sign,B,HalfLength-.06,.07),Jaw(Sign,A,HalfLength-.06,.07),Jaw(Sign,A,-HalfLength+.06,.07),Inside);else Mesh.Quad(Jaw(Sign,B,HalfLength-.06,.07),Jaw(Sign,B,-HalfLength+.06,.07),Jaw(Sign,A,-HalfLength+.06,.07),Jaw(Sign,A,HalfLength-.06,.07),Inside);
   for(int End:{-1,1})if(End*Sign>0)Mesh.Quad(Hinge+FVector(0,End*HalfLength,0),Jaw(Sign,A,End*HalfLength),Jaw(Sign,B,End*HalfLength),Hinge+FVector(0,End*HalfLength,0),Steel*.85f);else Mesh.Quad(Hinge+FVector(0,End*HalfLength,0),Jaw(Sign,B,End*HalfLength),Jaw(Sign,A,End*HalfLength),Hinge+FVector(0,End*HalfLength,0),Steel*.85f);
   for(double Y:{-1.5,0.,1.5})Mesh.Rod(Jaw(Sign,A,Y,-.045),Jaw(Sign,B,Y,-.045),.055,Steel*1.2f,6);
  }
  Mesh.Rod(Jaw(Sign,PI*.5,-HalfLength),Jaw(Sign,PI*.5,HalfLength),.07,Edge,8); // cutting lip
  Mesh.Rod(Jaw(Sign,0,-HalfLength),Jaw(Sign,0,HalfLength),.07,Edge,8);
  for(int End:{-1,1})Mesh.Rod(Hinge+FVector(0,End*HalfLength,0),Jaw(Sign,0,End*HalfLength),.06,Edge,6);
 }
 Mesh.Rod(Hinge-FVector(0,2.12,0),Hinge+FVector(0,2.12,0),.16,Dark,12); // shared hinge axle
 const auto Pin=Jaw(1,PI/6,1.55);const double DX=Pin.X-Lip.X-.55;
 const double HeadZ=Pin.Z+FMath::Sqrt(FMath::Max(.01,ArmLength*ArmLength-DX*DX));
 // Open crosshead leaves the sheaves and closing-rope reeving readable,
 // instead of a solid slab obscuring the mechanism in the reference view.
 for(int Side:{-1,1})Mesh.BeveledBox(FVector(Lip.X,Lip.Y+Side*1.55,HeadZ),FVector(2.4,.32,.45),.08,Steel);
 for(int End:{-1,1})Mesh.BeveledBox(FVector(Lip.X+End*1.04,Lip.Y,HeadZ),FVector(.32,3.1,.45),.08,Steel);
 Mesh.BeveledBox(Hinge+FVector(0,0,.3),FVector(1.8,1.7,.4),.10,Steel);
 GrabArmLengthErrorMeters=0;
 for(int Sign:{-1,1})for(int Side:{-1,1})
 {
  const FVector A(Lip.X+Sign*.55,Lip.Y+Side*1.55,HeadZ),B=Jaw(Sign,PI/6,Side*1.55);
  GrabArmLengthErrorMeters=FMath::Max(GrabArmLengthErrorMeters,FMath::Abs(FVector::Distance(A,B)-ArmLength));
  Mesh.Rod(A,B,.10,Steel*1.15f,8);
  for(auto P:{A,B}){Mesh.Rod(P-FVector(0,.13,0),P+FVector(0,.13,0),.18,Edge,12);Mesh.Rod(P-FVector(0,.15,0),P+FVector(0,.15,0),.07,Dark,8);}
 }
 for(int Side:{-1,1})for(int Pair:{-1,1})
 {
  const FVector P(Lip.X+Pair*.5,Lip.Y+Side*.63,HeadZ-.10);
  Mesh.Rod(P-FVector(0,.11,0),P+FVector(0,.11,0),.39,Dark,12);Mesh.Rod(P-FVector(0,.13,0),P+FVector(0,.13,0),.30,Edge,12);
  const FVector Lower(Hinge.X+Pair*.45,Hinge.Y+Side*.45,Hinge.Z+.56);Mesh.Rod(Lower-FVector(0,.09,0),Lower+FVector(0,.09,0),.29,Dark,12);
  Mesh.Rod(P,Lower,.018,Dark,6); // visible closing-rope reeving
  Mesh.Rod(FVector(Lip.X+Pair*.65,Lip.Y+Side*.65,H+.1),P,.022,Dark,8);
 }
 Mesh.BeveledBox(FVector(0,Lip.Y,H+.6),FVector(3.2,3.8,1.2),.12,FLinearColor(.012,.13,.075)); // moving hoist carriage
 TArray<FVector> Normals;Normals.Init(FVector::ZeroVector,Mesh.V.Num());for(int I=0;I<Mesh.T.Num();I+=3){const auto N=FVector::CrossProduct(Mesh.V[Mesh.T[I+2]]-Mesh.V[Mesh.T[I]],Mesh.V[Mesh.T[I+1]]-Mesh.V[Mesh.T[I]]);for(int J=0;J<3;++J)Normals[Mesh.T[I+J]]+=N;}for(auto& N:Normals)N.Normalize();
 const auto* Old=GrabPresentation->GetProcMeshSection(0);bool Same=Old&&Old->ProcVertexBuffer.Num()==Mesh.V.Num()&&Old->ProcIndexBuffer.Num()==Mesh.T.Num();if(Same)for(int I=0;I<Mesh.V.Num();++I)if(!FVector(Old->ProcVertexBuffer[I].Position).Equals(Mesh.V[I],1.e-4)){Same=false;break;}
 if(!Same){if(Old&&Old->ProcVertexBuffer.Num()==Mesh.V.Num()&&Old->ProcIndexBuffer.Num()==Mesh.T.Num())GrabPresentation->UpdateMeshSection_LinearColor(0,Mesh.V,Normals,TArray<FVector2D>(),Mesh.C,TArray<FProcMeshTangent>());else GrabPresentation->CreateMeshSection_LinearColor(0,Mesh.V,Mesh.T,Normals,TArray<FVector2D>(),Mesh.C,TArray<FProcMeshTangent>(),true);++GrabMeshCommits;}
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
 {auto* MID=Cast<UMaterialInstanceDynamic>(GrabPresentation->GetMaterial(0));if(!MID||MID->Parent!=Base){MID=UMaterialInstanceDynamic::Create(Base,UnloaderPresentation);MID->SetScalarParameterValue(TEXT("Hardware"),1);MID->SetScalarParameterValue(TEXT("Metallic"),.55);GrabPresentation->SetMaterial(0,MID);}}
 GrabPresentation->SetVisibility(bShowGrab);GrabPresentation->SetCollisionEnabled(bShowGrab?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
 LastGrabMeshBuildMs=(FPlatformTime::Seconds()-Started)*1000;MaxGrabMeshBuildMs=FMath::Max(MaxGrabMeshBuildMs,LastGrabMeshBuildMs);
}
