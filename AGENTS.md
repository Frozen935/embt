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

# Agent Submission and Review Rules

These rules apply to agent work in this repository, including code,
configuration, documentation, and repository policy changes. They take
precedence over conflicting inherited upstream submission workflows.

## 1. Work branches and human-only merging

- Prepare changes on a task-specific work branch. Do not create new commits
  on the default branch, protected branches, or shared integration branches.
- When submission is authorized, push only the work branch and open a PR.
  A request to submit or push changes does not authorize integration.
- Do not directly update integration branches through Git, GitHub APIs,
  browser actions, scripts, or other automation.
- Do not merge PRs, enable auto-merge, schedule merges, or approve your own
  delivery on behalf of the human reviewer. Final approval and merging are
  performed by a human.
- Do not bypass these restrictions by changing credentials, branch
  protections, review requirements, or repository permissions.
- Report a submitted PR as awaiting human review and merge. Do not report
  a branch push or PR creation as a completed merge.

### Pre-runtime integration hold

- Keep all open and future bring-up PRs in draft state and unmerged until
  the agreed Bluetooth runtime baseline has been demonstrated and its
  validation evidence recorded.
- Do not merge, enable auto-merge, or mark a PR ready for integration while
  this hold is active. Passing configuration, compilation, or lint checks
  alone does not demonstrate a working runtime.
- Continue development locally on task branches. A follow-up branch may
  start from the required unmerged work branch so the accumulated changes
  remain available for debugging without updating main.
- Keep new development changes local and unstaged until the user explicitly
  requests submission. Publish only the changes covered by that request;
  subsequent changes remain local until another submission instruction.
- When submission is requested, update the original task patch and draft PR,
  stating its base branch and dependencies. Do not wait for a main-branch
  merge to continue local work.
- After the runtime baseline is validated, integration still requires
  human review and a human decision to release the hold. Agents never
  perform the final merge.

## 2. Submission scope and documentation

- Include only the files required by the current task.
- Publish project documentation and rule updates through their existing
  documentation PRs. Keep these changes out of code and directory PRs;
  document pending implementation on its work branch without merging it.
- Keep local planning documents and unapproved documentation drafts out of
  code and directory-relocation PRs unless explicitly requested.
- Preserve existing project documentation when the task does not change it.
- Stage explicit paths. Do not use broad staging commands that can include
  unrelated drafts, generated files, or local artifacts.
- For updates to the same unmerged work area, amend or consolidate its
  original task commit and update the existing PR. Do not append a separate
  patch or open another PR for corrections within that work area.
- Use separate commits and PRs for independent work areas. Preserve upstream
  commit messages and provenance when importing community patches.
- A directory-only change must preserve file contents, permissions,
  symbolic links, copyright notices, and upstream comments.

## 3. English-only repository communication

- Write all agent-authored commit titles and bodies in English.
- Write all PR titles and descriptions in English.
- Write all agent-authored code comments, commit comments, PR comments,
  issue comments, and review comments in English.
- Do not include Chinese text in these fields or comments.
- Preserve existing upstream source and provenance. Do not rewrite imported
  files merely to normalize their language.

## 4. Commit titles and detailed descriptions

Every commit must have a descriptive English title and a meaningful body.
The body must explain:

- The problem or purpose of the change.
- The files, components, or behavior changed and the reason for the approach.
- The validation performed and any validation that was not performed.

Do not use an empty body, placeholder text, or a title alone as a submission.
Keep independent changes in separate commits and preserve original upstream
messages and complete SHAs when importing community patches.

## 5. PR titles and detailed descriptions

Every PR must have a descriptive English title and an English description
with enough detail for a reviewer who has not seen the conversation.
The description must cover:

- **Summary:** the concrete problem or goal.
- **Changes:** the final scope and relevant behavior or directory changes.
- **Validation:** checks actually completed and their results.
- **Limitations:** unperformed builds, tests, or hardware verification when
  relevant, plus any dependency on another PR.

