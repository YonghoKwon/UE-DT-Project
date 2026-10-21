#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/IndustrialLidarProfiles.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorRayQuery.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Json.h"

namespace
{
class FGpuProof final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Submitted=0,MeasureStart=0;
    int Phase=0,Frames=0,DigFrames=0;
    TOptional<EVirtualLidarDeviceProfile> OverrideProfile;
    AShipHatchSimulation* Sim=nullptr;
    TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> Query;
    TArray<FVirtualSensorCollisionMesh> Meshes;
    TArray<FSensorRtRay> Rays;
    TArray<FVirtualSensorRayHit> Reference;
    TArray<FSensorRtHit> Before;
    TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> Scan;
    TArray<double> Errors,FrameTimes;
    int Compared=0,HitMatch=0,IdentityMatch=0,LabelMatch=0,ReferenceHits=0,BoundaryHits=0;
    double BoundaryMaxErrorCm=0;
    TMap<TWeakObjectPtr<UPrimitiveComponent>,FName> AcquisitionLabels;
    uint32 OriginalHash=0;
    FString Root=FPaths::ProjectSavedDir()/TEXT("Reports/GPU-LIDAR-01/progress/proof");
    void Submit()
    {
        FSensorRtRequest R;for(const auto& M:Meshes)R.Meshes.Add({M.Revision,M.Vertices,M.NormalsAndSurfaces,M.LocalToWorld});R.Rays=Rays;
        Submitted=FPlatformTime::Seconds();Scan=SensorOwnedRayTracing::Submit(MoveTemp(R));
        if(!Scan)Test->AddError(TEXT("bounded GPU scan admission failed"));
    }
    void Save()
    {
        Errors.Sort();auto R=MakeShared<FJsonObject>();
        R->SetNumberField(TEXT("rays"),Compared);R->SetNumberField(TEXT("cpu_hit_count"),ReferenceHits);
        R->SetNumberField(TEXT("hit_agreement"),Compared?double(HitMatch)/Compared:0);
        R->SetNumberField(TEXT("identity_agreement_on_cpu_hits"),ReferenceHits?double(IdentityMatch)/ReferenceHits:0);
        R->SetNumberField(TEXT("semantic_label_agreement_on_cpu_hits"),ReferenceHits?double(LabelMatch)/ReferenceHits:0);
        R->SetNumberField(TEXT("median_distance_error_cm"),Errors.Num()?Errors[Errors.Num()/2]:0);
        R->SetNumberField(TEXT("p95_distance_error_cm"),Errors.Num()?Errors[FMath::Min(Errors.Num()-1,FMath::CeilToInt(Errors.Num()*.95)-1)]:0);
        R->SetNumberField(TEXT("max_distance_error_cm"),Errors.IsEmpty()?0:Errors.Last());
        R->SetNumberField(TEXT("x_zero_boundary_hits"),BoundaryHits);R->SetNumberField(TEXT("x_zero_boundary_max_error_cm"),BoundaryMaxErrorCm);
        R->SetNumberField(TEXT("completed_raw_scans"),Frames);R->SetNumberField(TEXT("raw_scan_wall_hz"),Frames/FMath::Max(.001,FPlatformTime::Seconds()-MeasureStart));
        R->SetStringField(TEXT("measurement_scope"),TEXT("one sensor full beam pattern, owned collision GPU geometry + readback; normalized frame processing not yet measured"));
        R->SetNumberField(TEXT("dynamic_changed_rays"),DigFrames);
        int Outliers=0,NativeGpu=0,NativeCpu=0;double OutlierMaxX=0;TArray<TSharedPtr<FJsonValue>> NativeRows;
        for(int I=0;I<Reference.Num();++I)if(Reference[I].Component.IsValid()&&Before[I].NormalAndDistance.W>=0&&FMath::Abs(Reference[I].Time*Rays[I].MaxDistance-Before[I].NormalAndDistance.W)>1)
        {
            ++Outliers;OutlierMaxX=FMath::Max(OutlierMaxX,double(FMath::Abs(Rays[I].Direction.X)));
            FHitResult Native;const FVector A(Rays[I].Origin),B=A+FVector(Rays[I].Direction)*Rays[I].MaxDistance;
            const bool Hit=Sim->GetWorld()->LineTraceSingleByChannel(Native,A,B,ECC_Visibility,FCollisionQueryParams(SCENE_QUERY_STAT(GpuOutlierNative),false,Sim->SensorAt(0)));
            NativeGpu+=Hit&&FMath::Abs(Native.Distance-Before[I].NormalAndDistance.W)<=1;NativeCpu+=Hit&&FMath::Abs(Native.Distance-Reference[I].Time*Rays[I].MaxDistance)<=1;
            auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("ray"),I);Row->SetBoolField(TEXT("native_hit"),Hit);Row->SetNumberField(TEXT("native_cm"),Native.Distance);Row->SetNumberField(TEXT("cpu_bvh_cm"),Reference[I].Time*Rays[I].MaxDistance);Row->SetNumberField(TEXT("gpu_cm"),Before[I].NormalAndDistance.W);NativeRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        R->SetNumberField(TEXT("distance_outliers_above_1cm"),Outliers);R->SetNumberField(TEXT("outlier_max_abs_direction_x"),OutlierMaxX);
        R->SetNumberField(TEXT("native_agrees_with_gpu_on_outliers"),NativeGpu);R->SetNumberField(TEXT("native_agrees_with_cpu_bvh_on_outliers"),NativeCpu);R->SetArrayField(TEXT("native_outlier_checks_after_exact_undo"),NativeRows);
        R->SetBoolField(TEXT("valid"),!Test->HasAnyErrors());
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));IFileManager::Get().MakeDirectory(*Root,true);FFileHelper::SaveStringToFile(Text,*(Root/TEXT("raw-proof.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        FString Csv=TEXT("ray,cpu_hit,gpu_hit,cpu_distance_cm,gpu_distance_cm,cpu_surface,gpu_surface,gpu_mesh,origin_x,origin_y,origin_z,direction_x,direction_y,direction_z,cpu_actor,gpu_actor\n");
        for(int I=0;I<Reference.Num();++I){const auto& A=Reference[I];const auto& B=Before[I];const auto& Ray=Rays[I];auto* CpuComponent=A.Component.Get();UPrimitiveComponent* GpuComponent=Meshes.IsValidIndex(B.Mesh)&&Meshes[B.Mesh].Surfaces.IsValidIndex(B.Surface)?Meshes[B.Mesh].Surfaces[B.Surface].Get():nullptr;Csv+=FString::Printf(TEXT("%d,%d,%d,%.6f,%.6f,%s,%u,%u,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%s,%s\n"),I,A.Component.IsValid()?1:0,B.NormalAndDistance.W>=0?1:0,A.Time*Ray.MaxDistance,B.NormalAndDistance.W,*GetNameSafe(CpuComponent),B.Surface,B.Mesh,Ray.Origin.X,Ray.Origin.Y,Ray.Origin.Z,Ray.Direction.X,Ray.Direction.Y,Ray.Direction.Z,*GetNameSafe(CpuComponent?CpuComponent->GetOwner():nullptr),*GetNameSafe(GpuComponent?GpuComponent->GetOwner():nullptr));}
        FFileHelper::SaveStringToFile(Csv,*(Root/TEXT("cpu-gpu-rays.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
public:
    explicit FGpuProof(FAutomationTestBase* T,TOptional<EVirtualLidarDeviceProfile> Profile={}):Test(T),OverrideProfile(Profile)
    {
        FString Output;if(FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Output))Root=Output/TEXT("measurements/proof");
        if(OverrideProfile)Root/=StaticEnum<EVirtualLidarDeviceProfile>()->GetNameStringByValue(int64(*OverrideProfile));
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("GPU LiDAR proof timed out"));if(Scan)Scan->Cancel();return true;}
        if(Phase==0)
        {
            UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W||!FinalAcceptanceViewportReady(W))return false;
            for(TActorIterator<AShipHatchSimulation>I(W);I;++I)Sim=*I;if(!Sim||Sim->IsGenerating()||!Sim->HasFreshMeasurement())return false;
            for(int I=0;I<3;++I)if(auto* A=Sim->SensorAt(I))A->ScanComponent->StopScan();
            if(!Test->TestTrue(TEXT("real D3D12 HWRT pipeline is available; no skip substitutes for proof"),SensorOwnedRayTracing::IsAvailable()))return true;
            auto* Sensor=Sim->SensorAt(0);auto* C=Sensor->ScanComponent.Get();
            if(OverrideProfile)C->ApplyDeviceProfile(*OverrideProfile);
            Query=Sim->CaptureRayQuery(Sensor,ECC_Visibility);if(!Query||!Query->CollisionMeshes(Meshes)){Test->AddError(TEXT("CPU collision snapshot export unavailable"));return true;}
            for(const auto& M:Meshes)for(const auto& Weak:M.Surfaces)if(auto* Component=Weak.Get())AcquisitionLabels.Add(Weak,C->ResolveSemanticLabelForComponent(Component));
            auto Profile=UVirtualLidarScanComponent::ResolveProfilePreset(C->DeviceProfile,EVirtualSensorSimulationQuality::FullSpec);
            const int Width=Profile.HorizontalSamples,Height=Profile.VerticalChannels;
            const auto Pose=C->GetComponentTransform();Rays.SetNum(Width*Height);Reference.SetNum(Width*Height);
            for(int I=0;I<Rays.Num();++I)
            {
                const int Row=I/Width,Col=I%Width;const float Pitch=Height>1?FMath::Lerp(Profile.MinVerticalAngleDegrees,Profile.MaxVerticalAngleDegrees,float(Row)/(Height-1)):0;
                const float Yaw=Width>1?FMath::Lerp(-Profile.HorizontalFovDegrees*.5f,Profile.HorizontalFovDegrees*.5f,float(Col)/(Width-1)):0;
                const FVector D=Pose.TransformVectorNoScale(IndustrialLidar::Beam(C->DeviceProfile,I,Width,Height,1,Pitch,Yaw).Vector()).GetSafeNormal();
                const float Range=C->DeviceProfile==EVirtualLidarDeviceProfile::HesaiXT32&&(Row<8||Row>=24)?FMath::Min(C->MaxDistance,5000.f):C->MaxDistance;
                Rays[I]={FVector3f(Pose.GetLocation()),Range,FVector3f(D),0};Query->TraceGeometry(Pose.GetLocation(),Pose.GetLocation()+D*Range,Reference[I]);
            }
            // Production cargo rays ensure the next-scan revision assertion is exercised.
            for(int I=0;I<64;++I){const FVector P=Sim->GetActorTransform().TransformPosition(FVector(5+I*.01,-5,Sim->HoldDepth+6)*100);FSensorRtRay R{FVector3f(P),10000,FVector3f(0,0,-1),0};Rays.Add(R);FVirtualSensorRayHit H;Query->TraceGeometry(P,P+FVector(0,0,-10000),H);Reference.Add(H);}
            OriginalHash=Sim->Surface().Hash();MeasureStart=FPlatformTime::Seconds();Submit();Phase=1;return false;
        }
        TArray<FSensorRtHit> Hits;FString Error;if(!Scan)return true;if(!Scan->Poll(Hits,Error))return false;
        if(!Error.IsEmpty()){Test->AddError(Error);return true;}
        if(Hits.Num()!=Rays.Num()){Test->AddError(TEXT("GPU result cardinality differs from original beam count"));return true;}
        ++Frames;
        if(Phase==1)
        {
            Before=Hits;
            for(int I=0;I<Hits.Num();++I)
            {
                const auto& A=Reference[I];const auto& B=Hits[I];const bool Cpu=A.Component.IsValid(),Gpu=B.NormalAndDistance.W>=0;++Compared;HitMatch+=Cpu==Gpu;
                if(Cpu){++ReferenceHits;if(Gpu){const double DistanceError=FMath::Abs(A.Time*Rays[I].MaxDistance-B.NormalAndDistance.W);Errors.Add(DistanceError);if(FMath::Abs(Rays[I].Direction.X)<1.e-7){++BoundaryHits;BoundaryMaxErrorCm=FMath::Max(BoundaryMaxErrorCm,DistanceError);}if(Meshes.IsValidIndex(B.Mesh)&&Meshes[B.Mesh].Surfaces.IsValidIndex(B.Surface)){const auto GpuComponent=Meshes[B.Mesh].Surfaces[B.Surface];if(GpuComponent==A.Component)++IdentityMatch;if(AcquisitionLabels.FindRef(GpuComponent)==AcquisitionLabels.FindRef(A.Component))++LabelMatch;}}}
            }
            Test->TestTrue(TEXT("same-snapshot hit agreement >=99%"),double(HitMatch)/Compared>=.99);
            Errors.Sort();Test->TestTrue(TEXT("static distance p95 <=1cm"),Errors.Num()>0&&Errors[FMath::Min(Errors.Num()-1,FMath::CeilToInt(Errors.Num()*.95)-1)]<=1);
            Test->TestTrue(TEXT("X=0 grid-boundary CPU/GPU distances agree within1mm"),BoundaryHits>0&&BoundaryMaxErrorCm<=.1);
            Test->TestTrue(TEXT("component identity agreement >=99%"),ReferenceHits>0&&double(IdentityMatch)/ReferenceHits>=.99);
            Test->TestTrue(TEXT("semantic label agreement >=99%"),ReferenceHits>0&&double(LabelMatch)/ReferenceHits>=.99);
            Phase=2;Submit();return false;
        }
        if(Phase==2&&Frames<30){Submit();return false;}
        if(Phase==2)
        {
            const FVector O=Sim->GetActorTransform().TransformPosition(FVector(5,-5,Sim->HoldDepth+6)*100),D=Sim->GetActorTransform().TransformVectorNoScale(FVector(0,0,-1));
            if(!Test->TestTrue(TEXT("production Dig changes cargo collision revision"),Sim->Dig(O,D,2,1.5)))return true;
            Query=Sim->CaptureRayQuery(Sim->SensorAt(0),ECC_Visibility);Meshes.Reset();Query->CollisionMeshes(Meshes);Phase=3;Submit();return false;
        }
        for(int I=0;I<Hits.Num();++I)if(Before[I].NormalAndDistance.W>=0&&Hits[I].NormalAndDistance.W>=0&&FMath::Abs(Before[I].NormalAndDistance.W-Hits[I].NormalAndDistance.W)>5)++DigFrames;
        Test->TestTrue(TEXT("first GPU scan after digging sees updated surface"),DigFrames>0);
        Test->TestTrue(TEXT("restore production surface with undo"),Sim->UndoDig()&&Sim->Surface().Hash()==OriginalHash);
        Save();return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGpuLidarProofTest,"MA0T10.GpuLidar.OwnedCollisionProof",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGpuLidarProofTest::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGpuProof(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGpuLidarProfileAccuracyTest,"MA0T10.GpuLidar.ProfileAccuracyRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FGpuLidarProfileAccuracyTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(auto P:{EVirtualLidarDeviceProfile::Generic,EVirtualLidarDeviceProfile::LivoxMid360S,EVirtualLidarDeviceProfile::IYOBOT_MLX80,EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE,
  EVirtualLidarDeviceProfile::LivoxMid360,EVirtualLidarDeviceProfile::LivoxAvia,EVirtualLidarDeviceProfile::OusterOS1_64_Rev05,EVirtualLidarDeviceProfile::HesaiXT32,EVirtualLidarDeviceProfile::VelodyneVLP16})
 {Names.Add(StaticEnum<EVirtualLidarDeviceProfile>()->GetNameStringByValue(int64(P)));Commands.Add(LexToString(int32(P)));}
}
bool FGpuLidarProfileAccuracyTest::RunTest(const FString& Parameters)
{
 const auto Profile=EVirtualLidarDeviceProfile(FCString::Atoi(*Parameters));
 if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;
 ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FGpuProof(this,Profile));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
