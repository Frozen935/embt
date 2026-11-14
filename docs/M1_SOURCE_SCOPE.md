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

# M1 蓝牙源码与依赖范围

当前构建进展：已在本地实现并验证独立 Kconfig/CMake、真实 menuconfig 和 mkbt.sh。
实际 C 编译已启动，仍因旧日志/zephyr 头引用失败，尚未运行。实施范围、日志与命令见
[BUILD_SYSTEM_IMPLEMENTATION.md](BUILD_SYSTEM_IMPLEMENTATION.md)。本轮未提交。

最新配置模型：EMBT_STACK控制所有选项，EMBT_64BIT及互斥大小端配置替代具体架构/内核
开关，当前1003个定义。Flash/secure-storage强依赖配置已移除；原BT_SMP配对配置保留。
依赖评估见 [ABI_CONFIG_CLEANUP.md](ABI_CONFIG_CLEANUP.md)，密码库的三种接入方式见
[CRYPTO_PROVIDER_DESIGN.md](CRYPTO_PROVIDER_DESIGN.md)，Mbed TLS的SYSTEM/SOURCE/EXTERNAL已实现，crypto检查通过；完整协议栈运行仍待移植。




状态：本文由合并后的文档 PR #2 跟踪；目录整理在代码 Draft PR #4 的工作分支实施，二者尚未合入 main。独立构建和 POSIX 运行适配尚未完成。

## 1. 当前状态与已经确认的决定

| 项目 | 当前状态或决定 |
| --- | --- |
| M1 目标 | 独立编译并在 Linux/POSIX 上通过 HCI user socket 运行 BLE、BR/EDR Host |
| 源码现状 | 目录与后续裁剪在 PR #4 跟踪：本次 logging 目录删除后 720 个文件；include/embt 为 202 个，lib 为 16 个；独立运行尚未实现 |
| Controller 实现 | 原 bluetooth/subsys/bluetooth/controller/ 的 262 个文件已删除 |
| 当前执行状态 | controller 及其直接父级入口已移除，目录迁移已完成；头文件引用尚未从 zephyr/ 改为 embt/ |
| 原 subsys/bluetooth 其他内容 | 457 个文件已迁移；本轮明确删除根部 hardening.yaml 元数据，其余 456 个文件保留 |
| PR 状态 | 所有后续研发 PR 保持 Draft、未合入，直到实际运行基线验证后由人决定合入 |

保留源码与是否参与首期构建是两件事：mesh、audio、shell、services 等可以按配置不编译，但源码、头文件和配置文件不能因此删除。35 个核心源码种子只是构建候选，不是其余源码的删除依据。

这里删除的是 Zephyr 内置 Controller/Link Layer 实现。EmBT 仍然需要通过 HCI
与外部物理 Controller 通信，Host 的 HCI 命令、事件、ACL 及其类型定义继续保留。

## 2. 已实施的目录整理与删除范围

目录整理基于工作分支 `docs/m1-source-dependency-scope` 的提交
`227aadab8f2b3b969628a9d3857494a6ff0d5535`。原始 Zephyr 来源和 Git 历史不变。

### 2.1 路径映射

| 原路径 | 当前路径 | 范围 |
| --- | --- | --- |
| bluetooth/subsys/bluetooth/，不含 controller/ | bluetooth/ | 457 个文件，全部保留，包括各模块内的 shell |
| bluetooth/include/zephyr/ | include/embt/ | 迁移 2343 个文件，按首期 Host 来源进一步裁剪后剩 202 个；存活内容未改 |
| bluetooth/lib/ | lib/ | 迁移 252 个文件，删除 os、heap 和无关工具后剩 16 个；保留 net_buf、utils、uuid |

蓝牙辅助库从原 `subsys/bluetooth/lib/` 迁移到 `bluetooth/lib/`；通用库迁到根目录，
避免两者重名。原 Zephyr 整体 CMake/Kconfig 入口移除，`bluetooth/` 使用迁入的蓝牙入口。

### 2.2 明确删除项

| 原路径或文件 | 删除文件数 | 原因 |
| --- | --- | --- |
| bluetooth/subsys/bluetooth/controller/ | 262 | 使用外部 Controller，移除进程内 Controller 实现 |
| bluetooth/samples/ | 8239 | 首期不保留上游 samples |
| bluetooth/tests/ | 16350 | 首期不保留上游 tests |
| bluetooth/doc/ | 1252 | 移除上游文档树，项目文档使用根目录 docs/ |
| bluetooth/.github/ | 59 | 移除仅服务于 Zephyr 仓库的 CI 和社区配置 |
| 原 bluetooth/CMakeLists.txt、bluetooth/Kconfig | 2 | 整体 Zephyr 入口与迁入蓝牙入口重名，保留蓝牙入口 |

