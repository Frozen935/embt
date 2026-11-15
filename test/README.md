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

# EmBT tests

The current commands, profiles and validation boundaries are maintained in the
[testing guide](../docs/TESTING.md). Current results and deferred work are in
[STATUS](../docs/STATUS.md); this documentation change does not rerun tests.

Run real OSI contract tests with the installed cmocka provider:

```sh
./mkbt.sh test --build-dir build/osi-tests
```

New build directories enable ASan/UBSan by default. Existing caches retain their
selection; see the [build guide](../docs/BUILDING.md) for providers and options.
The OSI tests use OSI, standard C and cmocka; native workers publish results and
the controlling thread owns assertions. Timer storage obeys the non-draining
native notification contract. See the [OSI contract](../docs/OSI_CONTRACT_TESTS.md).

[Bluetooth tests](bluetooth/README.md) include C, configuration variants and
Classic/Bumble integration. Platform/filesystem/audio tests have separate
requirements. Passing a model or skipping a case is not physical radio validation.
