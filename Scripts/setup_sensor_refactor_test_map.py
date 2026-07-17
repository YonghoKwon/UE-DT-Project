"""Build the V2-only regression map without changing the default startup map."""

import importlib.util
import os


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILDER_PATH = os.path.join(SCRIPT_DIR, "setup_sensor_test_map.py")
SPEC = importlib.util.spec_from_file_location("ma0t10_sensor_map_builder", BUILDER_PATH)
BUILDER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILDER)

BUILDER.MAP_PATH = "/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"
BUILDER.DELETE_LEGACY_WIDGET_ASSETS = False
BUILDER.main()
