#!/usr/bin/env bash
# Run alongside PIE. Auto-discovers one PCD topic; does not start any UE stream.
set -eo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=71 ROS_LOCALHOST_ONLY=1
report=$1
seconds=${2:-310}
mkdir -p "$report"
topic=''
for i in {1..120}; do
  topic=$(ros2 topic list | grep -E '^/ma0t10/s_[0-9a-f]+/points$' | head -1 || true)
  test -z "$topic" || break
  sleep 1
done
test -n "$topic"
printf '%s\n' "$topic" > "$report/ros-topic.txt"
timeout --signal=INT "$seconds" ros2 topic hz "$topic" --window 6000 --wall-time > "$report/ros2-topic-hz.log" 2>&1 &
hzpid=$!
timeout --signal=INT "$seconds" ros2 topic bw "$topic" --window 100 > "$report/ros2-topic-bw.log" 2>&1 &
bwpid=$!
trap 'kill -INT "$hzpid" "$bwpid" 2>/dev/null || true' INT TERM
wait "$hzpid" || test $? = 124
wait "$bwpid" || test $? = 124
