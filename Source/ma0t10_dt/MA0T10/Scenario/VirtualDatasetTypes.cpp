#include "VirtualDatasetTypes.h"
#include "Json.h"

bool FVirtualDatasetSettings::Validate(FString& Error) const
{
    Error.Empty();
    if(!Enabled){Error=TEXT("데이터셋 생성을 먼저 켜세요 · 기본 꺼짐");return false;}
    if(ExportFormat!=TEXT("none")&&ExportFormat!=TEXT("coco")&&ExportFormat!=TEXT("kitti")&&ExportFormat!=TEXT("coco,kitti"))
    {Error=TEXT("내보내기 형식을 확인하세요");return false;}
    if(ExportFormat!=TEXT("none")&&!CameraTruth){Error=TEXT("표준 형식 내보내기에는 카메라 정답 포함을 켜세요");return false;}
    if(Scenes<1||Scenes>10000||SettleFrames<1||SettleFrames>600||Name.IsEmpty()||Name.Len()>64||Name.Contains(TEXT("..")))
    {Error=TEXT("이름·장면 수(1–10000)·대기 프레임(1–600)을 확인하세요");return false;}
    for(const TCHAR C:Name)if(!FChar::IsAlnum(C)&&C!=TEXT('_')&&C!=TEXT('-')){Error=TEXT("이름은 문자·숫자·_·-만 사용하세요");return false;}
    for(const auto* R:{&Yaw,&Pitch,&Roll,&X,&Y,&Z,&Size,&Surface,&Roughness,&Oxidation,&LightIntensity,&LightPitch,&LightYaw,&SensorPosition,&SensorAngle})
        if(!R->IsValid()){Error=TEXT("랜덤화 최솟값·최댓값을 확인하세요");return false;}
    if(Size.Min<.1||Size.Max>10||Surface.Min<0||Surface.Max>4||Roughness.Min<0||Roughness.Max>1||Oxidation.Min<0||Oxidation.Max>1||LightIntensity.Min<0||LightIntensity.Max>1000000||SensorPosition.Min < -100||SensorPosition.Max>100||SensorAngle.Min < -10||SensorAngle.Max>10)
    {Error=TEXT("크기 배율(.1–10)·표면(0–4)·재질(0–1)·조명(0–1000000)·설치 오차(100cm/10° 이내)를 확인하세요");return false;}
    if(Surface.Min!=FMath::FloorToDouble(Surface.Min)||Surface.Max!=FMath::FloorToDouble(Surface.Max))
    {Error=TEXT("표면 프리셋 범위는 0–4 정수로 입력하세요");return false;}
    for(const auto* R:{&Yaw,&Pitch,&Roll})if(R->Min < -180||R->Max>180){Error=TEXT("Slab 각도는 ±180° 이내로 입력하세요");return false;}
    for(const auto* R:{&X,&Y,&Z})if(R->Min < -1000000||R->Max>1000000){Error=TEXT("Slab 위치 오프셋은 ±1000000cm 이내로 입력하세요");return false;}
    if(LightPitch.Min < -90||LightPitch.Max>90||LightYaw.Min < -180||LightYaw.Max>180){Error=TEXT("조명 pitch는 ±90°·yaw는 ±180° 이내로 입력하세요");return false;}
    return true;
}
FVirtualDatasetSample FVirtualDatasetSample::Make(const FVirtualDatasetSettings& S,int32 Scene)
{
    // Independent fixed channels: toggling one option never shifts another option's draws.
    auto Draw=[&](const FVirtualDatasetRange& R,uint32 Channel,double Default=0.)
    {
        if(!R.Enabled)return Default;
        uint32 V=uint32(S.Seed)^((uint32(Scene)+1)*0x9e3779b9u)^(Channel*0x85ebca6bu);
        V^=V>>16;V*=0x7feb352du;V^=V>>15;V*=0x846ca68bu;V^=V>>16;
        return R.Min+(R.Max-R.Min)*double(V&0x00ffffffu)/16777216.;
    };
    FVirtualDatasetSample V;
    V.SlabRotation=FRotator(Draw(S.Pitch,1),Draw(S.Yaw,2),Draw(S.Roll,3));
    V.SlabOffset=FVector(Draw(S.X,4),Draw(S.Y,5),Draw(S.Z,6));V.Size=Draw(S.Size,7,1);
    V.Surface=FMath::Clamp(FMath::FloorToInt(Draw({S.Surface.Enabled,S.Surface.Min,S.Surface.Max+1},8)),0,4);
    V.Roughness=Draw(S.Roughness,9);V.Oxidation=Draw(S.Oxidation,10);V.Intensity=Draw(S.LightIntensity,11);
    V.LightRotation=FRotator(Draw(S.LightPitch,12),Draw(S.LightYaw,13),0);
    V.LidarOffset=FVector(Draw(S.SensorPosition,14),Draw(S.SensorPosition,15),Draw(S.SensorPosition,16));
    V.CameraOffset=FVector(Draw(S.SensorPosition,17),Draw(S.SensorPosition,18),Draw(S.SensorPosition,19));
    V.LidarRotation=FRotator(Draw(S.SensorAngle,20),Draw(S.SensorAngle,21),Draw(S.SensorAngle,22));
    V.CameraRotation=FRotator(Draw(S.SensorAngle,23),Draw(S.SensorAngle,24),Draw(S.SensorAngle,25));return V;
}
FString FVirtualDatasetSample::ToJson() const
{
    auto O=MakeShared<FJsonObject>();
    auto Vec=[&](const TCHAR* Key,const FVector& V){TArray<TSharedPtr<FJsonValue>> A;for(double X:{V.X,V.Y,V.Z})A.Add(MakeShared<FJsonValueNumber>(X));O->SetArrayField(Key,A);};
    Vec(TEXT("slab_offset_cm"),SlabOffset);Vec(TEXT("slab_pitch_yaw_roll_deg"),FVector(SlabRotation.Pitch,SlabRotation.Yaw,SlabRotation.Roll));
    Vec(TEXT("lidar_offset_cm"),LidarOffset);Vec(TEXT("camera_offset_cm"),CameraOffset);
    Vec(TEXT("lidar_pitch_yaw_roll_deg"),FVector(LidarRotation.Pitch,LidarRotation.Yaw,LidarRotation.Roll));
    Vec(TEXT("camera_pitch_yaw_roll_deg"),FVector(CameraRotation.Pitch,CameraRotation.Yaw,CameraRotation.Roll));
    Vec(TEXT("light_pitch_yaw_roll_deg"),FVector(LightRotation.Pitch,LightRotation.Yaw,LightRotation.Roll));
    O->SetNumberField(TEXT("size_multiplier"),Size);O->SetNumberField(TEXT("surface_preset"),Surface);O->SetNumberField(TEXT("roughness"),Roughness);O->SetNumberField(TEXT("oxidation"),Oxidation);O->SetNumberField(TEXT("light_intensity"),Intensity);
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));return Text;
}
