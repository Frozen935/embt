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

状态：本文由文档 PR #3 跟踪；目录整理在代码 Draft PR #4 的工作分支实施，二者尚未合入 main。独立构建和 POSIX 运行适配尚未完成。

## 1. 当前状态与已经确认的决定

| 项目 | 当前状态或决定 |
| --- | --- |
| M1 目标 | 独立编译并在 Linux/POSIX 上通过 HCI user socket 运行 BLE、BR/EDR Host |
| 源码现状 | 蓝牙模块已提升到 bluetooth/，头文件在 include/embt/，通用库在 lib/；其余待适配依赖暂存于 bluetooth/ |
| Controller 实现 | 原 bluetooth/subsys/bluetooth/controller/ 的 262 个文件已删除 |
| 当前执行状态 | controller 及其直接父级入口已移除，目录迁移已完成；头文件引用尚未从 zephyr/ 改为 embt/ |
| 原 subsys/bluetooth 其他内容 | 其余 457 个文件全部迁移保留；保护边界随文件迁移，不因路径改变而失效 |
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
| bluetooth/include/zephyr/ | include/embt/ | 迁移 2343 个文件，按保留源码引用裁剪后剩 706 个；内容未改 |
| bluetooth/lib/ | lib/ | 迁移 252 个文件，移除无关库后剩 81 个；保留 heap、net_buf、os、utils、uuid |

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

风格配置 `.clang-format`、`.editorconfig`、`.checkpatch.conf` 移到根目录；保留 Kconfig
工具及必要 CMake/checkpatch 风格工具。所有保留的原始源码和接口头文件内容保持不变。

从迁入的 [CMakeLists.txt](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/CMakeLists.txt) 移除
`CONFIG_BT_LL_SW_SPLIT` 下的 `add_subdirectory(controller)`，并将蓝牙内部 include
路径改为当前源码目录。从 [Kconfig](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/bluetooth/Kconfig) 移除
`rsource "controller/Kconfig"`。其余蓝牙入口和功能配置保留。

### 2.3 本轮验证与尚未实施的内容

- 共迁移 3052 个文件。目录迁移后逐项验证所有保留文件的 Git blob 和文件模式，内容及执行权限一致。
- 原蓝牙目录除 controller 外的 457 个文件全部有目标路径；迁移后仅修改蓝牙 CMake/Kconfig 的上述必要入口。
- C/C++、头文件和其他迁移文件内容不变；`bt_*`、`net_buf_*`、include guards 和 `<zephyr/...>` 引用尚未改名。
- 头文件目录已采用 `include/embt/`，后续单独迁移源码引用及构建 include 路径。本轮不提供兼容别名，也不表示可独立编译。
- 原硬件平台、设备树实现、无关驱动/子系统、HAL 和 west/native_sim 构建工具已清理。
- kernel、CRC、日志、settings、随机数、shell、heap/os 工具和密码学/codec 集成作为待适配依赖保留；保留不表示已经能在 POSIX 上运行。
- 头文件按保留源码的已有字面 include 关系检查，共保留 6694 条可解析的仓库内 include 边，没有因此新增已能解析的头文件缺失。生成头文件、宏展开、配置条件和最终链接闭包仍需在构建阶段验证。
- 不把名称含 controller、BT_CTLR 或 HAS_BT_CTLR 的所有接口和配置一并删除；Host HCI、事件、ACL 和外部硬件能力仍需保留。
- 保留项目许可、原始文件版权、完整来源记录和 Zephyr coding style 配置。

本轮未执行独立配置、编译、链接或硬件运行验证。剩余 `BT_CTLR_*`、`HAS_BT_CTLR`、
Zephyr CMake 函数和全局配置依赖在后续独立构建中处理，不以默认常量或空实现掩盖。

## 3. 本轮盘点的输入与限制

- 静态盘点快照：`2579bbc789709aec93e192dd5755493b72aaff07`。
- 原始源码 tree：`3dfaeb6628ab061e0a441013ab4b6d3caeb78551`。
- Zephyr 原始提交：f8da8ea370508f274685a25ff19dbe5a9ff54790。
- 平台：Linux/POSIX；HCI 通道：HCI_CHANNEL_USER；Controller 在 Host 进程之外。