第二批清理在同一个代码 patch 中继续更新，新增删除 42921 个文件：

| 区域 | 新增删除文件数 | 处理依据 |
| --- | --- | --- |
| arch、boards、soc、dts | 29859 | Linux/POSIX Host 不保留硬件平台和设备树实现 |
| drivers | 7550 | 仅保留 userchan 三个源码/头文件及父级入口，其他硬件驱动移除 |
| subsys | 1747 | 保留 CRC、日志、随机数、settings、shell，删除其他子系统 |
| modules | 492 | 保留 Mbed TLS/PSA、SBC、LC3 集成参考，移除 HAL 和无关模块 |
| include/embt | 1637 | 保留全部 Bluetooth 头文件、公共兼容接口和已有字面 include 的传递依赖 |
| lib | 171 | 删除 libc/C++、hash、midi2、mem_blocks、min_heap、runtime、smf 等无关库 |
| 其他构建、工具和社区残留 | 1465 | 删除 west/native_sim、旧平台 CMake、SDK、社区元数据和无关脚本 |

风格配置 `.clang-format` 保留于根目录；`.editorconfig` 和 `.checkpatch.conf` 已删除。旧 scripts 在后续批次删除，项目工具后续自行实现。存活原始源码和接口头文件内容保持不变。

从迁入的 [CMakeLists.txt](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/CMakeLists.txt) 移除
`CONFIG_BT_LL_SW_SPLIT` 下的 `add_subdirectory(controller)`，并将蓝牙内部 include
路径改为当前源码目录。从 [Kconfig](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/Kconfig) 移除
`rsource "controller/Kconfig"`。其余蓝牙入口和功能配置保留。

第三批在原 PR #4 继续更新：

- 再删除 264 个未使用头文件：232 个架构头文件、8 个其他编译器头文件、15 个 sys 工具头文件，以及 9 个其他无引用头文件。
- 按 Linux/POSIX 兼容架构及 GCC/Clang 分支分析依赖。显式的其他架构/编译器条件视为不参与；其他配置条件保持未知，可能使用的头文件继续保留。此处是裁剪分析假设，不是已经生成或验证的构建配置。
- 删除 bluetooth/LICENSES/ 的 14 个许可登记文件，以及 bluetooth/README.license、bluetooth/REUSE.toml、bluetooth/hardening.yaml，共 17 个点名元数据文件。
- 使用根目录 LICENSE，bluetooth/LICENSE 重复副本已删除；存活源码许可声明保留。
- 该批次之后曾保留 1324 个文件、442 个头文件目录条目和 81 个 lib 文件；下面记录后续按明确指令追加的裁剪，当前统计以最新批次为准。

### 2.3 前次清理范围（日志目录删除前）

这些修改先按指令保留本地，收到本轮提交指令后统一 update 原代码 PR #4；文档放原 PR #2。
相对于此前工作分支 `92ef7a2639915e1c7ba03f20d986edea37b6a67e`，累计删除 587 个文件、
调整 15 个直接构建/配置文件，当时保留 737 个文件；最新日志删除见 2.6。

| 删除区域 | 文件数 | 范围与原因 |
| --- | ---: | --- |
| include/embt 低关联项 | 240 | 移除网络、USB、shell、设备树、平台架构、IPC、PM 等；旧 include 留给适配 |
| lib/os、lib/heap | 49 | 原实现移除，后续由 POSIX 接口实现接替 |
| kernel | 99 | Zephyr 内核实现不再保留 |
| modules | 26 | 原集成移除，后续使用 third_party 管理依赖 |
| scripts | 18 | 原脚本移除，后续实现项目自有工具 |
| 通用 shell | 63 | 原 Zephyr shell 框架及平台 backend 移除，蓝牙模块内的 shell 源文件保持 |
| 日志附加实现 | 52 | 移除网络、硬件、IPC、frontend、字典和其他低关联实现，保留核心参考 |
| settings 附加实现 | 15 | 移除 flash/文件/TF-M backend、shell 和原静态初始化，保留核心参考 |
| 随机数附加实现 | 7 | 移除硬件、模拟器和测试 provider，保留 PSA 随机数参考 |
| CRC 硬件/shell 前端 | 2 | 保留软件 CRC 算法 |
| 无关 utils | 16 | 移除 JSON、COBS、getopt、通知/onoff、旧 kernel 耦合和其他未使用工具 |

