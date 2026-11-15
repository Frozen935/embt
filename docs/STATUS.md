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

# 当前状态与待办

更新时间：2026-11-23。文档梳理与架构总结已完成本地草案，等待用户 review。
先前线程句柄统一已更新本地 patch；本轮 include 聚合、宏清理和 embt namespace 已按职责更新本地 patch。
其他代码收尾、运行验收、合入准备和社区同步仍为待办；未调整配置或 PR 依赖。

## 当前基线

源码来源为 Zephyr main，完整 SHA `f8da8ea370508f274685a25ff19dbe5a9ff54790`。
最近审查的源码快照为 `a258575094560a7c69561532fb4574e73f4d4754`，
位于本地 `refactor/bringup-patch-series`。后续文档重排保留了该源码内容；
2026-11-23 的线程句柄统一是此后的独立变更，不能与旧审查快照混为一谈。
原始 Git 历史与来源记录保留。
代码已具备独立构建、完整链接、shell 和基础系统机制；完整 M1 无线运行验收尚未通过。

## 最近验证

以下是 2026-11-22 整体审查的结果；本轮线程变更的验证在下一节单独记录。

| 范围 | 结果与边界 |
| --- | --- |
| GCC 广泛配置 | 生产库 whole-archive 链接、btshell 构建通过 |
| Clang 18 OSI | ASan/UBSan 下 23/23 通过 |
| 独立 framework profile | 79/79 通过；默认 core test-port 入口仍有 CCM 配置前提缺失 |
| Bluetooth C 默认配置 | 60 个单元；1086 项通过、18 项跳过、0 项失败 |
| 已声明配置变体 | 49 个 profile；940 项通过、180 项跳过、0 项失败 |
| Classic/Bumble | 规范入口 176 项通过；另行执行的 L2CAP server 16 项通过，但规范入口的元数据选择错误仍未修复 |
| 实机 | 历史记录覆盖 USB Controller 初始化与重复启停；未完成实机 BLE/BR 连通矩阵 |

测试分类与结果口径见 [测试指南](TESTING.md)，原始审查材料作为本地归档保留。
软件通过数不代表全部 API、
全部配置、实际无线互通或目标平台已通过。

## 线程句柄统一（2026-11-23）

线程与身份统一使用 `os_thread_t`，POSIX 直接采用 `pthread_t`；self 返回借用身份，
create 产生一次 join 责任，copy 不增加所有权。比较改为 `osi_thread_equal`，
is_current 接收只读句柄指针；HIDS 用锁深度控制 owner 有效性，不再依赖零值或直接比较。
priority、stack、start、cancel 和无超时 join 的原有语义保留。

| 本轮验证 | 结果 |
| --- | --- |
| GCC / Clang 18，ASan/UBSan | 同一份 OSI 契约各 24/24 通过 |
| 线程契约重复执行 | 5 项各 20 次，共 100 次通过 |
| 广泛 framework profile | 80/80 通过，含新句柄身份 case 和共享断言框架 |
| Bluetooth C 默认 profile | 60 个单元成功，1086 项通过、18 项跳过、0 项失败 |

本机证据在 `/tmp/embt-single-thread-handle-20261123/`，没有执行实机无线、原生 NuttX
或完整 Classic/配置变体矩阵。现有命令在途关闭/Mesh 调度 skip 保留，其他已知问题未修复。

## Include 聚合与宏清理（本地提交，未推送）

头文件集中为 include/embt、include/embt/base 和 include/embt/osi；sys 与 types 目录已退役。
类型与操作按 queue/work/thread/sync/timer 等功能合并，原结构布局和运行合同保持。
后端 thread_config.h 留在 osdep/posix。list_gen 已移除，slist 直接实现。

清理无当前消费者的 div64/旧 types、section 头和两个未用 SCO 生成宏；移除确认无用的
COND_CASE、indexed/fixed foreach、GENMASK64、参数截取/拼接及 DEC/X2 宏与专属实现。
共享 FOR_EACH/LISTIFY/UTIL_INC、位操作和必要辅助层保留。类型和头文件可见不授权
直接操作内部状态。后续已统一为 embt/base 与 embt/osi namespace，保留 lib 原目录；
本地提交不表示运行验收或合入，也未推送或调整 PR 依赖。

