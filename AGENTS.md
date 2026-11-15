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
- Keep new development changes local, unstaged, and uncommitted until the
  user explicitly requests submitting or updating a PR. Earlier submission
  requests do not authorize publication of subsequent changes.
- When submission is requested, update the original task patch and draft PR,
  stating its base branch and dependencies. Do not wait for a main-branch
  merge to continue local work.
- After the runtime baseline is validated, integration still requires
  human review and a human decision to release the hold. Agents never
  perform the final merge.

## 2. Submission scope and documentation

- Include only the files required by the current task.
- Do not commit or publish `docs/history/` or `test/**/PROVENANCE.md`.
  Keep local archives outside tracked documentation and record upstream
  baselines and provenance centrally under `docs/upstream/`.
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
- When the user explicitly requests decomposition of an existing task PR,
  keep it as an ordered series of focused commits unless separate PRs are
  requested. Each commit must have one primary responsibility and describe
  dependencies and actual validation. Subsequent corrections update the
  affected commit in that series rather than adding unrelated fixup commits.
- Group logging-only module declarations, level conditions, and provider
  includes in the logging commit across all components. Other component
  commits preserve those declarations rather than repeating the migration.
  Keep declarations at their upstream code locations, with early upstream
  declarations moved after the include block as requested by the reviewer.
- Correct an unintended removal in the original unmerged commit that
  introduced it, then rebuild its dependent commits. Preserve diagnostic
  calls during porting; do not append a late restoration patch that leaves
  the original deletion visible earlier in the review series.
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
- For project-owned JSON metadata, follow the JSON exception in the canonical
  header template.
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
- The user also retired the USB PCM shell adapter (`bap_usb.c` and
  `bap_usb_backend.h`) and the Mesh Flash BLOB adapter (`blob_io_flash.c`
  and its public header). Remove only their specific references and
  configuration. Preserve generic BAP/LC3/A2DP and BLOB IO/protocol sources.
- Retire the original Zephyr logging implementation and deprecated Kconfig
  options. Preserve LOG_* interfaces, messages and diagnostic helpers;
  replace their provider with the internal adapter in base/log.h. Removing
  the Zephyr logger does not authorize removing protocol diagnostic calls.
  Preserve unrelated protocol files and migrate configuration-header includes
  as described below.
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
- Migrate existing explicit `#include <zephyr/autoconf.h>` directives to
  `#include <embt/autoconf.h>`. Keep explicit includes when migrating their
  path; target injection alone is not a reason to remove them. New code
  can rely on the injected header without adding a manual include.
- Keep one canonical header at
  `<build-dir>/generated/include/embt/autoconf.h`. The existing generated
  zephyr forwarding header may support remaining references temporarily;
  retire it and its build tracking after those references have migrated.
  Do not generate an independent set of configuration values.
- Export the generated include directory through the configuration target.
  Use include guards so forced and explicit inclusion can coexist. Track
  and recover the canonical header when missing.
- Keep generated headers in the build directory and out of Git. Do not add
  per-file configuration defines or a second configuration parser. Do not
  propagate EmBT configuration requirements into unrelated third-party
  compilation.
- Temporary legacy configuration forwarding does not restore retired
  logging headers or authorize a general header-namespace compatibility layer.

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
- Delete Zephyr's old logging Kconfig definitions, backend selections,
  templates and implementation dependencies. Preserve LOG_ERR/WRN/INF/DBG,
  hexadecimal dump and rate-limited call interfaces, and buffer wrappers.
  Use the real internal provider in base/log.h and the root Kconfig.log.
  EMBT_LOG is the master switch; EMBT_LOG_LEVEL is the shared default, not
  a global compiled ceiling. Module *_LOG_LEVEL integers use -1 to inherit
  and 0..4 for explicit levels; do not restore boolean level choices.
  LOG_MODULE_REGISTER/DECLARE create constant file-local tags and levels,
  without startup registration, mutable descriptors, or linker sections.
  Use BT_LOG_LEVEL_ENABLED(module, level), for example
  BT_LOG_LEVEL_ENABLED(BT_CONN, DBG), for compiled conditions in C and #if.
  It resolves inheritance and the master switch without consulting runtime
  filtering. LOG_LEVEL is a C constant and cannot be evaluated by #if.
  Keep BT_LOG_ENABLED(level) for current-module runtime output admission.
  Preserve the original module configuration sharing and pair debug
  declarations with their implementations. The global runtime filter
  defaults to Debug and may restrict, but cannot expand, compiled levels.
  EMBT_LOG_SNIFFER_INFO remains a separate disabled-by-default switch.
  Do not restore Zephyr logging queues, backends, or Kconfig templates,
  and do not use empty macros as a substitute for real output.
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

