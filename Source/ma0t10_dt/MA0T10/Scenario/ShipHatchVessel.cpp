#include "ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "Engine/World.h"

namespace
{
struct FVesselMesh
{
 TArray<FVector> V;TArray<int32> T;TArray<FLinearColor> C;
 void Quad(FVector A,FVector B,FVector D,FVector E,FLinearColor Color)
 {const int N=V.Num();for(auto P:{A,B,D,E}){V.Add(P*100);C.Add(Color);}T.Append({N,N+2,N+1,N,N+3,N+2});}
 void Beam(FVector A,FVector B,double W,double D,FLinearColor Color)
 {
  const FVector Z=(B-A).GetSafeNormal(),X=FVector::CrossProduct(FMath::Abs(Z.Z)>.95?FVector::RightVector:FVector::UpVector,Z).GetSafeNormal(),Y=FVector::CrossProduct(Z,X);
  const int Base=V.Num();for(int K=0;K<2;++K)for(int J=0;J<2;++J)for(int I=0;I<2;++I){V.Add(((K?B:A)+X*(I-.5)*W+Y*(J-.5)*D)*100);C.Add(Color);}
  const int Idx[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
  for(int I=0;I<36;I+=3)T.Append({Base+Idx[I],Base+Idx[I+2],Base+Idx[I+1]});
 }
 void Box(FVector P,FVector Size,FLinearColor Color){Beam(P-FVector(0,0,Size.Z*.5),P+FVector(0,0,Size.Z*.5),Size.X,Size.Y,Color);}
 void Commit(UProceduralMeshComponent* M,int Index,bool Collision)
 {
  TArray<FVector> N;N.Init(FVector::ZeroVector,V.Num());for(int I=0;I<T.Num();I+=3){const auto Normal=FVector::CrossProduct(V[T[I+2]]-V[T[I]],V[T[I+1]]-V[T[I]]);for(int J=0;J<3;++J)N[T[I+J]]+=Normal;}for(auto& A:N)A.Normalize();
  M->CreateMeshSection_LinearColor(Index,V,T,N,TArray<FVector2D>(),C,TArray<FProcMeshTangent>(),Collision);
 }
};
}
AShipHatchVessel::AShipHatchVessel()
{
 PrimaryActorTick.bCanEverTick=false;Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VesselGeometry"));SetRootComponent(Mesh);
 Mesh->bUseComplexAsSimpleCollision=true;Mesh->SetCollisionProfileName(TEXT("BlockAll"));Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Mesh->SetCastShadow(false);Mesh->SetCanEverAffectNavigation(false);
 Optical=CreateDefaultSubobject<UVirtualLidarSurfaceResponseComponent>(TEXT("Optical"));Optical->bTwoSided=true;Optical->Reflectivity940Nm=.3;
 ScaleMarker=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ScaleReference180cm"));ScaleMarker->SetupAttachment(Mesh);ScaleMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);ScaleMarker->SetHiddenInSceneCapture(true);ScaleMarker->SetCastShadow(false);ScaleMarker->SetCanEverAffectNavigation(false);ScaleMarker->SetVisibility(false);
 Tags.Add(TEXT("HatchOutside"));Tags.Add(TEXT("ShipHatchVessel"));
}
bool AShipHatchSimulation::ValidateSceneScale(FString& Error) const
{
 for(double V:{VesselLength,VesselBeam,MouldedDepth,DesignDraft,RailSpan,BerthGap,QuayAboveWater,BoomLength,BoomHeight,CabHeight})if(!FMath::IsFinite(V)){Error=TEXT("선박/하역기 치수는 유한수여야 합니다.");return false;}
 if(VesselLength<290||VesselLength>300||VesselBeam<45||VesselBeam>47||MouldedDepth<24||MouldedDepth>25||DesignDraft<17||DesignDraft>19||
    RailSpan<22||RailSpan>30||BerthGap<2||BerthGap>5||QuayAboveWater<2||QuayAboveWater>5||VesselHolds!=9||HoldWidth>VesselBeam-1||
    MouldedDepth-HoldDepth<1||MouldedDepth-HoldDepth>3||BoomLength<35||BoomLength>45||BoomHeight<18||BoomHeight>35||CabHeight<12||CabHeight>24||
    SeaRailY()>=QuayEdgeY()||BoomTipY()<=0||BoomTipY()>=VesselBeam*.5)
 {Error=TEXT("케이프사이즈/그랩 하역기 제원 범위를 확인하세요. 선박290–300×45–47×24–25m, 레일22–30m, 아웃리치35–45m.");return false;}
 Error.Reset();return true;
}
void AShipHatchSimulation::ShowScaleReference(bool Enabled){if(Vessel)Vessel->ScaleMarker->SetVisibility(Enabled);}
bool AShipHatchSimulation::IsScaleReferenceVisible() const{return Vessel&&Vessel->ScaleMarker->IsVisible();}
void AShipHatchSimulation::ShowWater(bool Enabled){bWaterVisible=Enabled;if(Vessel)Vessel->Mesh->SetMeshSectionVisible(2,Enabled);}
void AShipHatchSimulation::BuildVessel()
{
 if(!bRealScale){if(Vessel)Vessel->Destroy();Vessel=nullptr;return;}
 if(!Vessel){FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;Vessel=GetWorld()->SpawnActor<AShipHatchVessel>(AShipHatchVessel::StaticClass(),GetActorTransform(),P);}if(!Vessel)return;
 const double Half=VesselLength*.5,Deck=HoldDepth,Keel=KeelZ(),Water=WaterZ();
 const FLinearColor Red(.22,.035,.025),Black(.025,.035,.042),White(.57,.6,.59),DeckColor(.30,.15,.135),Cover(.46,.27,.25),Glass(.012,.035,.047),Steel(.22,.09,.065),Rope(.35,.29,.18);
 auto Breadth=[&](double X){if(X>Half-34)return VesselBeam*.5*FMath::Max(.018,FMath::Sqrt(FMath::Clamp((Half-X)/34.,0.,1.)));if(X<-Half+30){const double U=FMath::Clamp((X+Half)/30.,0.,1.);return FMath::Lerp(8.,VesselBeam*.5,U*U*(3-2*U));}return VesselBeam*.5;};
 auto Sheer=[&](double X){return .75*FMath::Square(FMath::Clamp((FMath::Abs(X)-110)/(Half-110),0.,1.));};
 auto SectionPoint=[&](double X,double Z,double Factor,int Sign){const double Rake=X>Half-34?4*(X-(Half-34))/34*FMath::Clamp((Deck-Z)/(Deck-Water),0.,1.):0.;return FVector(X-Rake,Sign*Breadth(X)*Factor,Z+Sheer(X)*FMath::Clamp((Z-Water)/(Deck-Water),0.,1.));};
 TArray<double> Stations;for(double U:{-1.,-.985,-.96,-.93,-.85,-.79,-.7,-.5,-.3,-.116,.116,.3,.5,.7,.79,.85,.90,.94,.97,.99,1.})Stations.Add(U*Half);
 for(int Sign:{-1,1})for(double X=Half-32;X<Half-2;X+=3)Stations.Add(Sign*X);Stations.Sort();
 FVesselMesh Hull;const double Levels[]={Keel,Keel+2,Water-.2,Water,Water+.15,Deck},Factors[]={.72,.84,.97,.98,.98,1};
 for(int I=1;I<Stations.Num();++I)for(int Sign:{-1,1})for(int J=1;J<6;++J)
 {
  const auto A=SectionPoint(Stations[I-1],Levels[J-1],Factors[J-1],Sign),B=SectionPoint(Stations[I],Levels[J-1],Factors[J-1],Sign),C=SectionPoint(Stations[I],Levels[J],Factors[J],Sign),D=SectionPoint(Stations[I-1],Levels[J],Factors[J],Sign);
  const float Wear=.88f+.12f*FMath::Abs(FMath::Sin(I*2.73+Sign));const auto Color=(J<4?Red:J==4?White:Black)*Wear;if(Sign>0)Hull.Quad(B,A,D,C,Color);else Hull.Quad(A,B,C,D,Color);
 }
 for(int I=1;I<Stations.Num();++I)Hull.Quad(SectionPoint(Stations[I-1],Keel,.72,1),SectionPoint(Stations[I],Keel,.72,1),SectionPoint(Stations[I],Keel,.72,-1),SectionPoint(Stations[I-1],Keel,.72,-1),Red);
 Hull.Quad(SectionPoint(-Half,Keel,.72,1),SectionPoint(-Half,Keel,.72,-1),SectionPoint(-Half,Deck,1,-1),SectionPoint(-Half,Deck,1,1),Black);
 Hull.Quad(SectionPoint(Half,Keel,.72,-1),SectionPoint(Half,Keel,.72,1),SectionPoint(Half,Deck,1,1),SectionPoint(Half,Deck,1,-1),Black);
 // Raked stem above a submerged bulb. Its forward extremity stays at LOA/2.
 auto Bulb=[&](double A,double B){return FVector(Half-7+7*FMath::Cos(A)*FMath::Cos(B),4.5*FMath::Sin(A)*FMath::Cos(B),Keel+5+4*FMath::Sin(B));};
 for(int I=0;I<16;++I)for(int J=0;J<8;++J){const double A=2*PI*I/16,B=2*PI*(I+1)/16,C=-PI/2+PI*J/8,D=-PI/2+PI*(J+1)/8;Hull.Quad(Bulb(A,C),Bulb(B,C),Bulb(B,D),Bulb(A,D),Red);}
 Hull.Commit(Vessel->Mesh,0,true);
 FVesselMesh Fit;
 // The original operational deck owns its own aperture. Adjacent deck strips
 // meet it at exactly the same beam, with no opaque plate over the active hold.
 const double End=(HoldLength+8)*.5;
 TArray<double> DeckStations=Stations;DeckStations.Add(-End);DeckStations.Add(End);DeckStations.Sort();
 for(int I=1;I<DeckStations.Num();++I){const double A=DeckStations[I-1],B=DeckStations[I];if(A>=-End-1.e-5&&B<=End+1.e-5)continue;
  auto CenterZ=[&](double X){return Deck+Sheer(X)+.22*FMath::Clamp((FMath::Abs(X)-End)/8.,0.,1.);};
  for(int Sign:{-1,1}){const FVector P(A,0,CenterZ(A)),Q(B,0,CenterZ(B)),U(B,Sign*Breadth(B),Deck+Sheer(B)),W(A,Sign*Breadth(A),Deck+Sheer(A));if(Sign>0)Fit.Quad(P,Q,U,W,DeckColor);else Fit.Quad(Q,P,W,U,DeckColor);}}
 const double Pitch=27.;for(int I=-4;I<=4;++I)if(I)
 {
  const double X=I*Pitch;for(int Sign:{-1,1}){Fit.Box(FVector(X+Sign*(OpeningLength+.3)*.5,0,Deck+CoamingHeight*.5),FVector(.3,OpeningWidth+.6,CoamingHeight),Steel);Fit.Box(FVector(X,Sign*(OpeningWidth+.3)*.5,Deck+CoamingHeight*.5),FVector(OpeningLength,.3,CoamingHeight),Steel);}
  for(int Sign:{-1,1})Fit.Box(FVector(X,Sign*OpeningWidth*.25,Deck+CoamingHeight+.16),FVector(OpeningLength,OpeningWidth*.5,.32),Cover);
  for(int Rib=-4;Rib<=4;++Rib)Fit.Box(FVector(X+Rib*2,0,Deck+CoamingHeight+.36),FVector(.12,OpeningWidth,.08),Steel);
 }
 // ref4 cross-deck fittings: compact vent houses, capped vents and a pair of
 // mooring winches. Between hatches only; no fitting covers an aperture.
 for(int I=-4;I<4;++I)
 {
  const double X=(I+.5)*Pitch;
  for(int Sign:{-1,1})
  {
   Fit.Box(FVector(X,Sign*15.2,Deck+.7),FVector(1.5,2.2,1.4),Cover);
   Fit.Box(FVector(X,Sign*15.2,Deck+1.45),FVector(1.8,2.5,.14),White*.65f);
   Fit.Box(FVector(X,Sign*19.5,Deck+.85),FVector(.55,.55,1.7),Steel);
   Fit.Box(FVector(X+.3,Sign*19.5,Deck+1.65),FVector(1.1,.8,.3),Cover);
   Fit.Box(FVector(X,Sign*21.5,Deck+.45),FVector(1.8,.9,.6),Black);
   for(double Offset:{-.5,.5})Fit.Box(FVector(X+Offset,Sign*21.5,Deck+.85),FVector(.6,1.1,.6),Steel);
  }
 }
 // Two side-rolling panels travel only along hold breadth (+/-Y). The
 // configured47m beam leaves35cm outside each fully open11.5m panel.
 FVesselMesh Covers;
 const double PanelWidth=OpeningWidth*.5,ParkY=OpeningWidth*.75+.15,PanelZ=Deck+CoamingHeight+.56;
 // First16 vertices are the two actual panel boxes, for independent geometry QA.
 for(int Sign:{-1,1})Covers.Box(FVector(0,Sign*ParkY,PanelZ),FVector(OpeningLength+.3,PanelWidth,.36),Cover);
 for(int Sign:{-1,1})
 {
  for(int Rib=-4;Rib<=4;++Rib)Covers.Box(FVector(Rib*2.3,Sign*ParkY,PanelZ+.23),FVector(.12,PanelWidth,.10),Steel);
  for(int PanelEnd:{-1,1})for(double Offset:{-PanelWidth*.3,PanelWidth*.3})
  {
   const FVector P(PanelEnd*(OpeningLength*.5+.15),Sign*ParkY+Offset,Deck+CoamingHeight+.21);const int B=Covers.V.Num();
   for(int Side=0;Side<2;++Side)for(int I=0;I<8;++I){const double U=2*PI*I/8;Covers.V.Add((P+FVector((Side-.5)*.16,.18*FMath::Cos(U),.18*FMath::Sin(U)))*100);Covers.C.Add(Black);}
   for(int I=0;I<8;++I){const int J=(I+1)%8;Covers.T.Append({B+I,B+J,B+8+J,B+I,B+8+J,B+8+I});}
  }
 }
 for(int PanelEnd:{-1,1})
 {
  const double X=PanelEnd*(OpeningLength*.5+.15),RailLength=VesselBeam-.4;
  // Rail runs on top of the coaming, rather than sharing its upper face.
  Covers.Box(FVector(X,0,Deck+CoamingHeight+.02),FVector(.24,RailLength,.04),Steel);
  Covers.Box(FVector(X,0,Deck+CoamingHeight+.05),FVector(.18,RailLength,.02),White);
  for(int Sign:{-1,1})for(double Y:{OpeningWidth*.5+1,VesselBeam*.5-.7})Covers.Box(FVector(X,Sign*Y,Deck+CoamingHeight*.5),FVector(.2,.2,CoamingHeight),Steel);
 }
 Covers.Commit(Vessel->Mesh,3,true);
 // Five accommodation decks, bridge wings/windows, funnel and antenna mast.
 const double Aft=-Half+13;Fit.Box(FVector(Aft,0,Deck+3),FVector(20,30,6),White);
 for(int I=0;I<5;++I)
 {
  const double Z=Deck+6+(I+.5)*2.8;Fit.Box(FVector(Aft,0,Z),FVector(18-I,28-I*2,2.6),White);
  Fit.Box(FVector(Aft+(18-I)*.5+.025,0,Z+.35),FVector(.06,24-I*2,.9),Glass);
  for(int Sign:{-1,1})Fit.Box(FVector(Aft,Sign*((28-I*2)*.5+.025),Z+.35),FVector(12,.06,.9),Glass);
 }
 Fit.Box(FVector(Aft+3,0,Deck+21.5),FVector(22,36,3),White);Fit.Box(FVector(Aft+14.04,0,Deck+21.8),FVector(.08,34,1.1),Glass);
 Fit.Box(FVector(Aft-6,0,Deck+22),FVector(5,7,9),Black);Fit.Box(FVector(Aft-6,0,Deck+25.5),FVector(5.1,7.1,1),Red);
 Fit.Beam(FVector(Aft+5,0,Deck+23),FVector(Aft+5,0,Deck+31.5),.15,.15,White);Fit.Beam(FVector(Aft+5,-3,Deck+30),FVector(Aft+5,3,Deck+30),.1,.1,White);
 Fit.Box(FVector(Half-13,0,Deck+1),FVector(10,16,2),DeckColor);
 // 1.1m handrails, simple fairleads/bollards, mooring springs and fenders.
 for(int Sign:{-1,1})for(double X=-Half+12;X<Half-12;X+=10)
 {
  const double B=FMath::Min(X+10,Half-12),Y=Sign*(Breadth(X)-.5),YB=Sign*(Breadth(B)-.5);
  Fit.Beam(FVector(X,Y,Deck),FVector(X,Y,Deck+1.1),.055,.055,White);Fit.Beam(FVector(X,Y,Deck+1.1),FVector(B,YB,Deck+1.1),.055,.055,White);
  if(FMath::Abs(X)>Half-32)Fit.Box(FVector((X+B)*.5,(Y+YB)*.5,Deck+.4),FVector(B-X,.13,.8),Black);
 }
 Fit.Box(FVector(0,QuayEdgeY()-500,QuayZ()-.6),FVector(VesselLength+1000,1000,1.2),FLinearColor(.25,.24,.215));
 // Baked vertex variation: stained concrete slabs and dark rail wear, with
 // no texture assets, per-frame material work or extra mesh components.
 for(int IX=-22;IX<22;++IX)for(int IY=0;IY<18;++IY)
 {
   const double X=IX*8.,Y=QuayEdgeY()-IY*6.;const float Tone=.94f+.06f*FMath::Abs(FMath::Sin(IX*4.31+IY*7.13));
  const FLinearColor Concrete=FLinearColor(.40,.375,.33)*Tone;
  Fit.Quad(FVector(X+.035,Y-5.965,QuayZ()+.025),FVector(X+7.965,Y-5.965,QuayZ()+.025),FVector(X+7.965,Y-.035,QuayZ()+.025),FVector(X+.035,Y-.035,QuayZ()+.025),Concrete);
 }
 for(double Y:{SeaRailY(),SeaRailY()-RailSpan})
 {
  Fit.Box(FVector(0,Y,QuayZ()+.035),FVector(330,1.2,.025),FLinearColor(.15,.11,.075));
  Fit.Box(FVector(0,Y,QuayZ()+.11),FVector(330,.14,.18),FLinearColor(.11,.12,.12));
 }
 for(double X=-140;X<145;X+=8)for(int Sign:{-1,1})Fit.Box(FVector(X,Sign*(VesselBeam*.5-.08),Deck-.35),FVector(.15,.05,.7),Red*.6f);
 for(double X:{-120.,-80.,-40.,0.,40.,80.,120.})Fit.Box(FVector(X,QuayEdgeY()+.25,Water+1.6),FVector(2,.5,3.2),Black);
 for(double X:{-126.,-108.,108.,126.})
 {
  const double ShipY=-Breadth(X)+1.2;Fit.Box(FVector(X,ShipY,Deck+.4),FVector(1.8,1,.8),Steel);
  const double QX=X+(X<0?-8:8);Fit.Box(FVector(QX,QuayEdgeY()-2,QuayZ()+.45),FVector(1.6,1.2,.9),FLinearColor(.65,.4,.05));
  Fit.Beam(FVector(X,ShipY,Deck+.7),FVector(QX,QuayEdgeY()-2,QuayZ()+.7),.07,.07,Rope);
 }
 Fit.Commit(Vessel->Mesh,1,true);
 // Static water is presentation only; it never fabricates opaque LiDAR water.
 FVesselMesh Sea;for(int I=0;I<64;++I)
 {
  const double A=2*PI*I/64,B=2*PI*(I+1)/64;auto Inner=[&](double U){return FVector((Half+.5)*FMath::Cos(U),(VesselBeam*.5+.3)*FMath::Sin(U),Water);};auto Outer=[&](double U){return FVector(2000*FMath::Cos(U),2000*FMath::Sin(U),Water);};Sea.Quad(Inner(A),Outer(A),Outer(B),Inner(B),FLinearColor(.018,.075,.10));
 }Sea.Commit(Vessel->Mesh,2,false);Vessel->Mesh->SetMeshSectionVisible(2,bWaterVisible);
 FVesselMesh Scale;const FVector Origin(4,SeaRailY()+1,QuayZ()+.05);const FLinearColor Safety(.85,.5,.05),Dark(.035,.045,.05);
 Scale.Box(Origin+FVector(0,0,1.1),FVector(.4,.23,.65),Safety);Scale.Box(Origin+FVector(0,0,1.675),FVector(.23,.23,.25),Dark);
 for(int Sign:{-1,1}){Scale.Beam(Origin+FVector(Sign*.11,0,.8),Origin+FVector(Sign*.12,0,0),.13,.16,Dark);Scale.Beam(Origin+FVector(Sign*.25,0,1.4),Origin+FVector(Sign*.28,0,.8),.10,.12,Safety);}
 Scale.Beam(Origin+FVector(.6,0,0),Origin+FVector(.6,0,1.8),.045,.045,White);for(double Z:{0.,.6,1.2,1.8}){const double TickZ=FMath::Clamp(Z,.018,1.782);Scale.Beam(Origin+FVector(.48,0,TickZ),Origin+FVector(.72,0,TickZ),.035,.035,White);}
 Scale.Commit(Vessel->ScaleMarker,0,false);Vessel->ScaleMarker->SetVisibility(false);
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
 {auto* MID=UMaterialInstanceDynamic::Create(Base,Vessel);MID->SetScalarParameterValue(TEXT("Hardware"),1);for(int I=0;I<4;++I)Vessel->Mesh->SetMaterial(I,MID);Vessel->ScaleMarker->SetMaterial(0,MID);}
}

void AShipHatchSimulation::BuildCorrugatedWalls(AShipHatchPart* Part)
{
 if(!Part)return;FVesselMesh Walls;
 for(bool End:{false,true})for(int Sign:{-1,1})
 {
  const double Span=End?HoldWidth:HoldLength,Across=(End?HoldLength:HoldWidth)*.5;
  const int Bays=FMath::CeilToInt(Span/1.5);const double Pitch=Span/Bays;
  for(int I=0;I<Bays;++I)
  {
   const double Base=-Span*.5+I*Pitch,Steps[]={0.,.2,.6,.8,1.},Depth[]={.015,.265,.265,.015,.015};
   for(int J=0;J<4;++J)
   {
    auto P=[&](int K,double Z){return End?FVector(Sign*(Across-Depth[K]),Base+Steps[K]*Pitch,Z):FVector(Base+Steps[K]*Pitch,Sign*(Across-Depth[K]),Z);};
    const auto A=P(J,0),B=P(J+1,0),C=P(J+1,HoldDepth),D=P(J,HoldDepth);const FLinearColor Brown=J==1?FLinearColor(.11,.075,.05):FLinearColor(.075,.045,.028);
    if((End&&Sign>0)||(!End&&Sign<0))Walls.Quad(B,A,D,C,Brown);else Walls.Quad(A,B,C,D,Brown);
   }
  }
 }
 Walls.Commit(Part->Mesh,1,true);
 if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
 {auto* MID=UMaterialInstanceDynamic::Create(Base,Part);MID->SetScalarParameterValue(TEXT("Hardware"),1);Part->Mesh->SetMaterial(1,MID);}
}
