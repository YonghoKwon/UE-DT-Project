#!/usr/bin/env bash
# Named distro only: wsl -d ma0t10-ros2 -u root -- bash <this> topic list
set -eo pipefail
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=71 ROS_LOCALHOST_ONLY=1
exec ros2 "$@"
