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

# Third-party dependencies

EmBT integrates one Mbed TLS crypto implementation through `EmBT::psa`.
Installed system libraries, explicitly prepared source, and existing platform
targets are selected by `EMBT_CRYPTO_SYSTEM`, `EMBT_CRYPTO_SOURCE`, and
`EMBT_CRYPTO_EXTERNAL`.

The Kconfig entry, adapter, source-build options and capability checks are
in `mbedtls/`. Root Kconfig sources `mbedtls/Kconfig`; AES ROM tables and
p256-m can be selected only for the explicitly built source library. Source releases
are pinned in [dependencies.lock.json](dependencies.lock.json). Prepare source
explicitly with:

```sh
python3 tools/dependencies/prepare_mbedtls.py
```

The prepared `mbedtls/upstream/` directory is ignored by Git. The build does not
download dependencies or install packages automatically. Existing libraries keep
their original configuration; source builds use provider-specific generated
headers rather than EmBT's configuration header.

Implementation details and validation are documented in
[CRYPTO_PROVIDER_DESIGN.md](../docs/CRYPTO_PROVIDER_DESIGN.md).
