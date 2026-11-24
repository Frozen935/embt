#!/usr/bin/env bash
# /****************************************************************************
#  *  Copyright (C) 2026 EmBT contributors
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  ***************************************************************************/


set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
  echo "Usage: $0 BUILD_DIR HCI_TCP_PORT [CONSOLE_PORT_BASE]" >&2
  exit 2
fi

embt_root=$(cd "$(dirname "$0")/../.." && pwd)
embt_build=$(realpath "$1")
embt_port=$2
embt_console=${3:-5710}
embt_state=${EMBT_EMULATOR_STATE:-"$embt_build/embt-emulator-state"}

for required in .config nuttx vela_data.bin vela_system.bin; do
  if [ ! -f "$embt_build/$required" ]; then
    echo "Missing native build artifact: $embt_build/$required" >&2
    exit 1
  fi
done

mkdir -p "$embt_state/home" "$embt_state/cache" "$embt_state/runtime"
chmod 700 "$embt_state/runtime"
export ANDROID_EMULATOR_HOME="$embt_state/home"
export ANDROID_USER_HOME="$embt_state/home"
export ANDROID_AVD_HOME="$embt_state/home/avd"
export XDG_CACHE_HOME="$embt_state/cache"
export XDG_RUNTIME_DIR="$embt_state/runtime"
export ADB_SERVER_PORT=$((embt_console + 3))

# Keep the Controller in QEMU's ordinary I/O event loop. The packaged
# Rootcanal MMIO character backend asserts during HCI initialization.
# This NuttX serial driver creates one device node per virtio device.
exec "$embt_root/emulator.sh" "$embt_build" \
  -no-window -no-snapshot -no-audio -gpu swiftshader_indirect \
  -feature -BluetoothEmulation \
  -ports "$embt_console,$((embt_console + 1))" -grpc "$((embt_console + 2))" \
  -qemu \
  -device virtio-serial-device,id=embtconsolebus,bus=virtio-mmio-bus.3 \
  -chardev null,id=embtconsole \
  -device virtconsole,chardev=embtconsole,bus=embtconsolebus.0,nr=0 \
  -device virtio-serial-device,id=embth4,bus=virtio-mmio-bus.7 \
  -chardev "socket,id=embtcontroller,host=127.0.0.1,port=$embt_port" \
  -device virtconsole,chardev=embtcontroller,bus=embth4.0,nr=0
