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

# OSI tests

Run all POSIX OSI tests with the installed cmocka 1.1.7:

```sh
./mkbt.sh test
```

The default test directory is `build/osi-tests`. Test code belongs in `test/osdep/`.
Each CTest case runs in its own process and uses the real OSI backend objects.
Native workers publish results; the cmocka controlling thread owns assertions.

Prepare the pinned source explicitly, then select SOURCE:

```sh
python3 tools/dependencies/prepare_cmocka.py
./mkbt.sh test --build-dir build/osi-source \
  --cmake-arg -DEMBT_CMOCKA_PROVIDER=SOURCE
```

Run ASan and UBSan in a separate build:

```sh
./mkbt.sh test --build-dir build/osi-asan \
  --cmake-arg -DEMBT_OSI_SANITIZER=ADDRESS
```

`EMBT_OSI_SANITIZER=THREAD` selects a separate TSan build when the host supports it.
Do not combine it with ADDRESS. Test selection is explicit through CTest; unknown
case names fail. The build directory contains the case manifest, generated
configuration, CTest logs and JUnit output when the installed CTest supports it.
Timer storage remains valid until process exit because native deletion does not
drain callbacks. Fatal tests use fork and exec before calling the backend.

The implementation and validation records are maintained in
[OSDEP_TEST_PLAN.md](../docs/OSDEP_TEST_PLAN.md) and
[OSDEP_TEST_RESULTS.md](../docs/OSDEP_TEST_RESULTS.md).
