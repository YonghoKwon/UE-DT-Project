#!/usr/bin/env bash
# Run alongside PIE. Auto-discovers one PCD topic; does not start any UE stream.
# Read the entire script once. Editing this checkout during a long wait must not
# change the instructions an already running collector resumes from.
if [ "${ROS2_01_FROZEN:-}" != 1 ]; then
  export ROS2_01_FROZEN=1
  exec bash -c "$(cat -- "$0")" "$0" "$@"
fi
set -eo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=71 ROS_LOCALHOST_ONLY=1
report=$1
seconds=${2:-310}
mkdir -p "$report"
topic=''
for i in {1..120}; do
  # A previous probe subscriber can keep an old topic in the graph after its
  # publisher exited. Select a currently published topic, not a stale graph entry.
  while read -r candidate; do
    if ros2 topic info "$candidate" --spin-time 3 --no-daemon | grep '^Publisher count: [1-9]' > /dev/null; then
      topic=$candidate
      break
    fi
  done < <(ros2 topic list --spin-time 3 --no-daemon | grep -E '^/ma0t10/s_[0-9a-f]+/points$' || true)
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
