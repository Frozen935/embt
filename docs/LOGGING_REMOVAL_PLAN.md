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

# 原 Zephyr 日志系统废弃计划

当前决定：完整废弃原 Zephyr logging 系统，包括日志专用文件、头文件、调用/注册宏、
Kconfig 选项和构建入口。此前保留 logging 核心、迁移日志模板、采用 LOG_MODE_MINIMAL
或补 cbprintf/MPSC 日志依赖的方案全部撤销。

执行状态：logging 目录删除已提交；本轮构建改造又在本地完成 Kconfig/CMake 中的日志
依赖清理，并删除 bluetooth/Kconfig.logging。C 层日志调用和 include/embt/logging/ 的四个
头文件仍待处理。文件/配置删除归PR #4，CMake框架改造归独立构建Draft PR，详情见 [BUILD_SYSTEM_IMPLEMENTATION.md](BUILD_SYSTEM_IMPLEMENTATION.md)。

目录删除和日志配置清理通过 [PR #4](https://github.com/Frozen935/embt/pull/4) 更新原提交，
保持一个commit、Draft、未合入。`c3d9a114998cbdefa0fa56d8b4ddaa08f10fb290`是本轮拆分前的已验证历史版本。

## 1. 本次目录删除

删除前提交：`1f8bcb583d899ecccacfa3feacf46ca862b43c62`。删除 17 个文件、4,250 行，
代码分支树由 737 个文件变为 720 个。202 个 include/embt 文件、16 个根 lib 文件，以及
456 个原非 Controller 蓝牙文件在这一步保持不变。此次删除的是原 subsys/logging 的残留，
不是删除 Host、Mesh、audio、services 等协议目录。

| 本次删除文件 |
| --- |
| `bluetooth/subsys/logging/CMakeLists.txt` |
| `bluetooth/subsys/logging/Kconfig` |
| `bluetooth/subsys/logging/Kconfig.filtering` |
| `bluetooth/subsys/logging/Kconfig.formatting` |
| `bluetooth/subsys/logging/Kconfig.misc` |
| `bluetooth/subsys/logging/Kconfig.mode` |
| `bluetooth/subsys/logging/Kconfig.processing` |
| `bluetooth/subsys/logging/Kconfig.template.log_config` |
| `bluetooth/subsys/logging/Kconfig.template.log_config_inherit` |
| `bluetooth/subsys/logging/Kconfig.template.log_format_config` |
| `bluetooth/subsys/logging/log_cache.c` |
| `bluetooth/subsys/logging/log_cache.h` |
| `bluetooth/subsys/logging/log_core.c` |
| `bluetooth/subsys/logging/log_mgmt.c` |
| `bluetooth/subsys/logging/log_minimal.c` |
| `bluetooth/subsys/logging/log_msg.c` |
| `bluetooth/subsys/logging/log_output.c` |

目录外的父级 CMake/Kconfig 入口、日志模板和日志专用配置已在本轮本地构建改造中清理。
C 实现仍保持原内容，旧 logging 头引用及宏调用是下一阶段任务。

## 2. 后续清理范围

| 范围 | 计划动作 | 边界 |
| --- | --- | --- |
| include/embt/logging/ | 删除 log.h、log_core.h、log_instance.h、log_msg.h | 尚未实施 |
| bluetooth/Kconfig.logging | 删除日志专用配置文件和 bluetooth/Kconfig:423 的入口 | 已在本地删除；不扩展到其他协议文件 |
| Bluetooth 全部模块、drivers 和保留的 net_buf/settings 支撑代码 | 删除旧 logging include、LOG_MODULE_REGISTER/DECLARE、LOG_INST、LOG_ERR/WRN/INF/DBG/HEXDUMP 等依赖 | 混合文件只清日志内容，保留协议和 buffer/API 行为 |
| 各 Kconfig 日志块 | 删除 LOG/BT_*_LOG/NET_BUF_LOG/SETTINGS_LOG 等日志专用配置及 select/depends/default/rsource | 删除没有消费者的 module/parent-module/module-str 模板变量，不补隐藏 bool 占位 |
| CMake | 删除 logging 子目录入口、旧日志源码/库依赖、仅因日志存在的构建条件 | 新框架不再定义 embt_logging 组件 |
| 根 Kconfig 缺失项规划 | 撤销仅服务于旧日志的定义，包括旧 backend、cbprintf 打包、MPSC 和 MIPI 日志支持 | 清理后重新按非日志消费者判定缺失项，不把先前 124 个候选全部补回来 |
| 配置条件中的混合逻辑 | 去掉日志专用分支和条件，保留业务条件 | 保留非日志用途的 ASSERT、错误处理、统计和公共格式化 API，按调用关系判断 |

`printk`、断言、HCI monitor/btsnoop/传输和 shell 的功能不应仅因名字中有 log/debug 就整体
删除；属于旧 logging 注册、backend、宏或配置依赖的部分纳入清理，独立功能和接口保留。
日志参数表达式如有必要的状态更新、资源释放或其他副作用，删除打印时必须保留对应行为。
不使用空 LOG_* 宏或旧 logging 路径转发头来伪装清理完成；这不限制配置头 autoconf.h 的
构建目录转发机制。替代日志接口及级别/输出格式以后单独设计，
这次不新增另一个日志框架。

## 3. 扫描规模与实施记录

以下是本次扫描在排除已删除目录后的匹配规模，包含保留源码/头文件中的定义和调用行，
不是运行调用次数，也不是自动删除脚本的输入：

| 匹配类别 | 匹配行数 | 涉及文件数 |
| --- | ---: | ---: |
| 旧 logging 头引用 | 197 | 191 |
| LOG_* 宏形式匹配 | 10705 | 193 |
| 旧日志 CONFIG 引用匹配 | 435 | 202 |

此前统计的 107 处全局日志模板 source 中，1 处随 logging 目录删除，其余 106 处已随本轮日志配置清理删除；相对 rsource 和 LOG 配置引用也已处理。

LOG/LOG_*、模块日志等级和 backend 配置直接删除，已从根定义候选中移出；
不采用“暂时 bool/default n”或旧名兼容策略。NET_BUF_WARN_ALLOC_INTERVAL 等日志专用
子选项随所属块删除，独立完整性检查保留。逐项位置见
[KCONFIG_MIGRATION.md](KCONFIG_MIGRATION.md) 第 4 节。

先前的 124 个缺失候选、1936 个定义项和日志模板统计均为删除前快照；日志引用清理后
已重新解析配置树并通过验证，当前完整模型为 1064 个定义项；旧计数不再代表当前配置。

## 4. 分阶段验收与提交

1. 本次：相对删除前提交只减少这 17 个路径；存活文件 blob/mode 不变，文档不进入代码 PR。
2. 更新原代码提交/PR #4，不另建纠正提交；英文提交信息和 PR 描述写明目录外引用仍待清理。
3. 后续：逐文件清除旧日志引用和配置，检查混合分支和日志参数副作用；更新此处实际文件范围。
4. 引用清理完成后：没有旧日志头/模板/组件依赖；根配置按剩余真实消费者补齐，menuconfig
   和实际协议 C 文件编译按构建设计验收。保留原错误返回、断言和协议状态处理。
5. 所有 bring-up PR 保持 Draft/未合入，最终由人决定集成；不以这次删除声称编译或运行成功。
