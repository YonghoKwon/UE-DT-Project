#!/usr/bin/env bash
set -euo pipefail
# Invoked only as root in the owned Ubuntu-24.04 WSL2 distribution. No global upgrade.
report=$1
decoder=${2:-no}
here=$(cd -- "$(dirname -- "$0")" && pwd)
export DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8 PIP_NO_CACHE_DIR=1
test "$(id -u)" = 0
. /etc/os-release
test "$VERSION_ID" = 24.04
mkdir -p "$report"
# Scope version preferences to this disposable distro. This also pins bootstrap
# dependencies before the ROS repository is installed.
while IFS='=' read -r package pinned; do
  printf 'Package: %s\nPin: version %s\nPin-Priority: 1001\n\n' "$package" "$pinned"
done < "$here/apt-packages.lock" > /etc/apt/preferences.d/ma0t10-ros2
# Keep downloaded apt archives until their byte count has been recorded, then clean.
trap 'apt-get clean; rm -f /tmp/ros2-apt-source.deb' EXIT
apt-get update > "$report/apt-update.log" 2>&1
bootstrap=()
for package in ca-certificates curl python3-venv; do
  pin=$(grep -E "^${package}=" "$here/apt-packages.lock")
  test -n "$pin"
  bootstrap+=("$pin")
done
apt-get -y --no-install-recommends install "${bootstrap[@]}" > "$report/apt-bootstrap.log" 2>&1
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["ros_apt_source_version"])' "$here/environment-lock.json")
curl --fail --location --silent --show-error \
  "https://github.com/ros-infrastructure/ros-apt-source/releases/download/$version/ros2-apt-source_${version}.noble_all.deb" \
  -o /tmp/ros2-apt-source.deb
stat -c %s /tmp/ros2-apt-source.deb > "$report/ros-source-download-bytes.txt"
dpkg -i /tmp/ros2-apt-source.deb > "$report/apt-source.log" 2>&1
apt-get update > "$report/apt-ros-update.log" 2>&1
packages=(ros-jazzy-ros-base ros-jazzy-foxglove-bridge)
if [ "$decoder" = yes ]; then packages+=(python3-pil); fi
mapfile -t resolved < "$here/apt-packages.lock"
for package in "${packages[@]}"; do
  pinned=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["apt"].get(sys.argv[2],""))' "$here/environment-lock.json" "$package")
  if [ -z "$pinned" ]; then pinned=$(apt-cache policy "$package" | awk '/Candidate:/ {print $2}'); fi
  test -n "$pinned" && test "$pinned" != '(none)'
  if ! printf '%s\n' "${resolved[@]}" | grep -Fx "$package=$pinned" > /dev/null; then
    resolved+=("$package=$pinned")
  fi
done
printf '%s\n' "${resolved[@]}" > "$report/apt-direct-lock.txt"
# ROS dependencies from official Ubuntu/ROS repositories only. No DDS or desktop extras.
apt-get -y --no-install-recommends install "${resolved[@]}" > "$report/apt-install.log" 2>&1
dpkg-query -W -f='${Package}=${Version}\n' > "$report/apt-installed-lock.txt"
python3 -m venv --system-site-packages /opt/ma0t10-ros2-venv
mkdir -p /tmp/ma0t10-pip
/opt/ma0t10-ros2-venv/bin/pip download --no-deps --dest /tmp/ma0t10-pip wheel==0.45.1 > "$report/pip-build-download.log" 2>&1
/opt/ma0t10-ros2-venv/bin/pip install --no-index --find-links /tmp/ma0t10-pip wheel==0.45.1 > "$report/pip-build-install.log" 2>&1
/opt/ma0t10-ros2-venv/bin/pip download --no-deps --no-build-isolation --dest /tmp/ma0t10-pip \
  stomp.py==8.2.0 docopt==0.6.2 websocket-client==1.8.0 > "$report/pip-download.log" 2>&1
find /tmp/ma0t10-pip -type f -printf '%s\n' | awk '{n+=$1} END {print n+0}' > "$report/pip-download-bytes.txt"
/opt/ma0t10-ros2-venv/bin/pip install --no-index --no-build-isolation --find-links /tmp/ma0t10-pip \
  stomp.py==8.2.0 docopt==0.6.2 websocket-client==1.8.0 > "$report/pip-install.log" 2>&1
rm -f /tmp/ma0t10-pip/*
rmdir /tmp/ma0t10-pip
/opt/ma0t10-ros2-venv/bin/pip freeze > "$report/pip-installed-lock.txt"
apt-get clean
echo 'ROS2-01 installation complete; apt/pip caches cleaned.'
