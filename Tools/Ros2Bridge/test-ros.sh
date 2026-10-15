#!/usr/bin/env bash
set -eo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=71 ROS_LOCALHOST_ONLY=1
export MA0T10_STOMP_HOST=${MA0T10_STOMP_HOST:-$(ip -4 route show default | awk '{print $3; exit}')}
exec /opt/ma0t10-ros2-venv/bin/python "$here/ros_golden_check.py" -v
