#include "ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodySetup.h"

void AShipHatchSimulation::BuildUnloaderDetails()
{
 if(!UnloaderPresentation)return;
 TArray<FVector> V,N;TArray<int32> T;TArray<FLinearColor> C;
 const FLinearColor Steel(.012,.13,.075),Brace(.02,.18,.10),Glass(.006,.025,.025),Dark(.035,.045,.04),Safety(.8,.52,.025);
 auto Beam=[&](FVector A,FVector B,double W,double D,FLinearColor Color)
 {
  const FVector Z=(B-A).GetSafeNormal(),X=FVector::CrossProduct(FMath::Abs(Z.Z)>.95?FVector::RightVector:FVector::UpVector,Z).GetSafeNormal(),Y=FVector::CrossProduct(Z,X);
  const int32 Base=V.Num();for(int K=0;K<2;++K)for(int J=0;J<2;++J)for(int I=0;I<2;++I){V.Add(((K?B:A)+X*(I-.5)*W+Y*(J-.5)*D)*100);C.Add(Color);}
  const int32 Indices[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
  for(int I=0;I<36;I+=3){T.Add(Base+Indices[I]);T.Add(Base+Indices[I+2]);T.Add(Base+Indices[I+1]);}
 };
 auto Box=[&](FVector P,FVector Size,FLinearColor Color){Beam(P-FVector(0,0,Size.Z*.5),P+FVector(0,0,Size.Z*.5),Size.X,Size.Y,Color);};
 const double Tip=BoomTipY(),Root=Tip-BoomLength,H=HoldDepth+BoomHeight+1.2;
 // 2,250t/h reference class: 28m rail span, 42m seaward outreach,
 // 23m backreach. Hoist height24m above rail plus structural headroom.
 const double RailZ=bRealScale?QuayZ():HoldDepth-8,SeaRail=bRealScale?SeaRailY():Root+3,LandRail=SeaRail-(bRealScale?RailSpan:18),Back=SeaRail-23;
 for(double Y:{SeaRail,LandRail})
 {
  Beam(FVector(-14,Y,RailZ),FVector(14,Y,RailZ),.18,.20,Dark);
  for(int Sign:{-1,1})
  {
   const double X=Sign*7.;Box(FVector(X,Y,RailZ+.65),FVector(4.6,1.1,.65),Steel);
   for(double Offset:{-1.5,-.5,.5,1.5})
   {
    // Eight-sided steel wheels on the two rail bogies.
    const FVector Center(X+Offset,Y,RailZ+.35);const int B=V.Num();
    for(int Side=0;Side<2;++Side)for(int I=0;I<8;++I){const double A=2*PI*I/8;V.Add((Center+FVector(.36*FMath::Cos(A),(Side-.5)*.28,.36*FMath::Sin(A)))*100);C.Add(Dark);}
    for(int I=0;I<8;++I){const int J=(I+1)%8;T.Append({B+I,B+J,B+8+J,B+I,B+8+J,B+8+I});}
   }
   Beam(FVector(X,Y,RailZ+1),FVector(Sign*3,Y==LandRail?Back:Y,H),.75,.75,Steel);
  }
  const double TopY=Y==LandRail?Back:Y;Beam(FVector(-3,TopY,H),FVector(3,TopY,H),.8,.8,Steel);
 }
 // Reference1: box-girder silhouette, central A-frame, stays, green machine
 // house and yellow access rails. SI dimensions remain the same.
 for(int Sign:{-1,1})
 {
  Beam(FVector(Sign*1.5,Back,H+1),FVector(Sign*1.5,Tip,H+1),.8,1.5,Steel);
  Beam(FVector(Sign*3,SeaRail+2,H+3),FVector(Sign*.6,SeaRail+2,H+15),.5,.5,Steel);
  Beam(FVector(Sign*.6,SeaRail+2,H+15),FVector(Sign*1.5,Tip-2,H+3),.09,.09,Dark);
  Beam(FVector(Sign*.6,SeaRail+2,H+15),FVector(Sign*1.5,Back+1,H+3),.09,.09,Dark);
  Beam(FVector(Sign*2.5,Back,H+3.2),FVector(Sign*2.5,Tip,H+3.2),.8,.15,Steel);
  Beam(FVector(Sign*2.8,Back,H+4.25),FVector(Sign*2.8,Tip,H+4.25),.06,.06,Safety);
  for(double Y=Back;Y<=Tip;Y+=5)Beam(FVector(Sign*2.8,Y,H+3.2),FVector(Sign*2.8,Y,H+4.25),.055,.055,Safety);
  for(double Y:{SeaRail,LandRail})
  {
   const FVector Bottom(Sign*8,Y,RailZ+1),Top(Sign*4,Y==LandRail?Back:Y,H+3);
   for(int Side:{-1,1})Beam(Bottom+FVector(0,Side*.45,0),Top+FVector(0,Side*.45,0),.055,.055,Safety);
   const int Rungs=FMath::CeilToInt((Top-Bottom).Size()/.3);
   for(int I=0;I<=Rungs;++I){const FVector P=FMath::Lerp(Bottom,Top,double(I)/Rungs);Beam(P-FVector(0,.45,0),P+FVector(0,.45,0),.04,.04,Safety);}
  }
 }
 Beam(FVector(-.6,SeaRail+2,H+15),FVector(.6,SeaRail+2,H+15),.5,.5,Steel);
 Box(FVector(0,Back+5,H+4.6),FVector(13,14,9),Steel);
 for(int Sign:{-1,1})Beam(FVector(Sign*3,SeaRail,RailZ+2),FVector(-Sign*3,SeaRail,H-2),.24,.24,Brace);
 for(int Sign:{-1,1})
 {
  Beam(FVector(Sign*7,LandRail,RailZ+2),FVector(Sign*3,SeaRail,H-1),.22,.22,Brace);
  // 3m deep two-plane Warren truss, including the landward backreach.
  for(double Z:{H,H+3})Beam(FVector(Sign*1.5,Back,Z),FVector(Sign*1.5,Tip,Z),.28,.28,Steel);
  const int Bays=FMath::CeilToInt((Tip-Back)/4.);
  for(int I=0;I<Bays;++I)
  {
   const double A=FMath::Lerp(Back,Tip,double(I)/Bays),B=FMath::Lerp(Back,Tip,double(I+1)/Bays);
   Beam(FVector(Sign*1.5,A,H),FVector(Sign*1.5,B,H+3),.13,.13,Brace);
   Beam(FVector(Sign*1.5,A,H+3),FVector(Sign*1.5,B,H),.13,.13,Brace);
   Beam(FVector(-1.5,A,H+3),FVector(1.5,A,H+3),.13,.13,Brace);
  }
 }
 Box(FVector(0,Back+2,H+1.2),FVector(4,5,2.2),Dark); // counterweight / machinery
 // Fixed under-trolley cab; existing cab center and two sensor mounts retained.
 Box(FVector(CabX,CabY+1.42,HoldDepth+CabHeight+1.7),FVector(4.4,.06,1.2),Glass);
 for(int Sign:{-1,1})Box(FVector(CabX+Sign*2.52,CabY,HoldDepth+CabHeight+1.7),FVector(.06,2.3,1.2),Glass);
 Beam(FVector(CabX,CabY,HoldDepth+CabHeight+2.6),FVector(CabX,CabY,H),.35,.35,Steel);
 // Four hoist ropes to a low-poly clamshell suspended above the current surface.
 const int GrabStartV=V.Num(),GrabStartT=T.Num();
 const double GrabZ=FMath::Min(H-3,Current.Height(0,4)+4);
 Box(FVector(0,4,H+.6),FVector(3.2,3.8,1.2),Steel);
 for(int X:{-1,1})for(int Y:{-1,1})Beam(FVector(X*.65,4+Y*.5,H),FVector(X*.65,4+Y*.5,GrabZ+1.4),.035,.035,Dark);
 Box(FVector(0,4,GrabZ+1.4),FVector(1.8,1.4,.3),Steel);
 // Four octagonal sheaves above the clamshell hinge, reference3.
 for(int X:{-1,1})for(int Y:{-1,1})
 {
  const FVector P(X*.55,4+Y*.4,GrabZ+1.75);const int B=V.Num();
  for(int Side=0;Side<2;++Side)for(int I=0;I<8;++I){const double A=2*PI*I/8;V.Add((P+FVector((Side-.5)*.2,.4*FMath::Cos(A),.4*FMath::Sin(A)))*100);C.Add(FLinearColor(.14,.16,.14));}
  for(int I=0;I<8;++I){const int J=(I+1)%8;T.Append({B+I,B+J,B+8+J,B+I,B+8+J,B+8+I});}
  for(int I=1;I<7;++I)T.Append({B,B+I+1,B+I,B+8,B+8+I,B+8+I+1});
 }
 // Representative ~20m3 clamshell envelope (4.0x4.4x2.4m); capacity is
 // illustrative, not a simulated certified payload or a throughput claim.
 for(int Sign:{-1,1})
 {
  auto Jaw=[&](int I,double Y){const double A=PI*.5*I/8;return FVector(Sign*2*FMath::Cos(A),Y,GrabZ+1.2-2.4*FMath::Sin(A));};
  auto Face=[&](FVector A,FVector B,FVector D,FVector E){const int Base=V.Num();for(auto P:{A,B,D,E}){V.Add(P*100);C.Add(FLinearColor(.15,.16,.13));}T.Append({Base,Base+2,Base+1,Base,Base+3,Base+2});};
  for(int I=0;I<8;++I){if(Sign>0)Face(Jaw(I,1.8),Jaw(I,6.2),Jaw(I+1,6.2),Jaw(I+1,1.8));else Face(Jaw(I,6.2),Jaw(I,1.8),Jaw(I+1,1.8),Jaw(I+1,6.2));
   for(double Y:{1.8,6.2})Face(FVector(0,Y,GrabZ+1.2),Jaw(I,Y),Jaw(I+1,Y),FVector(0,Y,GrabZ+1.2));}
  Beam(FVector(Sign*.2,4,GrabZ+1.4),FVector(Sign*1.8,4,GrabZ+1.0),.12,.12,Dark);
 }
 TArray<FVector> GripV;TArray<int32> GripT;TArray<FLinearColor> GripC;
 GripV.Append(V.GetData()+GrabStartV,V.Num()-GrabStartV);GripC.Append(C.GetData()+GrabStartV,C.Num()-GrabStartV);
 for(int I=GrabStartT;I<T.Num();++I)GripT.Add(T[I]-GrabStartV);
 V.SetNum(GrabStartV,false);C.SetNum(GrabStartV,false);T.SetNum(GrabStartT,false);
 // Landside tapered receiving hopper and conveyor under the portal.
 const double HopperY=(SeaRail+LandRail)*.5,TopZ=RailZ+15,LowZ=RailZ+7;
 for(int Sign:{-1,1})
 {
  Beam(FVector(Sign*5,HopperY,TopZ),FVector(Sign*1.4,HopperY,LowZ),8,.16,Steel);
  Beam(FVector(0,HopperY+Sign*4,TopZ),FVector(0,HopperY+Sign*1.4,LowZ),10,.16,Steel);
 }
 Box(FVector(0,HopperY,RailZ+4),FVector(2.8,2.8,6),Steel);
 for(int Sign:{-1,1})
 {
  Beam(FVector(Sign*5.5,HopperY-4.5,TopZ+1.1),FVector(Sign*5.5,HopperY+4.5,TopZ+1.1),.06,.06,Safety);
  Beam(FVector(-5.5,HopperY+Sign*4.5,TopZ+1.1),FVector(5.5,HopperY+Sign*4.5,TopZ+1.1),.06,.06,Safety);
  for(double X=-5.5;X<=5.5;X+=2.75)Beam(FVector(X,HopperY+Sign*4.5,TopZ),FVector(X,HopperY+Sign*4.5,TopZ+1.1),.055,.055,Safety);
 }
 Box(FVector(0,HopperY,RailZ+1.2),FVector(28,2,.35),Dark);
 for(int Sign:{-1,1})Beam(FVector(-14,HopperY+Sign*1.1,RailZ+1.5),FVector(14,HopperY+Sign*1.1,RailZ+1.5),.15,.15,Brace);
 N.Init(FVector::ZeroVector,V.Num());
 for(int I=0;I<T.Num();I+=3){const FVector Normal=FVector::CrossProduct(V[T[I+2]]-V[T[I]],V[T[I+1]]-V[T[I]]);for(int J=0;J<3;++J)N[T[I+J]]+=Normal;}
 for(auto& Normal:N)Normal.Normalize();
 auto* M=UnloaderPresentation->Mesh.Get();const auto* Existing=M->GetProcMeshSection(1);
 if(Existing&&Existing->ProcVertexBuffer.Num()==V.Num()&&Existing->ProcIndexBuffer.Num()==T.Num())
 {
  M->UpdateMeshSection_LinearColor(1,V,N,TArray<FVector2D>(),C,TArray<FProcMeshTangent>());
  // Transient query meshes have no serialized cooked payload. Avoid retaining
  // a new editor DDC blob for each regenerated cargo/hoist position.
  if(auto* Body=M->GetBodySetup()){Body->bHasCookedCollisionData=false;Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();M->RecreatePhysicsState();}
 }
 else M->CreateMeshSection_LinearColor(1,V,T,N,TArray<FVector2D>(),C,TArray<FProcMeshTangent>(),true);
 if(!GrabPresentation)
 {
  GrabPresentation=NewObject<UProceduralMeshComponent>(UnloaderPresentation,TEXT("PhysicalClamshell"));GrabPresentation->SetupAttachment(M);GrabPresentation->bUseComplexAsSimpleCollision=true;GrabPresentation->SetCollisionProfileName(TEXT("BlockAll"));GrabPresentation->SetCollisionEnabled(ECollisionEnabled::QueryOnly);GrabPresentation->SetCastShadow(false);GrabPresentation->SetCanEverAffectNavigation(false);GrabPresentation->RegisterComponent();
 }
 TArray<FVector> GripN;GripN.Init(FVector::ZeroVector,GripV.Num());for(int I=0;I<GripT.Num();I+=3){const auto Normal=FVector::CrossProduct(GripV[GripT[I+2]]-GripV[GripT[I]],GripV[GripT[I+1]]-GripV[GripT[I]]);for(int J=0;J<3;++J)GripN[GripT[I+J]]+=Normal;}for(auto& Normal:GripN)Normal.Normalize();
 if(const auto* Previous=GrabPresentation->GetProcMeshSection(0);Previous&&Previous->ProcVertexBuffer.Num()==GripV.Num())
 {GrabPresentation->UpdateMeshSection_LinearColor(0,GripV,GripN,TArray<FVector2D>(),GripC,TArray<FProcMeshTangent>());if(auto* Body=GrabPresentation->GetBodySetup()){Body->bHasCookedCollisionData=false;Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();GrabPresentation->RecreatePhysicsState();}}
 else GrabPresentation->CreateMeshSection_LinearColor(0,GripV,GripT,GripN,TArray<FVector2D>(),GripC,TArray<FProcMeshTangent>(),true);
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
 {auto* MID=Cast<UMaterialInstanceDynamic>(M->GetMaterial(1));if(!MID||MID->Parent!=Base){MID=UMaterialInstanceDynamic::Create(Base,UnloaderPresentation);MID->SetScalarParameterValue(TEXT("Hardware"),1);M->SetMaterial(1,MID);}GrabPresentation->SetMaterial(0,MID);}
}

UPrimitiveComponent* AShipHatchSimulation::GrabPrimitive() const{return GrabPresentation;}
