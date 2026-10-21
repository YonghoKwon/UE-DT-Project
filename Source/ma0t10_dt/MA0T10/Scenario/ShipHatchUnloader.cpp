#include "ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
bool HatchUnloaderSamePositions(const FProcMeshSection* Section,const TArray<FVector>& Vertices)
{if(!Section||Section->ProcVertexBuffer.Num()!=Vertices.Num())return false;for(int I=0;I<Vertices.Num();++I)if(!FVector(Section->ProcVertexBuffer[I].Position).Equals(Vertices[I],1.e-4))return false;return true;}
}

void AShipHatchSimulation::BuildUnloaderDetails()
{
 if(!UnloaderPresentation)return;
 TArray<FVector> V,N;TArray<int32> T;TArray<FLinearColor> C;
 const FLinearColor Steel(.012,.13,.075),Brace(.02,.18,.10),Glass(.006,.025,.025),Dark(.035,.045,.04),Safety(.8,.52,.025);
 auto Beam=[&](FVector A,FVector B,double W,double D,FLinearColor Color)
 {
  const FVector Z=(B-A).GetSafeNormal(),X=FVector::CrossProduct(FMath::Abs(Z.Z)>.95?FVector::RightVector:FVector::UpVector,Z).GetSafeNormal(),Y=FVector::CrossProduct(Z,X);
  const float Weather=.87f+.13f*FMath::Abs(FMath::Sin(A.X*1.71+A.Y*.63+A.Z*1.23));
  const int32 Base=V.Num();for(int K=0;K<2;++K)for(int J=0;J<2;++J)for(int I=0;I<2;++I){V.Add(((K?B:A)+X*(I-.5)*W+Y*(J-.5)*D)*100);C.Add(Color*Weather*(K?1.f:.88f));}
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
   Beam(FVector(X,Y,RailZ+1),FVector(X,Y==LandRail?Back:Y,H),1.15,1.35,Steel);
   Box(FVector(X,Y,RailZ+1.1),FVector(1.3,1.5,.18),FLinearColor(.11,.065,.035));
   for(double Z:{RailZ+9,RailZ+18,H-1})Box(FVector(X,FMath::Lerp(Y,Y==LandRail?Back:Y,(Z-RailZ-1)/(H-RailZ-1)),Z),FVector(1.25,1.45,.28),Brace);
  }
  const double TopY=Y==LandRail?Back:Y;Beam(FVector(-7,TopY,H),FVector(7,TopY,H),1.2,1.6,Steel);
  // ref4: open truck portal,14m transverse width; all bracing begins above
  // the9m access envelope. The landward legs rake5m over their full height.
  auto At=[&](double X,double Z){return FVector(X,FMath::Lerp(Y,TopY,(Z-RailZ-1)/(H-RailZ-1)),Z);};
  for(double Z:{RailZ+9,RailZ+18,H-1})Beam(At(-7,Z),At(7,Z),.65,.85,Steel);
  for(int Sign:{-1,1})Beam(At(Sign*7,RailZ+9),At(-Sign*7,H-1),.48,.55,Brace);
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
   const FVector Bottom(Sign*8,Y,RailZ+1),Top(Sign*8,Y==LandRail?Back:Y,H+3);
   for(int Side:{-1,1})Beam(Bottom+FVector(0,Side*.45,0),Top+FVector(0,Side*.45,0),.055,.055,Safety);
   const int Rungs=FMath::CeilToInt((Top-Bottom).Size()/.3);
   for(int I=0;I<=Rungs;++I){const FVector P=FMath::Lerp(Bottom,Top,double(I)/Rungs);Beam(P-FVector(0,.45,0),P+FVector(0,.45,0),.04,.04,Safety);}
  }
 }
 Beam(FVector(-.6,SeaRail+2,H+15),FVector(.6,SeaRail+2,H+15),.5,.5,Steel);
 Box(FVector(0,Back+5,H+4.6),FVector(13,14,9),Steel);
 for(int Sign:{-1,1})
 {
  for(double Z:{RailZ+9,RailZ+18,H-1})Beam(FVector(Sign*7,LandRail,Z),FVector(Sign*7,SeaRail,Z),.6,.75,Steel);
  Beam(FVector(Sign*7,LandRail,RailZ+9),FVector(Sign*7,SeaRail,H-1),.45,.5,Brace);
  Beam(FVector(Sign*7,SeaRail,RailZ+9),FVector(Sign*7,Back,H-1),.45,.5,Brace);
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
 // One separate grab component owns shells, rods, ropes and moving carriage.
 // Landside tapered receiving hopper and conveyor under the portal.
 const double HopperY=(SeaRail+LandRail)*.5,TopZ=RailZ+15,LowZ=RailZ+7;
 // ref4 lattice receiving-hopper tower: four posts, three platform rings,
 // braced bays and a narrow chute. It is physical geometry for scan occlusion.
 for(int SX:{-1,1})for(int SY:{-1,1})Beam(FVector(SX*5.8,HopperY+SY*4.8,RailZ+1),FVector(SX*5.8,HopperY+SY*4.8,TopZ+3),.32,.32,Brace);
 for(double Z:{RailZ+6,RailZ+12,TopZ+3})for(int Sign:{-1,1})
 {
  Beam(FVector(-5.8,HopperY+Sign*4.8,Z),FVector(5.8,HopperY+Sign*4.8,Z),.3,.4,Steel);
  Beam(FVector(Sign*5.8,HopperY-4.8,Z),FVector(Sign*5.8,HopperY+4.8,Z),.3,.4,Steel);
  Beam(FVector(-6.3,HopperY+Sign*5.3,Z+1.1),FVector(6.3,HopperY+Sign*5.3,Z+1.1),.055,.055,Safety);
  for(double X=-6.3;X<6.4;X+=2.1)Beam(FVector(X,HopperY+Sign*5.3,Z),FVector(X,HopperY+Sign*5.3,Z+1.1),.05,.05,Safety);
 }
 for(double Z:{RailZ+1,RailZ+6,RailZ+12})for(int Sign:{-1,1})
 {
  const double Next=Z==RailZ+1?RailZ+6:Z==RailZ+6?RailZ+12:TopZ+3;
  for(int Diag:{-1,1})
  {
   Beam(FVector(Diag*5.8,HopperY+Sign*4.8,Z),FVector(-Diag*5.8,HopperY+Sign*4.8,Next),.14,.14,Brace);
   Beam(FVector(Sign*5.8,HopperY+Diag*4.8,Z),FVector(Sign*5.8,HopperY-Diag*4.8,Next),.14,.14,Brace);
  }
 }
 // External switchback stair:180mm rise/300mm going, four flights,
 // yellow handrails and landings, clear of the central truck portal.
 for(int Flight=0;Flight<4;++Flight)
 {
  const int Direction=Flight%2?-1:1;const double Y0=LandRail+2+(Direction<0?10.8:0),Z0=RailZ+Flight*6.48;
  const FVector A(8.5,Y0,Z0),B(8.5,Y0+Direction*10.8,Z0+6.48);
  for(int Side:{-1,1}){Beam(A+FVector(Side*.65,0,0),B+FVector(Side*.65,0,0),.14,.24,Steel);Beam(A+FVector(Side*.65,0,1.1),B+FVector(Side*.65,0,1.1),.055,.055,Safety);}
  for(int Step=1;Step<=36;++Step){const auto P=FMath::Lerp(A,B,Step/36.);Box(P,FVector(1.3,.3,.08),Steel);if(Step%6==0)for(int Side:{-1,1})Beam(P+FVector(Side*.65,0,0),P+FVector(Side*.65,0,1.1),.05,.05,Safety);}
  Box(B+FVector(0,Direction*.7,-.1),FVector(1.8,1.5,.2),Steel);
 }
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
  if(!HatchUnloaderSamePositions(Existing,V))
  {
  M->UpdateMeshSection_LinearColor(1,V,N,TArray<FVector2D>(),C,TArray<FProcMeshTangent>());
  // Transient query meshes have no serialized cooked payload. Avoid retaining
  // a new editor DDC blob for each regenerated cargo/hoist position.
  if(auto* Body=M->GetBodySetup()){Body->bHasCookedCollisionData=false;Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();M->RecreatePhysicsState();}
  }
 }
 else M->CreateMeshSection_LinearColor(1,V,T,N,TArray<FVector2D>(),C,TArray<FProcMeshTangent>(),true);
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
 {auto* MID=Cast<UMaterialInstanceDynamic>(M->GetMaterial(1));if(!MID||MID->Parent!=Base){MID=UMaterialInstanceDynamic::Create(Base,UnloaderPresentation);MID->SetScalarParameterValue(TEXT("Hardware"),1);M->SetMaterial(1,MID);}}
 BuildGrabModel();
}

UPrimitiveComponent* AShipHatchSimulation::GrabPrimitive() const{return GrabPresentation;}
