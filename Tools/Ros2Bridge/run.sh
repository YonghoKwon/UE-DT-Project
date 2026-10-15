#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0  # Process-local: credential-bearing Python processes must never dump memory.
action=$1
report=$2
here=$(cd -- "$(dirname -- "$0")" && pwd)
runtime=/run/ma0t10-ros2
mkdir -p "$runtime" "$report"
pid_alive() { kill -0 "$1" 2>/dev/null && test "$(awk '{print $3}' "/proc/$1/stat" 2>/dev/null)" != Z; }
alive() { test -f "$runtime/$1.pid" && pid_alive "$(cat "$runtime/$1.pid")"; }
case "$action" in
  up)
    if alive bridge || alive foxglove || alive control; then echo 'Already running; down first.'; exit 1; fi
    : "${MA0T10_STOMP_USER:?Set process environment MA0T10_STOMP_USER}"
    : "${MA0T10_STOMP_PASSWORD:?Set process environment MA0T10_STOMP_PASSWORD}"
    bridge_args=()
    if [ "${4:-no}" = yes ]; then bridge_args+=(--publish-clock); fi
    if [ "${3:-no}" = yes ]; then : "${MA0T10_AUTOMATION_TOKEN:?Set token environment for ROS2 control}"; fi
    export ROS_DOMAIN_ID=71 ROS_LOCALHOST_ONLY=1
    export MA0T10_STOMP_HOST=${MA0T10_STOMP_HOST:-$(ip -4 route show default | awk '{print $3; exit}')}
    # ROS setup scripts are not nounset-safe.
    set +u
    source /opt/ros/jazzy/setup.bash
    set -u
    nohup /opt/ma0t10-ros2-venv/bin/python "$here/bridge_node.py" "${bridge_args[@]}" --metrics "$report/bridge-metrics.json" \
      > "$report/bridge.log" 2>&1 < /dev/null &
    echo $! > "$runtime/bridge.pid"
    nohup /opt/ros/jazzy/lib/foxglove_bridge/foxglove_bridge --ros-args -p port:=8765 -p address:=0.0.0.0 \
      > "$report/foxglove.log" 2>&1 < /dev/null &
    echo $! > "$runtime/foxglove.pid"
    if [ "${3:-no}" = yes ]; then
      nohup /opt/ma0t10-ros2-venv/bin/python "$here/control_node.py" --api-host "$(ip -4 route show default | awk '{print $3; exit}')" \
        --lidar "$5" --capture-sensor "$6" --relay-port "$7" --api-port "$8" \
        > "$report/control.log" 2>&1 < /dev/null &
      echo $! > "$runtime/control.pid"
    fi
    sleep 2
    alive bridge && alive foxglove
    if [ "${3:-no}" = yes ]; then alive control; fi
    echo 'Bridge and Foxglove started. ROS_DOMAIN_ID=71; ROS_LOCALHOST_ONLY=1.'
    ;;
  down)
    for name in control bridge foxglove; do
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
    for name in control bridge foxglove; do
      if alive "$name"; then echo "$name running"; else echo "$name stopped"; fi
    done
    ;;
  *) exit 2 ;;
esac
