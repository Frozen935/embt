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

# EmBT 项目规划（Review v1）

状态：Review v1，作为开发中的工作文档通过草稿 PR 跟踪。init 来源说明和整包目录迁移已在 main；源码尚未裁剪，独立构建和 OS 适配尚未完成。

## 1. 定位与当前基线

EmBT 是源码起源于 Zephyr Bluetooth、独立维护的开源蓝牙 Host 项目。GitHub fork 关系已解除，原始 Git 历史保留；后续按 EmBT 架构演进，只选择性 cherry-pick 有价值的社区补丁。

| 项目 | 当前事实 |
| --- | --- |
| 仓库 | https://github.com/Frozen935/embt |
| 源码起点 | Zephyr main |
| 基线 SHA | f8da8ea370508f274685a25ff19dbe5a9ff54790 |
| 导入的源码基线 | 完整 Zephyr 工作树，共 159,172 个提交，70,681 个受版本管理文件 |
| 平台目标 | Linux POSIX |
| HCI 通道 | Linux HCI user socket，HCI_CHANNEL_USER |
| 协议范围 | BLE 和 BR/EDR；首批不含 Mesh 和 LE Audio |
| 首期机制 | 保留 Zephyr buf 和应用 API，先移植原有执行机制 |
| 长期方向 | HCI Command 异步化、单线程、应用操作接口异步化、静态内存收敛 |

源码起点和社区维护规则见 [UPSTREAM.md](UPSTREAM.md)；初始化来源记录草案见 [upstream/INIT.md](upstream/INIT.md)。

## 2. 已确认规则

| 编号 | 规则 |
| --- | --- |
| R1 | 首先提交 init 来源文档；之后每批社区补丁按时间文件归档，保留原始 comments、完整 SHA、作者和对应关系 |
| R2 | 项目使用 Apache-2.0 和统一完整文件头模板；模板不固定公司名称，版权按实际主体填写；导入源码保留原始许可和版权 |
| R3 | 继续遵循 Zephyr coding style |
| R4 | Zephyr 蓝牙源码及必要依赖集中到 bluetooth/；项目文档单独放 docs/；其他源码、原 samples/tests 删除 |
| R5 | 先 review 规划，再按通过的规划实施 |
| R6 | 保留 Kconfig 和 CMake；移除、适配或替换 Zephyr 专有配置扩展；构建入口使用 EmBT 自己的函数 |
| R7 | 提交和 PR 使用英文标题及详细说明；代码和 review comments 不使用中文；当前阶段所有研发 PR 保持草稿、未合入，直到运行基线验证后由人决定合入；Agent 永不直接合入 |
| R8 | 移除设备树、重新适配驱动；保留 buffer；先适配 POSIX；运行期对象和注册机制改为显式初始化 |
| R9 | 基础目标是让协议栈实际跑起来 |

M0 的仓库及源码基线已建立，整份 Zephyr 已迁移到 bluetooth/。文档和后续实现均沿本地工作分支推进，通过草稿 PR 记录，调试到实际运行前不合入主线；继续本地开发不依赖 PR 已合入。向 Zephyr 提交贡献时遵守其上游规则。

## 3. 目标目录

当前已将整份 Zephyr 基线迁移到 bluetooth/。以下是后续裁剪和独立构建的目标结构，尚未按该细化层级实施。

~~~text
embt/
├── bluetooth/                  # 导入的协议源码和必要工具库
│   ├── host/
│   │   └── classic/
│   ├── common/
│   ├── services/               # 必要、可选的 GATT services
│   ├── lib/
│   ├── deps/
│   │   ├── net_buf/
│   │   └── sys/
│   └── include/zephyr/         # 首期保留必要头文件路径
├── port/                       # EmBT 的运行适配
│   ├── compat/                 # 必要的 Zephyr 接口兼容
│   ├── posix/
│   └── hci/linux_usersock/
├── third_party/                # 按需、固定版本的外部依赖
├── cmake/
├── config/
├── tools/                      # 配置生成和必要开发工具
├── docs/
│   ├── PLAN.md
│   ├── UPSTREAM.md
│   ├── OPENBLUE_REFERENCE.md
│   ├── upstream/
│   │   ├── INIT.md
│   │   └── YYYY-MM-DD_HHMMSS.md
│   └── templates/LICENSE_HEADERS.md
├── CMakeLists.txt
├── Kconfig
├── README.md
└── LICENSE
~~~

