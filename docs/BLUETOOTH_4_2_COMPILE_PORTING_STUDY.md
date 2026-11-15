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

# Bluetooth 4.2 基础 LE 与 BR/EDR：编译适配调研

最新状态：本调研的依赖适配已经实施，基础 LE+BR 已完成源码编译、完整链接和机制/传输测试。
[实施 Review](BRINGUP_IMPLEMENTATION_REVIEW.md) 记录最终分类改动、配置和验证。下面的首次编译失败是历史调研结果，不能作为当前状态。

状态：调研草稿，仅新增本文，未修改协议栈代码、未提交或推送。
目标：Linux/POSIX 上开启 Bluetooth 4.2 基础 LE 与 BR/EDR，将选定源码编译为 `libembt.a`；
不启动协议栈，不打开 HCI socket，不进行连接、配对或数据传输验证。建议同时完成一个只链接、不执行的检查，避免静态库
生成成功却仍缺少 OS/驱动实现。链接检查不等于增加一个运行目标。

调研日期：2026-11-14。

| 输入 | 本次读取的版本 |
| --- | --- |
| EmBT 构建分支 | `build/standalone-cmake-kconfig`，`0886297ef3b58590ce3027773cd5bdf16484594b`，Draft PR #5 |
| Zephyr 来源 | `f8da8ea370508f274685a25ff19dbe5a9ff54790`，保留原历史 |
| openblue 本地源码 | `/home/frozen/work/opensource/stack_dev/openblue`，`3f8697c608a4b23cd59981a18f09e3d3838e0f72` |
| 工具链 | Linux x86-64，GCC 13.3.0，CMake 3.28.3，Ninja 1.11.1，Kconfiglib 14.1.0 |

openblue 的 tracked 文件没有本地修改；其未跟踪环境目录没有作为源码参考，也没有运行其程序。
本次直接读取上述版本，区别于 [既有 openblue 研究](OPENBLUE_REFERENCE.md) 使用的较早快照。

## 1. 已确认的编译范围

用户已明确：本轮面向Bluetooth 4.2的标准基础功能，包含LE advertising、scanning、LE连接、
SMP及BR/EDR。LE Audio、Mesh、Channel Sounding等暂不纳入。
本轮目标继续是编译/链接通过、不运行，不扩展成全部4.2可选功能或协议一致性认证。

| 功能层 | 首期保留的编译范围 |
| --- | --- |
| LE广播与扫描 | Legacy advertising/scanning；Central、Peripheral、Observer、Broadcaster |
| LE连接与数据 | ACL连接、共享L2CAP、ATT/GATT server和client、MTU/读写/通知等基础流程 |
| LE安全 | SMP Legacy Pairing与LE Secure Connections；保留AES/CMAC、P-256/ECDH及PSA随机数需求 |
| BR/EDR | HCI、BR连接、密钥与SSP、L2CAP、SDP、SCO；现有LE/BR共享能力和CTKD配置保留 |
| 可选Classic profile | RFCOMM/SPP、A2DP、AVRCP、HFP按独立编译组合扩展，成本见3.8节 |
| 首期不启用 | LE Audio、Mesh、Channel Sounding、ISO及5.x以后引入的广播/PHY/ATT扩展等 |

Privacy和LE credit-based channels在4.2范围内，但版本范围不等于首批全部可选项必须开启。
Privacy保持现有profile的关闭值；BT_L2CAP_DYNAMIC_CHANNEL已经由Classic选择，保留其实现。
这里不删除任何协议源码，不伪造HCI/Core版本；本轮只收敛配置与编译覆盖。

### 1.1 保留当前LE/BR共享关系

[Classic Kconfig](../bluetooth/host/classic/Kconfig) 当前BT_CLASSIC选择LE Central、Peripheral、SMP，
与本轮范围一致。保留adv.c、scan.c、conn.c、l2cap.c、att.c、gatt.c、smp.c、keys.c及相关
HCI/identity/crypto路径，保留BR核心7个源文件及现有安全配置关联。
本轮保留LE/BR共享关系，先完成移植层和源码编译。