保留的根目录 lib 为 net_buf、utils、uuid。utils 源文件保留 dec、hex、base64、bitmap、utf8；
bitmap 和 UTF8 仍有被保留蓝牙可选模块的实际消费者，不由首期构建无条件启用。

### 2.4 头文件保留范围与明确适配缺口

当前保留 202 个头文件目录条目，其中 125 个 Bluetooth 公共头文件全部保留；其余 77 个
为首期 Host 所需的 buffer、字节序、链表、原子、断言、编译器和 OS/HCI/日志/settings 等接口。
裁剪输入以首期 35 个源码种子及必要扩展为主，不再因旧 backend 或可选平台文件引用而保留整组头文件。

即使旧代码仍有 include，明确低关联项目也按指令删除。首期来源扫描记录了 23 条因此需要
重新适配的引用，包括 hci_core.c/device.h 的设备树接口和 kernel/thread/spinlock 的架构接口。
该数量不是全仓库缺失引用总数，生成头文件、其他未启用模块及完整链接仍需在构建阶段核对。

| 当前保留项 | 处理方向 |
| --- | --- |
| Bluetooth 公共接口、net_buf、UUID/CRC/基础工具 | 保留算法和现有接口语义 |
| kernel.h、线程/同步和 HCI driver 接口声明 | 后续实现 POSIX/transport，不能靠保留原 kernel 恢复运行 |
| settings 核心参考 | 后续提供真实 backend 和显式初始化 |
| 原日志系统 | 废弃全部文件/引用/配置；本次先删除目录，详情见 2.6 |
| PSA 随机数参考与密码学调用 | 后续接 third_party 的真实 provider；本轮未新增或下载第三方库 |
| 旧头文件引用和静态注册 | 后续迁移为 embt/ 引用及实际运行期初始化 |

### 2.5 校验与未完成工作

- 清理清单与存活文件逐项匹配，456 个受保护蓝牙文件和 125 个公共 Bluetooth 头文件均保留。
- 存活 C/C++、头文件内容和文件模式保持；必要 CMake/Kconfig 行单独核对。
- 此前各目录清理时直接父级入口已处理；本次按要求先只删 logging 目录，日志相关入口暂留，不再满足所有入口指向现存文件的条件。
- Apache LICENSE、初始来源记录和存活文件版权保持；不按名称扩大 controller 或元数据删除范围。
- 两个活动 PR 各保留一个 commit，文档 PR 的源码基线不变，代码 PR 不包含项目文档修改。

独立 Kconfig/CMake、POSIX 内存/线程/同步、HCI transport、第三方 provider、独立日志方案/存储 backend
及动态注册尚未实现。未执行独立配置、编译、链接或硬件运行验证；文件范围检查不表示可运行。

### 2.6 废弃原日志系统：先删除目录

从 `1f8bcb583d899ecccacfa3feacf46ca862b43c62` 再删除 bluetooth/subsys/logging/ 的
17 个文件、4,250 行，代码树由 737 变为 720 个文件。202 个 include 文件、16 个根 lib
文件及 456 个原非 Controller 蓝牙文件在本次目录删除中保持不变。

只将目录删除 update 到原代码 PR #4，文档修改留本地。目录外 logging include、LOG_* 宏、
日志专用 Kconfig 及父级 CMake/Kconfig 入口仍然存在，后续全部清理；取消旧日志核心/backend
移植方案。后续日志专用文件删除是本次明确授权的范围，其他协议源码和错误处理继续保留。
详见 [LOGGING_REMOVAL_PLAN.md](LOGGING_REMOVAL_PLAN.md)。未执行构建或运行验收。

## 3. 本轮盘点的输入与限制

- 静态盘点快照：`2579bbc789709aec93e192dd5755493b72aaff07`。
- 原始源码 tree：`3dfaeb6628ab061e0a441013ab4b6d3caeb78551`。
- Zephyr 原始提交：f8da8ea370508f274685a25ff19dbe5a9ff54790。
- 平台：Linux/POSIX；HCI 通道：HCI_CHANNEL_USER；Controller 在 Host 进程之外。

以下盘点统计来自目录整理前的静态快照；存活文件链接指向当前代码分支，已裁剪候选链接保留原始上游快照，统计不代表当前构建闭包。

