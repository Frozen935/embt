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

Directory extraction, header migration, standalone builds and POSIX adaptation
are implemented on local work branches. Full M1 wireless acceptance is pending.
The current task is documentation consolidation; code follow-up and integration
remain deferred. See the [documentation index](docs/README.md) and
[status and worklist](docs/STATUS.md).

## License

[Apache-2.0](LICENSE). Imported files retain their original
copyright and license notices.