### 1.2 Bluetooth 4.2编译配置草案

现有POSIX profile大体覆盖LE+BR，但一些默认值超出此次版本范围，需要显式收敛：

| 配置/组合 | 草案值与原因 |
| --- | --- |
| BT_CLASSIC、BT_CENTRAL、BT_PERIPHERAL、BT_OBSERVER、BT_BROADCASTER、BT_SMP | y；明确保留用户列出的基础功能 |
| BT_GATT_CLIENT | y；现有profile为n，补齐LE Central侧GATT client编译覆盖；同时保留原GATT server |
| BT_DATA_LEN_UPDATE | y；DLE属于4.2功能，不因关闭较新扩展而删除 |
| BT_SMP_SC_PAIR_ONLY、BT_SMP_SC_ONLY、BT_SMP_OOB_LEGACY_PAIR_ONLY | n；不限制为仅SC或仅OOB，保留Legacy与4.2 SC两种基础配对路径 |
| BT_PHY_UPDATE | n；2M/Coded PHY/PHY Update不在4.2基础范围，相关自动PHY子选项随父项关闭 |
| BT_EXT_ADV、BT_PER_ADV、BT_PER_ADV_SYNC | n；保留Legacy adv/scan，不启用扩展/周期广播 |
| BT_EATT、BT_L2CAP_ECRED | n；不启用Enhanced ATT和Enhanced Credit Based Flow Control；不等于关闭基础ATT或旧LE CoC |
| BT_GATT_CACHING、BT_GATT_NOTIFY_MULTIPLE、BT_GATT_READ_MULT_VAR_LEN | n；关闭较新的GATT缓存和ATT扩展；保留Service Changed和原Read Multiple |
| BT_AUDIO、BT_MESH、BT_ISO、BT_CHANNEL_SOUNDING、BT_DF | n；不启用LE Audio/Mesh/ISO/CS/Direction Finding |
| BT_SUBRATING、BT_FRAME_SPACE_UPDATE、BT_SHORTER_CONNECTION_INTERVALS、BT_LE_EXTENDED_FEAT_SET | n；关闭较新连接/LL扩展 |
| BT_TRANSMIT_POWER_CONTROL、BT_PATH_LOSS_MONITORING、BT_SCA_UPDATE、BT_EAD | n；不纳入首期较新扩展 |
| HCI userchan、ACL flow control、TX processor、Host crypto与PSA CSPRNG | 保留现有有效配置；此次不进行单线程或异步HCI重构 |

该 profile 已实施到 config/posix_host_defconfig；最终开关差异见实施 Review 第 7 节。
已在`/tmp/embt-bt42-compile-study/`求值并生成CMake：请求开关全部生效，source graph仍为51个
EmBT C文件、11个组件。BT_GATT_CLIENT开启后，GATT自动重订阅与Central Address Resolution
读取辅助项也按现有软件策略默认开启，已记录在profile-changes.json。

现有profile默认BT_PHY_UPDATE、BT_GATT_CACHING、BT_GATT_READ_MULT_VAR_LEN为y，且
BT_SMP_SC_PAIR_ONLY默认y。前面三项的关闭是版本/首期范围收敛，最后一项关闭恢复Legacy编译
覆盖；都不是删除其源码。后续实现新增专用4.2 profile，保留现有综合POSIX profile。

## 2. 本次实际检查结果

没有修改源码或创建兼容头；只生成独立build目录。初期试探的有效配置已经包含LE+BR，
逐文件错误统计来自该快照；新4.2草案的配置与源列表已重新验证，未重复编译函数体。
此前按compile_commands中的真实参数分别编译C文件，输出写入 `/tmp/embt-classic-compile-study/objects/`。
未调用 `embt_crypto_check`，未执行加密探测程序或任何协议栈应用。