- 首期保留必要的 zephyr/... 头文件布局、bt_* API、net_buf/net_buf_simple 名称，减少与功能无关的改名。
- 重新适配的驱动、OSAL 放 port/；原始出处记录到路径映射。
- samples/ 和 tests/ 暂不建立。运行验证使用仓库外调用程序或现有工具，验证命令和结果记入文档。
- 不重写 Git 历史。目录迁移、裁剪和行为修改分开记录，纯迁移尽量保持文件内容不变。

### 3.1 保留、替换和删除边界

| 来源 | 处理 |
| --- | --- |
| subsys/bluetooth/host，包括 classic | 保留；按配置构建 BLE/BR 核心及相应 Profile |
| common、lib、必要 services | 按实际使用保留 |
| Bluetooth 公共头文件、lib/net_buf、必要 sys 工具 | 收敛到 bluetooth/ 的头文件和依赖目录 |
| kernel、日志、settings、密码学、HCI 接入 | 识别所需接口；保留必要公共代码，运行实现改接 POSIX 或外部库 |
| controller、mesh、LE Audio 目录 | 移除首批目标中不使用的代码和配置 |
| boards、soc、arch、设备树、无关 drivers/subsys | 替换其必要接口后删除 |
| samples、tests、Zephyr SDK/west/native_sim 构建设施 | 不保留为 EmBT 运行或构建依赖 |
| 风格配置、必要检查工具、版权和许可文件 | 按职责保留必要部分 |

LE Audio 的排除不影响 Classic 的 HFP、A2DP、AVRCP 源码。具体 Profile 的首轮运行验收在第 8 节单独确定。

### 3.2 裁剪依据

实施前产出路径和依赖清单：原路径、目标路径、消费者、配置条件、保留原因、适配方式及验证方法。依赖分析同时覆盖 include、Kconfig、编译目标、链接符号和运行期注册关系。

当前已确认的候选依赖包括 kernel.h、net_buf、原子操作、链表、字节序、日志、settings、PSA crypto、iterable sections。候选清单不是最终保留集合。禁止通过保留整个 Zephyr kernel、伪造成功返回值或空注册遍历掩盖缺失实现。

## 4. 项目规范

### 4.1 社区更新文档

- 首份文档为 docs/upstream/INIT.md，说明仓库、分支、完整 SHA、提交描述、获取方式和历史保留结果。
- 后续每批引入使用 docs/upstream/YYYY-MM-DD_HHMMSS.md，文件名按 Asia/Shanghai 时间生成，正文包含带时区时间。
- 先列补丁，再附每个原始 commit 的完整消息、作者、日期和原始 SHA；建立上游 SHA → EmBT SHA 对应关系。
- 保留原始代码注释及有效语义。适配说明追加到独立段落，保留原始 commit message 的段落和 trailers。
- 本版把 comments 定义为原始 commit message 与代码注释；PR/review 讨论保留出处链接，是否全量归档待 review。
- 每周检查游标与已应用补丁列表分开记录。补丁适用性依据当前代码，不仅依据 SHA。
- EmBT main 不整体 merge/rebase 到 Zephyr main。

### 4.2 License 与 copyright

原始 Apache-2.0 正文保留在 bluetooth/LICENSE。统一新文件头模板见 [LICENSE_HEADER_TEMPLATE.md](../LICENSE_HEADER_TEMPLATE.md)。

文件头使用完整 Apache-2.0 通知和通用版权主体占位，不固定公司名称。迁移或 cherry-pick 时保留原始 copyright、SPDX 和适用的第三方许可；实际新增或修改内容按其真实版权归属填写。

### 4.3 Coding style 与提交

- C 沿用 Zephyr：8 列 tab、100 列行宽、snake_case、所有控制语句有花括号、C 注释使用 /* */，公共接口使用 Doxygen。
- CMake 沿用 Zephyr 的 2 空格缩进及相关命名规范；Kconfig 按原有 style 维护。
- 风格只作用于新增、修改行，避免迁移时批量格式化。
- 每个提交和 PR 都使用英文标题及详细说明，描述目的、范围和实际验证结果。代码及 commit/PR/review comments 不使用中文；社区补丁保留其原始说明、作者和来源。
- 代码、文档及配置均通过工作分支和 PR 交付，Agent 禁止直接更新 main 等集成分支、合并 PR 或启用 auto-merge。最终合入由人操作，没有 init 或小改动例外。
- 当前及后续研发 PR 都保持草稿、未合入，直到已约定的运行基线实际验证完成。编译成功或 PR 已创建不表示运行门槛已达到；解除该阶段限制和最终合入由人决定。
- 后续任务可以从所需的未合入工作分支继续开发，在本地累积可调试代码，每一步通过增量草稿 PR 跟踪并声明依赖。
- “提交”“提上去”等指令不包含主线合入授权。具体边界遵循仓库 AGENTS.md 的规则。
- 目录迁移、配置调整、接口适配分别组织，便于定位和复查。