| 本轮验证 | 结果 |
| --- | --- |
| GCC / Clang 广泛配置，ASan/UBSan | 各 80/80 framework/机制回归通过 |
| OSI-only GCC | 24/24 通过 |
| Bluetooth C | 60 个单元成功；1086 项通过、18 项原有跳过 |
| 配置变体 | 49 个 profile 成功；940 项通过、180 项原有跳过 |
| 功能头独立检查 | C 169/169；C++ 168/169，HFP HF 旧 operator 字段的关键字冲突保留 |
| 布局与工具合同 | GCC/Clang 各 39 项布局记录与原版一致；C/C++ 宏及链表合同 8 项通过 |

布局结果只对应当前 Linux x86_64，不是原生 NuttX/32 位验证。private detail 头由所属入口
间接检查，OSI 头使用其真实 target feature flags；不把实现辅助头当成独立应用入口。
HFP HF 的 C++ 字段命名问题已在旧接口复现，本轮不改变该回调 API。
本机记录在 `/tmp/embt-header-consolidation-20261123/`，未执行实机无线或完整 Classic 矩阵。

### 最终 namespace 与 BR fixture 验证

最终路径统一为 `include/embt/base` 和 `include/embt/osi`，lib 只更新必要的头文件引用。
记录在 `/tmp/embt-namespaced-headers-20261123/`，当前头声明与结构布局未因路径变化改变。

初始并发回归复现 BR RET/FC fixture 的两个状态观察失败：CONFIG 尚未成为 CONNECTED，
以及 CONNECTED 尚未成为 DISCONNECTING。FIFO 已发布报文不表示发送调用和完成回调已结束。
fixture 现在经过现有 Host 锁和 system workqueue 完成边界再观察状态，原协议断言保留。
重复验证另复现分配重试后的报文等待超时：协议重试最长 100 ms，原等待期也为 100 ms。
只把应当出现报文的等待预算设为 1 s，并静态检查它小于协议重传超时；原 100 ms echo/
无报文检查保留。没有修改协议生产逻辑、优先级、命令关闭延期或 held-buffer 场景。
两项失败 case 修正后分别连续 100 次通过；原始失败、阶段复测和最终回归分开保存。

### net_buf 正常复用日志修正（2026-11-23，本地提交，未推送）

`uninit_count == 0` 只表示所有 descriptor 都已经初始化过，不代表 free LIFO 为空。
此前正常复用时也直接进入 starvation 日志路径，导致重复输出 low on buffers 和 blocked
for 0 ms。现在先执行 `BT_NO_WAIT` 获取，成功即继续分配；只有无空闲 descriptor 且允许
等待时才输出原 warning，并继续使用原绝对 deadline。等待计时在该分支内读取，不依赖
当时的 runtime 日志开关。真实竞争仍可能在毫秒精度下显示 0 ms。

实际库接口探针在 ASan/UBSan 下验证：空闲复用零 warning；无等待耗尽返回 NULL 并保留
错误日志；25 ms 耗尽超时保留 warning/error；延迟释放后成功唤醒并获得同一 descriptor。
framework 回归 80/80 通过。全部 60 个 C 测试单元中 59 个成功，合计 1085 通过、18 原有
跳过、1 失败：`bt_disable__test_data_sent_during_disable` 在关闭后再次 enable 时偶发
返回 `-EIO`（`host_disable/src/main.c:808`）。以 HEAD 原始 buf.c 替换唯一对象后重新链接
的基线也在第 37 次复现相同失败；修正版本重复执行在第 12 次失败。本轮记录该已有问题，
不修改关闭/重启逻辑，也不重新实施延期的命令在途关闭修复。
记录在 `/tmp/embt-netbuf-warning-20261123/`；未重新执行实机无线验收。

### 本轮修复与验证（2026-11-23，本地提交，未推送）