| 检查 | 结果 |
| --- | --- |
| 调研初期的Classic开启/LE roles关闭试探 | 四个LE roles被select回y；该试探已不作为当前目标 |
| 已确认的4.2功能配置草案 | LE四角色、连接、SMP、GATT client及Classic保留；较新功能关闭，请求全部有效 |
| CMake configure/generate | 通过；11 个组件、51 个 EmBT C 源文件 |
| 按真实参数逐个编译这 51 个文件 | 0 个通过，51 个失败，均停在缺少头文件处 |
| Classic 子目录本身 | 7 个文件：br、conn_br、keys_br、l2cap_br、sdp、ssp、sco |
| 命名空间归一化后的字面 include 闭包 | 191 个项目文件，792 处 zephyr 路径引用，44 个未解析项目/native_sim 候选头路径 |
| 选中 C 文件的词法 OS/atomic 调用名 | 79 个候选名；包括局部 helper 和关闭配置分支，不等于 79 个必需实现 |
| 选中 C 文件的日志 token | 29 个文件，共 1998 处 LOG_* token；不是预处理后的实际有效调用数 |

44 个候选缺失头包含关闭的 tracing/其他架构分支。已存在的系统 `psa/crypto.h`、`sys/types.h`
不计入缺失项目头。上述闭包只用于定位范围，不能据此恢复全部被裁剪的 Zephyr 头文件。

首个错误按类型归并（来自调研初期有效LE+BR配置；4.2草案源列表未变）：

- 48 个文件停在非日志 `zephyr/...` 路径，例如 kernel.h、types.h、sys/crc.h、Bluetooth 公共头。
- 2 个 net_buf 文件首先停在 `zephyr/logging/log.h`。
- `userchan_bottom.c` 首先停在 `nsi_errno.h`。

当前日志/命名空间阻塞遮挡了后续类型和实现错误。下面的 OS、注册、驱动清单来自实际源码
依赖检查，而不是声称已经逐个编译到了这些错误。

## 3. 需要做的适配与改动

### 3.1 头文件命名空间与基础类型

将参与编译的源文件以及可达公共/内部头的 `zephyr/...` 引用迁到 `embt/...`，使其对应
现有 `include/embt/`。保留 Zephyr 风格的源码组织、net_buf 和 bt_* API，避免同时重命名全套函数。

以下是例外和边界：

- 原有 `#include <zephyr/autoconf.h>` 改为 `<embt/autoconf.h>`，保留语句；旧转发头在引用迁移后退役。
- 不建立覆盖全部 Zephyr 头的软链接/转发目录，logger 也不提供兼容头。
- `common/...`、`host/...` 内部引用使用现有 bluetooth include 路径，不混入公共命名空间。
- kernel、device、生成 syscall 及仅服务旧内核的头依赖默认直接删除。
  实际使用的 utils、类型或宏保留对应最小依赖；需要的 OSI/base 声明和实现另按模块补齐。
  不恢复 arch/syscall/kobject 等旧 provider，不删协议操作或同步来消除编译错误。
- 保留可用的类型、list、byteorder、util、atomic 类型与内建实现；GCC/Clang 的 __packed、
  __aligned、container_of、BUILD_ASSERT 等保持实际语义，不借用全局 shim 宏屏蔽类型错误。