以下盘点统计来自目录整理前的静态快照；文档中的文件链接已映射到当前路径，统计不代表当前构建闭包。

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
| subsys/bluetooth 其余所有源码 | 全部保留，不按首期种子名单裁掉文件 | 实际配置选择编译范围；移植按任务修改，不能删除 |
| Bluetooth 公共头文件 | 保留 API 和共享类型兼容 | 逐项核对消费者，不能仅因某功能关闭而删共享头 |
| lib/net_buf 与 net_buf.h | 保留算法和 API，替换 OS 对象及池注册依赖 | 分配、引用、分片、等待、pool ID 和销毁行为可用 |
| sys 工具和编译器支持 | 保留实际用到的部分 | Linux GCC/Clang 可用，不再拉入板级运行设施 |
| kernel、spinlock、clock、queue、memory 接口 | 用 POSIX 层提供所需契约 | 超时、取消、退出和对象生命周期真实实现 |
| device model 与 HCI driver 接口 | 改为显式 transport 配置和注册 | open/send/receive/close 和所需能力查询可用 |
| logging、printk、assert | 提供精简真实 backend | 输出、返回码和错误处理明确 |
| PSA crypto | 选择兼容并固定版本的外部 backend | 所选 AES/CMAC/ECDH 等算法及 API 可用 |
| settings/storage | 保留接口，按明确需求启用 | 已启用功能具备实际持久化或明确的内存语义 |
| 其他 Classic Profile 和 GATT service | 全部保留源码，首期可按配置不启用 | 配置、外部依赖和验证范围分别明确 |
| subsys/bluetooth 内的 Mesh、audio、shell 等 | 源码和相关文件全部保留，首期可不编译 | 禁止因功能未启用、依赖复杂或首期未验证而删除 |
| 保护范围外的 samples、tests、上游 doc 和 .github | 已按明确清单删除 | 不把这份清单扩展到待适配依赖 |
| arch、boards、soc、dts 及无关子系统 | 已在代码目录工作分支删除 | 必要接口声明保留，后续实现 POSIX/device/初始化替代 |

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
| logging/printk/assert | 实际输出和断言处理 |
| settings | 已启用能力的存储实现 |
| autoconf、syscall 生成接口 | 配置生成或必要兼容替换 |
| PSA/Mbed TLS | 外部库、版本和算法选择 |

必须覆盖 buffer pool 和 ID、system/TX workqueue、可选 long workqueue、transport、
LE/BR fixed channels、GATT services、连接及安全回调和 crypto 初始化。
先准备同步对象和 buffer；等待同步 HCI 响应前，接收处理必须已能运行；业务事件
进入前，其协议和回调已完成注册。失败和重试需要清理，不能重复注册对象。

## 7. 配置依赖不能用默认值掩盖

扫描范围之外有 100 个配置名候选，包含常量变量、日志模板生成符号、未启用功能、
平台全局符号和外部 provider 选项，并非 100 个已经确认缺失的配置。

- UINT8_MAX、UINT16_MAX、INT16_MAX 是标准常量替换，不是设备树扩展。
- NET_BUF、UUID、sys helpers 要同时考虑独立配置和实现。
- PSA_CRYPTO、PSA_WANT_* 需要与实际 backend 对齐。
- SYSTEM_WORKQUEUE_*、线程优先级和栈大小要映射到真实 POSIX 运行约束。
- HAS_BT_CTLR、BT_CTLR_* 要区分进程内 Controller 实现和外部硬件能力。
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

文档、规则和 license 准备继续更新 PR #2；规划及源码范围说明继续更新 PR #3。二者均为单提交 Draft PR。
目录整理作为独立代码板块通过 PR #4 跟踪，基于未合入的 PR #3 开发；代码 PR 不包含项目文档变更。同一板块的后续修正继续更新原 commit
和原 PR，不另加修正 patch；独立板块另行跟踪。所有 PR 在运行基线验证前保持 Draft、未合入。

下一步单独讨论头文件引用迁移和独立构建：把项目内必要引用从 zephyr/ 迁到 embt/，建立实际
源码/配置清单与 EmBT CMake/Kconfig 入口。随后实现 POSIX、HCI transport 和运行期注册，
再以编译及最终链接核对依赖。目录整理不改变协议算法、API 或 buffer 语义。

原 subsys/bluetooth 除 controller 外的内容迁移后继续受保护。其他目录按消费者和接口替代
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
- [include/embt/toolchain/armclang.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/armclang.h)
- [include/embt/toolchain/common.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/common.h)
- [include/embt/toolchain/gcc.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/gcc.h)
- [include/embt/toolchain/iar.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/iar.h)
- [include/embt/toolchain/iar/iccarm.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/iar/iccarm.h)
- [include/embt/toolchain/llvm.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/llvm.h)
- [include/embt/toolchain/mwdt.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/mwdt.h)
- [include/embt/toolchain/xcc.h](https://github.com/Frozen935/embt/blob/refactor/bluetooth-directory-layout/include/embt/toolchain/xcc.h)
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
