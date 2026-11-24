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

# OpenVela port baseline

This port uses existing local repositories. It does not import another copy of
EmBT or update an integration branch. The initial full goldfish preset triggered the
platform SpeexDSP 1.2.1 download; that source is excluded from delivery and the
raw log is retained.

| Repository | Starting commit | Task worktree |
| --- | --- | --- |
| EmBT | `be49a6da685351b933d0228835eccb0564a41e67` | `/tmp/embt-agents/nuttx-openvela-port` |
| OpenVela NuttX | `9eb6537886a576edf35402e72a410de4ec570b8c` | `/tmp/embt-agents/openvela-nuttx-port/nuttx` |
| OpenVela apps | `17304318047a5e98938d40e833252d4603a9c922` | `/tmp/embt-agents/openvela-nuttx-port/apps` |
| OpenVela vendor/openvela | `c01cc0254cec8c4839234217e51deb647b577f14` | `/tmp/embt-agents/openvela-nuttx-port/vendor/openvela` |
| OpenVela external | `d4eb24208aee0a0413e8f2ec9583666cb64f179e` | `/tmp/embt-agents/openvela-nuttx-port/external` |

EmBT `origin/main` was fetched and verified at task startup. The upstream
Bluetooth import baseline remains `f8da8ea370508f274685a25ff19dbe5a9ff54790`.
All five worktrees use `task/nuttx-openvela-port`. Publication is authorized for the EmBT task branch. Runtime logs identify the starting commits plus local changes. The delivery
commit series and final head are recorded separately in the draft pull request.

The original workspace is `/home/frozen/work/opensource/vela-opensource`.
Unmodified nested repositories and prepared dependency sources are referenced
from the isolated worktrees. In particular, the complete zblue repository is
present at `external/zblue/zblue`, commit
`1fe758b09c76592678a18f13893aca913a9835bb`; its configuration and compilation are
excluded by the EmBT profile, rather than relying on missing source files.

| Provider | Existing source or installation | Observed version |
| --- | --- | --- |
| Native Mbed TLS | `apps/crypto/mbedtls/mbedtls` with the platform configuration and existing patches | 3.4.0 |
| Native cmocka | `apps/testing/cmocka/cmocka` with the platform patches | 1.1.5 |
| Linux Mbed TLS | installed `mbedcrypto` | 2.28.8 |
| Linux cmocka | installed library | 1.1.7 |
| Controller model | existing EmBT Classic model and existing Python environment | Bumble 0.0.220 |

The native Mbed TLS target is the EXTERNAL provider. Its source and configuration
headers are preserved. Codec providers are not enabled in this initial profile.
Detailed commands, results and limitations are in
[the port report](../NUTTX_OPENVELA_PORT.md). Raw artifacts remain under
`/tmp/embt-agents/nuttx-openvela-evidence/`.

The revised build uses the OpenVela build repository at
`b76ce5b5ab3c81166faa44adfbba6c3d03647b3f`, its build.sh/envsetup.sh CMake path,
and the installed emulator/QEMU 32.1.15.0. Shell integration is now entirely in
external/embt. Source H4 contract reference:
external/zblue/zblue/port/drivers/bluetooth/hci/h4.c at the zblue commit above.
The Linux socket endpoint mapping and apps registration from revision 1 were
withdrawn locally. Revision 1 raw snapshots remain outside Git under /tmp.

The submission packages deployment files in integration/openvela and implements
Host menu selection there. The experimental nuttx/cmake/nuttx_kconfig.cmake
EXCLUDE extension is restored to the original source and is not delivered.
The original NuttX commit above is used for the packaging verification build.
