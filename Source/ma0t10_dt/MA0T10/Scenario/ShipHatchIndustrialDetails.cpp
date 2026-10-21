#include "ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextRenderComponent.h"

namespace
{
// Sub-centimetre fittings use flat face normals and dark edge bands. These
// render details have no trace geometry; the structural query mesh is retained.
struct FIndustrialDetailMesh
{
 TArray<FVector> V,N;TArray<int32> T;TArray<FLinearColor> C;
 void Quad(FVector A,FVector B,FVector D,FVector E,FLinearColor Color)
 {
  const int I=V.Num();const auto Normal=FVector::CrossProduct(D-A,B-A).GetSafeNormal();
  for(auto P:{A,B,D,E}){V.Add(P*100);N.Add(Normal);C.Add(Color);}T.Append({I,I+2,I+1,I,I+3,I+2});
 }
 void Beam(FVector A,FVector B,double Width,double Depth,FLinearColor Color)
 {
  const auto Z=(B-A).GetSafeNormal(),X=FVector::CrossProduct(FMath::Abs(Z.Z)>.95?FVector::RightVector:FVector::UpVector,Z).GetSafeNormal(),Y=FVector::CrossProduct(Z,X);
  FVector P[8];for(int K=0;K<2;++K)for(int J=0;J<2;++J)for(int I=0;I<2;++I)P[K*4+J*2+I]=(K?B:A)+X*(I-.5)*Width+Y*(J-.5)*Depth;
  const int F[][4]={{0,2,3,1},{4,5,7,6},{0,1,5,4},{2,6,7,3},{0,4,6,2},{1,3,7,5}};
  for(int I=0;I<6;++I)Quad(P[F[I][0]],P[F[I][1]],P[F[I][2]],P[F[I][3]],Color*(I==0?.68f:I==1?1.05f:.88f));
 }
 void Box(FVector P,FVector Size,FLinearColor Color){Beam(P-FVector(0,0,Size.Z*.5),P+FVector(0,0,Size.Z*.5),Size.X,Size.Y,Color);}
 void Cylinder(FVector A,FVector B,double Radius,FLinearColor Color,int Segments=8)
 {
  const auto Z=(B-A).GetSafeNormal(),X=FVector::CrossProduct(FMath::Abs(Z.Z)>.95?FVector::RightVector:FVector::UpVector,Z).GetSafeNormal(),Y=FVector::CrossProduct(Z,X);
  for(int I=0;I<Segments;++I){const auto U=(X*FMath::Cos(2*PI*I/Segments)+Y*FMath::Sin(2*PI*I/Segments))*Radius,W=(X*FMath::Cos(2*PI*(I+1)/Segments)+Y*FMath::Sin(2*PI*(I+1)/Segments))*Radius;Quad(A+U,B+U,B+W,A+W,Color);Quad(A,A+U,A+W,A,Color*.7f);Quad(B,B+W,B+U,B,Color*1.05f);}
 }
 void Commit(UProceduralMeshComponent* Mesh)
 {Mesh->CreateMeshSection_LinearColor(0,V,T,N,TArray<FVector2D>(),C,TArray<FProcMeshTangent>(),false);}
};
UProceduralMeshComponent* DetailComponent(AActor* Owner,const TCHAR* Name,double CullMeters)
{
 auto* M=NewObject<UProceduralMeshComponent>(Owner,Name);Owner->AddInstanceComponent(M);M->SetupAttachment(Owner->GetRootComponent());M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCanEverAffectNavigation(false);M->SetCastShadow(false);M->SetCullDistance(CullMeters*100);M->ComponentTags.Add(TEXT("HatchFineDetail"));M->RegisterComponent();return M;
}
void FinishDetail(UProceduralMeshComponent* M,AActor* Owner,FIndustrialDetailMesh& Mesh)
{
 Mesh.Commit(M);
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
 {auto* MID=UMaterialInstanceDynamic::Create(Base,Owner);MID->SetScalarParameterValue(TEXT("Hardware"),1);M->SetMaterial(0,MID);}
}
}

