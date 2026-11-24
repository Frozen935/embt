<!--
/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/
-->

# OpenVela external integration

This directory contains deployment files for the OpenVela external tree. The
Host, H4 transport, shared POSIX backend and configuration driver remain in
EmBT's normal source directories. One EmBT checkout is used by the library,
btshell and test applications.

The verified target is ARMv7-A goldfish, with builtin applications and Ninja
through the OpenVela `build.sh --cmake` wrapper. Legacy Make, protected/kernel
builds, modular `m` applications and other boards are not validated.

## Install in an OpenVela workspace

Prepare the existing platform Mbed TLS and cmocka sources explicitly. This port
uses their platform targets and configuration; it does not fetch replacement
crypto or test providers.

Create the following deployment layout in a task workspace:

```text
external/embt/
  CMakeLists.txt      <- integration/openvela/CMakeLists.txt
  Kconfig            <- integration/openvela/Kconfig
  run_goldfish.sh    <- integration/openvela/run_goldfish.sh
  .gitignore        <- integration/openvela/.gitignore
  source/            -> the single EmBT checkout
```

The four deployment files may be copied or linked. Run them through their
OpenVela deployment paths; their relative paths refer to the external tree.
The source link and local build/settings/emulator files must not be committed.
The source checkout includes this integration directory; it is not duplicated.

In OpenVela `external/CMakeLists.txt`, keep `nuttx_add_subdirectory()` and replace
its final `nuttx_generate_kconfig(MENUDESC "External")` call with:

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/embt/source/integration/openvela/external_menu.cmake")
embt_generate_external_kconfig()
```

This helper generates source entries using the existing NuttX menu naming
convention, preserves recursive submenus, and chooses one Host tree. The `embt`
profile excludes zblue; other profiles exclude EmBT. Kconfig still uses the
platform evaluator. No NuttX source modification or EXCLUDE extension is needed.
Use a fresh build directory when changing Host profiles, because platform menus
are generated during the first configuration pass.

Copy `configs/goldfish-armv7a/defconfig` to
`vendor/openvela/boards/vela/configs/embt/defconfig`. In the existing vendor
`boards/build/vendorsetup.sh`, preserve its entries and add:

```sh
source "$(gettop)/external/embt/source/integration/openvela/vendor_hook.sh"
```

The fragment registers the lunch target and rejects missing prepared providers
before the build starts. Do not replace the original vendor setup file.

## Configure and build

From the OpenVela root, use an isolated output and ccache directory:

```sh
CCACHE_DIR=/tmp/embt-ccache CCACHE_TEMPDIR=/tmp/embt-ccache/tmp \
./build.sh vendor/openvela/boards/vela/configs/embt --cmake \
  -b /tmp/openvela-embt -j4
```

Change configuration through the platform `setconfig`, then persist with the
same wrapper's `savedefconfig` target. Do not set CONFIG values in the CMake
cache, define platform macros by hand or create a separate EmBT .config.
The existing EmBT driver exports the evaluated native .config and rejects
standalone configuration operations in native output directories.

The profile uses platform Mbed TLS as EXTERNAL and directly opens the configured
H4 device. It requests 8192-byte system/timeout stacks and uses
`/data/embt.settings`; shared source defaults and priority numbering are retained.
Library compilation and btshell registration are separate targets in external.

## Run the native H4 shell

From the EmBT source root, start the existing Controller model in a Python environment containing the
requirements from `test/bluetooth/classic/requirements.txt`:

```sh
python test/bluetooth/classic/bumble/common/controllers.py tcp-server:127.0.0.1:0
```

Read its ready line for the allocated TCP port. Then run the deployed helper:

```sh
external/embt/run_goldfish.sh /tmp/openvela-embt CONTROLLER_PORT 5710
```

The helper calls OpenVela `emulator.sh`. It creates two virtio MMIO serial
**devices**, which the verified driver exposes as ttyV0 and ttyHCI0. The HCI
socket character backend connects to the existing Controller; EmBT itself only
uses H4 device open/read/write/poll. No kernel Bluetooth driver is involved.
The Controller model is not a physical radio or an interoperability certificate.

The packaged Rootcanal MMIO character backend triggered a QEMU iothread-lock
assertion during initialization in the verified emulator. That failure remains
open; the socket-backed model path is a separate validation result.

In NSH, inspect `help`, `ls /dev`, and run `embt_crypto_probe`, then `btshell`.
Within btshell, `bt init sync` performs actual Controller/settings initialization.
The verified command sequence includes advertising/scanning start and stop,
BR pscan/iscan, inquiry, `bt disable`, and `quit`. It uses one Controller and
therefore does not prove peer connection, pairing or data profile operation.

Run base work tests in a fresh boot. Reusing shared static base runtime after
btshell's task group exits blocks the work application in the verified tree.
No Host lock barriers, scheduling interception or new skip was added.

## Validation and limitations

The native validation executed OSI cases (23 passed, one timer-remaining failure),
base work contracts (8/8 in a fresh boot), PSA operations, H4 shell commands,
and FATFS settings save/load/delete including cross-boot recovery. Linux
validation includes OSI 24/24, default C 1085 passed / 19 existing skips, and
202/202 test-port entries. Case and CTest totals are different measures.

The original timer upper-bound assertion, Rootcanal failure and static-runtime
reuse limitation remain open. Failed and unperformed checks are not counted as
passing. Firmware compilation does not establish all Bluetooth or native target
features. See [the report](../../docs/NUTTX_OPENVELA_PORT.md) and
[upstream/provider baselines](../../docs/upstream/OPENVELA_BASELINE.md) for the
exact versions and remaining scope.