## 11. Header include migration

- Follow [HEADER_INCLUDE_RULES.md](docs/HEADER_INCLUDE_RULES.md) for new and
  modified includes. For path migrations, resolve the real file, provider,
  consumers, and target visibility. Apply the default removal rules below
  to obsolete dependencies.
- Use embt paths for project headers and osi paths for OSI dependencies.
  OSI and bt_queue operations are internal APIs, including internal tests.
  Internal structures may directly embed OSI types. Consolidate headers under
  include/embt, include/embt/base and include/embt/osi. Keep module types and operations
  together in their functional header instead of separate types directories.
  Header visibility does not make base/OSI operations a stable application API.
  Preserve standard, system, third-party, and module-private namespaces.
- Change legacy configuration includes to embt/autoconf.h under Section 9.
  Do not create a general zephyr include tree, symlink layer, empty header,
  or empty logging shim.
- Remove legacy Zephyr kernel, device, and generated syscall header
  dependencies by default. Retain directly used utility/type dependencies
  or include their minimal real providers; do not restore an entire legacy
  dependency bundle to hide errors.
- Replace old logger includes in internal consumers with <embt/base/log.h>.
  Preserve log calls, messages, diagnostic helpers, protocol operations,
  synchronization, independent assertions, and resource cleanup. Keep the
  adapter an internal API even though the shared include root makes its header
  visible. Do not publish backend-only helpers or treat header visibility as
  authorization for arbitrary application calls to base/OSI operations.
- Distinguish namespace changes from OS/mechanism porting and logger
  retirement. Header existence alone does not prove provider equivalence.
- Keep include-only changes free of API, layout, behavior, guard, copyright,
  and unrelated formatting changes. Record actual scope, validation, and
  unresolved dependencies; preserve retained protocol sources.

## 12. Internal atomic interface

- Atomic belongs to base and uses bt_atomic_* functions/types and
  BT_ATOMIC_* macros. Keep its header at include/embt/base/atomic.h and retain its
  internal API contract; do not provide old-name aliases.
- Follow [ATOMIC_DESIGN.md](docs/ATOMIC_DESIGN.md). Resolve existing public
  inline helpers, storage declarations, and L2CAP callback dependencies.
  Shared header placement does not authorize direct application manipulation
  of internal atomic storage or a silent business callback contract change.
- Preserve native long/pointer width, alignment, old-value returns, strong
  CAS, and sequentially consistent ordering. Using compiler intrinsics
  does not authorize changing buffer layout or resource lifetime.

## 13. Physical time units

- Use relative milliseconds for waits, sleeps and work scheduling. Store
  timeout values as .ms and absolute monotonic deadlines as .deadline_ms.
  Preserve -1/zero and deadline saturation semantics without restarting a
  finite timeout after each wakeup.
- Do not introduce scheduler tick types, fields, rates, conversions, or
  compatibility aliases. Use direct remaining milliseconds from work APIs.
- Use osi_time_get_us for protocol requirements needing microsecond
  measurement. It shares the monotonic origin of osi_time_get_ms and keeps
  error outputs unchanged. Do not obtain a microsecond timestamp by scaling
  an already-truncated millisecond timestamp.
- Convert positive microsecond work intervals up to milliseconds; preserve
  zero and avoid intermediate arithmetic overflow. This does not create a
  microsecond scheduler or change the runtime threading model.
- Preserve standard wire encodings and field widths. The ETS elapsed-counter
  flag retains bit zero and its resolution encoding while project names use
  elapsed-counter terminology. Do not alter unrelated Stick/Joystick or
  sticky-error concepts through substring matching.

## 14. Mechanism modules and direct OSI calls

- Implement queue, FIFO, LIFO, fixed-block pools and work in separate base
  modules with bt_* APIs. Keep operation declarations in their module headers.
