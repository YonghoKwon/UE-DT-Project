#!/usr/bin/env bash
set -eo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=71 ROS_LOCALHOST_ONLY=1
exec /opt/ma0t10-ros2-venv/bin/python "$here/probe_ros.py" "$@"