盘点依据包括上游 CMake 源码列表、C 文件 include、候选传递头文件、Kconfig
声明和 source 关系、OS 调用名以及初始化/注册宏。遇到需要重写的 OS、device
和注册接口时停止继续递归，不把整个 Zephyr 内核自动列为保留代码。

本轮没有执行独立 Kconfig 解析、预处理、编译或最终链接。原始扫描会同时看到
未启用功能和其他编译器分支，因此下列数量是候选范围，不是最终编译闭包。

## 4. 首期功能与源码种子

建议首期保留 Host HCI、LE 连接、扫描/广播、central/peripheral、SMP 安全流程、
Classic 核心和 RFCOMM。先保留原有多线程、同步 HCI 命令、TX processor 和 workqueue。
这些执行依赖必须真实实现，不能为了链接通过而变成空操作。

Mesh、LE Audio、ISO 数据操作、DF、Channel Sounding、raw-HCI、shell 和 monitor
不进入首期运行目标。其他 Classic Profile、标准 GATT service、持久化存储及独立
long workqueue 按实际配置启用。

首轮选取 35 个 C 文件作为保守源码种子，均已核对存在且属于对应上游 CMake 列表。
仍需保留其配置条件，不能将全部文件无条件编译。

| 分组 | 源码 | 保留理由或条件 |
| --- | --- | --- |
| host | [bluetooth/host/uuid.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/uuid.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/addr.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/addr.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/buf.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/buf.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/data.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/data.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/hci_core.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/hci_core.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/hci_common.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/hci_common.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/id.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/id.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/adv.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/adv.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/scan.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/scan.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/crypto_psa.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/crypto_psa.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/ecc.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/ecc.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/conn.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/conn.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/l2cap.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/l2cap.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/att.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/att.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/gatt.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/gatt.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/smp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/smp.c) | Host 核心或选定的 BLE/安全功能 |
| host | [bluetooth/host/keys.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/keys.c) | Host 核心或选定的 BLE/安全功能 |
| classic | [bluetooth/host/classic/br.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/br.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| classic | [bluetooth/host/classic/conn_br.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/conn_br.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| classic | [bluetooth/host/classic/keys_br.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/keys_br.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| classic | [bluetooth/host/classic/l2cap_br.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/l2cap_br.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| classic | [bluetooth/host/classic/sdp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/sdp.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| classic | [bluetooth/host/classic/ssp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/ssp.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| classic | [bluetooth/host/classic/sco.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/sco.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| classic | [bluetooth/host/classic/rfcomm.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/rfcomm.c) | Classic 核心；RFCOMM 用于首期数据链路 |
| common | [bluetooth/common/addr.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/addr.c) | 公共构建成员；RPA 按安全/隐私配置启用 |
| common | [bluetooth/common/dummy.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/dummy.c) | 公共构建成员；RPA 按安全/隐私配置启用 |
| common | [bluetooth/common/bt_str.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/bt_str.c) | 公共构建成员；RPA 按安全/隐私配置启用 |
| common | [bluetooth/common/hci_pkt.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/hci_pkt.c) | 公共构建成员；RPA 按安全/隐私配置启用 |
| common | [bluetooth/common/hci_lockstep.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/hci_lockstep.c) | 公共构建成员；RPA 按安全/隐私配置启用 |
| common | [bluetooth/common/rpa.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/rpa.c) | 公共构建成员；RPA 按安全/隐私配置启用 |
| crypto | [bluetooth/crypto/bt_crypto.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/crypto/bt_crypto.c) | SMP 使用的 Bluetooth 密码学工具箱 |
| crypto | [bluetooth/crypto/bt_crypto_psa.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/crypto/bt_crypto_psa.c) | SMP 使用的 Bluetooth 密码学工具箱 |
| net_buf | [lib/net_buf/buf.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/lib/net_buf/buf.c) | 保留 buffer 操作，运行接口另行适配 |
| net_buf | [lib/net_buf/buf_simple.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/lib/net_buf/buf_simple.c) | 保留 buffer 操作，运行接口另行适配 |

种子之外还需明确：

- common/long_wq.c：仅在 BT_LONG_WQ 启用时编译；ecc.c 在相应配置下提交到 long
  workqueue，否则提交到 system workqueue，两条路径都需要真实执行语义。
- host/settings.c：启用 BT_SETTINGS 时需要实际存储 backend。
- host/gatt_gap_svc_validate.c、host/aes_ccm.c、common/hci_public_addr.c：按实际功能配置。
- lib/uuid/uuid.c：BT_HCI_HOST 选择 UUID，不能仅定义符号而遗漏所需实现。
- lib/utils/hex.c 等工具实现：依据选定调用点和链接结果保留。

同名文件不表示重复：host/addr.c 与 common/addr.c 需要按内容和消费者判断。

## 5. 保留、替换、暂缓与删除边界

| 区域 | 当前决定 | 完成条件 |
| --- | --- | --- |
| 原 subsys/bluetooth/controller | 已删除整个目录及两个直接父级入口 | 删除范围与第 2 节一致，其余蓝牙文件保留 |
| 原 subsys/bluetooth 其余源码 | 本次目录删除后 456 个文件仍保留；后续日志专用文件为明确授权例外 | 不把元数据删除扩展到协议文件或其他目录 |
| Bluetooth 公共头文件 | 保留 API 和共享类型兼容 | 逐项核对消费者，不能仅因某功能关闭而删共享头 |
| lib/net_buf 与 net_buf.h | 保留算法和 API，替换 OS 对象及池注册依赖 | 分配、引用、分片、等待、pool ID 和销毁行为可用 |
| sys 工具和编译器支持 | 已按首期源码收敛，低关联接口移除 | Linux GCC/Clang 接口仍需随实际构建验证 |
| kernel、spinlock、clock、queue、memory 接口 | 原 kernel、os、heap 实现已删除，后续接 POSIX | 超时、取消、退出和对象生命周期真实实现 |
| device model 与 HCI driver 接口 | 改为显式 transport 配置和注册 | open/send/receive/close 和所需能力查询可用 |
| 原 logging | 废弃文件/引用/配置，当前先删目录 | 日志清理不改变错误返回和资源生命周期 |
| 非日志用途的 printk、assert | 按实际非日志消费者适配 | 输出和断言语义真实有效 |
| PSA crypto | 选择兼容并固定版本的外部 backend | 所选 AES/CMAC/ECDH 等算法及 API 可用 |
| settings/storage | 保留接口，按明确需求启用 | 已启用功能具备实际持久化或明确的内存语义 |
| 其他 Classic Profile 和 GATT service | 全部保留源码，首期可按配置不启用 | 配置、外部依赖和验证范围分别明确 |
| subsys/bluetooth 内的 Mesh、audio、shell 等 | 源码和相关文件全部保留，首期可不编译 | 禁止因功能未启用、依赖复杂或首期未验证而删除 |
| 保护范围外的 samples、tests、上游 doc 和 .github | 已按明确清单删除 | 不把这份清单扩展到待适配依赖 |
| arch、boards、soc、dts、kernel、modules、scripts 及无关子系统 | 原目录和低关联接口已删除 | 后续使用 POSIX、third_party 和项目工具替代 |

## 6. OS、头文件和注册依赖

原始扫描得到 93 个传递头文件候选、58 个 OS 调用名及 85 处初始化/注册宏位置。
宏遍历位置数量不等于待初始化对象数量。211 个未在源码树中解析的 include 边
主要是标准 libc 头，不应描述成 211 个缺失的移植接口。

| 接口族 | 适配内容 |
| --- | --- |
| kernel/thread、spinlock、IRQ | 线程、同步、work、queue 及其生命周期 |
| atomic、clock、time units | 原子操作、单调时间及超时转换 |
| device/init、iterable sections | 显式运行期初始化和真实注册表 |
| drivers/bluetooth | 脱离 DT 的 HCI transport 配置及回调 |
| 原 logging 与非日志输出/断言 | 删除旧 logger 依赖；独立输出/断言按消费者保留 |
| settings | 已启用能力的存储实现 |
| autoconf 生成接口 | CMake 自动生成并由 target -include 注入；保留原配置头 include，构建目录提供旧路径转发头 |
| syscall 生成接口 | 必要的兼容替换，另行适配 |
| PSA/Mbed TLS | 外部库、版本和算法选择 |

必须覆盖 buffer pool 和 ID、system/TX workqueue、可选 long workqueue、transport、
LE/BR fixed channels、GATT services、连接及安全回调和 crypto 初始化。
先准备同步对象和 buffer；等待同步 HCI 响应前，接收处理必须已能运行；业务事件
进入前，其协议和回调已完成注册。失败和重试需要清理，不能重复注册对象。

## 7. 配置依赖不能用默认值掩盖

根配置不再保留旧 EXPERIMENTAL、DEPRECATED 或 LOG/LOG_* 兼容定义。实验标志统一为
EMBT_EXPERIMENTAL，deprecated 的完整配置块和旧日志配置直接删除。具体 8 个废弃项、
54 处改名及关联条件见 [KCONFIG_MIGRATION.md](KCONFIG_MIGRATION.md)。这些 Kconfig 修改已在本地实现并验证；已经提交的日志改动仍只有目录删除，
C 层日志调用清理及完整编译仍未完成。


扫描范围之外有 100 个配置名候选，包含常量变量、日志模板生成符号、未启用功能、
平台全局符号和外部 provider 选项，并非 100 个已经确认缺失的配置。

- UINT8_MAX、UINT16_MAX、INT16_MAX 是标准常量替换，不是设备树扩展。
- NET_BUF、UUID、sys helpers 要同时考虑独立配置和实现。
- PSA_CRYPTO、PSA_WANT_* 需要与实际 backend 对齐。
- SYSTEM_WORKQUEUE_*、线程优先级和栈大小要映射到真实 POSIX 运行约束。
- HAS_BT_CTLR、BT_CTLR_* 及指定旧平台配置已删除；外部硬件能力由 HCI 运行期确认。
- SMP 是内核多处理配置，BT_SMP 是蓝牙安全配置，不能混淆。
- BT_USERCHAN、UART、native_sim、board/SoC 设施改为明确的 EmBT 平台配置。

controller/Kconfig 入口已随 controller 目录移除。Host Kconfig 目前还引用 mesh、audio、
services 和 Classic 配置，这些源码和配置文件均保留。独立构建可以选择配置入口和功能开关，
但不能把删除相关文件作为缩减首期编译范围的手段。
不能保留整个原 Kconfig 根来掩盖依赖，也不能把未知扩展统一替换成真假常量。

## 8. 需要避免的误删

1. common/dummy.c 有数据布局、对齐和优先级断言，不是无用的测试文件。
2. hci_lockstep.c 管理命令额度及响应等待，上游公共构建包含它；首期是否链接需核对。
3. conn.h 引用 direction.h，conn.c 引用 iso_internal.h，关闭功能不等于可删共享类型。
4. smp.c 使用 crypto/bt_crypto.h 的工具箱，单独保留 host/crypto_psa.c 不足以覆盖安全函数。
5. ecc.c 的 long/system workqueue 选择必须与实现匹配。
6. 原 user-channel driver 依赖 device、DT、native_sim 和 nsi_errno；socket 处理可参考，
   但该文件不是已经可直接使用的独立 POSIX 驱动。
7. A2DP/SBC、OBEX/文件 Profile 及可选 services 的额外依赖不自动加入首期目标。

## 9. 提交组织和下一步

文档、规则、license、规划和源码范围说明合并到原 PR #2，保持一个文档 commit。
原 PR #3 被其替代并关闭；PR #4 改为直接基于 PR #2，继续保持单个代码 commit。
项目文档变更不进入代码 PR。同一板块的后续修正更新原 commit 和原 PR，不额外开修正 PR。
所有开发 PR 在运行基线验证前保持 Draft、未合入，最终 review 和合入由人执行。

下一步单独讨论头文件引用迁移和独立构建：把项目内必要引用从 zephyr/ 迁到 embt/，建立实际
源码/配置清单与 EmBT CMake/Kconfig 入口。随后实现 POSIX、HCI transport 和运行期注册，
再以编译及最终链接核对依赖。目录整理不改变协议算法、API 或 buffer 语义。

原 subsys/bluetooth 除 controller 外的内容迁移后继续受保护；仅本轮点名的根部 hardening.yaml 元数据例外。其他目录按消费者和接口替代
结果收敛，不能把首期源码种子当作删除白名单。

仍需明确：crypto backend/version、首期可选 Classic Profile、持久化要求、long workqueue
配置，以及实际控制器和 peer。最终 review 和合入由人执行。

## 附录 A：可选 Classic 源码

以下源码保留为可选，不自动加入首期 RFCOMM 目标。

- [bluetooth/host/classic/a2dp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/a2dp.c)
- [bluetooth/host/classic/a2dp_codec_sbc.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/a2dp_codec_sbc.c)
- [bluetooth/host/classic/at.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/at.c)
- [bluetooth/host/classic/avctp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/avctp.c)
- [bluetooth/host/classic/avdtp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/avdtp.c)
- [bluetooth/host/classic/avrcp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/avrcp.c)
- [bluetooth/host/classic/avrcp_cover_art.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/avrcp_cover_art.c)
- [bluetooth/host/classic/bip.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/bip.c)
- [bluetooth/host/classic/did.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/did.c)
- [bluetooth/host/classic/ftp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/ftp.c)
- [bluetooth/host/classic/goep.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/goep.c)
- [bluetooth/host/classic/hfp_ag.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/hfp_ag.c)
- [bluetooth/host/classic/hfp_hf.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/hfp_hf.c)
- [bluetooth/host/classic/hid_device.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/hid_device.c)
- [bluetooth/host/classic/hid_host.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/hid_host.c)
- [bluetooth/host/classic/map.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/map.c)
- [bluetooth/host/classic/obex.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/obex.c)
- [bluetooth/host/classic/opp.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/opp.c)
- [bluetooth/host/classic/pbap.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/pbap.c)

## 附录 B：其他 Host 源码

以下按实际配置决定，不作为一刀切删除清单。

- [bluetooth/host/aes_ccm.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/aes_ccm.c)
- [bluetooth/host/cs.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/cs.c)
- [bluetooth/host/direction.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/direction.c)
- [bluetooth/host/gatt_gap_svc_validate.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/gatt_gap_svc_validate.c)
- [bluetooth/host/hci_raw.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/hci_raw.c)
- [bluetooth/host/iso.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/iso.c)
- [bluetooth/host/monitor.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/monitor.c)
- [bluetooth/host/settings.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/settings.c)
- [bluetooth/host/smp_null.c](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/smp_null.c)

## 附录 C：原始头文件候选

这 93 个路径包含未启用编译器/功能分支，需要实际配置和编译收敛其构建使用范围。subsys/bluetooth 内的头文件依照保护边界全部保留，不能用候选名单删除；保护范围之外的依赖另行分析。

- [include/embt/bluetooth/addr.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/addr.h)
- [include/embt/bluetooth/assigned_numbers.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/assigned_numbers.h)
- [include/embt/bluetooth/att.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/att.h)
- [include/embt/bluetooth/bluetooth.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/bluetooth.h)
- [include/embt/bluetooth/buf.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/buf.h)
- [include/embt/bluetooth/byteorder.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/byteorder.h)
- [include/embt/bluetooth/classic/avrcp.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/classic/avrcp.h)
- [include/embt/bluetooth/classic/classic.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/classic/classic.h)
- [include/embt/bluetooth/classic/l2cap_br.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/classic/l2cap_br.h)
- [include/embt/bluetooth/classic/rfcomm.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/classic/rfcomm.h)
- [include/embt/bluetooth/classic/sdp.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/classic/sdp.h)
- [include/embt/bluetooth/conn.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/conn.h)
- [include/embt/bluetooth/crypto.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/crypto.h)
- [include/embt/bluetooth/data.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/data.h)
- [include/embt/bluetooth/direction.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/direction.h)
- [include/embt/bluetooth/gap.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/gap.h)
- [include/embt/bluetooth/gatt.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/gatt.h)
- [include/embt/bluetooth/hci.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/hci.h)
- [include/embt/bluetooth/hci_pkt.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/hci_pkt.h)
- [include/embt/bluetooth/hci_types.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/hci_types.h)
- [include/embt/bluetooth/hci_vs.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/hci_vs.h)
- [include/embt/bluetooth/iso.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/iso.h)
- [include/embt/bluetooth/l2cap.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/l2cap.h)
- [include/embt/bluetooth/testing.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/testing.h)
- [include/embt/bluetooth/uuid.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/bluetooth/uuid.h)
- [include/embt/debug/stack.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/debug/stack.h)
- [include/embt/net_buf.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/net_buf.h)
- [include/embt/random/random.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/random/random.h)
- [include/embt/sys/byteorder.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/byteorder.h)
- [include/embt/sys/crc.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/crc.h)
- [include/embt/sys/list_gen.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/list_gen.h)
- [include/embt/sys/math_extras.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/math_extras.h)
- [include/embt/sys/math_extras_impl.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/math_extras_impl.h)
- [include/embt/sys/minmax.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/minmax.h)
- [include/embt/sys/slist.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/slist.h)
- [include/embt/sys/util.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util.h)
- [include/embt/sys/util_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_internal.h)
- [include/embt/sys/util_internal_is_eq.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_internal_is_eq.h)
- [include/embt/sys/util_internal_util_dec.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_internal_util_dec.h)
- [include/embt/sys/util_internal_util_inc.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_internal_util_inc.h)
- [include/embt/sys/util_internal_util_x2.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_internal_util_x2.h)
- [include/embt/sys/util_listify.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_listify.h)
- [include/embt/sys/util_loops.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_loops.h)
- [include/embt/sys/util_macro.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/util_macro.h)
- [include/embt/sys/uuid.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/uuid.h)
- [include/embt/sys/zassert.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/sys/zassert.h)
- [include/embt/toolchain.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain.h)
- [include/embt/toolchain/armclang.h](https://github.com/zephyrproject-rtos/zephyr/blob/f8da8ea370508f274685a25ff19dbe5a9ff54790/include/zephyr/toolchain/armclang.h)
- [include/embt/toolchain/common.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/common.h)
- [include/embt/toolchain/gcc.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/gcc.h)
- [include/embt/toolchain/iar.h](https://github.com/zephyrproject-rtos/zephyr/blob/f8da8ea370508f274685a25ff19dbe5a9ff54790/include/zephyr/toolchain/iar.h)
- [include/embt/toolchain/iar/iccarm.h](https://github.com/zephyrproject-rtos/zephyr/blob/f8da8ea370508f274685a25ff19dbe5a9ff54790/include/zephyr/toolchain/iar/iccarm.h)
- [include/embt/toolchain/llvm.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/llvm.h)
- [include/embt/toolchain/mwdt.h](https://github.com/zephyrproject-rtos/zephyr/blob/f8da8ea370508f274685a25ff19dbe5a9ff54790/include/zephyr/toolchain/mwdt.h)
- [include/embt/toolchain/xcc.h](https://github.com/zephyrproject-rtos/zephyr/blob/f8da8ea370508f274685a25ff19dbe5a9ff54790/include/zephyr/toolchain/xcc.h)
- [include/embt/types.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/types.h)
- [bluetooth/common/assert.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/assert.h)
- [bluetooth/common/bt_settings_commit.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/bt_settings_commit.h)
- [bluetooth/common/bt_str.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/bt_str.h)
- [bluetooth/common/hci_common_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/hci_common_internal.h)
- [bluetooth/common/long_wq.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/long_wq.h)
- [bluetooth/common/rpa.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/common/rpa.h)
- [bluetooth/crypto/bt_crypto.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/crypto/bt_crypto.h)
- [bluetooth/host/addr_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/addr_internal.h)
- [bluetooth/host/adv.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/adv.h)
- [bluetooth/host/att_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/att_internal.h)
- [bluetooth/host/buf_view.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/buf_view.h)
- [bluetooth/host/classic/avctp_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/avctp_internal.h)
- [bluetooth/host/classic/avrcp_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/avrcp_internal.h)
- [bluetooth/host/classic/br.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/br.h)
- [bluetooth/host/classic/conn_br_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/conn_br_internal.h)
- [bluetooth/host/classic/did_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/did_internal.h)
- [bluetooth/host/classic/l2cap_br_interface.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/l2cap_br_interface.h)
- [bluetooth/host/classic/l2cap_br_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/l2cap_br_internal.h)
- [bluetooth/host/classic/rfcomm_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/rfcomm_internal.h)
- [bluetooth/host/classic/sco_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/sco_internal.h)
- [bluetooth/host/classic/sdp_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/sdp_internal.h)
- [bluetooth/host/classic/ssp.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/classic/ssp.h)
- [bluetooth/host/conn_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/conn_internal.h)
- [bluetooth/host/crypto.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/crypto.h)
- [bluetooth/host/direction_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/direction_internal.h)
- [bluetooth/host/ecc.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/ecc.h)
- [bluetooth/host/gatt_gap_svc_validate.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/gatt_gap_svc_validate.h)
- [bluetooth/host/gatt_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/gatt_internal.h)
- [bluetooth/host/hci_core.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/hci_core.h)
- [bluetooth/host/id.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/id.h)
- [bluetooth/host/iso_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/iso_internal.h)
- [bluetooth/host/keys.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/keys.h)
- [bluetooth/host/l2cap_internal.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/l2cap_internal.h)
- [bluetooth/host/monitor.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/monitor.h)
- [bluetooth/host/scan.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/scan.h)
- [bluetooth/host/settings.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/settings.h)
- [bluetooth/host/smp.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/host/smp.h)