- Do not restore include/embt/storage.h or use base.h as an aggregate mechanism
  header. Keep embedded types with their owning base/OSI functionality. The
  shared include root does not turn OSI or atomic operations into public APIs.
- Do not implement k_* functions or old-name compatibility aliases. Call
  existing osi_* synchronization, thread and sleep operations directly.
- Preserve timeout/error semantics deliberately at callers. Native objects must
  not be copied, overwritten, reinitialized or destroyed while active. A waiter
  being awakened does not prove that its semaphore giver has returned.
- Keep relative waits in scalar milliseconds. Preserve protocol cancellation
  semantics and the internal OSI boundary. Passing the basic build does not
  establish support for excluded components.

- Do not restore a project-owned variable-size heap allocator. Dynamic net_buf
  payloads use osi_malloc or osi_aligned_alloc directly and osi_free on the last
  data reference. Payload allocation has no wait/retry timeout; allocation failure
  or size overflow asserts and terminates even when ordinary assertions are off.
  Descriptor-pool waits keep their existing semantics.
- Each queue directly embeds its own os_mutex_t. Do not serialize unrelated
  queues with a global lock, allocate a separate mutex runtime, or initialize
  resources in queue operations. Zero storage and explicitly initialize it before
  use; the owner destroys it before release or overwrite. Reinitialization needs
  quiescent ownership and retains the native mutex. Buffer-pool owners explicitly
  call net_buf_pool_init before allocation. OSI/queue operations remain internal;
  this restriction does not prohibit internal structures from embedding OSI types.
- Keep the existing spinlock implementation until a separate change is requested.

- Port work from the imported Zephyr state machine, replacing kernel services
  with OSI dependencies. Preserve pending/flusher/canceller transitions,
  separate delayed-target ownership, and drain/plug behavior. Do not replace
  these contracts with worker-owned deadlines or submission-sequence shortcuts.
- Work waits use OSI semaphores, not condition variables. Timeout delivery must
  enqueue independently of a busy protocol worker and serialize cancellation
  against in-flight delivery before work storage may be released.
- Embed work-queue resources; do not restore a separately allocated runtime.
  Use explicit init/start/drain/stop/deinit. Stop requires a plugged queue;
  cancel delayed work before releasing its target queue. Work storage remains
  alive through the worker's post-handler state updates and a completion barrier.

## 15. Assertion and formatted-output responsibilities

- Preserve LOG_* calls, messages, module metadata and level semantics. The LOG
  macros construct complete literal-format records; base adapts their arguments
  to the existing io.c osi_vprint(fmt, args) interface.
  base/log.c owns filtering/rate control and bounded hexdump rows, not a second
  printf implementation. Platform printing has no level argument or filtering.
  Use Linux stderr and fixed-priority NuttX syslog within io.c; do not add
  separate OSI log headers, level enums or backend-selection infrastructure.
- Do not allocate message memory in logging or assertion adapters. Do not
  restore growing buffers or multipart raw-write logging. Keep assertion
  reporting in base/assert.c, bypass stack filters, and terminate via osi_fatal.
  Hexdump rows are separate records; do not add a dump-wide synchronization layer.
- Implement the assertion adapter under include/embt/base/assert.h. Preserve
  bluetooth/common/assert.h as the Bluetooth-specific entry point and keep
  Bluetooth internal files; do not remove them merely as wrapper cleanup.
  ASSERT and BT_ASSERT enablement stay independent. Failure reports bypass
  log filtering and logging locks and include condition, location and message.
- Do not restore embt/sys/__assert.h, ZASSERT module levels, printk/unlocked/
  panic variants or the separate diagnostic implementation. Public inline
  utilities retain their checks without depending on private base headers.
  Their enabled checks must still terminate with NDEBUG defined.
- Format caller-owned strings directly with snprintf/vsnprintf and include
  stdio.h. Do not tie HFP or settings string encoding to a console-output switch.
- Output failure must not recursively assert through logging. Fatal reporting
  still invokes the terminal action even when the byte sink returns an error.

## 13. Upstream Bluetooth test history

- Retain the scope-selected upstream Bluetooth tests in the cleanup patch.
  Controller tests, board configuration, YAML runners and device-tree overlays
  remain excluded under the approved migration scope.
