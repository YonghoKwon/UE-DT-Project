#include "CoilLoadingSimulation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/MaterialInstanceDynamic.h"

void ACoilLoadingSimulation::ApplyCoilAppearance(ACoilLoadingBody* A,const CoilLoading::FSpec& S)
{
    if(CoilSurfaceMaterials.IsEmpty())
    {
        auto* Surface=CoilSurfaceAsset.Get();if(!Surface)Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Coil/Materials/M_CoilSurface.M_CoilSurface"));
        if(!Surface)Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain"));
        for(int32 I=0;I<2;++I)
        {
            auto* M=UMaterialInstanceDynamic::Create(Surface,this);M->SetScalarParameterValue(TEXT("ColdRolled"),I);M->SetVectorParameterValue(TEXT("ColdColor"),I?FLinearColor(.52,.55,.58):FLinearColor(.09,.115,.145));M->SetScalarParameterValue(TEXT("Metallic"),I?.65:.35);M->SetScalarParameterValue(TEXT("Roughness"),I?.47:.8);CoilSurfaceMaterials.Add(M);
        }
        const FLinearColor Colors[]={FLinearColor(.035,.04,.045),FLinearColor(.6,.63,.66),FLinearColor(.86,.84,.68),FLinearColor(.45,.30,.14)};
        for(const auto& C:Colors)DetailMaterials.Add(Material(C));
    }
    A->Body->SetMaterial(0,CoilSurfaceMaterials[int32(S.Type)]);A->Detail->SetStaticMesh(CoilDetailMesh(S));
    A->Detail->SetMaterial(0,DetailMaterials[0]);A->Detail->SetMaterial(1,CoilSurfaceMaterials[int32(S.Type)]);A->Detail->SetMaterial(2,DetailMaterials[1]);A->Detail->SetMaterial(3,DetailMaterials[2]);A->Detail->SetMaterial(4,DetailMaterials[3]);
    A->Label->SetText(FText::FromString(S.CoilId.Left(18)+TEXT("\n")+(S.Type==CoilLoading::ECoilType::ColdRolled?TEXT("CR / VCI"):TEXT("HR / BARE"))));
    A->Label->SetRelativeLocation(FVector(-S.OverallWidth()*50-.85,S.OuterDiameter()*29,S.OuterDiameter()*14));A->Label->SetRelativeRotation(FRotator(0,180,0));
}
UStaticMesh* ACoilLoadingSimulation::CoilDetailMesh(const CoilLoading::FSpec& S)
{
    const FString Key=FString::Printf(TEXT("%d/%.6f/%.6f/%.6f"),int32(S.Type),S.OuterDiameter(),S.ClearBore(),S.OverallWidth());
    if(auto* Existing=SharedDetailMeshes.Find(Key))return *Existing;
    FMeshDescription D;FStaticMeshAttributes Attr(D);Attr.Register();auto Positions=Attr.GetVertexPositions();auto Normals=Attr.GetVertexInstanceNormals();auto UV=Attr.GetVertexInstanceUVs();UV.SetNumChannels(1);auto Slots=Attr.GetPolygonGroupMaterialSlotNames();TArray<FPolygonGroupID> Groups;
    auto* Mesh=NewObject<UStaticMesh>(this,NAME_None,RF_Transient);
    for(int32 I=0;I<5;++I){const auto G=D.CreatePolygonGroup();const FName Name(*FString::Printf(TEXT("CoilDetail%d"),I));Slots[G]=Name;Groups.Add(G);Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr,Name));}
    const auto Quad=[&](FVector A,FVector B,FVector C,FVector E,int32 Group)
    {
        const FVector N=-FVector::CrossProduct(B-A,C-A).GetSafeNormal();TArray<FVertexInstanceID> VI;
        for(const FVector& P:{A,B,C,E}){auto V=D.CreateVertex();Positions[V]=FVector3f(P);auto I=D.CreateVertexInstance(V);Normals[I]=FVector3f(N);UV.Set(I,0,FVector2f(P.Y*.01,P.Z*.01));VI.Add(I);}D.CreatePolygon(Groups[Group],VI);
    };
    const auto Point=[](double X,double R,double A){return FVector(X,R*FMath::Cos(A),R*FMath::Sin(A));};
    const double R=S.OuterDiameter()*50,r=S.ClearBore()*50,X=S.OverallWidth()*50;
    const auto EndRing=[&](double X1,double Radius,double Width,int32 Group,double Ripple)
    {
        for(int32 I=0;I<64;++I){const double A=I*2*PI/64,B=(I+1)*2*PI/64;const double DX=Ripple*FMath::Sin(A*3+Radius*.1);const auto A0=Point(X1+DX,Radius,A),B0=Point(X1+DX,Radius+Width,A),C0=Point(X1+DX,Radius+Width,B),D0=Point(X1+DX,Radius,B);if(X1<0)Quad(A0,B0,C0,D0,Group);else Quad(D0,C0,B0,A0,Group);}
    };
    // Mill-scale HR: visible representative laps plus small axial telescoping.
    // These meshes are exclusively NoCollision; the measured annulus stays smooth.
    if(S.Type==CoilLoading::ECoilType::HotRolled)
    {
        for(double Side:{-1.,1.})for(int32 I=0;I<22;++I){const double Radius=r+(R-r)*(I+1)/23;EndRing(Side*(X+.12+.16*FMath::Sin(I*1.7)),Radius,.10,1,.12);}
        for(int32 I=0;I<5;++I){const double A=.3+I*.015,B=A+.015;Quad(Point(-X,R+.18,A),Point(X,R+.18,A),Point(X,R+.18,B),Point(-X,R+.18,B),1);}
    }
    else
    {
        // CR: edge-protector rings, silver side covers, brown VCI paper edge.
        for(double Side:{-1.,1.}){EndRing(Side*(X+.12),R-1.6,1.8,2,0);EndRing(Side*(X+.13),r-.15,1.8,2,0);EndRing(Side*(X+.08),R-3.4,1.7,4,0);}
    }
    // Three circumferential straps. Dark steel stands clear of either surface.
    for(double BandX:{-S.Width*32,0.,S.Width*32})for(int32 I=0;I<64;++I)
    {
        const double A=I*2*PI/64,B=(I+1)*2*PI/64;
        Quad(Point(BandX-1.2,R+.28,A),Point(BandX+1.2,R+.28,A),Point(BandX+1.2,R+.28,B),Point(BandX-1.2,R+.28,B),0);
    }
    // Four radial straps pass along both faces and the eye, purely visual.
    for(int32 I=0;I<4;++I)
    {
        const double A=.45+I*PI*.5,HalfAngle=.014;
        for(double Side:{-1.,1.})Quad(Point(Side*(X+.35),r-.12,A-HalfAngle),Point(Side*(X+.35),R+.3,A-HalfAngle),Point(Side*(X+.35),R+.3,A+HalfAngle),Point(Side*(X+.35),r-.12,A+HalfAngle),0);
        Quad(Point(-X,r-.2,A-HalfAngle),Point(X,r-.2,A-HalfAngle),Point(X,r-.2,A+HalfAngle),Point(-X,r-.2,A+HalfAngle),0);
    }
    const double Y=S.OuterDiameter()*29,Z=S.OuterDiameter()*14;
    Quad({-X-.6,Y-17,Z-8},{-X-.6,Y+17,Z-8},{-X-.6,Y+17,Z+8},{-X-.6,Y-17,Z+8},3);
    UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bBuildSimpleCollision=false;Params.bFastBuild=true;Params.bAllowCpuAccess=true;Mesh->BuildFromMeshDescriptions({&D},Params);SharedDetailMeshes.Add(Key,Mesh);return Mesh;
}
