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

# 头文件引用与迁移规则

状态：2026-11-15，本地规则草案。定义后续新增及修改 include 的边界。
本规则已应用到本轮编译依赖闭包；显式配置头迁移及旧转发头退役已实施。具体范围见 BRINGUP_IMPLEMENTATION_REVIEW.md。

本规范结合 [AGENTS.md](../AGENTS.md)、[基础分层](BASE_OSI_ARCHITECTURE.md) 与
[编译适配调研](BLUETOOTH_4_2_COMPILE_PORTING_STUDY.md)。配置头按最新要求迁到 embt/autoconf.h，取代原来保留旧路径的规则。

## 1. 引用路径与提供者

| 类型 | 使用形式 | 路径与依赖边界 |
| --- | --- | --- |
| 项目公共/共用头 | `<embt/bluetooth/conn.h>`、`<embt/sys/util.h>` | 对应 include/embt 下的真实文件；路径统一不自动承诺公共 ABI 稳定 |
| OSI 内部接口 | `<osi/thread.h>`、`<osi/sync.h>` | osdep/include/osi，由内部组件依赖提供；不向应用安装或导出 |
| POSIX backend 类型 | `<osi/native_types.h>` | 由已选择的 backend 提供，当前位于 osdep/posix/include/osi |
| 模块私有头 | `"hci_core.h"`、`"private.h"` | 使用同模块真实路径；跨子目录私有头由模块的内部 include 路径提供 |
| C/C++ 标准头 | `<stdint.h>`、`<stddef.h>`、`<string.h>` | 保留标准库原名 |
| POSIX/系统头 | `<pthread.h>`、`<sys/types.h>`、`<sys/socket.h>` | 保留系统名称，受平台与内部 target 条件约束 |
| 第三方头 | `<mbedtls/constant_time.h>`、`<psa/crypto.h>`、`<cmocka.h>` | 保留真实库命名空间，由对应 provider/target 提供 |
| 配置头 | `<embt/autoconf.h>` | 原 zephyr/autoconf.h 引用迁到 canonical 路径，配置值仍由 CMake 生成 |

新代码使用明确的项目命名空间，不新增普通 `zephyr/...` 依赖。
显式配置头引用保留语句并更换路径，不继续把 zephyr/autoconf.h 作为长期例外。
不因为所有头文件都存放在仓库中，就把第三方或系统路径改成 embt 前缀。

模块私有头优先使用双引号；共享的命名空间头使用尖括号。旧代码中已有的 common/...、host/...
引用按模块实际解析保留或定向调整，不能机械改成 embt/common/...、embt/host/...。
base 的 header 布局由对应机制任务确定，不提前创建空目录/别名来消除报错。
atomic 按[内部Atomic设计](ATOMIC_DESIGN.md)使用 <base/bt_atomic.h> 与 bt_atomic_*，不作为
普通公共头迁到 <embt/sys/atomic.h>；公开字段和helper的依赖需要单独收敛。

## 2. 引用迁移与接口适配分开

路径迁移先确定目标文件、提供者、声明及消费者；旧内核/logger 引用删除按下列默认规则执行，
只对实际直接使用的工具、类型和功能补齐依赖。

### 2.1 同义路径迁移

当目标文件存在且保留同一接口，可以在当前任务范围内做路径迁移，例如：

```c
#include <zephyr/bluetooth/conn.h>
```

迁到：

```c
#include <embt/bluetooth/conn.h>
```

本类修改只改变引用路径。保留 bt_*、net_buf 等名字，不能顺带重命名函数、字段、结构体、
enum、宏或修改布局、调用顺序和行为。保留上游 license、copyright、原注释及现有 include guards。
导入 guard 的 ZEPHYR 字样不是要求改接口或重写整份文件的理由。

目标存在只说明路径有落点，不证明它的传递依赖、初始化或运行实现已适配。
尚未完成的依赖要记录；不能将 namespace 修改或某个文件编译通过报告为完整移植完成。

### 2.2 旧 kernel/device/syscall 依赖默认删除

原 Zephyr 的 kernel、device、生成 syscall 及仅服务旧内核/设备模型的 include 默认直接删除，
不要求先保留整套头文件再完成移植。这里指旧 provider，不按 sys/kernel/device 子串删除系统头。

