"""Create or refresh the repository-owned virtual sensor smoke-test map.

Run with Unreal Editor 5.3 after the editor target has been built:

    UnrealEditor-Cmd.exe ma0t10_dt.uproject \
        -ExecutePythonScript=Scripts/setup_sensor_test_map.py \
        -Unattended -NoSplash -NoSound

Only actors tagged ``SensorTestManaged`` are replaced when the script is run
again. This keeps the map reproducible without touching unrelated maps.
"""

import re
import unreal


CONTENT_ROOT = "/Game/MA0T10"
MAP_PATH = CONTENT_ROOT + "/Maps/SensorTestMap"
DELETE_LEGACY_WIDGET_ASSETS = True
MANAGED_TAG = unreal.Name("SensorTestManaged")
LEVEL_SUBSYSTEM = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTOR_SUBSYSTEM = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def _load_class(path):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError("Unable to load class: {}".format(path))
    return cls


def _snake_case(name):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def _set_property(obj, cpp_name, value):
    candidates = [cpp_name, _snake_case(cpp_name)]
    if cpp_name.startswith("b") and len(cpp_name) > 1 and cpp_name[1].isupper():
        candidates.append(_snake_case(cpp_name[1:]))

    last_error = None
    for candidate in dict.fromkeys(candidates):
        try:
            obj.set_editor_property(candidate, value)
            return
        except Exception as error:  # Unreal reports property aliases at runtime.
            last_error = error
    object_name = obj.get_name() if hasattr(obj, "get_name") else type(obj).__name__
    raise RuntimeError(
        "Unable to set {}.{}: {}".format(object_name, cpp_name, last_error)
    )


def _try_set_property(obj, cpp_name, value):
    """Set an engine-version-sensitive presentation property when available."""
    try:
        _set_property(obj, cpp_name, value)
        return True
    except Exception as error:
        unreal.log_warning("Optional property {} skipped: {}".format(cpp_name, error))
        return False


def _mark_managed(actor, label, folder="SensorTest"):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [MANAGED_TAG])
    return actor


def _spawn(actor_class, label, location, rotation=unreal.Rotator()):
    actor = ACTOR_SUBSYSTEM.spawn_actor_from_class(actor_class, location, rotation)
    if not actor:
        raise RuntimeError("Unable to spawn {}".format(label))
    return _mark_managed(actor, label)


def _spawn_static_mesh(label, location, scale, mesh_path, extra_tags=None):
    actor = _spawn(unreal.StaticMeshActor, label, location)
    actor.set_actor_scale3d(scale)
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        raise RuntimeError("Unable to load mesh: {}".format(mesh_path))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(mesh)
    component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    tags = [MANAGED_TAG]
    if extra_tags:
        tags.extend(unreal.Name(tag) for tag in extra_tags)
    actor.set_editor_property("tags", tags)
    return actor


def _remove_managed_actors():
    for actor in ACTOR_SUBSYSTEM.get_all_level_actors():
        if MANAGED_TAG in actor.get_editor_property("tags"):
            ACTOR_SUBSYSTEM.destroy_actor(actor)


def _ensure_widget_class(asset_name, parent_class_path, content_root=CONTENT_ROOT):
    widget_path = content_root + "/UI/" + asset_name
    widget_class_path = widget_path + "." + asset_name + "_C"
    if not unreal.EditorAssetLibrary.does_asset_exist(widget_path):
        factory = unreal.WidgetBlueprintFactory()
        _set_property(
            factory,
            "ParentClass",
            _load_class(parent_class_path),
        )
        widget_blueprint = ASSET_TOOLS.create_asset(
            asset_name,
            content_root + "/UI",
            unreal.WidgetBlueprint,
            factory,
        )
        if not widget_blueprint:
            raise RuntimeError("Unable to create {}".format(widget_path))
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            widget_blueprint, only_if_is_dirty=False
        ):
            raise RuntimeError("Unable to save {}".format(widget_path))

    return _load_class(widget_class_path)


def _ensure_widget_classes(content_root=CONTENT_ROOT):
    return {
        "monitor": _ensure_widget_class(
            "WBP_VirtualSensorMonitorPanel",
            "/Script/ma0t10_dt.VirtualSensorMonitorPanelWidget",
            content_root,
        ),
        "settings": _ensure_widget_class(
            "WBP_VirtualSensorSettingsPanel",
            "/Script/ma0t10_dt.VirtualSensorSettingsPanelWidget",
            content_root,
        ),
        "capture_export": _ensure_widget_class(
            "WBP_VirtualSensorCaptureExportPanel",
            "/Script/ma0t10_dt.VirtualSensorCaptureExportPanelWidget",
            content_root,
        ),
    }


