<!--
/****************************************************************************
 *  Copyright (C) 2026 <COPYRIGHT HOLDER>
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

# EmBT

[Chinese](README_CN.md)

EmBT is an independently maintained project for extracting and porting the
Bluetooth Host stack from [Zephyr](https://github.com/zephyrproject-rtos/zephyr).

## Source

The initial codebase comes from Zephyr's `main` branch at commit
`f8da8ea370508f274685a25ff19dbe5a9ff54790`. The original Git history and
upstream copyright notices are preserved. Details are recorded in the
[initial source record](docs/upstream/INIT.md).

Selected upstream Bluetooth patches will be adopted with their original
messages and SHAs recorded.

## Current status

Directory extraction is tracked in [Draft PR #4](https://github.com/Frozen935/embt/pull/4),
with Bluetooth modules under `bluetooth/`, headers under `include/embt/`,
and general libraries under `lib/`. These changes are pending integration.
Header references still use `zephyr/`; reference migration, standalone build,
and OS adaptation are in progress.

## License

[Apache-2.0](bluetooth/LICENSE). Imported files retain their original
copyright and license notices.