只有代码实际使用、具有直接关联的依赖才保留或引入真实的最小头，例如 utils、类型、
list、byteorder、atomic 工具及必要的编译器宏。判断依据是实际类型/宏/函数使用，
不是“曾被 kernel.h 间接 include”或“现在文件还存在”。

删除后暴露缺失声明或类型时，补充对应的真实工具、OSI 或 base 依赖并记录未完成项；
允许这一步暂时出现编译错误。不能重新引入整个 kernel/device/syscall bundle，
也不能删除实际协议操作、同步或资源管理来隐藏错误。

不把 `<zephyr/kernel.h>` 一律替为 `<osi/osi.h>`，因为 OSI 不提供 queue/work/pool 等机制，
也不自动保持 k_* 契约。原 __syscall、dispatch 和生成 syscalls/*.h 不通过空头恢复。
真实接口的声明、实现和调用适配另按模块推进，保留原有返回值与生命周期要求。

### 2.3 logger 提供者替换，保留 LOG 接口

内部源码的旧 logger include 改为 <base/log.h>，采用项目自己的同步输出及级别过滤。
保留 LOG_ERR/WRN/INF/DBG、HEXDUMP、RATELIMIT、buffer 包装接口和原消息；
Zephyr 的注册换成轻量 LOG_TAG，不恢复原日志队列、backend 或模板，也不提供空 LOG 宏。
保留日志依赖的诊断 helper 和配置条件；模块级别条件接 EMBT_LOG_LEVEL，
原 sniffer 信息接默认关闭的 EMBT_LOG_SNIFFER_INFO。base 日志头保持内部可见性。
替换 logger 提供者不授权删除协议文件、日志调用或裁掉实际功能。

其他缺失 provider 仍按实际使用判断：直接使用的工具/类型保留真实依赖，无用的旧关联移除。
首期未编译的协议源文件继续保留，其必要实现列为待适配项。

## 3. 配置头迁到 canonical 路径

将现有源码和头文件中的：

```c
#include <zephyr/autoconf.h>
```

改为：

```c
#include <embt/autoconf.h>
```

保留显式 include 语句，只更换路径；不能因 target 已 -include 就顺手删掉这行。
新代码继续依赖 target 注入，不要求另写手工配置 include。

配置值只有一份，位于 `<build-dir>/generated/include/embt/autoconf.h`。
现有 driver 仍生成旧路径转发头，它只是未迁移代码的临时构建支持；迁移完成后删除
该转发头的生成和 build tracking。CMake/唯一 Kconfig driver 负责 canonical 头的生成、恢复和导出。

不在源码树提交生成头，不引入逐文件 CONFIG_ 定义或另一套配置解析。
不建立 include/zephyr、全目录 symlink、通用 forwarding layer 或旧 logger 兼容头。
第三方库使用自己的配置，不接收 EmBT 的强制 include。

## 4. include 路径和可见性

- 编译需求按 target 绑定，不增加全局 include_directories、全局 -I 或环境搜索路径来绕过依赖。
- 内部组件可以通过自身 usage requirements 向内部消费者提供路径；产品通过 PRIVATE 依赖接入，
  应用从 EmBT::embt 获取的编译条件不能泄漏 osdep、test、模块私有或第三方内部目录。
- 不把 osdep 的 native 类型放回外部 include/embt，也不为应用导出 osi 路径来补公共头的缺失依赖。
- 如果公共头要求消费者构造私有 OSI 对象，先整理声明/类型边界；不能只导出私有路径消除错误。
- 同名头文件有歧义时使用明确路径或收窄组件搜索范围；不依赖 include 目录先后顺序碰巧选中。
- 禁止源文件使用绝对路径、build/ 中的具体路径或 ../../include/embt 之类跨层引用。
  同模块确需相对引用时保持最短真实路径；跨模块通过明确 target 依赖解决。
- POSIX feature test macro 如 _GNU_SOURCE 由组件编译定义提供，不在公共头里修改系统宏。
- 不新增隐式默认 backend；native_types.h 必须由已选择 backend 的路径提供。

## 5. 排列与直接依赖

新 C/C++ 实现文件优先包含自身接口头，用于检查接口声明在所属 target 编译条件下可独立使用。
其他头按标准库、系统/第三方、项目共享、其他私有依赖分组；组内保持稳定顺序，不做无关重排。
新 header 在自身需要类型或声明时直接 include 相应头，不依赖消费者先包含某个 umbrella。

例如，OSI 实现可以使用：

```c
#include <osi/sync.h>

#include <limits.h>

#include "private.h"
```

自身接口头优先不意味着无视库的前置要求。cmocka 的 stdarg/stddef/setjmp 等前置头由
测试 support header 显式提供；不要机械排序破坏真实声明依赖。

新建或实质修改 header 时，在所属 target 的真实编译条件下单独检查它。
“独立使用”允许 target 提供 feature macro 和自动配置注入，不能在源码里补假的默认值。
已有导入代码只调整任务涉及的引用及必要依赖；不趁迁移批量格式化或排序整个文件。

## 6. 每批修改记录

每批按目录/模块推进，记录以下信息：

| 旧引用 | 新引用/处理 | 真实提供者 | 修改文件与消费者 | 验证及剩余依赖 |
| --- | --- | --- | --- | --- |
| zephyr/bluetooth/conn.h | embt/bluetooth/conn.h | include/embt/bluetooth/conn.h | 实际修改文件清单 | 编译范围、传递依赖情况 |
| zephyr/autoconf.h | embt/autoconf.h | 当前 build-dir 的 canonical 配置头 | 保留语句、更换路径 | 强制与显式 include 使用同一配置 |
| zephyr/logging/log.h | base/log.h | 内部真实日志适配器 | 实际迁移文件 | 保留 LOG 调用及必要 helper |
| zephyr/syscalls/kernel.h | 默认删除旧依赖 | 需要的声明使用真实接口 | 具体使用者 | 记录缺失项、验证真实适配 |

路径迁移、提供者适配、日志退役、构建调整属于不同改动性质，应明确说明。
同一未合入板块继续更新原 patch/PR；文档规则留在文档板块，遵守现有提交与 Draft 规则。
修改范围外的引用保留并记录，不为全仓 grep 清零扩展到未授权代码。

## 7. 验证要求

- 检查所有修改文件及其直接/传递依赖，条件分支中的引用也纳入清单。
- 每个迁移目标都是真实文件或已确认的生成/系统/第三方提供者，无软链接或空头兜底。
- 对纯路径修改，检查 diff 没有 API、布局、行为、guard、版权或无关格式改变。
- 对提供者改动，执行对应功能测试，尤其同步、buffer 生命周期、回调和错误返回；
  不能只以声明可编译证明等价。需要改 API 的情况按相应接口任务记录。
- 检查 CMake PRIVATE/PUBLIC/INTERFACE、compile_commands 和 include 解析，确认选中了预期提供者。
- 检查普通 zephyr 引用的剩余清单，将待迁移 autoconf 路径、已删除旧依赖和未完成直接依赖分开报告。
- 只验证实际允许的配置和范围；首期 BLE/BR 的通过结果不能代表 Mesh/Audio 等所有保留代码完成。
- 编译失败可用于定位未完成依赖，但必须如实记录，不能配上虚假后端或漏掉必需 source。

## 8. 当前扫描快照

2026-11-15，对 bluetooth、lib、include/embt 中的 .c/.h 进行字面 include 扫描：

| 分类 | 引用次数 |
| --- | --- |
| include/embt 下存在同相对路径文件 | 3051 |
| logger 退役引用 | 197 |
| 待迁移到 embt/autoconf.h 的旧配置路径 | 108 |
| 没有同路径提供者 | 174 |
| 合计 | 3530，252 个不同 legacy 路径 |

该结果覆盖保留源码中的条件分支，不是首期实际编译依赖数；存在文件的类别也包含待适配内核接口，
不能把 3051 条全部视为已允许自动替换。

代表性引用：conn.h、sys/util.h、net_buf.h 的目标文件存在；kernel.h、device.h 等旧 provider 已在目录清理中删除，相关 include 同步移除；确有直接用途的工具依赖保留最小提供者。
旧 logging/log.h 实现已退役；日志接口通过内部 base/log.h 提供，不建立旧路径转发层。
缺失项包括 shell、devicetree、arch 和生成 syscall 等，按消费者及实现依赖分组处理。

当前实际编译依赖的头引用已迁移并通过完整链接；禁用组件不因此被认定完成移植。所有修改仍在本地，未提交或推送。
