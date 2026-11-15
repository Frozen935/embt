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

# LOG 接口适配与恢复

最新决定：保留 LOG 接口、消息和诊断 helper，替换 Zephyr 原生 logging 实现。
此前把 logger 退役扩展成删除 LOG 调用的描述作废。LOG 调用首次在 PR #4 的历史提交
b2748fe8062 中被删除，上一版对应 a33f38e5f6e。本轮将 29 个源码文件的调用及 helper
恢复回原清理 patch；Host/Classic/shell/settings 各自在自身 patch 中保留调用。
独立 provider 提前为 PR #5 第 6 个 commit，规则和文档通过 PR #2 更新。
拆分关系见 [构建 Patch Series](BUILD_PATCH_SERIES.md)。

## 1. 保留接口，替换提供者

参考 OpenBlue 的 base/log.h、base/log.c、base/bt_debug.c：保留调用接口，
由项目自己的级别过滤和格式化输出承接，不再链接 Zephyr 的日志核心及后台处理机制。
EmBT 的实际实现位于 base/include/base/log.h 和 base/log.c，使用内部 include 路径。

| 内容 | 实际处理 |
| --- | --- |
| LOG_ERR / LOG_WRN / LOG_INF / LOG_DBG | 保留原调用和消息，通过内部适配器输出 |
| LOG_HEXDUMP_ERR/WRN/INF/DBG | 标签和数据共用级别检查与输出锁，关闭后两者均不输出 |
| LOG_*_RATELIMIT / LOG_*_RATELIMIT_RATE | 保留接口及限频，使用单调时间和 call-site atomic 状态 |
| NET_BUF_* / NET_BUF_SIMPLE_* | 保留包装和调用，连接相同 LOG 接口 |
| LOG_MODULE_REGISTER / DECLARE | 模块名转换为 LOG_TAG，不恢复自动注册和链接段 |
| 模块级别条件 | 接到全局 EMBT_LOG_LEVEL，不补回原模块 Kconfig 模板 |
| 原 sniffer 信息 | 接到 EMBT_LOG_SNIFFER_INFO，保留原配置 guard 和数据转换，默认关闭 |
| Zephyr logging 实现、backends、消息队列、模板 | 继续退役，不恢复为构建依赖 |
| printk、断言、shell、HCI monitor 等独立能力 | 保留，不因日志实现替换而删除 |

日志调用参数仍采用已经适配的真实接口：atomic_* 改为 bt_atomic_*，
pool_id 改为 pool 指针，三处对应的 pool 输出格式从 %u 改为 %p。
不恢复旧 pool id、Zephyr kernel/device、设备树或未使用的驱动入口。

## 2. 恢复范围和原始记录

来源是项目已导入的 Zephyr 基线 f8da8ea370508f274685a25ff19dbe5a9ff54790。
按当前保留文件的原路径映射进行核对，恢复被误删的 Host、Classic、common、crypto、
settings 和 driver 日志；已有 Mesh、Audio、services 等日志调用保持，统一内部提供者。
不把恢复日志扩大成回退已完成的 OS、buffer、settings、驱动或目录适配。

恢复的 helper/条件包括：

- L2CAP LE/BR 状态变化、非法转换和未知状态诊断，Debug 条件改为全局级别。
- Controller transport、identity、HCI/LMP 版本输出；transport 名来自当前真实驱动。
- RPA/NRPA、IRK、LTK、passkey/OOB 的原 sniffer 信息及对应配置条件。
- settings handler 和初始化错误、后台 identity/IRK 保存错误。
- net_buf 分配、数据分配、释放、引用饱和和 buffer 压力诊断。

net_buf 保留当前等待实现和有限超时语义；压力诊断在现有等待前后输出，
没有恢复旧日志分支中的周期重试循环及旧 debug 分配 API。
旧 pool-name/pool-id 形式使用当前实际 pool 指针表示。

bt_hex/bt_uuid_str 原先使用全局可写字符串。恢复并发日志后改为线程内缓冲，
避免 RX、worker、application 线程同时格式化导致数据竞争；同线程的后续调用仍会覆盖前一结果。
没有引入缓存数据库或另一套消息队列。

原目录删除记录：lib/logging/ 的以下 17 个 Zephyr 实现/配置文件已在清理板块删除，
它们继续保持删除。本轮恢复的是接口和调用。

