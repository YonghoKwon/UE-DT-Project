#include "CoilLoadingSimulation.h"
#include "CoilLoadingQuayLayout.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Algo/Reverse.h"
#include "Materials/MaterialInstanceDynamic.h"

void ACoilLoadingSimulation::BuildQuayPresentation()
{
    // All additions here are visual. Tanktop/box-hold/crane structural collision
    // remains in BuildScene; decorative geometry never enters sensor snapshots.
    const double Q=BaseQuayZ(),Side=Settings.QuaySide,Rail=QuayOffset(Settings.RailSetback),Mid=Settings.RailCenterY(),X=-8;
    auto* Cube=StructureCubeAsset?StructureCubeAsset.Get():LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    // Reuse the coil material's already supported instancing shader in uniform
    // CR mode. The plain slab material has no instanced-mesh permutation.
    TArray<UMaterialInterface*> Palette;auto* Surface=CoilSurfaceAsset.Get();if(!Surface)Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Coil/Materials/M_CoilSurface.M_CoilSurface"));
    const FLinearColor Swatches[]={FLinearColor(.14,.2,.25),FLinearColor(.4,.42,.42),FLinearColor(.92,.58,.025),FLinearColor(.2,.1,.04),FLinearColor(.29,.34,.38),FLinearColor(.07,.28,.12),FLinearColor(.8,.12,.025),FLinearColor(.025,.09,.15),FLinearColor(.84,.86,.85),FLinearColor(.035,.095,.13),FLinearColor(.82,.22,.055),FLinearColor(.095,.26,.17),FLinearColor(.065,.075,.08),FLinearColor(.52,.31,.19),FLinearColor(.055,.11,.23),FLinearColor(.55,.12,.085)};
    for(const auto Color:Swatches){auto* M=UMaterialInstanceDynamic::Create(Surface,this);M->SetVectorParameterValue(TEXT("ColdColor"),Color);M->SetScalarParameterValue(TEXT("ColdRolled"),1);M->SetScalarParameterValue(TEXT("Roughness"),.76);M->SetScalarParameterValue(TEXT("Metallic"),.08);Palette.Add(M);}
    TMap<FString,UInstancedStaticMeshComponent*> Groups;bool Shore=true;
    const auto Part=[&](FVector C,FVector Size,int32 Color,FRotator Rotation=FRotator(0,0,0),int32 Moving=0,UStaticMesh* Mesh=nullptr,bool Detail=true,const TCHAR* Category=TEXT(""))
    {
        if(!Mesh)Mesh=Cube;const FString Key=FString::Printf(TEXT("%s/%d/%d/%d/%s"),*Mesh->GetName(),Color,Moving,Detail,Category);auto*& B=Groups.FindOrAdd(Key);
        if(!B)
        {
            B=NewObject<UInstancedStaticMeshComponent>(this);B->SetupAttachment(Moving==1||Moving==2?PortalRoot.Get():Moving==3||Moving==4?GetRootComponent():Shore?ShoreRoot.Get():GetRootComponent());B->SetMobility(EComponentMobility::Movable);B->SetStaticMesh(Mesh);B->SetMaterial(0,Palette[Color]);B->SetCollisionEnabled(ECollisionEnabled::NoCollision);B->SetGenerateOverlapEvents(false);B->SetCastShadow(false);B->ComponentTags.Add(TEXT("CoilDisplayOnly"));B->ComponentTags.Add(TEXT("CoilQuayPresentation"));B->ComponentTags.Add(FName(*FString::Printf(TEXT("CoilPalette%d"),Color)));if(*Category)B->ComponentTags.Add(FName(Category));if(Detail)B->SetCullDistances(9000,15000);B->RegisterComponent();PresentationBatches.Add(B);Structure.Add(B);
            if(Moving==2)TrolleyDetails.Add(B);else if(Moving==3)HookDetails.Add(B);else if(Moving==4)HoistFalls=B;
        }
        B->AddInstance(FTransform(Rotation,C*100,Size),false);
    };
    const auto Beam=[&](FVector A,FVector B,double Width,int32 Color,int32 Moving=0,bool Detail=true,const TCHAR* Category=TEXT(""))
    {const FVector D=B-A;Part((A+B)*.5,{D.Size(),Width,Width},Color,D.Rotation(),Moving,nullptr,Detail,Category);};
    const auto Tube=[&](FVector A,FVector B,double Diameter,int32 Color,int32 Moving=0,bool Detail=true,const TCHAR* Category=TEXT(""))
    {const FVector D=B-A;Part((A+B)*.5,{Diameter,Diameter,D.Size()},Color,FRotationMatrix::MakeFromZ(D).Rotator(),Moving,Cylinder,Detail,Category);};
    const auto Railing=[&](FVector A,FVector B,int32 Moving=0)
    {
        for(double H:{.55,1.1})Tube(A+FVector(0,0,H),B+FVector(0,0,H),.055,2,Moving);
        const int32 N=FMath::Max(1,FMath::CeilToInt(FVector::Distance(A,B)/1.6));for(int32 I=0;I<=N;++I){const auto P=FMath::Lerp(A,B,double(I)/N);Tube(P,P+FVector(0,0,1.15),.065,2,Moving);}
        Beam(A+FVector(0,0,.12),B+FVector(0,0,.12),.10,2,Moving);
    };
    const auto TextSign=[&](FVector C,const FString& Text,double Height,FRotator Rotation=FRotator(0,0,0),bool Moving=false)
    {auto* T=NewObject<UTextRenderComponent>(this);T->SetupAttachment(Moving?PortalRoot.Get():Shore?ShoreRoot.Get():GetRootComponent());T->SetRelativeLocation(C*100);T->SetRelativeRotation(Rotation);T->SetText(FText::FromString(Text));T->SetWorldSize(Height*100);T->SetTextRenderColor(FColor(225,230,223));T->SetCollisionEnabled(ECollisionEnabled::NoCollision);T->SetCastShadow(false);T->SetCullDistance(18000);T->ComponentTags.Add(TEXT("CoilDisplayOnly"));T->ComponentTags.Add(TEXT("CoilQuayPresentation"));T->RegisterComponent();return T;};
    // Ref6 is the principal construction reference. Four tapered box legs,
    // multi-level tubular portal bracing and two four-wheel bogies per leg.
    // These render-only members surround the unchanged simple collision frame.
    FMeshDescription LegD;FStaticMeshAttributes LegA(LegD);LegA.Register();LegA.GetVertexInstanceUVs().SetNumChannels(1);const auto LegPG=LegD.CreatePolygonGroup();
    const auto LegFace=[&](FVector A,FVector B,FVector C,FVector D)
    {
        const FVector N=-FVector::CrossProduct(B-A,C-A).GetSafeNormal();TArray<FVertexInstanceID> Corners;
        for(const auto P:{A,B,C,D}){auto V=LegD.CreateVertex();LegA.GetVertexPositions()[V]=FVector3f(P*100);auto I=LegD.CreateVertexInstance(V);LegA.GetVertexInstanceNormals()[I]=FVector3f(N);LegA.GetVertexInstanceColors()[I]=FVector4f(1,1,1,1);LegA.GetVertexInstanceUVs().Set(I,0,FVector2f(P.X,P.Z));Corners.Add(I);}LegD.CreatePolygon(LegPG,Corners);
    };
    const FVector LB[]={FVector(-.5,-.5,-.5),FVector(-.5,.5,-.5),FVector(.5,.5,-.5),FVector(.5,-.5,-.5)};
    const FVector LT[]={FVector(-.34,-.34,.5),FVector(-.34,.34,.5),FVector(.34,.34,.5),FVector(.34,-.34,.5)};
    for(int I=0;I<4;++I)LegFace(LB[I],LB[(I+1)%4],LT[(I+1)%4],LT[I]);LegFace(LT[0],LT[1],LT[2],LT[3]);LegFace(LB[3],LB[2],LB[1],LB[0]);
    auto* LegMesh=NewObject<UStaticMesh>(this);LegMesh->GetStaticMaterials().Add(FStaticMaterial());UStaticMesh::FBuildMeshDescriptionsParams LegBuild;LegBuild.bFastBuild=true;LegBuild.bBuildSimpleCollision=false;LegMesh->BuildFromMeshDescriptions({&LegD},LegBuild);PresentationMeshes.Add(LegMesh);
    // Wheel housing side plates have real arched clearances; rail wheels have
    // a steel tread/flange/hub profile rather than a rubber cylinder silhouette.
    const auto VisualMesh=[&](FMeshDescription& D){auto* M=NewObject<UStaticMesh>(this);M->GetStaticMaterials().Add(FStaticMaterial());UStaticMesh::FBuildMeshDescriptionsParams P;P.bFastBuild=true;P.bBuildSimpleCollision=false;M->BuildFromMeshDescriptions({&D},P);PresentationMeshes.Add(M);return M;};
    const auto Polygon=[](FMeshDescription& D,FStaticMeshAttributes& A,FPolygonGroupID G,TArray<FVector> Points,FVector Desired)
    {
        FVector N=-FVector::CrossProduct(Points[1]-Points[0],Points[2]-Points[0]).GetSafeNormal();if(FVector::DotProduct(N,Desired)<0){Algo::Reverse(Points);N=-N;}
        TArray<FVertexInstanceID> Corners;for(auto P:Points){auto V=D.CreateVertex();A.GetVertexPositions()[V]=FVector3f(P*100);auto I=D.CreateVertexInstance(V);A.GetVertexInstanceNormals()[I]=FVector3f(N);A.GetVertexInstanceColors()[I]=FVector4f(1,1,1,1);A.GetVertexInstanceUVs().Set(I,0,FVector2f(P.X,P.Z));Corners.Add(I);}D.CreatePolygon(G,Corners);
    };
    FMeshDescription HousingD;FStaticMeshAttributes HA(HousingD);HA.Register();HA.GetVertexInstanceUVs().SetNumChannels(1);auto HG=HousingD.CreatePolygonGroup();
    TArray<FVector2D> Profile={{-1.25,.4}};
    for(double Axle:{-.72,.72})for(int I=0;I<=16;++I){const double Angle=PI*(1-double(I)/16);Profile.Add({Axle+.46*FMath::Cos(Angle),.57+.46*FMath::Sin(Angle)});}
    Profile.Append({FVector2D(1.25,.4),FVector2D(1.25,1.25),FVector2D(-1.25,1.25)});
    for(double SideFace:{-1.,1.})
    {
        for(double Face:{-.055,.055}){TArray<FVector> Points;for(auto P:Profile)Points.Add({P.X,SideFace*.68+Face,P.Y});Polygon(HousingD,HA,HG,Points,{0,FMath::Sign(Face),0});}
        for(int I=0;I<Profile.Num();++I){auto A=Profile[I],B=Profile[(I+1)%Profile.Num()];Polygon(HousingD,HA,HG,{{A.X,SideFace*.68-.055,A.Y},{B.X,SideFace*.68-.055,B.Y},{B.X,SideFace*.68+.055,B.Y},{A.X,SideFace*.68+.055,A.Y}},{B.Y-A.Y,0,A.X-B.X});}
    }
    auto* Housing=VisualMesh(HousingD);
    FMeshDescription WheelD;FStaticMeshAttributes WA(WheelD);WA.Register();WA.GetVertexInstanceUVs().SetNumChannels(1);auto WG=WheelD.CreatePolygonGroup();
    const FVector2D Rim[]={{.0,-.16},{.17,-.16},{.19,-.12},{.41,-.12},{.41,.08},{.45,.09},{.45,.14},{.17,.16},{0.,.16}};
    for(int Ring=0;Ring<8;++Ring)for(int I=0;I<64;++I)
    {const double A=I*2*PI/64,B=(I+1)*2*PI/64;const auto P=Rim[Ring],N=Rim[Ring+1];TArray<FVector> Points={{P.X*FMath::Cos(A),P.X*FMath::Sin(A),P.Y},{P.X*FMath::Cos(B),P.X*FMath::Sin(B),P.Y},{N.X*FMath::Cos(B),N.X*FMath::Sin(B),N.Y},{N.X*FMath::Cos(A),N.X*FMath::Sin(A),N.Y}};if(P.X==0)Points.RemoveAt(1);if(N.X==0)Points.RemoveAt(3);Polygon(WheelD,WA,WG,Points,{FMath::Cos(A),FMath::Sin(A),Ring==0?-1.:Ring==7?1.:0.});}
    auto* RailWheel=VisualMesh(WheelD);
    const double Sea=CoilQuay::GirderSea(Settings),Land=CoilQuay::GirderLand(Settings),Center=(Sea+Land)*.5,Length=FMath::Abs(Sea-Land);
    const auto Node=[&](double Sign,int RailIndex,double Z){return CoilQuay::LegNode(Settings,Sign,RailIndex,Z)+FVector(0,0,Q);};
    for(int RailIndex=0;RailIndex<2;++RailIndex)
    {
        const double Y=Rail+Side*RailIndex*Settings.RailGauge;
        for(double Sign:{-1.,1.})
        {
            const double LX=X+Sign*4;const FVector A=Node(Sign,RailIndex,1.4),B=Node(Sign,RailIndex,28),D=B-A;const FRotator Rotation=FRotationMatrix::MakeFromZ(D).Rotator();
            Part((A+B)*.5,{1.6,1.6,D.Size()},2,Rotation,1,LegMesh,false,TEXT("CoilPortalLeg"));
            Part({LX,Y,Q+1.65},{5.3,1.4,.52},2,FRotator(0,0,0),1,nullptr,false,TEXT("CoilEqualizer"));
            for(double End:{-1.,1.})
            {
                const double BX=LX+End*1.55;Part({BX,Y,Q},{1,1,1},2,FRotator(0,0,0),1,Housing,false,TEXT("CoilRailBogie"));Part({BX,Y,Q+1.27},{2.65,1.55,.12},2,FRotator(0,0,0),1);
                for(double Wheel:{-.72,.72})for(double WY:{-.59,.59})Part({BX+Wheel,Y+WY,Q+.57},{1,1,1},4,FRotator(0,0,WY<0?90:-90),1,RailWheel,true,TEXT("CoilBogieWheel"));
                Tube({BX,Y-.75,Q+1.13},{BX,Y+.75,Q+1.13},.33,4,1);
                Part({BX+.62,Y+Side*.99,Q+.85},{.65,.5,.55},4,FRotator(0,0,0),1);
                Tube({BX+.45,Y+Side*1.1,Q+.85},{BX+.45,Y+Side*1.85,Q+.85},.42,4,1,true,TEXT("CoilBogieMotor"));
                for(int Fin=0;Fin<6;++Fin)Part({BX+.45,Y+Side*(1.16+Fin*.11),Q+.85},{.49,.035,.49},4,FRotator(0,0,0),1);
                Part({BX+End*1.45,Y,Q+.75},{.30,.75,.65},12,FRotator(0,0,0),1,nullptr,true,TEXT("CoilBuffer"));
                for(int Stripe=0;Stripe<5;++Stripe)Part({BX+End*1.61,Y-.32+Stripe*.16,Q+.76},{.02,.11,.54},Stripe%2?12:2,FRotator(0,0,30),1);
                for(double WY:{-.24,.24})Part({BX,Y+WY,Q+.25},{.42,.12,.3},4,FRotator(0,0,0),1,nullptr,true,TEXT("CoilRailClamp"));Tube({BX,Y,Q+.30},{BX,Y,Q+.65},.13,4,1);
                Part({BX-End*.6,Y+Side*.82,Q+.25},{.35,.35,.4},2,FRotator(0,0,0),1,nullptr,true,TEXT("CoilStormAnchor"));
            }
            // Each face has its own outward, non-mirrored text basis.
            const FVector Up=Rotation.RotateVector(FVector::UpVector),Normal=Rotation.RotateVector(FVector::ForwardVector);
            for(double Face:{-1.,1.})
            {
                auto* Number=TextSign(Node(Sign,RailIndex,3.2)+Normal*Face*.80-Up*.35,TEXT("31"),.6,FRotationMatrix::MakeFromXZ(Normal*Face,Up).Rotator(),true);
                Number->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);Number->SetTextRenderColor(FColor(22,25,21));Number->ComponentTags.Add(TEXT("CoilEquipmentNumber"));
            }
        }
        for(double Z:{4.3,11.,19.,26.})Tube(Node(-1,RailIndex,Z),Node(1,RailIndex,Z),.32,2,1,false,TEXT("CoilPortalTube"));
        for(int Bay=0;Bay<3;++Bay){const double Low=4.3+Bay*7.2,High=Low+6.7;Tube(Node(-1,RailIndex,Low),Node(1,RailIndex,High),.27,2,1,false,TEXT("CoilPortalTube"));Tube(Node(1,RailIndex,Low),Node(-1,RailIndex,High),.27,2,1,false,TEXT("CoilPortalTube"));}
    }
    for(double Sign:{-1.,1.})
    {
        for(double Z:{6.,13.,20.})Tube(Node(Sign,0,Z),Node(Sign,1,Z),.36,2,1,false,TEXT("CoilPortalTube"));
        for(double Z:{6.,13.})for(int End=0;End<2;++End)Tube((Node(Sign,0,Z)+Node(Sign,1,Z))*.5,Node(Sign,End,Z+6),.30,2,1,false,TEXT("CoilPortalTube"));
        Beam(Node(Sign,0,26.4),Node(Sign,1,26.4),1.4,2,1,false);
        const double GX=X+Sign*CoilQuay::TopHalfX;
        Part({GX,Center,Q+28},{1.3,Length,1.7},2,FRotator(0,0,0),1,nullptr,false);
        for(double DZ:{-.9,.9})Part({GX,Center,Q+28+DZ},{1.85,Length,.20},2,FRotator(0,0,0),1,nullptr,false,TEXT("CoilGirderFlange"));
        for(double Along=1;Along<Length;Along+=1.8)for(double Face:{-1.,1.})Part({GX+Face*.69,Land-Side*Along,Q+28},{.12,.16,1.62},2,FRotator(0,0,0),1,nullptr,true,TEXT("CoilGirderStiffener"));
        for(double Face:{-1.,1.}){const double WalkX=GX+Face*1.1;Part({WalkX,Center,Q+29.12},{.72,Length,.10},4,FRotator(0,0,0),1);Railing({WalkX+Face*.37,Land,Q+29.2},{WalkX+Face*.37,Sea,Q+29.2},1);}
        Part({GX,Center,Q+29.06},{.14,Length,.18},4,FRotator(0,0,0),1,nullptr,false,TEXT("CoilTrolleyRail"));
        // Ref1/ref4 upper-left slender stays; no cantilever lower knee struts.
        const FVector Post=Node(Sign,0,28),Anchor=Post+(Post-Node(Sign,0,1.4)).GetSafeNormal()*6;
        Beam(Post,Anchor,.75,2,1,false,TEXT("CoilUpperStayPost"));Tube(Anchor,{GX,Sea,Q+28.9},.10,2,1,false,TEXT("CoilUpperStay"));
    }
    for(double Y:{Sea,Land}){Part({X,Y,Q+28},{2*CoilQuay::TopHalfX+1.8,.9,2.05},2,FRotator(0,0,0),1,nullptr,false,TEXT("CoilEndCarriage"));for(double DX:{-CoilQuay::TopHalfX,CoilQuay::TopHalfX})Part({X+DX,Y,Q+29.3},{.6,.4,.5},12,FRotator(0,0,0),1);}
    // Machinery house at girder end, with painted roof, door and ventilation.
    Part({X,Land+Side*.7,Q+30.6},{5.8,3.7,2.6},2,FRotator(0,0,0),1,nullptr,false,TEXT("CoilMachineryHouse"));Part({X,Land+Side*.7,Q+31.96},{6,3.9,.14},8,FRotator(0,0,0),1);
    for(int I=0;I<14;++I)Part({X-2.35+I*.34,Land-Side*1.17,Q+30.5},{.23,.035,1.35},12,FRotator(0,0,0),1);
    Part({X+2.92,Land+Side*.8,Q+30.5},{.035,.86,1.95},8,FRotator(0,0,0),1);Part({X+2.95,Land+Side*.55,Q+30.5},{.08,.08,.12},12,FRotator(0,0,0),1);
    // Ref6 zigzag stair tower lies outside the land rail, within the 5m apron.
    const auto StairPoint=[&](double Sign,double Z){return Node(Sign,1,Z)+FVector(Sign*.1,Side*2.15,0);};
    for(double Sign:{-1.,1.})for(double DY:{-.8,.8})Tube(StairPoint(Sign,.2)+FVector(0,DY,0),StairPoint(Sign,29.2)+FVector(0,DY,0),.13,2,1);
    for(int Flight=0;Flight<8;++Flight)
    {
        const double Z=.4+Flight*3.55,Direction=Flight%2?-1:1;const FVector A=StairPoint(-Direction,Z)+FVector(Direction*.75,0,0),B=StairPoint(Direction,Z+3.3)-FVector(Direction*.75,0,0);
        for(double DY:{-.58,.58})Beam(A+FVector(0,DY,0),B+FVector(0,DY,0),.15,4,1);
        for(int Step=0;Step<18;++Step)Part(FMath::Lerp(A,B,double(Step)/18),{.43,1.15,.075},4,FRotator(0,0,0),1,nullptr,true,TEXT("CoilStairTread"));
        for(double DY:{-.64,.64})Railing(A+FVector(0,DY,0),B+FVector(0,DY,0),1);
        Part(B,{1.4,1.7,.13},4,FRotator(0,0,0),1,nullptr,true,TEXT("CoilStairLanding"));Railing(B+FVector(-.65,-.85,0),B+FVector(.65,-.85,0),1);Railing(B+FVector(-.65,.85,0),B+FVector(.65,.85,0),1);
    }
    // Visible cable festoon: hangers ride an outer beam and carry slack loops.
    const double CableX=X-CoilQuay::TopHalfX-1.7;
    for(int I=0;I<17;++I)
    {
        const double Y=Land-Side*(I*1.7+.3);Tube({CableX,Y,Q+29.0},{CableX,Y,Q+28.25},.055,4,1);
        Part({CableX,Y,Q+29.05},{.20,.26,.18},4,FRotator(0,0,0),1);
        if(I<16)for(int Segment=0;Segment<8;++Segment)
        {
            const double A=double(Segment)/8,B2=double(Segment+1)/8;
            Tube({CableX,Y-Side*A*1.7,Q+28.25-1.0*FMath::Sin(PI*A)},{CableX,Y-Side*B2*1.7,Q+28.25-1.0*FMath::Sin(PI*B2)},.065,12,1,true,TEXT("CoilFestoonCable"));
        }
    }
    // Moving trolley: four rail wheels, open frame, winch drums and gearboxes.
    const double TY=PickupPosition().Y;
    for(double DY:{-.95,.95})Part({X,TY+DY,Q+29.45},{6.95,.28,.6},4,FRotator(0,0,0),2,nullptr,false);
    for(double DX:{-3.1,0.,3.1})Part({X+DX,TY,Q+29.45},{.3,2.2,.6},4,FRotator(0,0,0),2,nullptr,false);
    for(double DX:{-CoilQuay::TopHalfX,CoilQuay::TopHalfX})for(double DY:{-.8,.8})Part({X+DX,TY+DY,Q+29.29},{.48,.48,.24},12,FRotator(0,0,90),2,Cylinder,false,TEXT("CoilTrolleyWheel"));
    for(double DY:{-.48,.48})
    {
        Tube({X-1.35,TY+DY,Q+30.05},{X+1.35,TY+DY,Q+30.05},.85,4,2,false,TEXT("CoilWinchDrum"));
        for(double DX:{-1.42,1.42})Part({X+DX,TY+DY,Q+30.05},{1.0,1.0,.10},2,FRotator(0,90,90),2,Cylinder);
        Part({X+2,TY+DY,Q+30.02},{.85,.65,.8},2,FRotator(0,0,0),2,nullptr,false);Tube({X+2.4,TY+DY,Q+30.02},{X+3.25,TY+DY,Q+30.02},.48,4,2);
        for(int Fin=0;Fin<6;++Fin)Part({X+2.45+Fin*.12,TY+DY,Q+30.02},{.035,.58,.58},4,FRotator(0,0,0),2);
    }
    for(double DX:{-.3,.3})for(double DY:{-.23,.23})Part({X+DX,TY+DY,Q+29.1},{.5,.5,.10},4,FRotator(0,0,90),2,Cylinder,true,TEXT("CoilTopSheave"));
    // Underhung glazed operator cab follows trolley, as in ref4/ref6.
    const FVector Cab(X+2.0,TY,Q+24.9);
    for(double DX:{-.95,.95})for(double DY:{-.8,.8})Tube(Cab+FVector(DX,DY,1.2),{Cab.X+DX,TY+DY,Q+29.3},.13,2,2,false);
    Part(Cab,{2.3,2.2,2.55},8,FRotator(0,0,0),2,nullptr,false,TEXT("CoilOperatorCab"));Part(Cab+FVector(0,0,1.35),{2.6,2.5,.18},2,FRotator(0,0,0),2);
    for(double Face:{-1.,1.})
    {
        Part(Cab+FVector(Face*1.16,0,.23),{.025,1.96,1.52},9,FRotator(0,0,0),2);
        Part(Cab+FVector(0,Face*1.11,.23),{2.06,.025,1.52},9,FRotator(0,0,0),2);
        for(double Split:{-.55,.55})Part(Cab+FVector(Face*1.18,Split,.23),{.055,.055,1.6},2,FRotator(0,0,0),2);
        for(double Z:{-.55,1.02})Part(Cab+FVector(Face*1.18,0,Z),{.055,2.03,.065},2,FRotator(0,0,0),2);
    }
    Part(Cab+FVector(0,1.65,-1.32),{2.8,1,.12},4,FRotator(0,0,0),2);Railing(Cab+FVector(-1.4,2.15,-1.25),Cab+FVector(1.4,2.15,-1.25),2);
    for(double DX:{-.45,.45})Tube(Cab+FVector(DX,1.65,-1.2),{Cab.X+DX,Cab.Y+1.65,Q+29.3},.07,2,2);
    for(double Z=Cab.Z-1.1;Z<Q+29.3;Z+=.3)Tube({Cab.X-.45,Cab.Y+1.65,Z},{Cab.X+.45,Cab.Y+1.65,Z},.055,4,2);
    // Six falls are drawn between the top sheaves and this multi-sheave block.
    HookVisualOrigin=PickupPosition()+FVector(0,0,Settings.GearHeadroom);
    for(double DX:{-.30,0.,.30})for(double DY:{-.23,.23})Tube({X+DX,TY+DY,Q+29.1},HookVisualOrigin+FVector(DX,DY,.25),.04,12,4,false,TEXT("CoilHoistFalls"));
    for(double DY:{-.29,.29})Part(HookVisualOrigin+FVector(0,DY,.18),{1.0,.10,.85},2,FRotator(0,0,0),3,nullptr,false,TEXT("CoilHookBlock"));
    for(double DX:{-.30,0.,.30})Part(HookVisualOrigin+FVector(DX,0,.25),{.48,.48,.42},4,FRotator(0,0,90),3,Cylinder);
    Tube(HookVisualOrigin+FVector(0,0,-.2),HookVisualOrigin+FVector(0,0,-.65),.16,4,3,false);
    // Solid C-hook yoke/prong supplements the adaptive live rig lines. Only the
    // selected lifting attachment is shown; physical lifting remains analytic.
    const double Drop=Settings.GearHeadroom,Jaw=NextSpec.Width*.5+.35;
    Part(HookVisualOrigin+FVector(-Jaw*.5,0,-.80),{Jaw+.2,.30,.32},2,FRotator(0,0,0),3,nullptr,false,TEXT("CoilCHook"));
    Part(HookVisualOrigin+FVector(-Jaw,0,-Drop*.5-.35),{.32,.36,Drop-.35},2,FRotator(0,0,0),3,nullptr,false,TEXT("CoilCHook"));
    Part(HookVisualOrigin+FVector(-.1,0,-Drop+.13),{NextSpec.Width+1,.28,.25},2,FRotator(0,0,0),3,nullptr,false,TEXT("CoilCHook"));
    for(double DX:{-.45,.45})Tube(HookVisualOrigin+FVector(0,0,-.55),HookVisualOrigin+FVector(DX,0,-Drop+.15),.075,4,3,false,TEXT("CoilSling"));
    // Ref2 yellow quay cradle, guarded access deck and stairs.
    const double CY=QuayOffset(8);
    Part({X,CY,Q+.55},{3.5,3.7,1.1},2,FRotator(0,0,0),0,nullptr,false);Part({X,CY,Q+1.12},{3.4,3.6,.12},4,FRotator(0,0,0),0,nullptr,false);
    Railing({X-1.75,CY-1.8,Q+1.2},{X+1.75,CY-1.8,Q+1.2});Railing({X-1.75,CY-1.8,Q+1.2},{X-1.75,CY+1.8,Q+1.2});
    for(int I=0;I<5;++I)Part({X+2.1+I*.3,CY,Q+.12+I*.22},{.32,1.1,.12},2);
    for(int I=0;I<12;++I)Part({X-1.6+I*.28,CY-1.87,Q+.58},{.13,.02,.5},I%2?12:2,FRotator(0,0,-30));
    // Ref3 alternating green/orange sheet cladding, ribs, open front and cradles.
    const double Shed=ShedCenterY(),Back=Shed+Side*5;
    for(int I=0;I<16;++I)
    {
        const double SX=-13+I*2;Part({SX,Back+Side*.2,Q+3.2},{1.95,.15,6.25},I%5==0?10:11,FRotator(0,0,0),0,nullptr,false);
        for(double Rib:{-.8,-.4,0.,.4,.8})Part({SX+Rib,Back+Side*.32,Q+3.2},{.055,.12,6.3},I%5==0?10:11);
        Part({SX,Shed-Side*5,Q+5.45},{1.95,.18,1.1},I%5==0?10:11,FRotator(0,0,0),0,nullptr,false);
    }
    for(double End:{-14.,18.}){Part({End,Shed,Q+3.15},{.18,10,6.3},10,FRotator(0,0,0),0,nullptr,false);for(int I=0;I<24;++I)Part({End-.1,Shed-4.8+I*.4,Q+3.2},{.12,.04,6.3},11);}
    for(double Y:{Shed-2.6,Shed+2.6})Part({2,Y,Q+6.28},{32,5.4,.14},11,FRotator(0,0,Y<Shed?-6:6),0,nullptr,false);
    for(int I=0;I<8;++I){const double SX=-10+I*3.2;Part({SX,Shed,Q+.35},{2.6,2.4,.7},2,FRotator(0,0,0),0,nullptr,false);for(double DY:{-1.1,1.1})Part({SX,Shed+DY,Q+.75},{2.4,.2,.3},2);}
    TextSign({-13,Shed-Side*5.15,Q+5.4},TEXT("COIL STORAGE  /  HR · CR"),.45,FRotator(0,90,0));
    // Quay bollards, fenders, lane paint and safety reference (1.8m standing).
    for(double SX=-60;SX<140;SX+=12)
    {
        Part({SX,QuayOffset(.8),Q+.25},{.5,.5,.5},12,FRotator(0,0,0),0,Cylinder);Part({SX,QuayEdgeY()-Side*.2,Q-1},{1.2,.6,1.8},12,FRotator(0,0,0),0,nullptr,false);
        Part({SX,QuayOffset(Settings.RailSetback+Settings.RailGauge+11),Q+.025},{5,.13,.015},8);Part({SX,QuayOffset(Settings.RailSetback+Settings.RailGauge+11)+Side*ServiceLaneWidth()*.5,Q+.025},{5,.13,.015},8);
    }
    const FVector Person(-2,QuayOffset(5),Q);
    const auto Human=[&](FVector C,FVector Size,int Color,UStaticMesh* Mesh=nullptr){Part(Person+C,Size,Color,FRotator(0,0,0),0,Mesh);};
    for(double DX:{-.13,.13}){Human({DX,0,.43},{.17,.21,.72},14,Cylinder);Human({DX,-.055,.08},{.21,.35,.14},12);}
    Human({0,0,1.13},{.48,.28,.59},10);Human({0,-.151,1.2},{.49,.025,.07},8);Human({0,-.151,1.02},{.49,.025,.07},8);Human({0,0,1.62},{.23,.23,.25},13,Sphere);Human({0,0,1.76},{.29,.29,.08},2,Sphere);Human({0,0,1.79},{.34,.32,.025},2);
    Beam(Person+FVector(-.23,0,1.37),Person+FVector(-.37,0,.96),.11,14);Beam(Person+FVector(.23,0,1.37),Person+FVector(.42,0,1.92),.11,14);Human({.42,0,1.98},{.12,.12,.16},13,Sphere);
    Shore=false;
    // Tapered general-cargo hull visual shell; primary measured box holds stay
    // unchanged. Published four-hold geared N-class proportions.
    const double VesselX=Settings.VesselCenterX(),L=Settings.LOA,B=Settings.Beam,H=Settings.WeatherDeckZ();
    FMeshDescription D;FStaticMeshAttributes Attr(D);Attr.Register();auto Pos=Attr.GetVertexPositions();auto Norm=Attr.GetVertexInstanceNormals();auto UV=Attr.GetVertexInstanceUVs();UV.SetNumChannels(1);const auto PG=D.CreatePolygonGroup();
    const auto Quad=[&](FVector A,FVector BB,FVector C,FVector E)
    {TArray<FVector> Points={A,BB,C,E};FVector N=-FVector::CrossProduct(BB-A,C-A).GetSafeNormal();const bool Deck=FMath::IsNearlyEqual(A.Z,H)&&FMath::IsNearlyEqual(BB.Z,H)&&FMath::IsNearlyEqual(C.Z,H);const FVector Desired=Deck?FVector::UpVector:FVector(0,FMath::Sign((A.Y+BB.Y+C.Y+E.Y)*.25),0);if(FVector::DotProduct(N,Desired)<0){Swap(Points[1],Points[3]);N=-N;}TArray<FVertexInstanceID> VI;for(const auto& P:Points){const auto V=D.CreateVertex();Pos[V]=FVector3f(P*100);const auto I=D.CreateVertexInstance(V);Norm[I]=FVector3f(N);Attr.GetVertexInstanceColors()[I]=FVector4f(1,1,1,1);UV.Set(I,0,FVector2f(P.X*.1,P.Z*.1));VI.Add(I);}D.CreatePolygon(PG,VI);};
    const double Station[]={-.5,-.47,-.40,-.31,.30,.39,.45,.49,.5};const double Breadth[]={.32,.7,.95,1.,1.,.93,.65,.25,0.};
    for(int I=0;I<8;++I)for(double S:{-1.,1.})
    {
        const double XA=VesselX+Station[I]*L,XB=VesselX+Station[I+1]*L,YA=S*B*.5*Breadth[I],YB=S*B*.5*Breadth[I+1];
        Quad({XA,YA,H},{XB,YB,H},{XB,YB*.7,-1.8},{XA,YA*.7,-1.8});
        // Deck strips outside the unchanged rectangular hold openings.
        TArray<FVector2D> DeckSpans;DeckSpans.Add({VesselX-L*.5,-Settings.HoldLength*.5});
        for(int Gap=0;Gap<Settings.HoldCount-1;++Gap)DeckSpans.Add({Settings.HoldCenterX(Gap)+Settings.HoldLengthAt(Gap)*.5,Settings.HoldCenterX(Gap+1)-Settings.HoldLengthAt(Gap+1)*.5});
        DeckSpans.Add({Settings.HoldCenterX(Settings.HoldCount-1)+Settings.HoldLengthAt(Settings.HoldCount-1)*.5,VesselX+L*.5});
        for(const auto Span:DeckSpans){const double A=FMath::Max(XA,Span.X),B2=FMath::Min(XB,Span.Y);if(B2>A){const double W1=FMath::Lerp(YA,YB,(A-XA)/(XB-XA)),W2=FMath::Lerp(YA,YB,(B2-XA)/(XB-XA));Quad({A,0,H},{B2,0,H},{B2,W2,H},{A,W1,H});}}
    }
    auto* Hull=NewObject<UStaticMesh>(this);Hull->bAllowCPUAccess=true;UStaticMesh::FBuildMeshDescriptionsParams Build;Build.bBuildSimpleCollision=false;Build.bFastBuild=true;Hull->BuildFromMeshDescriptions({&D},Build);PresentationMeshes.Add(Hull);
    auto* Shell=NewObject<UStaticMeshComponent>(this);Shell->SetupAttachment(GetRootComponent());Shell->SetStaticMesh(Hull);Shell->SetMaterial(0,Materials[0]);Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);Shell->SetCastShadow(false);Shell->ComponentTags.Add(TEXT("CoilDisplayOnly"));Shell->RegisterComponent();Structure.Add(Shell);
    // Only the working aft hold is open. Ref4 folding panels stand beyond
    // both ends of its opening; other hatch covers are closed in BuildScene.
    for(int Hold=0;Hold<Settings.HoldCount;++Hold)
    {
        const double HX=Settings.HoldCenterX(Hold),HL=Settings.HoldLengthAt(Hold);
        for(double S:{-1.,1.})Part({HX,S*(Settings.HoldWidth+Settings.Beam)*.25,H-.06},{HL,(Settings.Beam-Settings.HoldWidth)*.5,.12},11,FRotator(0,0,0),0,nullptr,false);
        for(double S:{-1.,1.})Railing({HX-HL*.5,S*(B*.5-.25),H+.05},{HX+HL*.5,S*(B*.5-.25),H+.05});
    }
    if(Settings.bFoldingCovers)for(double End:{-1.,1.})for(int Panel=0;Panel<2;++Panel)
    {
        const double PH=Settings.OpeningLength*.25,PX=End*(Settings.OpeningLength*.5+.48+Panel*.48),Z=Settings.HoldHeight+PH*.5;
        Part({PX,0,Z},{.30,Settings.OpeningWidth,PH},8,FRotator(0,0,0),0,nullptr,false,TEXT("CoilStandingCoverPanel"));
        for(double Y=-Settings.OpeningWidth*.5+.5;Y<Settings.OpeningWidth*.5;Y+=1.2)Part({PX+End*.20,Y,Z},{.14,.13,PH-.2},8);
        for(double Edge:{-1.,1.})
        {
            const double Y=Edge*(Settings.OpeningWidth*.5-.25);
            Tube({PX-.3,Y,Settings.HoldHeight+.2},{PX+.3,Y,Settings.HoldHeight+.2},.22,4,0,true,TEXT("CoilCoverHinge"));
            Tube({PX,Y-.20,Z+PH*.5-.25},{PX,Y+.20,Z+PH*.5-.25},.18,4,0,true,TEXT("CoilCoverLug"));
            if(Panel==0){const FVector Foot(PX+End*1.2,Y,H+.2),Eye(PX,Y,Settings.HoldHeight+1.7);Tube(Foot,FMath::Lerp(Foot,Eye,.65),.22,4,0,true,TEXT("CoilCoverCylinder"));Tube(FMath::Lerp(Foot,Eye,.55),Eye,.10,8);}
        }
        Tube({PX, -Settings.OpeningWidth*.5,Settings.HoldHeight+PH-.1},{PX,Settings.OpeningWidth*.5,Settings.HoldHeight+PH-.1},.15,8);
    }
    // Closed hydraulic covers retain one simple collision slab per hold. Panel
    // seams, edge compression bars and cleats are visible, display-only details.
    for(int Hold=1;Hold<Settings.HoldCount;++Hold)
    {
        const double CX=Settings.HoldCenterX(Hold),HL=Settings.HoldLengthAt(Hold),CZ=Settings.HoldHeight+.32;
        for(int Panel=1;Panel<4;++Panel)Part({CX-HL*.5+HL*Panel/4,0,CZ},{.07,Settings.HoldWidth,.035},4,FRotator(0,0,0),0,nullptr,true,TEXT("CoilClosedCoverSeam"));
        for(double S:{-1.,1.})for(double A=1;A<HL;A+=2.5)
        {const double SX=CX-HL*.5+A,Y=S*(Settings.HoldWidth*.5+.12);Part({SX,Y,Settings.HoldHeight+.12},{.28,.32,.15},4,FRotator(0,0,0),0,nullptr,true,TEXT("CoilCoverCleat"));Tube({SX-.16,Y,Settings.HoldHeight+.15},{SX+.16,Y,Settings.HoldHeight+.15},.055,12);}
    }
    // Three white/light-grey hydraulic deck cranes, stowed fore/aft outside
    // the swept opening. Dedicated neutral paint avoids pink cover reflections.
    if(Settings.bDeckCranes)for(int Gap=0;Gap<Settings.HoldCount-1;++Gap)
    {
        const double CX=Settings.HoldCenterX(Gap)+Settings.HoldLengthAt(Gap)*.5+Settings.HoldGap*.5,DeckY=B*.5-.8;
        Part({CX,DeckY,H+2.5},{1.9,1.9,5},8,FRotator(0,0,0),0,Cylinder,false,TEXT("CoilDeckCranePost"));
        Part({CX,DeckY,H+5.2},{2.3,2.3,.4},4,FRotator(0,0,0),0,Cylinder);Part({CX,DeckY,H+6.5},{2.4,2,2.4},8,FRotator(0,0,0),0,nullptr,false);
        const FVector Pivot(CX+.6,DeckY,H+7),Tip(CX+15,DeckY,H+7.7);
        for(double DY:{-.25,.25})Beam(Pivot+FVector(0,DY,0),Tip+FVector(0,DY,0),.24,8,0,false,TEXT("CoilDeckCraneJib"));
        Beam(Pivot+FVector(0,0,.6),Tip+FVector(0,0,.16),.28,8,0,false);for(int Rib=1;Rib<8;++Rib){const double T=double(Rib)/8;Beam(FMath::Lerp(Pivot,Tip,T)+FVector(0,0,-.12),FMath::Lerp(Pivot,Tip,T)+FVector(0,0,.6*(1-T)),.12,8);}
        Tube({CX-.4,DeckY,H+9.1},Tip,.045,12);Tube({CX-.4,DeckY,H+9.1},{CX+.6,DeckY,H+7},.045,12);
        Tube({CX+1.4,DeckY,H+5.5},{CX+5.8,DeckY,H+7.1},.24,4);Tube({CX+5.2,DeckY,H+6.9},{CX+8,DeckY,H+7.35},.12,8);
        Part({CX+.5,DeckY-.95,H+6.5},{1.8,.05,1.25},9);Part({CX+1.43,DeckY-.25,H+6.5},{.05,1.25,1.25},9);
        for(double DX:{-.4,1.4})Part({CX+DX,DeckY-1,H+6.5},{.07,.07,1.4},8);Part({CX+.5,DeckY-1,H+7.18},{1.9,.07,.07},8);
        Part(Tip+FVector(0,0,-.28),{.65,.55,.75},8);Tube(Tip+FVector(0,0,-.4),{Tip.X,DeckY,H+1.2},.04,12);Part({Tip.X,DeckY,H+1},{.7,.7,.2},8);
        for(int Rung=0;Rung<12;++Rung)Tube({CX-.9,DeckY-.35,H+.3+Rung*.4},{CX-.9,DeckY+.35,H+.3+Rung*.4},.045,8);
    }
    // Aft accommodation floors, wheelhouse windows, funnel and antennas.
    const double House=-Settings.HoldLength*.5-7;
    for(int Deck=0;Deck<4;++Deck)
    {
        const double Z=H+1+Deck*2.6;Part({House,0,Z},{11.5-Deck*.65,14-Deck*.5,2.5},8,FRotator(0,0,0),0,nullptr,false);Part({House,0,Z+1.3},{12.3-Deck*.65,14.7-Deck*.5,.18},8,FRotator(0,0,0),0,nullptr,false);
        for(double SY:{-1.,1.})for(int Window=0;Window<7;++Window)Part({House-4.5+Window*1.35,SY*(7-Deck*.25+.02),Z+.25},{.8,.035,.75},9);
    }
    Part({House+4,0,H+9.1},{.04,11.3,1.35},9,FRotator(0,0,0),0,nullptr,false);Part({House-2,1.5,H+12.4},{2.8,2.5,4},6,FRotator(0,0,0),0,nullptr,false);Part({House-2,1.5,H+14.5},{3.1,2.8,.7},12,FRotator(0,0,0),0,nullptr,false);
    Beam({House+1,-2,H+11},{House+1,-2,H+16},.15,8);Beam({House-1,-2,H+15},{House+3,-2,H+15},.1,8);
    TextSign({VesselX+L*.43,-B*.34,H-2.5},TEXT("COIL DEMONSTRATOR"),.8,FRotator(0,-90,0));
    TextSign(Person+FVector(-1,-.5,.15),TEXT("1.8 m"),.22,FRotator(0,-90,0));
}
void ACoilLoadingSimulation::ApplyOwnedTransform(USceneComponent* C,FVector Location,FQuat Rotation)
{
    if(!C||(C->GetRelativeLocation().Equals(Location,1.e-6)&&C->GetRelativeRotation().Quaternion().Equals(Rotation,1.e-8)))return;
    ++RigTransformWrites;C->SetRelativeLocationAndRotation(Location,Rotation);
}
void ACoilLoadingSimulation::UpdatePortalPosition(){ApplyOwnedTransform(PortalRoot,FVector((RailX+8)*100,0,0));}
void ACoilLoadingSimulation::UpdateHydrostaticPresentation()
{
    double Mass=0;for(const auto& P:Placements)Mass+=P.Spec.GrossMass();LoadedHydro=CoilLoading::DraftResponse(Settings,Mass);
    const FVector Shift(0,0,LoadedHydro.Sinkage*100);if(ShoreRoot&&!ShoreRoot->GetRelativeLocation().Equals(Shift,1.e-6)){ApplyOwnedTransform(ShoreRoot,Shift);++GeometryRevision;}
}
void ACoilLoadingSimulation::UpdateTrolley()
{
    const FVector Shift(0,(CurrentHook.Y-PickupPosition().Y)*100,0);const bool ForeAft=Pending.bEyeForeAft;const double FallTop=QuayZ()+29.1;
    if(bRigTransformsReady&&LastTrolley.Equals(Shift,1.e-6)&&LastHook.Equals(CurrentHook,1.e-8)&&bLastCHook==Settings.bCHook&&bLastForeAft==ForeAft&&LastFallTopZ==FallTop)return;
    for(auto C:TrolleyDetails)ApplyOwnedTransform(C,Shift);
    for(auto C:HookDetails)if(C)
    {
        const bool Attachment=C->ComponentHasTag(TEXT("CoilCHook"))||C->ComponentHasTag(TEXT("CoilSling"));const FQuat Rotation(FVector::UpVector,Attachment&&!ForeAft?PI*.5:0);
        ApplyOwnedTransform(C,(CurrentHook-Rotation.RotateVector(HookVisualOrigin))*100,Rotation);
        if(C->ComponentHasTag(TEXT("CoilCHook"))&&C->bHiddenInGame==Settings.bCHook)C->SetHiddenInGame(!Settings.bCHook);
        if(C->ComponentHasTag(TEXT("CoilSling"))&&C->bHiddenInGame!=Settings.bCHook)C->SetHiddenInGame(Settings.bCHook);
    }
    if(HoistFalls&&(!bRigTransformsReady||!LastHook.Equals(CurrentHook,1.e-8)||LastFallTopZ!=FallTop))
    {
        int32 I=0;for(double DX:{-.30,0.,.30})for(double DY:{-.23,.23})
        {const FVector A(CurrentHook.X+DX,CurrentHook.Y+DY,FallTop),B=CurrentHook+FVector(DX,DY,.25),D=B-A;++RigTransformWrites;HoistFalls->UpdateInstanceTransform(I++,FTransform(FRotationMatrix::MakeFromZ(D).Rotator(),(A+B)*50,FVector(.04,.04,D.Size())),false,false,true);}HoistFalls->MarkRenderStateDirty();
    }
    LastFallTopZ=FallTop;LastTrolley=Shift;LastHook=CurrentHook;bLastCHook=Settings.bCHook;bLastForeAft=ForeAft;bRigTransformsReady=true;
}