Keep the title and description synchronized with the final implementation.
Do not claim that a check passed unless it was run and its result verified.

## 6. Pre-submission verification

- Confirm the work branch, base branch, staged files, and commit scope.
- Verify that commit messages, PR titles and descriptions, and authored
  comments comply with the English-only rule.
- Confirm that documentation drafts and unrelated files are not included.
- After pushing, verify the remote work-branch SHA and the PR file scope.
- Verify that the default branch has not been updated by the agent, that
  the PR remains unmerged, and that auto-merge is not enabled.

## 7. License headers and copyright

- Use [LICENSE_HEADER_TEMPLATE.md](LICENSE_HEADER_TEMPLATE.md) as the
  canonical file-header template. Do not duplicate template definitions
  in these rules.
- Use the full Apache-2.0 banner for new C, C++, and header files. Adapt
  the comment syntax for other file formats while keeping the full notice.
- Use the actual year and applicable copyright attribution for new or
  modified content. Do not hard-code an organization in the template.
- Preserve imported copyright and license notices. Do not bulk-rewrite
  upstream file headers or copy the example's company name and year.

## 8. Bluetooth source retention boundary

- The retention boundary follows the files originally imported from
  `subsys/bluetooth/` to their current locations under `bluetooth/`.
- The directory work branch removes the in-process `controller/` subtree.
  Preserve every other file and directory from the original Bluetooth
  subtree, including
  Host, Classic, common, crypto, libraries, services, mesh, audio, shells,
  headers, Kconfig files, and CMake files.
- The root `bluetooth/hardening.yaml` metadata is the explicitly authorized
  exception removed during directory cleanup. Do not extend this exception
  to other non-controller files.
- The user has also authorized retirement of the original logging system
  and deprecated Kconfig options. Logging-only files and configuration
  blocks may be removed within that scope. Preserve unrelated protocol
  files and the existing configuration-header includes described below.
- Excluding a feature from the first build does not authorize deleting its
  source. A minimal compilation seed list is not a source-retention limit.
- Required porting or controller-entry edits may change authorized files,
  but must not remove non-controller files.
- Scope decisions outside this retained source set require their own task
  and dependency analysis; do not extend the controller deletion
  authorization to them.

## 9. Generated configuration headers

- Generate configuration headers automatically during CMake configuration
  through the single project Kconfig driver. Inject the canonical generated
  header into EmBT C/C++ compilation with target-scoped `-include` options.
  Consumers linking `EmBT::embt` receive the same configuration requirements.
- Preserve existing explicit configuration-header includes in imported
  source and headers, including `#include <zephyr/autoconf.h>`. Automatic
  injection does not authorize deleting or renaming these existing includes.
  New code can rely on the injected header without adding a manual include.
- Keep one canonical header at
  `<build-dir>/generated/include/embt/autoconf.h`. Provide the existing
  `zephyr/autoconf.h` include path through a generated forwarding header at
  `<build-dir>/generated/include/zephyr/autoconf.h`, pointing to the canonical
  header from the same build directory. Do not generate an independent set
  of configuration values for the legacy path.
- Export the generated include directory through the configuration target.
  Use include guards so forced and explicit inclusion can coexist. Track
  both generated headers for regeneration and recover either if missing.
- Keep generated headers in the build directory and out of Git. Do not add
  per-file configuration defines or a second configuration parser. Do not
  propagate EmBT configuration requirements into unrelated third-party
  compilation.
- This compatibility path applies only to the configuration header. It
  does not restore retired logging headers or authorize a general legacy
  header-namespace compatibility layer.

## 10. Kconfig naming and retired options

- Use `EMBT_EXPERIMENTAL` for the experimental-feature marker. Define it
  once in the root Kconfig as a hidden boolean, and rename surviving
  `select EXPERIMENTAL` references to `select EMBT_EXPERIMENTAL`. Do not
  introduce an `EXPERIMENTAL` compatibility alias. Descriptive
  `[EXPERIMENTAL]` text is not a configuration-symbol reference.