- `__syscall` 和 syscalls/*.h 是原内核的生成机制。适配接口改成直接声明/实现，不生成原内核
  syscall 编号和 dispatch，不恢复原 arch/syscall 依赖。

重点入口：`kernel.h`（已移除）、`kernel_includes.h`（已移除）、
[atomic.h](../include/embt/sys/atomic.h)、[toolchain.h](../include/embt/toolchain.h)、
[net_buf.h](../include/embt/net_buf.h)、[hci_core.c](../bluetooth/host/hci_core.c)。

### 3.2 完成原日志系统的 C 层清理

废弃日志的目录和 Kconfig 已在 PR #4 清理，C 层还存在 logger include、注册宏、等级变量、
LOG_DBG/INF/WRN/ERR/HEXDUMP、log_strdup 及相关辅助代码。

实施时逐语句处理，先覆盖实际编译的 common/LE Host/Classic/net_buf/userchan：

- 删除 logger 专用 include、module 注册、等级定义、仅服务日志的格式化函数和临时变量。
- 处理多行参数、if/else、switch case 和标签，避免留下空分支、缺少语句或 unused warning。
- 检查日志参数的求值和副作用；协议操作、资源释放、错误判断不能随打印语句一起删除。
- net_buf 旧 NET_BUF_LOG 包围中的池状态/完整性检查需逐项判断，必要时由独立 ASSERT/校验机制保留。
- 保留断言和实际错误返回；PRINTK/HFP 等独立输出需求另行处理。
- 清理 include/embt/logging 剩余头及 logger 专用引用。若本次只完成编译子集，文档明确其范围，
  不声称所有非首期文件的 logging 清理已经完成。

禁止新增空 LOG 宏、假 logging/log.h 或在新日志层复用旧 LOG_* 名字来绕过这轮删除。
openblue 的 [base/log.h](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/base/log.h)
和 shim 保留这类兼容行为，不能照搬。

### 3.3 基础机制与POSIX系统接口的实现分工

系统边界以[基础机制与OSI设计](BASE_OSI_ARCHITECTURE.md)为准：base定义机制，
OSI定义系统基础接口，osdep/posix提供环境实现；lib保留相对独立的通用工具。
这些目录调整尚未实施，现有net_buf仍在lib/net_buf，不恢复原Zephyr kernel或lib/os。

第一阶段保留必要k_*调用契约；单线程、全部异步和HCI command异步改造属于后续阶段。
不要求本轮运行线程，但编译/链接的API必须有真实实现。函数前缀不能用来判断模块归属。

| 能力 | 归属 | 当前使用点与必须保留的语义 |
| --- | --- | --- |
| 时间/timeout | OSI提供时钟/等待，基础机制使用统一timeout契约 | sleep、k_uptime_get_32、K_MSEC/NO_WAIT/FOREVER；单调时钟、单位/精度/溢出，不能将包含ticks的结构体简单别名成整数 |
| atomic | base基础工具/编译器实现 | 原子操作、位操作和net_buf ref；使用私有bt_atomic_*接口，保留旧值/native宽度/内存序；公开头暴露点需收敛，见ATOMIC_DESIGN.md |
| mutex/condvar/sem | OSI/osdep | HCI credit和同步响应等；最大计数、饱和、超时、递归契约及负POSIX错误按接口定义；pthread返回码与errno分别处理 |
| queue/FIFO/LIFO | base，组合OSI锁与等待 | k_queue/k_fifo/k_lifo；保留intrusive节点、顺序、超时、取消与peek，不能用空队列替代 |
| thread | OSI/osdep | OpenBlue风格create/start/cancel/join及身份/命名；POSIX create立即运行，start不延迟启动；原生stack大小与调度限制 |
| 系统timer | OSI/osdep | 原生一次性通知；stop/delete不保证callback完成，storage/context须长期有效；base管理逻辑超时和动态对象回收需求 |
| work/delayed work | base，组合OSI执行能力 | pending/running/canceling状态、submit/reschedule、sync cancel/flush；返回状态和执行上下文需要独立定义 |
| 临界区 | OSI底层能力与调用者保护契约 | spin/sched lock、ISR判断；保留真实同步，评估原调度锁保护范围，不机械替换为空锁或普通mutex |
| 内存 | base管理pool/slab/策略，OSI提供系统heap来源 | fixed/variable/heap路径分开；静态storage允许显式init。关闭分支按配置裁剪，不使用成功/NULL占位实现满足链接 |
| random | 通用来源按OSI讨论，PSA来源保持crypto适配 | 真实POSIX来源与无返回值API失败契约；PSA_CSPRNG_GENERATOR保留，不打开缺少实现的能力占位 |
| assert/fatal/print | base定义调试行为，OSI/宿主提供输出与终止 | 独立断言和实际错误处理保留，原logger仍删除，不吞掉错误 |

`kernel_includes.h`（已移除）收敛为实际所需接口，避免继续暴露整个
Zephyr kernel。共用接口与POSIX实现一起加入CMake，分别使用明确target；Threads和环境特性
作用到对应target，不引入全局preinclude shim。

OSI使用osi_函数前缀、原os_*_t类型名和osdep内部头文件，join不带timeout；
一次性timer与其他契约见[精简版API草案](OSDEP_API_DESIGN.md)，POSIX基础实现已完成并测试，协议栈需要的k_*过渡机制已由base接入。
mutex/cond直接采用native类型，thread创建即运行；原生sem reset不等价于取消waiter的k_sem_reset。
所有OS模块需通过cmocka功能测试，见[测试规划](OSDEP_TEST_PLAN.md)。OSI测试运行属于基础能力验证，
与本节Bluetooth协议栈只编译/链接的目标分开验收；原demo已删除。
下一轮按[讨论顺序](BASE_OSI_ARCHITECTURE.md#7-下一轮osi讨论顺序)
先明确能力范围、存储和生命周期，再细化timeout与各类原语。

### 3.4 静态存储保留，运行初始化改成显式入口

允许继续用静态数组承载对象和 payload。需要替换的是自动启动、隐式对象登记和对原内核
静态初始化器布局的依赖，不是要求把所有全局数据搬到 heap。

必须处理：

- `K_FIFO/SEM/WORK/STACK_*DEFINE`：静态对象与显式初始化配套；thread 和 work queue 不在
  constructor 或 SYS_INIT 中自动启动。
- `SYS_INIT`：当前TX processor、可选conn TX queue及driver原启动钩子改成被调用的初始化函数。
- `STRUCT_SECTION_FOREACH/GET/START_EXTERN`：net_buf pool、连接回调、LE/BR L2CAP fixed channel及GATT静态service
  用真实注册表替换；disabled 的功能不登记，不能提供空遍历宏。
- `BT_CONN_CB_DEFINE`、LE ATT/SMP及BR fixed channel、GATT静态service和SDP等：保留对象和
  明确的register/init路径；引用和实现一起移植，不能只保留宏让对象永远不可达。

注册表采用明确对象指针/注册API，由所选组件的初始化入口收集。顺序建议为OS基础对象、
缓冲池、driver配置登记、LE/BR Host及GATT/SMP模块登记、work queue配置；运行线程/socket启动由将来的
显式start/open执行。实现顺序和失败回滚会进入代码，但此次只编译/链接，不调用这些入口。

### 3.5 保留 net_buf 和静态池机制

[net_buf/buf.c](../lib/net_buf/buf.c) 当前的 pool_id 使用 linker section 起点做指针差，
`net_buf_pool_get()` 则通过同一section查找。这在移除原链接器机制后必须整体替换。

- 保留NET_BUF_POOL_FIXED_DEFINE所表达的对象数量、payload和user_data尺寸、对齐、destroy回调。
- 显式注册pool并赋稳定ID，替换指针差；保持`net_buf.pool_id`的uint8_t范围，做容量和重复登记校验。
- 初始化free LIFO/等待对象/ref状态；保留alloc/unref、fragments和destroy所有权规则。
- 检查HCI cmd/event、BR ACL views、L2CAP signalling/TX、SDP等实际池。当前选中源码中有
  15个NET_BUF_POOL_*DEFINE词法位置，包含关闭配置分支，不等于实际运行池数量。
- fixed/variable/heap数据分配路径按实际配置和预处理结果区分，不因选择固定池而留下未实现的
  全局allocator符号；也不把net_buf池与Mbed TLS内部内存分配混为一谈。

不建议本次重写成openblue的bt_buf_* API，先保留当前Zephyr net_buf契约并移植依赖。

### 3.6 HCI去设备树与native_sim，保留Linux user socket

当前阻塞位于：[userchan.c](../drivers/bluetooth/hci/userchan.c)、
[userchan_bottom.c](../drivers/bluetooth/hci/userchan_bottom.c)、
[Host hci_core.c](../bluetooth/host/hci_core.c)、[drivers/bluetooth.h](../include/embt/drivers/bluetooth.h)。

- 删除DT_CHOSEN/DT_INST/DEVICE_DT_INST_DEFINE/DEVICE_API_GET和native_sim trampoline、
  nsi_errno、nsi_tracing、soc.h、cmdline层；不通过空DT宏制造一个假device。
- 增加显式transport descriptor与register/configure，含driver ops、private data、name/bus/quirks。
  Host从注册接口取得transport，替换bt_dev.hci的device访问；open/close/send/setup一起调整。
- 将userchan_bottom的真实socket/poll/read/write/close代码接到标准POSIX，直接处理errno。
  首期只需要AF_BLUETOOTH/BTPROTO_HCI/HCI_CHANNEL_USER；TCP和UNIX bridge源码是否删减另议，
  编译默认值不得切成openblue默认的UNIX bridge。
- socket H4 type byte、event/ACL/SCO包长与buffer ownership应在设计中保留。当前HCI API规定
  send成功转移buf引用，失败仍由caller持有；close成功后不得再交付RX callback。
- 头文件可使用BlueZ提供的Linux socket声明，或继续维护准确的最小UAPI声明；不需要因此链接
  BlueZ Host协议栈。当前bottom已自带sockaddr_hci/HCI常量，避免与系统头重复定义。
- CMake把真实driver和POSIX实现纳入编译；本轮不打开hci设备、不执行bind、不修改系统蓝牙服务。

EmBT的HCI API版本标注0.3.0，openblue为0.2.0；openblue的transport/回调对象和bt_buf与当前不同。
尤其当前open包装自行维护recv状态，不能整段复制旧driver ops签名后认为所有权/生命周期相同。

### 3.7 密码库与其他依赖

Mbed TLS/PSA接入已经完成，优先保留现有适配，不退回openblue旧的crypto选择方式。
移植OS/namespace时保持现有CONFIG_PSA_WANT_*到PSA_WANT_*映射和初始化需求。
保留LE SMP、LE Secure Connections、BR SSP及现有CTKD关系。

当前 `embt` 依赖 `embt_crypto_check`，本机默认会执行probe。
若本轮严格不执行任何构建产物，构建框架需要将“编译/链接检查”与“运行算法检查”分开：
保留probe编译/链接作为依赖，算法检查保留为显式run target或受明确的运行检查选项控制。
本次调研绕过该run target，仅调用源码编译器，没有执行probe。

CRC组件目前构建8个实现；首轮无需为了减少文件数再次裁剪，可随后按真实算法需求收敛。
settings保持关闭；BR内存中的key管理仍编译，持久化key store不纳入此次验收。

### 3.8 如果首批包含上层Classic profile

| 编译组合 | 新增源码/依赖 | 对适配范围的影响 |
| --- | --- | --- |
| RFCOMM/SPP通道 | BT_RFCOMM，rfcomm.c | 增加DLC/session、credits、retry和delayed work；sys_rand32_get需真实提供。当前bt_work_schedule/reschedule是hci_core中的k_work包装，不另造第二套work实现。RFCOMM编译不等于SPP应用验证 |
| HFP HF/AG | BT_HFP_HF/AG，hfp_hf.c、at.c、hfp_ag.c，并select RFCOMM | 增加AT解析、work/timeout、PRINTK/格式化依赖。音频codec协商不等于音频编解码或SCO设备运行验证 |
| A2DP | BT_A2DP选择AVDTP并imply LIBSBC；a2dp.c、a2dp_codec_sbc.c、avdtp.c、bluetooth/lib/sbc.c | 目前codec provider未接入；sbc.h直接要求sbc_encoder.h、oi_codec_sbc.h、oi_status.h，需额外解决对应库和头文件 |
| AVRCP | BT_AVRCP选择AVCTP；avrcp.c、avctp.c | 增加transaction/work/buffer与回调；browsing/cover-art按实际配置加入，cover-art还可能引入GOEP/OBEX |
| GOEP/OBEX认证 | BT_GOEP、BT_OBEX_AUTH；goep.c、obex.c | 认证所需MD5和Mbed constant-time helper已在当前adapter支持，仍需清理源码头路径/日志并验证整个链接 |

A2DP的SBC适配不能只安装一个名字叫libsbc的Linux库再链接。当前wrapper使用
SBC_ENC_PARAMS/SBC_Encode和OI decoder接口，来源于原Zephyr libsbc module的encoder/decoder。
需要在third_party下接入匹配的源码/头和真实CMake target，或显式改写wrapper适配另一库的API。
原module构建文件仍可从Zephyr历史读取，openblue保留的wrapper也有这些API，不能单靠其名称
认定已经替本项目完成codec接入。首个基础LE+BR组合未开启A2DP时，这项不阻塞该里程碑。

本次只对基础LE+BR对应的51文件配置做了实际编译，以上扩展项来自Kconfig/CMake/源调用核查，
没有宣称各profile已经编译通过。

## 4. openblue参考映射

以下链接固定到本次读取的SHA，不以floating main作为证据。

| EmBT适配项 | openblue参考文件 | 可借鉴与需要重新核对的部分 |
| --- | --- | --- |
| POSIX OS层 | [os.h](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/osdep/posix/os.h)、[os.c](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/osdep/posix/os.c) | pthread/sem/timer包装；重新核对timeout、pthread错误码、线程启动、静态初始化与资源销毁 |
| queue/FIFO/work | [queue](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/base/queue/bt_queue.c)、[work](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/base/bt_work.c) | 数据结构和工作状态机；当前k_work返回/同步取消契约逐个对齐，不只改名字 |
| pool | [bt_mem_pool.c](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/base/bt_mem_pool.c)、[bt_buf.c](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/base/bt_buf.c) | 静态payload和free list；不继承空section遍历，不替换EmBT的net_buf API |
| 显式初始化 | [stack_init.c](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/core/stack_init.c) | 按层调用的实际入口；EmBT要定义顺序、幂等与失败回滚，不仅保留entry表 |
| BR fixed channel注册 | [l2cap_br.c](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/bluetooth/host/classic/l2cap_br.c) | 已使用bt_l2cap_br_fixed_chan_register和显式init，适合对照移植当前对象 |
| HCI transport | [driver API](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/include/drivers/bluetooth.h)、[userchan.c](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/drivers/userchan.c)、[hci_sock.c](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/drivers/hci_sock.c) | 去device的descriptor和POSIX IO；保留EmBT当前接口契约，默认实际USER channel |
| 不复用的兼容层 | [shim.h](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/shim/include/shim.h)、[utils.h](https://github.com/Frozen935/openblue/blob/3f8697c608a4b23cd59981a18f09e3d3838e0f72/base/utils.h) | 全局注入、logger别名和空STRUCT_SECTION_FOREACH不符合本项目已确认规则 |

openblue的os_mutex_init用-errno处理pthread返回值，线程启动和静态初始化器也需逐项核对。
其utils.h中的空STRUCT_SECTION_FOREACH使mem_pool_list_init遍历不到对象；这证明该注册路径
不能直接移植，不代表它所有显式bt_mem_pool_init调用都会失败。引用参考源码不等于EmBT运行验证。

## 5. 推荐实施顺序与交付

| 阶段 | 改动范围 | 交付与编译检查 |
| --- | --- | --- |
| C1 配置范围 | 新增单独Bluetooth 4.2 LE+BR defconfig，显式保留基础功能并关闭较新扩展；保留LE/BR共享关系 | 有效.config、source graph和版本范围清单；不覆盖现有综合POSIX profile |
| C2 头路径和日志 | 参与编译的C/H，autoconf迁到embt；旧内核依赖默认删、直接工具依赖保留；logger引用直接删 | 消除对应首个fatal；输出下一层真实编译错误，不生成兼容目录 |
| C3 OS原语与基础机制 | OSI契约、osdep/posix环境实现，以及base的atomic/queue基础机制；target分开管理 | net_buf、CRC、common先编译；新增接口可链接，不使用空成功stub |
| C4 work、pool和注册 | work/delayed work、固定pool/slab、连接回调、LE/BR固定信道和GATT服务 | LE/BR Host、ATT/GATT、SMP类型与注册符号可编译/链接；初始化不自动执行 |
| C5 HCI适配 | descriptor、Host访问、userchan标准POSIX和显式配置 | 不依赖设备树/native_sim的真实driver对象编译通过 |
| C6 完整归档与链接检查 | 聚合全部选定源、crypto compile/run分离、仅链接的验证target | libembt.a及完整拉入对象的链接检查通过；不运行任何产物 |
| C7 可选profile扩展 | RFCOMM、AVDTP/A2DP、AVCTP/AVRCP、HFP等逐项开启 | 每个profile独立source清单和编译结果，不把核心通过外推成全部Classic profile通过 |

C2-C5实际会交错迭代：头文件迁移首先暴露OS类型；OS补齐后才暴露更多注册/驱动问题。
每批修改根据真实错误收敛，记录移植契约，不对整个裁剪后的include目录做无差别恢复。

后续实现保持代码/文档PR分离，同一板块更新原patch；新批次默认留本地，收到提交指令再发布。
本次文档是review输入，不是授权提前进行单线程或异步API重构。

## 6. 编译通过的验收标准

1. 单独Bluetooth 4.2 LE+BR profile可由mkbt.sh/CMake加载；LE adv/scan、连接、SMP、GATT client/server与Classic有效开启，排除项关闭，不依赖手工CONFIG宏。
2. 所有选定源码完成编译并生成libembt.a；compile_commands覆盖预定Classic核心和公共依赖，
   不因漏加adv/scan、ATT/GATT、SMP或BR源文件获得表面的绿色结果。
3. 对新适配源启用-Wall/-Wextra；不能遗留隐式函数声明、指针/整数宽度错误或函数签名不一致。
   不通过全局关闭warning来验收；是否启用全库-Werror再根据原上游告警评估。
4. 建议生成一个只含main返回的链接检查程序，用--whole-archive拉入libembt.a，并链接真实
   POSIX/crypto依赖。关闭可能丢弃对象的gc-sections，禁止运行该程序，不调用bt_enable。
   这是用于暴露遗漏实现的检查，不恢复原samples/tests目录。
5. 静态archive本身允许未解析引用；nm -u的libc/PSA/pthread引用不是全部错误。
   whole-archive链接检查用于确认外部实现和section边界符号有实际提供方。
6. 本轮不得保留指向原arch/native_sim/syscall/device机制的未提供符号，不接受假的device、
   空注册/日志宏、空队列或返回成功的占位实现。
7. 检查LE adv/scan、连接、ATT/GATT、SMP Legacy与SC，以及BR核心源全部进入编译。
   Mesh/LE Audio/ISO/CS/DF等专用实现不进入本轮产物；保留共享HCI和公共类型，避免误删基础功能。
8. 不启动协议栈、线程或transport，不执行crypto probe，不操作系统HCI设备或蓝牙服务。
   本轮没有运行结果，也不保证协议正确性、线程语义、pool耗尽、取消竞态或硬件链路；这些在
   后续POSIX运行阶段验证，编译通过不能替代。

## 7. 本次证据与未完成事项

记录目录：`/tmp/embt-classic-compile-study/`。

- `classic-request.conf`、`configure.log`、`current-classic/.config`：实际配置请求与调整结果。
- `sources.json`：实际51个EmBT源文件；`compile-results.json`和`compiler/*.log`：逐文件首个错误。
- `header-inventory.json`：归一化include闭包与缺失候选，包含条件分支；不是最终恢复清单。
- `os-api-inventory.json`、`macro-inventory.json`：源文件词法候选与位置，未经过完整预处理。
- `objects/`：编译器输出目录；本次所有51个文件均失败，尚无成功的EmBT对象或archive。

基础范围已确认是Bluetooth 4.2 LE+BR；上层Classic profile仍按独立组合扩展，详见3.8节。
4.2草案的记录目录为`/tmp/embt-bt42-compile-study/`，含bt42-proposed.conf、configure.log、
proposed/.config、proposed/compile_commands.json及profile-changes.json。
当前已完成POSIX OSI基础实现和测试，详见[测试结果](OSDEP_TEST_RESULTS.md)；
协议源码适配、完整Bluetooth归档/链接和协议运行仍未完成。