def _delete_legacy_widget_assets(content_root=CONTENT_ROOT):
    for asset_name in (
        "WBP_VirtualSensorMonitor",
        "WBP_VirtualSensorSettings",
        "WBP_VirtualSensorCaptureExport",
    ):
        asset_path = content_root + "/UI/" + asset_name
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            if not unreal.EditorAssetLibrary.delete_asset(asset_path):
                unreal.log_warning("Unable to delete legacy widget asset: " + asset_path)


def _prepare_level():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not LEVEL_SUBSYSTEM.load_level(MAP_PATH):
            raise RuntimeError("Unable to load {}".format(MAP_PATH))
    else:
        if not LEVEL_SUBSYSTEM.new_level(MAP_PATH):
            raise RuntimeError("Unable to create {}".format(MAP_PATH))
    _remove_managed_actors()


def _build_sensor_rig(widget_classes):
    manager_class = _load_class("/Script/ma0t10_dt.VirtualSensorCoordinator")
    lidar_actor_class = _load_class("/Script/ma0t10_dt.VirtualLidarSensorActor")
    camera_actor_class = _load_class("/Script/ma0t10_dt.VirtualCameraSensorActor")
    host_actor_class = _load_class("/Script/ma0t10_dt.VirtualSensorUiHostActor")
    external_source_host_class = _load_class("/Script/ma0t10_dt.VirtualSensorExternalSourceHostActor")

    manager = _spawn(
        manager_class,
        "SensorTest_Manager",
        unreal.Vector(0.0, 0.0, 80.0),
    )
    _set_property(manager, "bDiscoverOnBeginPlay", True)
    _set_property(manager, "bStartSensorsOnBeginPlay", True)
    _set_property(manager, "bUseSynchronizedCapture", False)
    _set_property(manager, "bPointCloudOnlyHideWorld", True)
    _set_property(manager, "bPointCloudOnlyAutoSelectLidarView", True)

    lidar_actor = _spawn(
        lidar_actor_class,
        "SensorTest_LiDAR",
        unreal.Vector(0.0, 0.0, 150.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    lidar_actor.set_editor_property(
        "tags", [MANAGED_TAG, unreal.Name("SensorTestPersistent_PrimaryLidar")]
    )
    lidar = lidar_actor.get_component_by_class(
        _load_class("/Script/ma0t10_dt.VirtualLidarScanComponent")
    )
    _set_property(lidar, "SensorId", "LIDAR-TEST-001")
    _set_property(lidar, "bAutoStartScan", True)
    _set_property(lidar, "bAutoRegisterToManager", True)
    _set_property(lidar, "bUseMultiHit", False)
    _set_property(lidar, "bDrawDebugRays", False)
    _set_property(lidar, "ScanInterval", 0.25)
    _set_property(lidar, "MaxDistance", 4000.0)
    _set_property(lidar, "HorizontalSamples", 120)
    _set_property(lidar, "VerticalChannels", 24)
    _set_property(lidar, "HorizontalFov", 360.0)
    _set_property(lidar, "MinVerticalAngle", -7.0)
    _set_property(lidar, "MaxVerticalAngle", 52.0)
    _set_property(lidar, "ServerPayloadStride", 1)
    _set_property(lidar, "MaxServerPayloadPoints", 0)
    _set_property(lidar, "bIncludeMissPointsInServerPayload", False)
    _set_property(lidar, "PreviewPointStride", 2)
    _set_property(lidar, "MaxPreviewPoints", 3000)
    # SensorTestMap is an immediately runnable feature map, so the spatial
    # point-cloud preview must be visible without first entering a special view.
    _set_property(lidar, "bPointCloudPreviewEnabled", True)
    _set_property(lidar, "bPointCloudPreviewHitOnly", True)
    _set_property(lidar, "bExportCsvOnScan", False)
    _set_property(lidar, "bExportJsonLinesOnScan", False)
    _set_property(lidar, "bExportPcdOnScan", False)
    _set_property(lidar, "SlabSemanticLabel", unreal.Name("Slab"))
    _set_property(lidar, "ReferenceSlabYawDegrees", 0.0)
    _set_property(lidar, "MinSlabPointsForAnalysis", 12)
    _set_property(lidar, "bIncludeSlabAnalysisInPayload", True)

    camera_actor = _spawn(
        camera_actor_class,
        "SensorTest_Camera",
        unreal.Vector(-350.0, -220.0, 170.0),
        unreal.Rotator(roll=0.0, pitch=-7.0, yaw=23.0),
    )
    camera_actor.set_editor_property(
        "tags", [MANAGED_TAG, unreal.Name("SensorTestPersistent_PrimaryCamera")]
    )
    camera = camera_actor.get_component_by_class(
        _load_class("/Script/ma0t10_dt.VirtualCameraCaptureComponent")
    )
    _set_property(camera, "SensorId", "VCAM-TEST-001")
    _set_property(camera, "bAutoStartCapture", True)
    _set_property(camera, "bAutoRegisterToManager", True)
    _set_property(camera, "CaptureResolution", unreal.IntPoint(640, 360))
    _set_property(camera, "CaptureInterval", 0.1)
    _set_property(camera, "FOVAngle", 87.0)

    if MAP_PATH == "/Game/MA0T10/Maps/Tests/SensorRefactorTestMap":
        overhead_camera_actor = _spawn(
            camera_actor_class,
            "SensorTest_CameraOverhead",
            unreal.Vector(0.0, 0.0, 1000.0),
            unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0),
        )
        overhead_camera_actor.set_editor_property(
            "tags", [MANAGED_TAG, unreal.Name("SensorTestPersistent_OverheadCamera")]
        )
        overhead_camera = overhead_camera_actor.get_component_by_class(
            _load_class("/Script/ma0t10_dt.VirtualCameraCaptureComponent")
        )
        _set_property(overhead_camera, "SensorId", "VCAM-TEST-002")
        _set_property(overhead_camera, "bAutoStartCapture", True)
        _set_property(overhead_camera, "bAutoRegisterToManager", True)
        _set_property(overhead_camera, "CaptureResolution", unreal.IntPoint(640, 360))
        _set_property(overhead_camera, "CaptureInterval", 0.1)
        _set_property(overhead_camera, "FOVAngle", 87.0)

    host = _spawn(
        host_actor_class,
        "SensorTest_MonitorHost",
        unreal.Vector(0.0, 0.0, 120.0),
    )
    _set_property(host, "MonitorWidgetClass", widget_classes["monitor"])
    _set_property(host, "SettingsWidgetClass", widget_classes["settings"])
    _set_property(host, "CaptureExportWidgetClass", widget_classes["capture_export"])
    _set_property(host, "bUseNativeMonitorWidgetFallback", True)
    _set_property(host, "bUseNativeToolWidgetFallback", True)
    _set_property(host, "bAutoCreateToolPanels", True)
    _set_property(host, "SensorManager", manager)
    _set_property(host, "bAutoCreateOnBeginPlay", True)
    _set_property(host, "bAutoFindSensorManager", True)
    _set_property(host, "bAllowViewportFallback", True)
    _set_property(host, "bShowLidarViewOnStart", False)
    _set_property(host, "bEnablePointCloudOnlyOnStart", False)
    _set_property(host, "ViewportZOrder", 10)
    _set_property(host, "SettingsViewportZOrder", 20)
    _set_property(host, "CaptureExportViewportZOrder", 30)
    _set_property(host, "bConfigurePlayerInputOnCreate", True)
    _set_property(host, "bShowMouseCursorOnCreate", True)
    _set_property(host, "bHideScreenDebugLogOnBeginPlay", "/Tests/" in MAP_PATH)

    if "/Tests/" in MAP_PATH:
        _spawn(
            external_source_host_class,
            "SensorTest_ExternalSources",
            unreal.Vector(-150.0, 0.0, 80.0),
        )


def _build_test_scene():
    # 16 m x 12 m x 4 m open-front indoor hall. Engine cubes are 100 cm.
    _spawn_static_mesh(
        "SensorTest_Floor",
        unreal.Vector(0.0, 0.0, -10.0),
        unreal.Vector(16.0, 12.0, 0.2),
        "/Engine/BasicShapes/Cube.Cube",
        ["KeepInPointCloudOnly", "SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_HallBackWall",
        unreal.Vector(800.0, 0.0, 200.0),
        unreal.Vector(0.2, 12.0, 4.0),
        "/Engine/BasicShapes/Cube.Cube",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_HallLeftWall",
        unreal.Vector(0.0, -600.0, 200.0),
        unreal.Vector(16.0, 0.2, 4.0),
        "/Engine/BasicShapes/Cube.Cube",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_HallRightWall",
        unreal.Vector(0.0, 600.0, 200.0),
        unreal.Vector(16.0, 0.2, 4.0),
        "/Engine/BasicShapes/Cube.Cube",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_TargetCube",
        unreal.Vector(160.0, -120.0, 60.0),
        unreal.Vector(1.0, 1.0, 1.0),
        "/Engine/BasicShapes/Cube.Cube",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_TargetSphere",
        unreal.Vector(260.0, 130.0, 100.0),
        unreal.Vector(1.2, 1.2, 1.2),
        "/Engine/BasicShapes/Sphere.Sphere",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_TargetPillar",
        unreal.Vector(100.0, 250.0, 130.0),
        unreal.Vector(0.8, 0.8, 2.6),
        "/Engine/BasicShapes/Cylinder.Cylinder",
        ["SensorTestTarget"],
    )

    directional = _spawn(
        unreal.DirectionalLight,
        "SensorTest_DirectionalLight",
        unreal.Vector(0.0, 0.0, 800.0),
        unreal.Rotator(roll=0.0, pitch=-42.0, yaw=-25.0),
    )
    directional_component = directional.get_component_by_class(
        unreal.DirectionalLightComponent
    )
    directional_component.set_editor_property("intensity", 3.0)
    _try_set_property(directional_component, "UseTemperature", True)
    _try_set_property(directional_component, "Temperature", 5500.0)

    skylight = _spawn(
        unreal.SkyLight,
        "SensorTest_SkyLight",
        unreal.Vector(0.0, 0.0, 500.0),
    )
    skylight_component = skylight.get_component_by_class(unreal.SkyLightComponent)
    skylight_component.set_editor_property("intensity", 1.5)
    _try_set_property(skylight_component, "RealTimeCapture", True)

    # Four broad ceiling fixtures give predictable neutral illumination without
    # relying on the directional light alone.
    for index, (x_pos, y_pos) in enumerate(
        [(-400.0, -280.0), (-400.0, 280.0), (350.0, -280.0), (350.0, 280.0)]
    ):
        rect_light = _spawn(
            unreal.RectLight,
            "SensorTest_CeilingRectLight_{:02d}".format(index + 1),
            unreal.Vector(x_pos, y_pos, 380.0),
            unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0),
        )
        rect_component = rect_light.get_component_by_class(unreal.RectLightComponent)
        rect_component.set_editor_property("intensity", 3500.0)
        _try_set_property(rect_component, "SourceWidth", 350.0)
        _try_set_property(rect_component, "SourceHeight", 220.0)
        _try_set_property(rect_component, "UseTemperature", True)
        _try_set_property(rect_component, "Temperature", 5000.0)

    _spawn(
        unreal.SkyAtmosphere,
        "SensorTest_SkyAtmosphere",
        unreal.Vector(0.0, 0.0, 0.0),
    )

    post_process = _spawn(
        unreal.PostProcessVolume,
        "SensorTest_PostProcess",
        unreal.Vector(0.0, 0.0, 150.0),
    )
    _try_set_property(post_process, "bUnbound", True)
    settings = post_process.get_editor_property("settings")
    # UE 5.3 exposes these fields on FPostProcessSettings. Keep this optional so
    # the map builder remains usable if an engine minor version renames them.
    _try_set_property(settings, "bOverride_AutoExposureMinBrightness", True)
    _try_set_property(settings, "bOverride_AutoExposureMaxBrightness", True)
    _try_set_property(settings, "AutoExposureMinBrightness", 1.0)
    _try_set_property(settings, "AutoExposureMaxBrightness", 1.0)
    post_process.set_editor_property("settings", settings)

    _spawn(
        unreal.PlayerStart,
        "SensorTest_PlayerStart",
        unreal.Vector(-650.0, -450.0, 120.0),
        unreal.Rotator(roll=0.0, pitch=0.0, yaw=30.0),
    )


def main():
    widget_classes = _ensure_widget_classes()
    _prepare_level()
    _build_sensor_rig(widget_classes)
    _build_test_scene()
    if not LEVEL_SUBSYSTEM.save_current_level():
        raise RuntimeError("Unable to save {}".format(MAP_PATH))
    if DELETE_LEGACY_WIDGET_ASSETS:
        _delete_legacy_widget_assets()
    unreal.log("Sensor test map created: {}".format(MAP_PATH))


if __name__ == "__main__":
    main()