## 5. 构建系统

### 5.1 Kconfig

保留标准 Kconfig 语义、CONFIG_BT_* 和必要变量替换。使用固定版本的 Kconfiglib 处理配置、生成最终 .config 和 autoconf.h；CMake 使用同一份解析结果决定源码和依赖。

| 配置类别 | 处理 |
| --- | --- |
| 标准 bool/int/hex/string、依赖和变量替换 | 保留；例如 UINT16_MAX 可放入独立常量文件 |
| Zephyr 的 dt_* Python 函数、板级/SoC/架构探测 | 移除设备树依赖，改为显式 EmBT 配置或平台能力 |
| 不支持功能的配置 | 删除或关闭对应功能入口，并同步处理源码 |
| 所需功能依赖的 Zephyr 全局符号 | 按实际语义定义 EmBT 配置或改写依赖 |
| 尚未识别的扩展 | 报出文件、符号和原因，明确处理后再通过配置阶段 |

配置解析不能静默吞掉未定义符号或扩展。切换配置后，头文件、CMake 源码选择及链接依赖必须一致。

### 5.2 CMake

不使用 find_package(Zephyr)、Zephyr SDK/west 入口或 zephyr_* 构建函数。建立最小的 embt_* helper 集合，底层使用标准 target_* 命令。

建议首批 helper 覆盖库创建、源码加入、按配置加入源码、头文件路径、链接依赖和按配置加入子目录；参数和命名在实施设计中确定。

产物以 libembt.a 和必要公共头文件为主。配置、include、编译选项和外部依赖按 target 绑定；协议源码只编译一次，PRIVATE/PUBLIC/INTERFACE 范围明确。依赖版本可追溯，构建过程不自动修改全局 Python 或系统依赖环境。

## 6. 运行适配

### 6.1 POSIX

提供必要的线程、同步、队列、work、delayed work、定时器、时钟、原子操作和存储接口。优先保持 Zephyr 调用语义，避免同时批量修改协议逻辑。

必须明确：NO_WAIT/FOREVER/有限超时、相对和绝对时间、线程创建与启动、取消/等待完成、对象销毁、信号量上限及返回码。pthread 的直接返回码不能按 errno 处理；有限超时不能退化成永久阻塞。线程和定时器只能在其依赖对象就绪后进入回调。

### 6.2 HCI 驱动

移除 DT_CHOSEN、DEVICE_DT_GET、DEVICE_DT_INST_DEFINE、native_sim trampoline 和命令行设施，使用显式 HCI transport 配置及注册。

一期只实现 Linux HCI_CHANNEL_USER：控制器编号、open/close、收发、接收线程及停止流程、buf 所有权和错误处理。检查控制器独占与权限条件。保留必要原始协议处理经验，默认配置不沿用 openblue 的 Unix bridge。

HCI 接收处理须在同步 HCI 命令等待响应前可运行；连接相关注册须在业务事件处理前就绪。不能把驱动注册、启动接收、完成 Host 初始化当作同一事件。

### 6.3 Buffer

保留 net_buf/net_buf_simple 的容量、引用计数、数据操作和分片语义。适配内存池、等待机制、pool ID 查找及销毁回调；Command/Event/ACL 的池配置和耗尽行为明确。

使用静态存储的 buf 和运行对象，也通过运行时函数完成初始化与注册。首期不整体更换 buf API；长期静态内存收敛仍单独验收。

### 6.4 显式初始化

将 SYS_INIT、自动 linker section 注册、K_*_DEFINE 及同类机制涉及的运行对象逐项纳入显式初始化。const 表和静态存储可以保留，OS 对象及注册动作由运行期调用完成。

登记表必须真实收集并调用对象，覆盖池、work queue、驱动、LE/BR fixed channels、GATT service、连接及其他回调。初始化顺序按依赖定义，不能只保留优先级字段而不实现其语义。

