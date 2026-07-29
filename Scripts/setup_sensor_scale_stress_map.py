"""Create the repository-owned large-scene sensor stress map.

The script duplicates SensorRefactorTestMap, then adds one HISM-based stress
fixture containing 10,000 static primitives and 1,000 moving proxies.
"""

import os
import unreal


SOURCE_MAP = "/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"
STRESS_MAP = "/Game/MA0T10/Maps/Tests/SensorScaleStressMap"
STRESS_LABEL = "SensorScaleStress_10K_1K"
MANAGED_TAG = unreal.Name("SensorTestManaged")


def main():
    stress_map_file = os.path.join(
        unreal.Paths.project_content_dir(),
        "MA0T10",
        "Maps",
        "Tests",
        "SensorScaleStressMap.umap",
    )
    if not os.path.isfile(stress_map_file):
        if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, STRESS_MAP):
            raise RuntimeError("Failed to duplicate {} to {}".format(SOURCE_MAP, STRESS_MAP))
        if not unreal.EditorAssetLibrary.save_asset(STRESS_MAP, only_if_is_dirty=False):
            raise RuntimeError("Failed to save duplicated stress map {}".format(STRESS_MAP))
        unreal.log("Created {}; run this script once more to populate the stress fixture".format(STRESS_MAP))
        return

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level_subsystem.load_level(STRESS_MAP):
        raise RuntimeError("Failed to load {}".format(STRESS_MAP))

    stress_class = unreal.load_class(None, "/Script/ma0t10_dt.VirtualSensorStressSceneActor")
    if not stress_class:
        raise RuntimeError("VirtualSensorStressSceneActor class is unavailable; build the Editor target first")

    stress_actor = None
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label() == STRESS_LABEL:
            stress_actor = actor
            break

    if stress_actor is None:
        stress_actor = actor_subsystem.spawn_actor_from_class(stress_class, unreal.Vector(0.0, 0.0, -100.0))
        if not stress_actor:
            raise RuntimeError("Failed to spawn stress fixture")
        stress_actor.set_actor_label(STRESS_LABEL)
        stress_actor.tags = [MANAGED_TAG, unreal.Name("VirtualSensorScaleStress")]

    stress_actor.set_editor_property("static_primitive_count", 10000)
    stress_actor.set_editor_property("moving_proxy_count", 1000)
    stress_actor.generate_stress_scene()

    if not level_subsystem.save_current_level():
        raise RuntimeError("Failed to save {}".format(STRESS_MAP))

    unreal.log("Saved {} with {} static primitives and {} moving proxies".format(
        STRESS_MAP,
        stress_actor.get_editor_property("static_primitive_count"),
        stress_actor.get_editor_property("moving_proxy_count"),
    ))


main()