| 本次删除文件 |
| --- |
| `lib/logging/CMakeLists.txt` |
| `lib/logging/Kconfig` |
| `lib/logging/Kconfig.filtering` |
| `lib/logging/Kconfig.formatting` |
| `lib/logging/Kconfig.misc` |
| `lib/logging/Kconfig.mode` |
| `lib/logging/Kconfig.processing` |
| `lib/logging/Kconfig.template.log_config` |
| `lib/logging/Kconfig.template.log_config_inherit` |
| `lib/logging/Kconfig.template.log_format_config` |
| `lib/logging/log_cache.c` |
| `lib/logging/log_cache.h` |
| `lib/logging/log_core.c` |
| `lib/logging/log_mgmt.c` |
| `lib/logging/log_minimal.c` |
| `lib/logging/log_msg.c` |
| `lib/logging/log_output.c` |

## 3. 当前轻量实现

- EMBT_LOG_LEVEL：0=关闭，1=错误，2=警告，3=信息，4=调试；默认 3。
- 编译上限由 Kconfig 生成，运行时通过内部 bt_log_set_level/get_level 调节，不能超过编译上限。
- 先检查级别再执行消息参数；关闭的 hexdump 不执行 data/len/label 参数。
- RATELIMIT 使用每个调用点的 atomic 状态和单调毫秒，默认间隔 1000 ms；显式间隔最大 INT32_MAX。
- POSIX 实现同步写 stderr；格式为 [level][tag:line] function: message。
- stdio 锁保护整条消息及整个 hexdump，不新增 logger 线程、堆分配或固定长度消息截断。
- 日志输出可能阻塞，不是 signal handler/硬中断可用接口，也不提供持久化或异步日志投递保证。
- base 日志头与其 atomic 依赖保持内部可见性；EmBT::embt 不向应用导出 base/OSI include 路径。
- EMBT_LOG_SNIFFER_INFO 保留原独立开关，不随普通 Debug 级别自动开启。

## 4. 验证与复现

新增 test/bringup/test_log.c 使用 cmocka 验证：实际文本、级别和参数过滤、
Debug hexdump、长消息、限频、四线程 200 条消息不拆行，以及两线程格式化 helper 不相互覆盖。
该程序包含 6 个用例，作为 bringup.base.logging 这一项 CTest 运行。
既有 settings/OSI/transport 测试继续执行。

普通 settings+btshell：

```sh
./mkbt.sh test-port --build-dir build/settings-file-normal \
  --fragment config/settings_file.conf --fragment config/btshell.conf
```

Debug + ASan/UBSan：

```sh
./mkbt.sh test-port --build-dir build/log-debug-asan \
  --fragment config/settings_file.conf --fragment config/log_debug.conf \
  --cmake-arg -DEMBT_OSI_SANITIZER=ADDRESS
```

SOURCE crypto + Debug + settings/btshell：

```sh
./mkbt.sh btshell --build-dir build/log-debug-source \
  --fragment config/settings_file.conf --fragment config/log_debug.conf \
  --fragment config/crypto_source.conf
```

关闭日志使用 EMBT_LOG_LEVEL=0 的独立 fragment 和独立 build 目录验证。
已有 build 的 fragment 列表变化需要显式 defconfig；本轮使用独立目录，不改用户已有配置。

最终验证：

| 配置 | 结果 |
| --- | --- |
| 普通 settings+btshell | 71/71 CTest 通过 |
| Debug + ASan/UBSan + settings | 70/70 CTest 通过，不含 shell CLI case |
| 关闭日志、关闭 settings | 52/52 CTest 通过 |
| Mbed TLS 3.6.5 SOURCE + Debug + settings/btshell | 编译、完整链接通过 |
| 新日志程序 | 6/6 cmocka 用例通过，计作一项 CTest |
| 新日志 printf 格式 | 上述最终构建无 format 警告 |

未在本轮占用 USB Controller 或执行无线配对；
Mesh/Audio 等未编入基础 profile，接口迁移不等于这些组件已完成全部适配。
原始快照、恢复候选、消息核对及构建日志位于 /tmp/embt-log-restore。

## 5. 提交边界

本轮调用恢复归首次删除它的原 PR #4 清理提交。后续适配不再删除这些调用，
日志实现及配置归原构建 PR #5 第 6 个 commit，文档/规则更新归原文档 PR #2。
已有 PR 保持 Draft、未合入；集成仍由人决定。
