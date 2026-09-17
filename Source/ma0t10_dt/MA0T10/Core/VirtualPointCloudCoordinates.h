#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace VirtualPointCloudCoordinates
{
/** PCD is right-handed X forward/Y left/Z up in metres. UE world is left-handed. */
inline FVector ToWorldMeters(const FTransform& AcquisitionPose, const FVector& PcdMeters)
{
    return AcquisitionPose.TransformPosition(FVector(PcdMeters.X, -PcdMeters.Y, PcdMeters.Z) * 100.0) * 0.01;
}
inline void AddMetadata(FJsonObject& Meta, const FTransform& Pose)
{
    Meta.SetStringField(TEXT("coordinate_frame"), TEXT("sensor_local"));
    Meta.SetStringField(TEXT("coordinate_units"), TEXT("m"));
    Meta.SetStringField(TEXT("coordinate_axes"), TEXT("x_forward_y_left_z_up"));
    Meta.SetStringField(TEXT("world_coordinate_axes"), TEXT("ue_x_forward_y_right_z_up"));
    Meta.SetStringField(TEXT("transform_convention"), TEXT("row_major_4x4_column_vector_local_m_to_ue_world_m"));
    const FVector T = Pose.GetTranslation() * 0.01;
    const FVector X = Pose.TransformVector(FVector::ForwardVector);
    const FVector Y = Pose.TransformVector(-FVector::RightVector);
    const FVector Z = Pose.TransformVector(FVector::UpVector);
    TArray<TSharedPtr<FJsonValue>> Matrix;
    for (const double V : {X.X,Y.X,Z.X,T.X, X.Y,Y.Y,Z.Y,T.Y, X.Z,Y.Z,Z.Z,T.Z, 0.,0.,0.,1.})
        Matrix.Add(MakeShared<FJsonValueNumber>(V));
    Meta.SetArrayField(TEXT("sensor_to_world_m"), Matrix);
    TArray<TSharedPtr<FJsonValue>> Translation, Quaternion;
    for (double V : {T.X,T.Y,T.Z}) Translation.Add(MakeShared<FJsonValueNumber>(V));
    const FQuat Q = Pose.GetRotation();
    for (double V : {Q.X,Q.Y,Q.Z,Q.W}) Quaternion.Add(MakeShared<FJsonValueNumber>(V));
    Meta.SetArrayField(TEXT("sensor_position_world_m"), Translation);
    Meta.SetArrayField(TEXT("sensor_rotation_ue_xyzw"), Quaternion);
}
}
