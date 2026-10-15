#!/usr/bin/env bash
set -euo pipefail
action=$1
report=$2
here=$(cd -- "$(dirname -- "$0")" && pwd)
runtime=/run/ma0t10-ros2
mkdir -p "$runtime" "$report"
pid_alive() { kill -0 "$1" 2>/dev/null && test "$(awk '{print $3}' "/proc/$1/stat" 2>/dev/null)" != Z; }
alive() { test -f "$runtime/$1.pid" && pid_alive "$(cat "$runtime/$1.pid")"; }
case "$action" in
  up)
    if alive bridge || alive foxglove; then echo 'Already running; down first.'; exit 1; fi
    : "${MA0T10_STOMP_USER:?Set process environment MA0T10_STOMP_USER}"
    : "${MA0T10_STOMP_PASSWORD:?Set process environment MA0T10_STOMP_PASSWORD}"
    export ROS_DOMAIN_ID=71 ROS_LOCALHOST_ONLY=1
    export MA0T10_STOMP_HOST=${MA0T10_STOMP_HOST:-$(ip -4 route show default | awk '{print $3; exit}')}
    # ROS setup scripts are not nounset-safe.
    set +u
    source /opt/ros/jazzy/setup.bash
    set -u
    nohup /opt/ma0t10-ros2-venv/bin/python "$here/bridge_node.py" --metrics "$report/bridge-metrics.json" \
      > "$report/bridge.log" 2>&1 < /dev/null &
    echo $! > "$runtime/bridge.pid"
    nohup /opt/ros/jazzy/lib/foxglove_bridge/foxglove_bridge --ros-args -p port:=8765 -p address:=0.0.0.0 \
      > "$report/foxglove.log" 2>&1 < /dev/null &
    echo $! > "$runtime/foxglove.pid"
    sleep 2
    alive bridge && alive foxglove
    echo 'Bridge and Foxglove started. ROS_DOMAIN_ID=71; ROS_LOCALHOST_ONLY=1.'
    ;;
  down)
    for name in bridge foxglove; do
      if alive "$name"; then
        pid=$(cat "$runtime/$name.pid")
        # PID files exist only in this disposable distro. Let rclpy and launch stop normally.
        if [ "$name" = foxglove ]; then kill -TERM "$pid"; else kill -INT "$pid"; fi
        for i in {1..150}; do pid_alive "$pid" || break; sleep .1; done
        if pid_alive "$pid"; then echo "$name did not stop; refusing forced kill"; exit 1; fi
      fi
      rm -f "$runtime/$name.pid"
    done
    echo 'Owned ROS processes stopped.'
    ;;
  status)
    for name in bridge foxglove; do
      if alive "$name"; then echo "$name running"; else echo "$name stopped"; fi
    done
    ;;
  *) exit 2 ;;
esac