void AShipHatchSimulation::BuildIndustrialDetails()
{
 if(!bRealScale||!Vessel||!UnloaderPresentation)return;
 if(RigFineDetails&&RigFineDetails->GetOwner()!=UnloaderPresentation)RigFineDetails=nullptr;
 if(VesselFineDetails&&VesselFineDetails->GetOwner()!=Vessel)VesselFineDetails=nullptr;
 uint32 Key=SceneScaleKey;for(double D:{HoldLength,HoldWidth,HoldDepth,BoomLength,BoomHeight,CabHeight,CabX,CabY})Key=HashCombineFast(Key,GetTypeHash(D));
 if(RigFineDetails&&VesselFineDetails&&IndustrialDetailKey==Key)return;
 IndustrialDetailKey=Key;
 if(!RigFineDetails)RigFineDetails=DetailComponent(UnloaderPresentation,TEXT("HatchRigFineDetails"),180);
 if(!VesselFineDetails)VesselFineDetails=DetailComponent(Vessel,TEXT("HatchVesselFineDetails"),220);
 FIndustrialDetailMesh R,S;
 const FLinearColor Teal(.021,.19,.125),Edge(.009,.065,.044),Yellow(.72,.45,.025),Iron(.07,.085,.085),Rust(.17,.071,.033),Pink(.40,.235,.215),White(.63,.64,.59),Glass(.012,.035,.039);
 const double Sea=SeaRailY(),Land=Sea-RailSpan,Back=Sea-23,Tip=BoomTipY(),H=HoldDepth+BoomHeight+1.2,Q=QuayZ(),D=HoldDepth;
 // Welded box girder flange strips, diaphragm ribs and bolted connection plates.
 for(int Sign:{-1,1})
 {
  const double X=Sign*1.5;
  for(double Z:{H+.27,H+1.73})R.Beam(FVector(X,Back,Z),FVector(X,Tip,Z),.99,.075,Teal);
  for(double Y=Back+.5;Y<Tip;Y+=2.4)
  {
   R.Box(FVector(X+Sign*.416,Y,H+1),FVector(.045,.11,1.46),Teal);
   for(double Z:{H+.4,H+1.6})for(double Offset:{-.16,.16})R.Cylinder(FVector(X+Sign*.44,Y+Offset,Z),FVector(X+Sign*.47,Y+Offset,Z),.037,Iron,6);
  }
  // Walkway:1.1m top rail,0.55m mid rail,1.5m posts and150mm toe plate.
  for(double Z:{H+3.75,H+4.25})R.Beam(FVector(Sign*2.8,Back,Z),FVector(Sign*2.8,Tip,Z),.045,.045,Yellow);
  R.Beam(FVector(Sign*2.8,Back,H+3.275),FVector(Sign*2.8,Tip,H+3.275),.03,.15,Teal);
  for(double Y=Back;Y<Tip;Y+=1.5)R.Beam(FVector(Sign*2.8,Y,H+3.2),FVector(Sign*2.8,Y,H+4.25),.045,.045,Yellow);
  // Conduit below walkway with saddle clamps, continuous cable tray.
  R.Cylinder(FVector(Sign*2.1,Back,H+2.9),FVector(Sign*2.1,Tip,H+2.9),.055,Iron);
  for(double Y=Back;Y<Tip;Y+=3)R.Box(FVector(Sign*2.1,Y,H+2.9),FVector(.16,.055,.16),Teal);
  for(double Y:{Sea,Land})
  {
   const double TopY=Y==Land?Back:Y;
   for(double Z:{Q+9,Q+18,H-1})
   {
    const double At=FMath::Lerp(Y,TopY,(Z-Q-1)/(H-Q-1));
    R.Box(FVector(Sign*7,At,Z),FVector(1.7,1.62,.08),Edge);
    for(double O:{-.48,.48})for(double B:{-.5,.5})R.Cylinder(FVector(Sign*7+O,At+B,Z+.05),FVector(Sign*7+O,At+B,Z+.11),.055,Iron,6);
    // Triangular gussets read as connected braces, rather than crossing rods.
    const FVector P(Sign*7,At-.72,Z);R.Quad(P,P+FVector(-Sign*1.15,0,0),P+FVector(0,0,1.15),P,Teal);
   }
   for(double O:{-1.5,-.5,.5,1.5})
   {R.Cylinder(FVector(Sign*7+O,Y-.19,Q+.35),FVector(Sign*7+O,Y+.19,Q+.35),.17,Iron);R.Box(FVector(Sign*7+O,Y,Q+.78),FVector(.68,.75,.13),Teal);}
  }
 }
 // Machine house: separate window panes, louvers, double doors and a roof lip.
 for(int Sign:{-1,1})
 {
  const double X=Sign*6.515;
  R.Box(FVector(Sign*6.55,Back+5,H+9.17),FVector(.16,14.4,.20),Edge);
  for(int I=0;I<5;++I){const double Y=Back+.6+I*2.15;R.Box(FVector(X,Y,H+6.5),FVector(.035,1.55,1.6),Iron);R.Box(FVector(X+Sign*.025,Y,H+6.52),FVector(.03,1.35,1.38),Glass);}
  for(int I=0;I<12;++I)R.Box(FVector(X+Sign*.025,Back+5,H+1.7+I*.16),FVector(.09,4.5,.06),Edge);
  R.Box(FVector(X,Back+10,H+2.2),FVector(.04,2,3.5),Edge);
  R.Box(FVector(X+Sign*.035,Back+10,H+2.2),FVector(.03,1.8,3.3),Teal);
  R.Beam(FVector(X+Sign*.07,Back+10,H+.6),FVector(X+Sign*.07,Back+10,H+3.7),.03,.03,Iron);
 }
 // Rail travelling power cable reel, end flanges and winding ribs.
 const FVector Reel(6,Land+2.2,Q+2.2);
 R.Cylinder(Reel-FVector(0,.85,0),Reel+FVector(0,.85,0),.85,Iron,16);
 for(double O:{-.95,.95})R.Cylinder(Reel+FVector(0,O-.04,0),Reel+FVector(0,O+.04,0),1.12,Teal,16);
 for(int I=0;I<10;++I)R.Cylinder(Reel+FVector(0,-.75+I*.16,0),Reel+FVector(0,-.71+I*.16,0),.90,Edge,12);
 // Hatch coaming buttresses, wheel housings and panel seam gaskets.
 for(int Hatch=-4;Hatch<=4;++Hatch)
 {
  const double X=Hatch*27.;
  for(int Sign:{-1,1})for(double DX=-8;DX<=8;DX+=2)
  {
   const double Y=Sign*(OpeningWidth*.5+.21);
   S.Box(FVector(X+DX,Y,D+.9),FVector(.13,.25,1.8),Rust);
   S.Box(FVector(X+DX,Y+Sign*.05,D+.88),FVector(.25,.08,1.75),Pink);
  }
  if(Hatch)for(int Sign:{-1,1})for(double DY=-8;DY<=8;DY+=2)S.Box(FVector(X+Sign*(OpeningLength*.5+.21),DY,D+.9),FVector(.25,.14,1.8),Pink);
  for(int Sign:{-1,1})
  {
   const double Y=Hatch?Sign*OpeningWidth*.25:Sign*(OpeningWidth*.75+.15);
   S.Box(FVector(X,Y,D+CoamingHeight+.365),FVector(OpeningLength-.2,.055,.055),Edge);
   for(double DX:{-9.,9.})S.Box(FVector(X+DX,Y,D+CoamingHeight+.34),FVector(.55,OpeningWidth*.5-.2,.07),Pink);
  }
 }
 // Fore/aft mooring stations: drum winches, cruciform bitts and roller fairleads.
 const double Half=VesselLength*.5;
 for(double X:{-Half+31,Half-26})for(int Sign:{-1,1})
 {
  const FVector P(X,Sign*13.,D+.7);S.Box(P-FVector(0,0,.55),FVector(4.8,3,.3),Pink);
  for(double Offset:{-1.25,1.25})
  {const FVector B=P+FVector(Offset,0,.2);S.Cylinder(B-FVector(0,.6,0),B+FVector(0,.6,0),.65,Iron,12);for(double F:{-.68,.68})S.Cylinder(B+FVector(0,F-.04,0),B+FVector(0,F+.04,0),.83,Pink,12);}
  for(double Offset:{-1.,1.}){const FVector B(X+Offset,Sign*19,D+.15);S.Cylinder(B,B+FVector(0,0,1.1),.19,Iron);S.Beam(B+FVector(-.4,0,.85),B+FVector(.4,0,.85),.18,.18,Iron);}
  for(double O:{-.45,.45}){const FVector F(X+O,Sign*21,D+.4);S.Cylinder(F-FVector(0,0,.2),F+FVector(0,0,.45),.27,Iron);}
 }
 // Pane mullions and bridge wing balustrades preserve the original bridge envelope.
 const double Aft=-Half+13;
 for(int Floor=0;Floor<5;++Floor)
 {
  const double Z=D+6+(Floor+.5)*2.8+.35;
  for(double Y=-11+Floor;Y<=11-Floor;Y+=1.5)S.Box(FVector(Aft+(18-Floor)*.5+.07,Y,Z),FVector(.045,.12,1.02),White);
  for(int Sign:{-1,1})for(double X=Aft-5.5;X<=Aft+5.5;X+=1.5)S.Box(FVector(X,Sign*((28-Floor*2)*.5+.07),Z),FVector(.12,.045,1.02),White);
 }
 for(int Sign:{-1,1})
 {
  S.Beam(FVector(Aft-7,Sign*17.8,D+23.02),FVector(Aft+13,Sign*17.8,D+23.02),.055,.055,White);
  for(double X=Aft-7;X<Aft+13;X+=1.4)S.Beam(FVector(X,Sign*17.8,D+21.6),FVector(X,Sign*17.8,D+23.02),.05,.05,White);
  // 200mm draft marks at bow/stern; contrasting edge bands, no label texture.
  for(double X:{-Half+24,Half-21})for(int I=0;I<18;++I)
  {const double Z=WaterZ()-3+I*.2;const double Breadth=X>Half-34?VesselBeam*.5*FMath::Sqrt((Half-X)/34.):X<-Half+30?FMath::Lerp(8.,VesselBeam*.5,(X+Half)/30.):VesselBeam*.5;const double Factor=Z<WaterZ()?FMath::Lerp(.84,.98,FMath::Clamp((Z-KeelZ()-2)/(WaterZ()-KeelZ()-2),0.,1.)):FMath::Lerp(.98,1.,(Z-WaterZ())/(D-WaterZ()));const double Y=Sign*(Breadth*Factor+.025);S.Box(FVector(X,Y,Z),FVector(I%5==0?.6:.32,.03,.045),White);}
 }
 // Fine concrete streaks at the quay face. Minor appearance details only.
 for(double X=-170;X<170;X+=3.7)S.Box(FVector(X,QuayEdgeY()+.008,QuayZ()-.55),FVector(.10,.025,.8),FLinearColor(.12,.13,.12));
 for(int Sign:{-1,1})
 {
  const FName Name=Sign>0?TEXT("HatchNameSea"):TEXT("HatchNameLand");
  auto* Label=FindObject<UTextRenderComponent>(Vessel,*Name.ToString());
  if(!Label){Label=NewObject<UTextRenderComponent>(Vessel,Name);Vessel->AddInstanceComponent(Label);Label->SetupAttachment(Vessel->Mesh);Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);Label->SetCastShadow(false);Label->SetCullDistance(22000);Label->RegisterComponent();}
  Label->SetText(FText::FromString(TEXT("HATCH 06")));Label->SetHorizontalAlignment(EHTA_Center);Label->SetWorldSize(95);Label->SetTextRenderColor(FColor(205,208,197));Label->SetRelativeLocationAndRotation(FVector(Half-45,Sign*(VesselBeam*.5+.04),WaterZ()+3)*100,FRotator(0,Sign*90,0));
 }
 FinishDetail(RigFineDetails,UnloaderPresentation,R);FinishDetail(VesselFineDetails,Vessel,S);
 RigFineDetails->SetVisibility(bShowUnloader);
}
