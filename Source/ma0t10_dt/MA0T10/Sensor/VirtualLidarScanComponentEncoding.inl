void AddVectorArray(TSharedRef<FJsonObject> Object, const TCHAR* FieldName, const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    Array.Add(MakeShared<FJsonValueNumber>(Value.X));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
    Object->SetArrayField(FieldName, Array);
}

FString BuildScheduledLidarJson(const FLidarScheduledPayloadSnapshot& S)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    static const TArray<FVirtualLidarPoint> EmptyPoints;
    const TArray<FVirtualLidarPoint>& Points = S.Points.IsValid() ? *S.Points : EmptyPoints;
    int32 HitPointCount = 0;
    for (const FVirtualLidarPoint& Point : Points) if (Point.bHit) ++HitPointCount;

    Root->SetStringField(TEXT("schemaVersion"), TEXT("virtual-lidar.v1"));
    Root->SetStringField(TEXT("sensorType"), TEXT("virtual_lidar"));
    Root->SetStringField(TEXT("sensorId"), S.SensorId);
    Root->SetStringField(TEXT("manufacturer"), S.Manufacturer);
    Root->SetStringField(TEXT("model"), S.Model);
    Root->SetNumberField(TEXT("frameId"), static_cast<double>(S.FrameId));
    Root->SetStringField(TEXT("timestampUtc"), (S.TimestampUtc.GetTicks()>0?S.TimestampUtc:FDateTime::UtcNow()).ToIso8601());
    Root->SetNumberField(TEXT("horizontalSamples"), S.HorizontalSamples);
    Root->SetNumberField(TEXT("verticalChannels"), S.VerticalChannels);
    Root->SetNumberField(TEXT("rayCount"), S.HorizontalSamples * S.VerticalChannels);
    Root->SetNumberField(TEXT("totalPointCount"), Points.Num());
    Root->SetNumberField(TEXT("hitPointCount"), HitPointCount);
    Root->SetNumberField(TEXT("maxDistance"), S.MaxDistance);
    Root->SetBoolField(TEXT("semanticClassification"), S.bSemanticClassification);
    Root->SetNumberField(TEXT("serverPayloadStride"), S.ServerPayloadStride);
    Root->SetNumberField(TEXT("maxServerPayloadPoints"), S.MaxServerPayloadPoints);
    Root->SetBoolField(TEXT("includeMissPointsInServerPayload"), S.bIncludeMissPoints);
    Root->SetNumberField(TEXT("previewPointStride"), S.PreviewPointStride);
    Root->SetNumberField(TEXT("maxPreviewPoints"), S.MaxPreviewPoints);
    Root->SetStringField(TEXT("previewBackend"), S.PreviewBackend);
    Root->SetBoolField(TEXT("gpuPreviewBackendRequested"), S.bGpuRequested);
    Root->SetBoolField(TEXT("gpuPreviewBackendActive"), S.bGpuActive);

    TSharedRef<FJsonObject> PayloadPolicy = MakeShared<FJsonObject>();
    PayloadPolicy->SetNumberField(TEXT("stride"), S.ServerPayloadStride);
    PayloadPolicy->SetNumberField(TEXT("maxPoints"), S.MaxServerPayloadPoints);
    PayloadPolicy->SetBoolField(TEXT("includeMissPoints"), S.bIncludeMissPoints);
    PayloadPolicy->SetStringField(TEXT("pointSelection"), S.bIncludeMissPoints ? TEXT("hit_and_miss") : TEXT("hit_only"));
    Root->SetObjectField(TEXT("payloadPolicy"), PayloadPolicy);

    TSharedRef<FJsonObject> PreviewPolicy = MakeShared<FJsonObject>();
    PreviewPolicy->SetNumberField(TEXT("stride"), S.PreviewPointStride);
    PreviewPolicy->SetNumberField(TEXT("maxPoints"), S.MaxPreviewPoints);
    PreviewPolicy->SetBoolField(TEXT("hitOnly"), S.bPreviewHitOnly);
    PreviewPolicy->SetStringField(TEXT("backend"), S.PreviewBackend);
    PreviewPolicy->SetBoolField(TEXT("gpuRequested"), S.bGpuRequested);
    PreviewPolicy->SetBoolField(TEXT("gpuActive"), S.bGpuActive);
    PreviewPolicy->SetBoolField(TEXT("experimentalGpuOptIn"), S.bExperimentalGpuOptIn);
    PreviewPolicy->SetStringField(TEXT("activePath"), S.bGpuActive ? TEXT("gpu") : TEXT("cpu_instanced_mesh_fallback"));
    Root->SetObjectField(TEXT("previewPolicy"), PreviewPolicy);

    TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
    AddVectorArray(TransformObject, TEXT("location"), S.SensorTransform.GetLocation());
    const FRotator Rotation = S.SensorTransform.Rotator();
    TArray<TSharedPtr<FJsonValue>> RotationJson;
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationJson.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    TransformObject->SetArrayField(TEXT("rotation"), RotationJson);
    AddVectorArray(TransformObject, TEXT("forward"), S.SensorTransform.GetUnitAxis(EAxis::X));
    AddVectorArray(TransformObject, TEXT("up"), S.SensorTransform.GetUnitAxis(EAxis::Z));
    Root->SetObjectField(TEXT("sensorTransform"), TransformObject);

    if (S.bIncludeSlabAnalysis)
    {
        TSharedRef<FJsonObject> SlabObject = MakeShared<FJsonObject>();
        SlabObject->SetBoolField(TEXT("valid"), S.SlabAnalysis.bValid);
        SlabObject->SetNumberField(TEXT("slabHitPointCount"), S.SlabAnalysis.SlabHitPointCount);
        AddVectorArray(SlabObject, TEXT("boundsMin"), S.SlabAnalysis.BoundsMin);
        AddVectorArray(SlabObject, TEXT("boundsMax"), S.SlabAnalysis.BoundsMax);
        AddVectorArray(SlabObject, TEXT("center"), S.SlabAnalysis.Center);
        SlabObject->SetNumberField(TEXT("estimatedYawDegrees"), S.SlabAnalysis.EstimatedYawDegrees);
        SlabObject->SetNumberField(TEXT("referenceYawDegrees"), S.SlabAnalysis.ReferenceYawDegrees);
        SlabObject->SetNumberField(TEXT("angleDeviationDegrees"), S.SlabAnalysis.AngleDeviationDegrees);
        SlabObject->SetNumberField(TEXT("confidence"), S.SlabAnalysis.Confidence);
        SlabObject->SetStringField(TEXT("status"), S.SlabAnalysis.StatusMessage);
        Root->SetObjectField(TEXT("slabAnalysis"), SlabObject);
    }

    TArray<TSharedPtr<FJsonValue>> JsonPoints;
    const int32 SafeStride = FMath::Max(1, S.ServerPayloadStride);
    const int32 SafeMax = FMath::Max(0, S.MaxServerPayloadPoints);
    int32 Added = 0;
    for (int32 I = 0; I < Points.Num(); I += SafeStride)
    {
        if (SafeMax > 0 && Added >= SafeMax) break;
        const FVirtualLidarPoint& P = Points[I];
        if (!S.bIncludeMissPoints && !P.bHit) continue;
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("pointIndex"), I);
        O->SetNumberField(TEXT("row"), P.bHasGridCoord ? P.Row : (S.HorizontalSamples > 0 ? I / S.HorizontalSamples : 0));
        O->SetNumberField(TEXT("col"), P.bHasGridCoord ? P.Col : (S.HorizontalSamples > 0 ? I % S.HorizontalSamples : I));
        O->SetNumberField(TEXT("returnIndex"), P.ReturnIndex);
        O->SetBoolField(TEXT("gridCoordValid"), P.bHasGridCoord);
        O->SetStringField(TEXT("gridCoordSource"), P.bHasGridCoord ? TEXT("point_metadata") : TEXT("derived_from_point_index"));
        O->SetBoolField(TEXT("hit"), P.bHit);
        O->SetNumberField(TEXT("distance"), P.Distance);
        O->SetStringField(TEXT("hitActor"), P.HitActorName.ToString());
        O->SetStringField(TEXT("hitActorClass"), P.HitActorClassName.ToString());
        O->SetStringField(TEXT("semanticLabel"), P.SemanticLabel.ToString());
        TArray<TSharedPtr<FJsonValue>> Tags;
        for (const FName& Tag : P.HitActorTags) Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        O->SetArrayField(TEXT("hitActorTags"), Tags);
        AddVectorArray(O, TEXT("worldLocation"), P.WorldLocation);
        AddVectorArray(O, TEXT("localDirection"), P.LocalDirection);
        JsonPoints.Add(MakeShared<FJsonValueObject>(O));
        ++Added;
    }
    Root->SetNumberField(TEXT("payloadPointCount"), Added);
    Root->SetArrayField(TEXT("points"), JsonPoints);
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}