- Move the retained tests from `bluetooth/tests/bluetooth/` to
  `test/bluetooth/` in a dedicated directory-only commit. Preserve original
  bytes, Git modes, symlinks, notices, comments and assertions at this stage.
- Port the framework and test programs in subsequent focused commits so a
  reviewer can inspect modifications against the relocated upstream files.
  Preserve original test descriptions; retire obsolete Zephyr build/config
  inputs only in the relevant test-porting commit and explain their replacements.
- Keep protocol and provider corrections in their original Host, Classic,
  HCI, crypto or mechanism commits. Group logger-only changes for imported
  test files in the logging commit. Keep project documentation and source
  provenance in the documentation patch.
- Record the upstream SHA, retention and relocation verification, adaptation
  reasons, configuration coverage and actual validation. Never claim that the
  unadapted intermediate commits build or run as standalone EmBT tests.

## 17. Native thread parameters and platform adaptation

- Preserve original PRIO/stack configuration numbers, defaults and conditional
  defaults. Callers pass their original requests instead of rewriting them.
- Retire NUM_PREEMPT_PRIORITIES and its BT_LONG_WQ_PRIO range reference;
  preserve the default priority and configurability. Native limits remain
  checked inside the OSI priority adapter.
- Linux intentionally creates threads with default pthread attributes and does
  not apply priority/stack-size requests. State this platform contract explicitly;
  do not claim custom Linux stacks or real-time priority control.
- NuttX conversion uses its configurable reference minus the original priority,
  defaulting to 150. Convert once inside the platform adapter with wide arithmetic
  and native-range validation, without clipping or fallback.
- Keep initialization order separate from scheduling priority. Preserve INIT
  defaults, sort within a dependency-ready stage and unwind in reverse order.
- Linux and NuttX share osdep/posix. Use the native __NuttX__ macro in C for
  necessary differences instead of Kconfig platform selectors or exported
  conversion macros. Keep the numeric priority reference configurable.
- Apply NuttX stack requests and checked priority attributes within the shared
  creation path. Detect target capabilities in CMake without defining another
  platform macro. Keep macOS adaptation outside this backend's current scope.
- Distinguish host models of NuttX branches from native target builds and runtime
  validation; do not report modeled attributes as effective native scheduling.
- Retire EMBT_BOOT_IMAGE_BACKEND and its version/confirm provider dependency.
  Preserve the standard Mesh DFU/DFD models and their shell commands; Nordic
  authorship alone does not imply a Nordic platform dependency.
- Keep the standard RTP default at two and the explicit configuration connected
  to serialized output. Preserve independent RNG, shell and build fixes.
- Export typed CMake values from the same evaluator as the canonical header and
  JSON. Do not restore a second parser or redundant dependency dumps.

## 18. OSI contract tests and test dependency boundaries

- Derive OSI test expectations from documented API contracts. Use only OSI
  functions/types/constants, standard C and cmocka in test/osdep, including
  its coordination helpers. Do not add a second POSIX test suite.
- Do not call native pthread, semaphore, clock or descriptor APIs, use C11
  atomics, read native object fields, or assert backend-specific object sizes
  in OSI tests. Keep time units, errors and lifecycle requirements explicit.
- Use OSI threads and notifications for coordination. Read worker results
  after synchronization and join; keep cmocka longjmp on the controlling thread.
- Retain timer callback storage and notifications for already-dispatched calls.
  Timer stop/delete is not a completion barrier. Do not reuse one case's timer
  or notification storage in another case.
- Reject unknown and actually filtered-out selections. Permit sequential cases
  within one test application; do not depend on per-case process exit for state isolation.
- Do not add production hooks to manufacture native errors or report invocation
  coverage as complete branch coverage. Record fatal/output observability gaps.
- Other component tests use project interfaces for generic helpers. Identify
  socket, filesystem, audio backend, process-isolation and host tooling tests
  explicitly as platform integration checks, preserving their real coverage.
- Report OSI contracts, platform integrations and actual target validation
  separately. Host compilation or models do not demonstrate NuttX execution.
- Follow [OSI_CONTRACT_TESTS.md](docs/OSI_CONTRACT_TESTS.md) for the current
  implementation, validation commands and remaining boundaries.
