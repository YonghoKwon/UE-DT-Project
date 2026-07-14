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
    raise RuntimeError(
        "Unable to set {}.{}: {}".format(obj.get_name(), cpp_name, last_error)
    )


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


def _ensure_monitor_widget_class(content_root=CONTENT_ROOT):
    monitor_widget_path = content_root + "/UI/WBP_VirtualSensorMonitor"
    monitor_widget_class_path = (
        monitor_widget_path + ".WBP_VirtualSensorMonitor_C"
    )
    if not unreal.EditorAssetLibrary.does_asset_exist(monitor_widget_path):
        factory = unreal.WidgetBlueprintFactory()
        _set_property(
            factory,
            "ParentClass",
            _load_class("/Script/ma0t10_dt.VirtualSensorMonitorWidget"),
        )
        widget_blueprint = ASSET_TOOLS.create_asset(
            "WBP_VirtualSensorMonitor",
            content_root + "/UI",
            unreal.WidgetBlueprint,
            factory,
        )
        if not widget_blueprint:
            raise RuntimeError(
                "Unable to create {}".format(monitor_widget_path)
            )
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            widget_blueprint, only_if_is_dirty=False
        ):
            raise RuntimeError(
                "Unable to save {}".format(monitor_widget_path)
            )

    return _load_class(monitor_widget_class_path)


def _prepare_level():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not LEVEL_SUBSYSTEM.load_level(MAP_PATH):
            raise RuntimeError("Unable to load {}".format(MAP_PATH))
    else:
        if not LEVEL_SUBSYSTEM.new_level(MAP_PATH):
            raise RuntimeError("Unable to create {}".format(MAP_PATH))
    _remove_managed_actors()


def _build_sensor_rig(monitor_widget_class):
    manager_class = _load_class("/Script/ma0t10_dt.VirtualSensorManager")
    lidar_actor_class = _load_class("/Script/ma0t10_dt.VirtualSensorAct")
    camera_actor_class = _load_class("/Script/ma0t10_dt.VirtualCameraAct")
    host_actor_class = _load_class("/Script/ma0t10_dt.VirtualSensorMonitorHostActor")

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
        unreal.Vector(0.0, 0.0, 180.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    lidar = lidar_actor.get_component_by_class(
        _load_class("/Script/ma0t10_dt.VirtualLidarSensorComp")
    )
    _set_property(lidar, "SensorId", "LIDAR-TEST-001")
    _set_property(lidar, "bAutoStartScan", True)
    _set_property(lidar, "bAutoRegisterToManager", True)
    _set_property(lidar, "bUseMultiHit", False)
    _set_property(lidar, "bDrawDebugRays", False)
    _set_property(lidar, "ServerPayloadStride", 1)
    _set_property(lidar, "MaxServerPayloadPoints", 0)
    _set_property(lidar, "bIncludeMissPointsInServerPayload", False)
    _set_property(lidar, "PreviewPointStride", 2)
    _set_property(lidar, "MaxPreviewPoints", 3000)
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
        unreal.Vector(0.0, -450.0, 300.0),
        unreal.Rotator(roll=0.0, pitch=-8.5, yaw=26.5),
    )
    camera = camera_actor.get_component_by_class(
        _load_class("/Script/ma0t10_dt.VirtualCameraComp")
    )
    _set_property(camera, "SensorId", "VCAM-TEST-001")
    _set_property(camera, "bAutoStartCapture", True)
    _set_property(camera, "bAutoRegisterToManager", True)

    host = _spawn(
        host_actor_class,
        "SensorTest_MonitorHost",
        unreal.Vector(0.0, 0.0, 120.0),
    )
    _set_property(host, "MonitorWidgetClass", monitor_widget_class)
    _set_property(host, "bUseNativeMonitorWidgetFallback", True)
    _set_property(host, "SensorManager", manager)
    _set_property(host, "bAutoCreateOnBeginPlay", True)
    _set_property(host, "bAutoFindSensorManager", True)
    _set_property(host, "bAddToViewport", True)
    _set_property(host, "bShowLidarViewOnStart", False)
    _set_property(host, "bEnablePointCloudOnlyOnStart", False)
    _set_property(host, "ViewportZOrder", 10)
    _set_property(host, "bConfigurePlayerInputOnCreate", True)
    _set_property(host, "bShowMouseCursorOnCreate", True)


def _build_test_scene():
    _spawn_static_mesh(
        "SensorTest_Floor",
        unreal.Vector(600.0, 0.0, -60.0),
        unreal.Vector(14.0, 10.0, 1.0),
        "/Engine/BasicShapes/Cube.Cube",
        ["KeepInPointCloudOnly", "SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_TargetWall",
        unreal.Vector(1100.0, 0.0, 240.0),
        unreal.Vector(0.4, 6.0, 3.0),
        "/Engine/BasicShapes/Cube.Cube",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_TargetCube",
        unreal.Vector(700.0, -260.0, 80.0),
        unreal.Vector(1.4, 1.4, 1.4),
        "/Engine/BasicShapes/Cube.Cube",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_TargetSphere",
        unreal.Vector(850.0, 260.0, 120.0),
        unreal.Vector(1.6, 1.6, 1.6),
        "/Engine/BasicShapes/Sphere.Sphere",
        ["SensorTestTarget"],
    )
    _spawn_static_mesh(
        "SensorTest_TargetPillar",
        unreal.Vector(500.0, 320.0, 160.0),
        unreal.Vector(1.2, 1.2, 3.2),
        "/Engine/BasicShapes/Cylinder.Cylinder",
        ["SensorTestTarget"],
    )

    directional = _spawn(
        unreal.DirectionalLight,
        "SensorTest_DirectionalLight",
        unreal.Vector(0.0, 0.0, 800.0),
        unreal.Rotator(roll=0.0, pitch=-45.0, yaw=-35.0),
    )
    directional_component = directional.get_component_by_class(
        unreal.DirectionalLightComponent
    )
    directional_component.set_editor_property("intensity", 7.5)

    skylight = _spawn(
        unreal.SkyLight,
        "SensorTest_SkyLight",
        unreal.Vector(0.0, 0.0, 500.0),
    )
    skylight_component = skylight.get_component_by_class(unreal.SkyLightComponent)
    skylight_component.set_editor_property("intensity", 1.0)

    _spawn(
        unreal.PlayerStart,
        "SensorTest_PlayerStart",
        unreal.Vector(-500.0, -700.0, 120.0),
        unreal.Rotator(roll=0.0, pitch=0.0, yaw=35.0),
    )


def main():
    monitor_widget_class = _ensure_monitor_widget_class()
    _prepare_level()
    _build_sensor_rig(monitor_widget_class)
    _build_test_scene()
    if not LEVEL_SUBSYSTEM.save_current_level():
        raise RuntimeError("Unable to save {}".format(MAP_PATH))
    unreal.log("Sensor test map created: {}".format(MAP_PATH))


main()