- Delete deprecated configuration options, including their complete
  Kconfig definitions and references that exist only for those options.
  Do not merely remove the `select DEPRECATED` annotation while keeping
  the deprecated option selectable. Do not define `DEPRECATED` or replace
  deprecated options with hidden booleans defaulting to `n`.
- Preserve supported replacement options and their existing defaults when
  removing deprecated aliases. Do not delete unrelated protocol source
  files as a side effect of configuration retirement.
- Delete the old logging configuration, including `LOG`, `LOG_*`, module
  logging levels and backends, and logging-only options such as `BT_LOG`,
  `BT_LOG_SNIFFER_INFO`, `NET_BUF_LOG` and `NET_BUF_SIMPLE_LOG`. Remove
  associated configuration blocks, template variables, source entries and
  stale selection/dependency references. Do not reintroduce these symbols
  as root-level placeholders or rename them to keep the old logger alive.
- Match actual symbol names and semantics. An incidental substring such
  as `LOG` in `TECHNOLOGY` does not identify a logging option. Preserve
  independent integrity checks, assertions and protocol behavior.
- Keep planning tables synchronized with these decisions. Removed symbols
  belong in explicit removal lists, not in the list of root definitions.
  Separate planned changes from changes already implemented or submitted.
- Delete the requested Controller and legacy platform configuration rather
  than retaining hidden defaults: `BT_CTLR_*`, `MODULES`, `MULTITHREADING`,
  `BT_ESP32`, `BT_LL_SW_SPLIT`, `BT_STM32*`, `ARM_MPU`, `BOARD_*`, `UART_BT`
  and `USBD_*`. Remove obsolete conditional-default and optional-selection
  expressions as whole lines. Preserve independent Host prerequisites when
  evaluating mixed dependencies; remove features that require a deleted
  provider and have no retained implementation. Present unresolved forced
  dependencies to the user for evaluation.
- Removing `MULTITHREADING` is configuration cleanup, not authorization to
  remove synchronization or switch the runtime to a single thread. Preserve
  existing active settings mutex operations.
- Use the root `LICENSE` as the project license. The user explicitly
  authorized deletion of the duplicate `bluetooth/LICENSE`; preserve all
  applicable notices in surviving source files.
- Use root `menuconfig EMBT_STACK` as the outer switch for the entire
  configuration tree. Keep `EMBT_64BIT` and the mutually exclusive
  `EMBT_LITTLE_ENDIAN`/`EMBT_BIG_ENDIAN` choices inside that switch.
  Validate pointer width and byte order against the compiler target;
  do not derive the target architecture from the Python host.
- Retire `SOC_SERIES_NRF51`, kernel `SMP`, `SECURE_STORAGE`,
  `FLASH_PAGE_LAYOUT`, `FLASH_MAP`, `BUILD_WITH_TFM`, `BT_SPI`,
  `EMBT_TARGET_ARCH` and `ARM64` configuration. Preserve Bluetooth
  `BT_SMP` and its security behavior. Keep real Host prerequisites,
  and remove configurations that only have a deleted storage provider.
- Crypto dependency integrations must distinguish feature requirements
  from provider availability. System and existing platform libraries keep
  their original configurations; builds from source use pinned inputs and
  provider-specific generated headers. Do not silently download a library
  when a requested system or external integration fails.
- Keep Mbed TLS and PSA configuration in `third_party/mbedtls/Kconfig`,
  sourced by the root Kconfig. This project-owned entry includes dependency
  requirements, integration selection and actual source-build options.
  Preserve functional Mbed TLS configuration there rather than replacing
  all library options with root-level placeholders. Source options must
  map to the pinned library's real macros and implementations; they must
  not reconfigure an installed or existing platform library.