测试入口统一校验新生成的 JUnit，拒绝零执行、全跳过、失败和旧结果；Classic L2CAP server
改为选择实际 server 文件，GAP/SMP 响应检查同时保留命令返回内容和后续异步内容。aggregate
core 使用项目 Kconfig driver 加载独立测试 fragment，补齐 CCM 和 AT/HFP provider 前提。

shell 恢复 mandatory 为零时由 handler 校验参数的原生语义；Mesh legacy advertiser 已启动
时返回成功，创建失败仍返回原错误。CDB 的两处显式注册匹配 KEY_SYNC 条件，A2DP 必须选择
SBC provider。PSA 使用要求随公开 Mesh 类型传给消费者；SYSTEM、SOURCE 和 EXTERNAL
保留各自 provider 配置，不改用另一套库或隐式下载。

关闭 Host crypto 时使用独立 `crypto_hci.c` 提供 HCI LE Rand/Encrypt，保留 LE/BE 字节序，
检查短响应并释放命令/响应 buffer；Host crypto 开启时仍使用原 PSA provider。该 fallback
需要 Controller 支持对应 HCI 命令，不能据模型测试宣称所有硬件均支持。

Host 本轮为关闭/重新打开新增的全部锁区已按用户 review 撤销，`hci_core.c` 恢复到本轮
修改前版本。测试新增的 recycled 通知、workqueue drain 和内部 Host 锁屏障一并撤销。
此前新增的 closed-purge/reopen 用例和 FIFO 调度 wrapper 也已撤销，受控复现及重复运行
记录仅作为历史证据，不计入当前验收。`data_sent_during_disable` 保留原断言和函数体，
在入口明确 skip；与既有命令在途关闭用例一起延期，等待生命周期和线程模型重新设计。

| 验证 | 结果 |
|---|---|
| 撤销锁方案前的历史 C 全量 | 60 个单元成功；1087 通过、18 跳过；含已删除的受控用例 |
| 撤销锁方案前的历史配置变体 | 49 个 profile 成功；941 通过、183 跳过；含已删除的受控用例 |
| 受影响 Classic/Bumble | GAP Central 6、GAP Server 10、L2CAP Server 16、SMP 10；共 42 通过 |
| broad Mesh/Audio 与 framework | CDB 开启、KEY_SYNC 关闭、legacy advertiser；82 项通过 |
| SOURCE/EXTERNAL 公开 Mesh 类型 | 消费者编译及真实 PSA capability probe 通过 |
| HCI crypto fallback | 命令参数/字节序、短响应、错误传播和 buffer 释放的模型测试通过 |

历史全量及此前的 1000/200 次重复结果不能作为撤销锁方案后的通过证明；当前方案仅使用
撤销后的实际复测结果。新跳过的 data_sent_during_disable 不计为通过，原失败和旧方案
记录保留，不能用 skip 声称关闭/重启并发问题已解决。
撤销后 Host 默认单元为 8 通过、5 跳过；4 个 Host 配置 profile 均成功，合计 12 通过、
40 条配置性/延期跳过。记录为 `host-disable-deferred/` 和 `host-variants-deferred/`。

### 本地提交前的最终复测（2026-11-23）

撤销锁方案后重新执行全量 C 单元与配置变体。60 个 C 单元中 59 个成功、1 个失败，
合计 1084 通过、19 跳过、1 失败。失败为 BR RET/FC 的 `test_max_transmit`：在
`host_l2cap_br_ret_fc/src/main.c:284`，`bt_work_submit(&completed_work) >= 0` 断言失败；
有限次数的独立重复运行也复现相同失败。当前尚未确定是 work 机制还是 fixture 的并发
生命周期问题，本轮只记录，不新增锁、wrapper 或修改该 case 的期望来制造通过。
49 个配置 profile 均成功，合计 939 通过、181 跳过，包含此次延期的 Host 生命周期用例。
受影响的 Classic/Bumble 在撤销锁方案后再次验证，4 个单元的 42 个场景全部通过；模型
Controller 的成功不代表实机无线验收已完成。
记录在 `c-commit-tree/`、`variants-commit-tree/` 及
`/tmp/embt-local-submit-20261123/ret-fc-repeat.log`，原失败不被后续记录覆盖。
原失败、修正阶段和最终记录保留在 `/tmp/embt-fix-now-20261123/`。本轮未执行实机无线
验收或原生 NuttX 运行；Bumble 使用模型 Controller，不计为真实设备互通。

