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

# Upstream Bluetooth regression tests

The tests originate from Zephyr commit
`f8da8ea370508f274685a25ff19dbe5a9ff54790`. Original test history, directory
relocation and subsequent adaptation remain separately reviewable. Baseline
and retention notes are maintained in the [upstream record](../../docs/upstream/BLUETOOTH_TEST_BASELINE.md)
and [case inventory](../../docs/upstream/BLUETOOTH_TEST_CASES_F8DA8EA.json).
Controller, original driver/ISR, board/YAML/overlay and retired
Flash-provider tests are excluded; Monitor uses real memory/file output sinks.

The [testing guide](../../docs/TESTING.md) owns current commands and profiles:

- C units use cmocka and standalone FFF where the upstream call fakes require it.
- Functional variants verify actual configuration and required case execution.
- Classic pytest runs real EmBT applications against Bumble Controller/peer models.
- Physical Controller tests require explicit device ownership and privileges.

Each directory retains its CMake, config.conf and test.json responsibilities;
the compiled executable provides the actual case inventory. No board/YAML runner
or source-presence success is used. Current selector, response-observation and
execution-gate issues are recorded in [STATUS](../../docs/STATUS.md).

Current results do not overwrite original failures, and deferred cases are not
counted as passes. Local planning archives and per-directory provenance pages
are excluded from repository delivery.
