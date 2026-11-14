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

# 独立构建系统实施与验证

状态：已在本地 `build/standalone-cmake-kconfig` 实现 B0 构建入口，基于
`c3d9a114998cbdefa0fa56d8b4ddaa08f10fb290`完成本地验证。提交按文档、清理、构建三个范围拆分，全部保持Draft、未合入。

menuconfig、CMake configure/generate 和真实 C 编译入口已打通。当前协议栈编译仍失败，
第一处错误来自保留源码的旧日志/zephyr 头引用；这符合本轮“能到达真实源码编译”的验收边界，
不代表静态库、最终应用链接或蓝牙运行成功。

最新根菜单与ABI清理见 [ABI_CONFIG_CLEANUP.md](ABI_CONFIG_CLEANUP.md)：EMBT_STACK控制
全部选项，EMBT_64BIT和互斥大小端替代具体架构配置；旧Flash/secure-store配置已删除。
SYSTEM/SOURCE/EXTERNAL已接入统一EmBT::psa并通过密码能力验证，运行限制见
[CRYPTO_PROVIDER_DESIGN.md](CRYPTO_PROVIDER_DESIGN.md)。

最新配置目录：Mbed TLS与PSA共27个定义已集中到third_party/mbedtls/Kconfig，根引用该入口。
MBEDTLS/provider标志及5个源码优化选项已恢复；AES/p256-m自动建议在模块入口恢复，
默认系统模式的既有配置值不变，详情见[密码库核查记录](CRYPTO_PROVIDER_DESIGN.md#11-加密关联核查与恢复记录)。

## 提交拆分与依赖

| 范围 | PR/分支 | 基础与内容 |
| --- | --- | --- |
| 文档和规则 | [PR #2](https://github.com/Frozen935/embt/pull/2)，chore/agent-submission-rules | 基于main；更新项目文档、规则及third_party/README.md，不提交构建实现 |
| 目录与代码清理 | [PR #4](https://github.com/Frozen935/embt/pull/4)，refactor/bluetooth-directory-layout | 基于PR #2；更新原目录/删除提交，包含废弃日志、Controller/平台/存储条件、deprecated配置及MULTITHREADING包围清理；保留mutex主体 |
| 配置与构建改造 | build/standalone-cmake-kconfig的独立Draft PR | 基于PR #4；新增框架、脚本、密码库接入，并提交配置命名/ABI迁移和组件CMake改造 |

三个PR全部保持Draft和未合入，agent不得执行merge或auto-merge。
部分现有Kconfig文件包含清理和构建两类改动，按内容分别进入两层提交；最上层构建分支
组合文档、清理与构建改造，作为后续开发基线。清理PR仍含尚未适配的旧构建接口，不能独立编译；
构建PR能进入真实C源码编译，当前仍因旧日志/命名空间引用失败。构建文档引用的是最上层代码。

## 1. 实际文件范围

新增 10 个构建文件：

| 文件 | 实际职责 |
| --- | --- |
| [CMakeLists.txt](../CMakeLists.txt) | 独立根入口、Linux/GCC/Clang 与 ABI 检查、静态库聚合 |
| [Kconfig](../Kconfig) | 根菜单、目标/运行参数、第三方需求、缺失符号定义 |
| [config/posix_host_defconfig](../config/posix_host_defconfig) | BLE central/peripheral、BR/EDR、HCI user channel 与 PSA 随机数配置 |
| [mkbt.sh](../mkbt.sh) | 菜单、configure、build、clean、显式配置重置与导出 |
| [tools/kconfig/configure.py](../tools/kconfig/configure.py) | 单次 Kconfig 求值、输入记录、配置/头/JSON 生成、真实菜单 |
| [tools/cmake/embt_kconfig.cmake](../tools/cmake/embt_kconfig.cmake) | 导入 typed JSON、生成物依赖、配置 target、菜单 target |
| [tools/cmake/embt_library.cmake](../tools/cmake/embt_library.cmake) | 显式 OBJECT 组件、源码归属/去重、真实依赖和聚合 |
| [tools/cmake/embt_dependencies.cmake](../tools/cmake/embt_dependencies.cmake) | 连接唯一Mbed TLS provider并挂入能力检查；codec仍待接入 |
| [tools/requirements-build.txt](../tools/requirements-build.txt) | 固定 Kconfiglib 14.1.0 |
| [.gitignore](../.gitignore) | 忽略本地 build、虚拟环境和 Python 缓存 |

更新 30 个已有 CMakeLists.txt；剩余一个 settings 分发入口原本就使用标准 CMake，无需改名。
日志命名清理更新的 26 个 Kconfig 文件继续保留；本轮另清 Controller/平台条件，
删除重复 bluetooth/LICENSE。配置的准确当前范围见 [CONTROLLER_CONFIG_CLEANUP.md](CONTROLLER_CONFIG_CLEANUP.md)。

本轮仅修改 settings.c：去掉 MULTITHREADING 条件包围，保留 mutex/lock/unlock 主体。
其余 C 实现保持；本轮14个头文件迁移位宽/大小端宏，工具链中的ARM64判断使用编译器
预定义宏；hci_lockstep.h 的原API注释改名继续保留。108处原配置头include全部保留。
旧日志 C 调用和 include/embt/logging/ 的四个头文件尚未清理，它们是后续源码工作。

## 2. 配置与构建行为

- BT 总开关改为 EMBT，54 处实验标志引用改为 EMBT_EXPERIMENTAL。
- 删除 8 个 deprecated 配置及专用条件；GATT 替代配置和安全默认值保留。
- 删除全部旧日志配置/模板入口，不定义 LOG 或 LOG_*。移除旧 UART/RTT 日志 backend
  选项、空余 debug-type choice 和 ISO/BAP 数据日志选项，保留协议文件及独立错误码格式化接口。
- BT_SMP_SELFTEST、BT_SMP_FORCE_BREDR 保留，依赖改为 BT_TESTING && BT_SMP，不再依赖日志级别。
- shell 源码保留，未接入通用 shell 前由 EMBT_PORT_HAS_SHELL=n 限制其菜单入口。
- 删除 43 个 Controller/平台根定义及 7 个强依赖 USB 的配置，不补占位；基线 ACL流控显式选择 y。
- 根模型当前共 1010 个定义符号，完整解析无未定义符号、无类型缺失或配置告警。
- CMake 只读取统一 driver 输出的 typed JSON；拒绝通过 CMake cache 的 CONFIG_* 绕过 Kconfig。
- 主配置头位于 generated/include/embt/autoconf.h；旧路径 zephyr/autoconf.h 是同目录树中的
  转发头。强制 include 与原显式 include 共存，生成物不进入源码树和 Git。
- 每个源文件只属于一个组件。基线生成 11 个 OBJECT 组件、51 个唯一 C 文件编译任务。
  最终目标为 libembt.a，CMake 接口为 EmBT::embt。
- PSA由真实Mbed TLS提供；SYSTEM默认，SOURCE/EXTERNAL显式选择。构建依赖crypto_check，
  缺库或算法失败直接返回，不创建空provider。

生成器额外保存 kconfig/inputs.json，记录初始 defconfig/有序 fragments 和内容摘要；
这样删除生成的 config.json 后仍能恢复原输入记录。已有 .config 不重放初始片段；显式
`defconfig` 才重置，并保存 .config.old。未知、已废弃选项和非法数值范围会报错。

## 3. 使用命令

```sh
./mkbt.sh menuconfig
./mkbt.sh configure
./mkbt.sh build -j 8 --verbose
./mkbt.sh clean
./mkbt.sh savedefconfig --output /tmp/embt_defconfig
./mkbt.sh defconfig --defconfig config/posix_host_defconfig

cmake -S . -B build/posix -G Ninja
cmake --build build/posix --target menuconfig
cmake --build build/posix --target embt --parallel 8
```

默认 build/posix。相对路径按 EmBT 根目录解释，路径空格已经验证；入口明确拒绝分号和换行
路径，避免 CMake 列表歧义。当前配置只表达目标指针位宽和字节序，编译器负责实际ISA；本机只验证 x86_64 小端。
使用 CMake >=3.20、Python >=3.10、Ninja、Kconfiglib 14.1.0；实际验证版本见下一节。

菜单可以直接通过 mkbt.sh 打开，不要求 CMake/编译器/provider 先配置成功。非 TTY 或 UI
初始化失败会返回非零。菜单保存不覆盖后续 build，退出不保存会重新加载磁盘上的选择。
新模型删除旧配置项时，带这些旧名的 .config 会明确报错；需要保留旧选择时先检查差异，
选择显式 defconfig 重置，而不是脚本静默丢掉旧值。

## 4. 已执行验证

环境：CMake 3.28.3、Ninja 1.11.1、GCC 13.3.0、Python 3.12.3、Kconfiglib 14.1.0。
以下结果均已实际运行，最低版本和 其他ABI交叉编译尚未验证。

| 检查 | 结果 |
| --- | --- |
| 根 Kconfig 和首期配置 | 解析通过；BLE、BR、buffer、HCI user channel、SMP/PSA 需求有效 |
| 真实 menuconfig | mkbt.sh 和 CMake target 均在 PTY 中打开；BASE64 修改/保存/取消/重载通过 |
| 增量配置 | 菜单 BASE64 y/n 切换后，下一次构建自动增删 base64.c，宏同步出现/消失 |
| 配置持久化 | build/configure 和 clean 保留已有选择；显式 defconfig 才重置 |
| 生成物恢复 | 分别删除主头、转发头、config.json、dependencies.json，普通构建可恢复 |
| 配置类型与路径 | bool/int/hex/string、含引号/反斜杠/分号的字符串、带空格构建路径通过 |
| 原配置头引用 | 实际编译同时使用 -include、zephyr/autoconf.h、embt/autoconf.h，通过且无重复定义 |
| 异常输入 | 非 TTY、废弃/未知选项、非法范围、不同初始输入、非法 CLI 参数、CONFIG_* cache 覆盖均非零失败 |
| OBJECT/helper 行为 | 同一文件去重、跨组件冲突、缺本地 owner、未知符号、缺 provider 和空组件校验通过 |
| 编译参数/链接传播 | 使用真实 helper 的隔离用例确认 consumer 获得配置；provider 不获得 EmBT 宏，私有选项不外泄，外部库可链接 |
| 真实 EmBT C 编译 | 已调用编译器处理 51 个源文件，包括 Host/Classic；编译因源码移植缺口失败 |

隔离 helper/配置消费程序的成功仅证明构建框架行为，不是 EmBT 运行成功。
原始日志和验证脚本保存在 `/tmp/embt-build-implementation/`，包括：

- final-configure.log：最终 configure/generate。
- first-build.log、all-source-build.log：真实源码编译及错误。
- menuconfig-pty.log、menu-script.log、menu-cmake.log：实际交互菜单记录。
- verification.log、helpers-verification.log：配置生命周期和组件验证。
- cache-rejection.log、final-scope.json：cache 拒绝和修改范围。

## 5. 已知失败与下一阶段

当前真实失败包括：

- lib/net_buf/buf.c、buf_simple.c：旧 zephyr/logging/log.h 引用。
- bluetooth/host/hci_core.c、att.c、smp.c：旧 zephyr/bluetooth/addr.h 引用。
- bluetooth/host/classic/br.c：旧 zephyr/sys/byteorder.h 引用。

下一阶段按顺序完成 C 层日志清理、头文件命名空间/生成接口迁移、POSIX 与 HCI user socket
适配与运行初始化。Mbed TLS提供方已接入构建，完整运行生命周期尚未验证。静态库生成、应用链接以及 BLE/BR 运行均尚未达成。
这次不恢复旧 kernel/modules/scripts，不借助空宏/空 provider 伪造编译或运行成功。