## 待办：代码收尾与验收（验收及合入准备暂不执行）

- [ ] RFCOMM 连接前失败/超时清理和同对象重试的资源生命周期；按用户决定留待线程模型改造，本轮不修改。
- [x] 修复默认 aggregate test-port 的 CCM 和 AT 配置前提；本地提交，未推送。
- [x] 修正 Classic L2CAP server 的测试选择；本地提交，未推送。
- [x] 统一 pytest/linked 的实际执行检查，拒绝 collect-only/零执行误报成功；本地提交，未推送。
- [x] 修正 GAP/SMP helper 丢失已读命令响应的问题；本地提交，未推送。
- [ ] 关闭态 TX 清理与再次 enable 的生命周期边界；本轮锁方案和测试同步屏障已撤销，data_sent_during_disable 明确跳过。
- [ ] 关闭与重新打开并发窗口的自然运行验证；FIFO 调度 wrapper 和受控用例已撤销，专项验证延期。
- [ ] 按原生测试模块和 fake 依赖边界复核新增 __wrap；逐项说明保留理由，禁止靠内部锁或调度拦截制造应用 case 通过。
- [ ] 排查 BR RET/FC test_max_transmit 中 completed_work 提交失败；最终 C 全量与独立重复运行均已复现，不能记为测试通过。
- [ ] 完成最终代码 SHA 的实机 BLE/BR 广播、扫描、连接、安全、GATT/RFCOMM 与重启恢复验收。
- [ ] 验收之后准备代码交付：核对 PR 依赖、必要 license、分组提交及最终 SHA；由用户 review 和合入。

## 待办：可选能力与后续优化（暂不执行）

- [x] Mesh legacy advertiser 的重复 enable 幂等语义；本地提交，未推送。
- [x] CDB/KEY_SYNC 注册条件与真实 provider 保持一致；本地提交，未推送。
- [x] DFU/BAP 零计数 shell descriptor 的参数校验语义；本地提交，未推送。
- [ ] socket ISO H4 收发与 buffer 类型支持。
- [x] SOURCE/EXTERNAL PSA 需求经公开 Mesh 类型正确传递；本地提交，未推送。
- [x] 关闭 Host crypto 时保留实际 RNG/Encrypt provider；本地提交，未推送。
- [x] A2DP/SBC 的配置与 provider 依赖；本地提交，未推送。
- [x] 删除无消费者、缺少实现的 div64 头；不据此宣称 32 位目标已验收。
- [x] 修正 net_buf 正常复用的错误 starvation 日志；真实耗尽和等待诊断保留，本地提交，未推送。
- [ ] 删除字符串转换函数的隐式 TLS 缓存，改为调用者存储。
- [ ] 原生 NuttX 平台适配和运行验证；macOS 另行设计。
- [ ] HFP HF 公开头的 C++ operator 字段命名兼容性，单独评估回调接口变更。
- [ ] 后续 HCI/应用接口全异步、单线程和静态内存收敛。

命令在途关闭、data_sent_during_disable 与三个 Mesh 调度契约 case 明确延期，不计为通过。
先前命令关闭修复尝试已撤回；本轮不重新实施。未证明的 ATT 并发疑点继续保留为分析项。

## 待办：社区同步（暂不执行）

- [ ] 建立上游检查范围、路径映射、依赖筛选和补丁状态记录。
- [ ] 选择 1～3 个有价值的补丁，验证导入、来源、适配、测试和重复检查。
- [ ] 试运行通过后建立每周筛选流程，禁止自动更新或合入 main。

规则见 [上游维护](UPSTREAM.md)。当前有 INIT 和维护约定，尚未验证实际同步工具或新增批次。
