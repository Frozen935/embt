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

# 头文件引用规则

应用接口使用 `<embt/...>`；模块私有头使用实际目录/provider。
`embt/base/...` 和 `embt/osi/...` 统一位于根 include，系统/机制操作仍遵守内部接口合同。
共享目录可见不等于正式应用 API；backend-only helper 和 shell 私有操作保持实现侧。
标准 C/POSIX/系统/第三方 namespace 保留，不机械改为 embt。

## 真实提供者与迁移边界

变更 include 前核对实际文件、类型/操作消费者及 target 可见性。
纯路径迁移不夹带 API、布局、guard、版权、行为或无关格式变化；
系统机制、logger 和配置注入的变化分别描述并验证。

删除旧 kernel/device/syscall、设备树和工具链选择依赖；直接使用的工具/类型由最小
真实 provider 提供。不能建立 zephyr 树、空 header、软链接或大聚合 shim 掩盖缺失实现。
配置引用迁到 `<embt/autoconf.h>` 并保留原 include；新代码可依赖 target 注入。
canonical 头仅在 build/generated/include/embt 生成，与强制 include 共用配置及 include guard。

旧 logger 引用改为内部 `<embt/base/log.h>`，保留 LOG_*、hexdump 和业务诊断调用。
内部通用 assert 用 `<embt/base/assert.h>`，蓝牙专用接口保留 common/assert.h；
公共工具使用自己的标准 C 检查，格式化用 stdio，不恢复 printk 或旧 logger。

## 类型、存储与操作

base 功能头位于 `include/embt/base`，OSI 功能头位于 `include/embt/osi`。
queue/FIFO/LIFO/work/spinlock 的类型与操作同头；timeout/deadline/物理单位集中在 base/time.h。
OSI thread/sync/timer 分别定义自己的类型和操作，不恢复 types.h/storage.h/base.h 大聚合。
可见的内嵌 storage 仍按对象合同管理；函数可被 include 不改变其内部使用属性。
不能复制、清零或直接操作活跃锁/queue/引用计数；拥有者负责 init/deinit。

atomic 操作仅通过内部 [atomic 规范](ATOMIC_DESIGN.md)，公开业务 helper 移到真实实现。
公共第三方类型依赖必须由对应 provider 正确传递，不暴露 backend-only helper 修复问题。
当前 PSA 传递缺口仍在 [待办](STATUS.md)；无消费者的 div64、旧 types 和 section 头已退役。

## 排列与检查

自己的声明头优先，随后直接所需的标准、系统、项目及模块私有头；
真实前置声明优先于机械排序，例如 cmocka 的必要标准头由 support header 提供。
新增/修改的头应在所属 target 的真实配置下单独检查；不通过源码默认 CONFIG 值“独立编译”。

核对 compile_commands、PUBLIC/PRIVATE/INTERFACE 和实际 include 解析。
纯路径变化检查内容差异；provider 变化检查调用与行为，包括条件分支、同步及回收。
以每批修改范围验证，不用一次基础 profile 通过推断全部 feature 可用。