初始化失败返回确定错误，并逆序回收已创建资源；重试不重复注册。退出先停止输入及异步任务，再确认回调和引用已回收，最后销毁资源。M1 保留原有多线程和同步 HCI 行为；全异步、单线程放到后续阶段。

### 6.5 密码学与存储

当前 main 的 Host crypto 使用 PSA 接口。评估可支持其算法和 API 的固定版本 backend，不能直接认定 openblue 的 mbed TLS v2.28.8 依赖适用于当前基线。保留配对、加密所需的 AES/CMAC/CCM/P-256 和随机数能力。

保留 settings/存储接口。首版是否必须完成重启后的 bond 恢复待 review；任何暂未实现的功能均显式禁用或报不支持。

## 7. 分阶段实施

| 阶段 | 交付 | 验收 |
| --- | --- | --- |
| Review | 本规划、openblue 参考、来源记录、license 模板 | 规则、范围和阶段通过 review；不修改代码 |
| M0 文档记录 | INIT 已记录；项目文档通过草稿 PR 持续跟踪 | 来源、SHA、许可和维护规则可读回，不要求先合入才能继续开发 |
| M1.1 依赖与结构 | 路径白名单、迁移、裁剪、EmBT 项目规范 | 来源可追溯；样例、测试、设备树和无关代码按清单移除 |
| M1.2 独立构建 | Kconfig、CMake、必要兼容接口 | 配置/头文件一致；能编译库；未解决链接符号有明确归属 |
| M1.3 运行适配 | POSIX、真实注册表、HCI、crypto/存储接入 | 外部调用程序完整链接；初始化和失败退出可执行 |
| M1.4 基础连通 | BLE/BR 协议流程、证据及剩余依赖清理 | 达到第 8 节运行标准，获得独立运行基线 |
| M2 | HCI Command 异步化及调用链状态机 | 命令额度、响应、超时和错误路径正确 |
| M3 | 单线程事件循环与应用操作接口异步化 | 统一协议上下文，无依赖本线程后续处理的阻塞等待 |
| M4 | 静态内存预算和运行期对象池收敛 | 容量可计算、占用可观测、耗尽可恢复 |

M1.1～M1.3 允许按依赖迭代，但通过各阶段验收后才标记完成。静态库编译成功不代表最终调用程序已链接或协议已运行。运行基线验证前所有研发 PR 不合入；本地工作分支保留已完成的依赖改动，供后续调试。

## 8. 基础目标与验收

### A. 初始化

外部程序链接 libembt.a，通过实际 HCI user socket 完成 Controller 和 Host 初始化。保留命令响应及错误证据，不能只依据 Bluetooth ready 文本判断完整连通。

### B. BLE 与 BR/EDR

建议首轮矩阵：

- BLE：扫描/广播、连接/断开、GATT 读写或通知、配对和加密。
- BR/EDR：发现、ACL 连接/断开、配对和加密、SDP、RFCOMM 双向数据。
- HFP/A2DP/AVRCP 等保留为可配置的 Classic 能力，其首轮必验范围待 review。

运行需要一个可独占、支持双模的 Linux 控制器和相应 peer。当前研究没有执行硬件验证。

### C. 资源与复现

覆盖初始化失败、重复初始化、退出/重启、命令超时、buf 耗尽及断开后的引用回收。记录 Release/Debug 配置、依赖版本、控制器和 peer、调用命令、HCI/日志证据及结果。外部验证程序不作为 samples/tests 导入仓库。

## 9. openblue 经验与 review 项

源码研究基于 openblue main 0012adc908c5d5217c320236c15739d141b2ba8e。详细机制、证据和适配边界见 [OPENBLUE_REFERENCE.md](OPENBLUE_REFERENCE.md)。

重点采用分层、显式 init、fixed channel 注册、POSIX 和 user channel 的经验；重新核对当前主线 API、完整注册路径、超时/线程语义、CMake target 范围和依赖版本。

本轮请优先 review：

1. bluetooth/ 收纳协议及依赖，port/ 收纳新运行适配的分层。
2. 首期保留必要 zephyr/... 头文件布局和 buf/API 名称。
3. 基础运行矩阵是否采用 BLE GAP/GATT + BR SDP/RFCOMM，哪些 Classic Profile 必须首轮跑通。
4. comments 是否需要扩展为全量 PR/review 评论归档。
5. 首版是否要求重启后的 bond 恢复。

这些项目的实现都在规划通过后开始。
