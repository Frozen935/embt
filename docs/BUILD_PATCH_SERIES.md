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

# 构建 PR 的职责拆分与 Review 顺序

本轮按用户要求，将原构建 PR #5 的单一大提交及其后已完成的 settings、日志适配
整理成 13 个有序 commit。保持同一个 Draft PR，文档和规则仍通过原文档 PR #2 提交。
日志调用最早在清理 PR #4 的历史提交 b2748fe8062 中被删除；上一版重挂后的提交为 a33f38e5f6e。
本轮把 29 个源码文件的调用、sniffer helper 和 buffer 包装恢复到这个原清理 patch，
其余清理边界保持。后续模块在各自适配 patch 内保留日志，不再追加最后一轮恢复。

## 1. Patch 职责

| 顺序 | Commit 标题 | 主要职责 |
| --- | --- | --- |
| 01 | build: add standalone CMake Kconfig and mkbt entry points | 根 CMake/Kconfig、统一配置生成与 -include、EmBT CMake 函数、mkbt.sh 和构建配置入口。 |
| 02 | build(crypto): integrate pinned and external Mbed TLS providers | Mbed TLS/PSA provider、SOURCE/SYSTEM/EXTERNAL 选择、固定输入、配置映射及能力校验。 |
| 03 | refactor(headers): adapt shared headers and utility dependencies | 公共头文件的 namespace、类型和直接依赖，以及 CRC/UUID/utility 的必要配套。 |
| 04 | feat(osi): implement POSIX system APIs with cmocka validation | 内部 OSI 类型/API、POSIX thread/sync/time/timer/memory/io、cmocka 和模块测试。 |
| 05 | feat(base): implement atomics queues buffers and work mechanisms | 私有 bt_atomic、queue/FIFO/pool/slab、net_buf pool 指针、work/deadline、断言与机制测试。 |
| 06 | feat(logging): add the internal level-filtered log provider | 内部同步 LOG provider、配置、级别/hexdump/限频和测试；不承担后补调用恢复。 |
| 07 | feat(hci): provide Linux user TCP and Unix socket transports | Linux HCI user channel、TCP、Unix socket、HCIDEVDOWN、H4 收发、transport 接口及测试。 |
| 08 | feat(host): add explicit lifecycle and standalone HCI integration | 显式 stack 生命周期、注册及初始化顺序、Host HCI 接入、common 和基本 service 依赖。 |
| 09 | feat(host): adapt LE connection ATT GATT and security mechanisms | LE advertising/scanning、connection、ATT/GATT/L2CAP、keys/SMP/crypto 的系统机制适配。 |
| 10 | feat(classic): adapt BR core and retained Classic interfaces | BR 核心、SDP/L2CAP/SSP/SCO，以及保留 Classic profile 的接口迁移。 |
| 11 | refactor(bluetooth): adapt retained Mesh and Audio dependencies | 保留的 Mesh/Audio 源码与构建依赖迁移；不把源码迁移描述为这些功能已编译或运行。 |
| 12 | feat(shell): add recursive btshell and Bluetooth command adapters | 递归 shell parser、入口、Bluetooth 命令 descriptors、bt/gatt/l2cap/br 命令适配及 CLI 测试。 |
| 13 | feat(settings): preserve handler design with a POSIX file backend | 原 handler/store/load/save/commit 设计、POSIX FILE backend、BT 静态注册、shell 接入及持久化测试。 |

| Patch | 文件数 | 新增/删除行 |
| --- | ---: | ---: |
| 01 | 15 | +1164 / -19 |
| 02 | 10 | +739 / -0 |
| 03 | 142 | +618 / -580 |
| 04 | 38 | +3915 / -0 |
| 05 | 13 | +3174 / -203 |
| 06 | 39 | +713 / -85 |
| 07 | 17 | +1172 / -121 |
| 08 | 57 | +637 / -522 |
| 09 | 34 | +1682 / -1615 |
| 10 | 38 | +1684 / -1596 |
| 11 | 105 | +1465 / -1280 |
| 12 | 58 | +2499 / -1413 |
| 13 | 20 | +2232 / -186 |

每个 commit 有英文标题、详细目的、范围、依赖和验证说明。公共 CMake 接入文件会随
相应模块出现而更新：shell 和 settings 的初始化/测试接入属于各自 patch，日志源和
日志配置接入属于日志 patch。不是把同一个大提交复制到多个分类名下。

## 2. 依赖与审查方式

- GitHub 的 Commits 页可逐个打开，每个 patch 的范围是它相对前一个提交的 diff。
- 这些是同一 bring-up 任务的累积 patch series，后续 patch 依赖前面的构建、头文件、
  OSI、base 和 transport 提供者；它们不是互相独立的 cherry-pick。
- 初始构建入口会引用随后提交的模块。单个中间提交不承诺完整配置、编译或运行；
  已通过的整体测试针对完整系列。每个提交正文必须准确说明这个范围。
- OSI tests、base/transport tests、shell CLI tests、settings tests 与 logger tests
  随各自模块提交；不把全部测试伪装成每个独立 commit 都已经运行通过。
- 相同模块的后续修正更新系列中的对应 commit，避免持续附加混杂 fixup patch。
- 未编入基础 profile 的 Mesh、Audio 和额外 Classic profiles 保留源码。
  相关接口迁移不构成独立功能的构建或运行验收。

## 3. 代码完整性与验证

拆分之前保存原提交、工作文件、模式、符号链接和 Git 对象快照。
将各 patch 的累积结果与本轮已调通的完整代码逐文件对比；最终源码、权限和链接目标一致。
拆分操作不新增协议行为，不覆盖用户的 .vscode 文件，不提交 generated headers、
第三方下载目录、build 目录和运行时 settings 数据。

完整系列的已有验证：

| 配置 | 已验证结果 |
| --- | --- |
| 普通 settings+btshell | 71/71 CTest 通过 |
| Debug + ASan/UBSan + settings | 70/70 CTest 通过，不含 shell CLI case |
| 关闭日志、关闭 settings | 52/52 CTest 通过 |
| Mbed TLS 3.6.5 SOURCE + Debug + settings/btshell | 编译、链接通过 |
| logger 模块 | 6 个 cmocka 用例，通过，计为一项 CTest |

本轮拆分不重复占用 USB Controller，也不新增无线 pairing/connection 验证。
提交前复核最终树、commit 范围与英文信息；推送后复核远端 SHA、PR diff 和 Draft/未合入状态。

## 4. 提交关系

文档 PR #2 基于 main；清理 PR #4 基于文档分支；构建 PR #5 基于清理分支。
调用恢复回原清理提交，模块适配回各自提交；日志 provider 在 base 后、HCI/Host 前。
只有各自的工作分支可以推送。重写构建系列时使用旧远端 SHA 作为 lease，
远端若出现其他提交则停止覆盖并重新核对。main 不更新。
全部 PR 保持 Draft、未合入、未启用 auto-merge；最终 review 和合入由人执行。

本轮快照、逐 patch 范围、SHAs 和验证记录位于 /tmp/embt-log-boundary；前一轮记录位于 /tmp/embt-patch-split。
