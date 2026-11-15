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

# btshell 的 POSIX 适配与验证

最新进展：已经完成 USB hci0 的真实同步/异步初始化和关闭验证，见
[Controller 初始化结果](CONTROLLER_INIT_RESULTS.md)。本 dongle 需要显式 --no-flow-control；
USER 驱动现在在 bind 前执行 HCIDEVDOWN，不停止 BlueZ。以下 CLI-only 验证记录属于前一阶段。

状态：本地 `build/btshell-posix` 分支，基于已提交的构建 PR #5。
本轮 shell 改动未暂存、未提交、未推送。之前的编译实现已经分别更新到文档 PR #2、清理 PR #4、构建 PR #5。
三个 PR 仍为 Draft、未合入，runtime integration hold 继续有效。

## 1. 目录与职责

| 位置 | 内容 | 原因 |
| --- | --- | --- |
| `apps/btshell/main.c` | POSIX 进程入口、启动参数、输入循环、help/version/quit | btshell 是使用协议栈的诊断程序，入口不混入 Host、driver 或 OSI |
| `lib/shell/` | 私有命令树、分词、参数验证、帮助、输出与数字转换 | 无协议依赖的工具机制；内部头为 `<shell/bt_shell.h>`，不导出给 EmBT 的普通消费者 |
| `bluetooth/common/bt_shell_private.*` | Bluetooth 通用输出与显式命令组注册 | 供异步 Bluetooth 回调和协议命令共享 |
| `bluetooth/host/shell/` | 当前 Zephyr 基线的 bt/gatt/l2cap 命令处理代码 | 保留既有命令和协议 API，迁移真实依赖；没有拿旧 OpenBlue Host 代码替换当前基线 |
| `bluetooth/host/classic/shell/bredr.*` | BR/EDR 命令及其多层 L2CAP 子命令 | 与当前 BR Host 代码保持一致 |
| `config/btshell.conf` | 显式开启 BT_SHELL 的配置片段 | 普通库构建不强制编入 CLI；shell 单独配置和构建 |
| `test/shell/smoke.py` | 真实子进程和 PTY 命令行检查 | 只验证命令行，不调用 Bluetooth handler |

## 2. OpenBlue 调研与取舍

直接读取 `/home/frozen/work/opensource/stack_dev/openblue`，版本
`3f8697c608a4b23cd59981a18f09e3d3838e0f72`，没有修改或运行 OpenBlue。

- `samples/cmds/bt_cmd_main.c` 是 btcmd 的入口：先初始化 stack/shell，再 getline 分词并派发。
- `bluetooth/common/bt_shell_private.*` 定义轻量命令描述符和显式 root 注册。
- `BT_SHELL_CMD_ARG_REGISTER` 提供注册函数；`bt_shell_init` 按配置收集协议命令组。
- 上述方向适合独立 Host，EmBT 同样使用显式注册，避免 Zephyr linker section 与自动初始化。

当前 OpenBlue 入口的 argv 为 32 项，但 strtok 循环没有检查上限；本实现用 64 项参数与 4096 字节行上限，
超长行整行丢弃，不能把剩余后缀当成新命令执行。OpenBlue 的空格分词不能表示引号中的字符串；
EmBT 支持单/双引号、空字符串、反斜杠、tab/CRLF，并拒绝不闭合的引号与转义。

OpenBlue `bt_shell_exec` 只找 root 和直接 subcommand；EmBT 递归解析明确的子命令表，
所以 `br l2cap ...`、`bt scan-filter-set ...` 这类命令树不会丢掉深层子命令。
参数 mandatory/optional 在 handler 调用前校验；帮助路径只读取描述符，不执行协议处理函数。

没有沿用 OpenBlue 的强制 preinclude/shim、`!` 系统命令路径、空的 subcommand 注册宏或未实现 unregister。
本轮不需要动态卸载 root 命令，因此不提供假 unregister API。每个 session 保存自己的 descriptor 指针，
避免把同一个全局 intrusive 注册节点挂到多个 session。

## 3. 命令模块适配

- 协议 command source 的 `struct shell`、`shell_*`、`SHELL_*` 迁移到实际 `bt_shell` provider。
- 保留当前命令表和 handler；删除原 kernel/shell 依赖，引用真实的 base/net_buf/embt 工具。
- root 描述符是静态只读数据；`bt_shell_commands_init()` 显式收集 bt/gatt/l2cap/br。
- 原 section-defined 连接回调改成调用 `bt_conn_cb_register`，避免命令看似存在却没有通知回调。
- L2CAP status 输出通过 `bt_l2cap_chan_status_get()` 获取快照，不再普通读取原子内部字段。
- GATT/L2CAP metrics 使用 POSIX 单调毫秒时钟，替代依赖 kernel cycle frequency 的计数器。
- scan/advertise 的长选项使用单一 CLI 线程里的 libc getopt_long，每次解析重置 optind；不复制 Zephyr getopt 状态管理。
- Audio/EAD/BAP 等条件依赖使用实际配置 guard；禁用功能的代码保留，不引入空 provider 满足编译。
- 同步输出通过 stdio stream lock 串行化；异步 Bluetooth 公共输出目前固定写 stdout，不借用临时 session 指针。

Kconfig `EMBT_PORT_HAS_SHELL` 的实际 Linux provider 已接入，hidden capability 默认 y；
`BT_SHELL` 仍由 `config/btshell.conf` 显式启用。普通编译配置不自动开启 shell。
本轮命令组范围与基础 LE+BR 配置一致，不宣称 Mesh、LE Audio、ISO/CS 或其他可选 profile 的 shell 已完成移植。

## 4. 构建和启动

```sh
./mkbt.sh btshell
./build/btshell/bin/btshell
```

入口支持下面三种互斥 endpoint 参数；这些参数只保存配置，不打开 transport：

```sh
./build/btshell/bin/btshell --hci 0
./build/btshell/bin/btshell --tcp 127.0.0.1 9000
./build/btshell/bin/btshell --unix /tmp/hci_bridge.sock
```

不指定 endpoint 时也能进入命令行。启动调用 `bt_stack_init()` 准备基础资源，再登记命令与回调；
不执行 `bt_enable()`，不发送 HCI Reset，也不自动加载或写入配对数据。
只有用户显式执行对应 Bluetooth handler 才可能开始协议操作；这条路径本轮没有测试。

可使用：

```text
btshell> help
btshell> help br l2cap
btshell> help bt scan-filter-set
btshell> version
btshell> quit
```

`help <path>` 和 `<path> --help` 都读取帮助描述，不进入协议 handler。
`quit`、`exit`、`q`、Ctrl-D 正常退出；Ctrl-C 使用 POSIX 默认 SIGINT 结束进程。
交互模式显示 `btshell>` 提示符，pipe 输入不输出提示符。没有增加 history、tab completion 或行编辑依赖。

工具退出时进程结束 process-lifetime worker；没有自动发出蓝牙命令进行清理。
实际启用/关闭、异步回调和连接对象的无线运行生命周期仍属于后续 bring-up 验证。

已有同名 build 目录如果之前配置里没有 BT_SHELL，需要显式重置或从 menuconfig 开启它；
不要为了重放片段悄悄覆盖已有用户配置。新目录会读取基础 defconfig 与 shell fragment。

## 5. 验证与边界

本轮实际执行：

1. SYSTEM crypto 下构建 btshell，保留 bt/gatt/l2cap/br 命令源码，完成可执行文件链接。
2. pinned Mbed TLS 3.6.5 SOURCE crypto 下构建和链接 btshell。
3. 正常构建执行 16 个 CLI 行为检查：启动参数、root/多层帮助、引号转义、tab/CRLF、无效输入恢复、
   64 项参数限制、4096 字节行溢出、NUL 拒绝、未知命令、EOF、三类 endpoint 只配置，以及 PTY prompt/quit/Ctrl-D/Ctrl-C。
4. shell-enabled ASan/UBSan 构建：原有 50 项测试和一个包含 16 个检查的 CLI integration test，共 51/51 通过。
5. strace 检查 `--hci 0` + help/quit：没有 socket、connect 或 bind 系统调用，未接管真实控制器。

验证命令：

```sh
python3 test/shell/smoke.py build/btshell/bin/btshell
./mkbt.sh test-port --build-dir build/btshell-asan \
  --fragment config/btshell.conf --cmake-arg -DEMBT_OSI_SANITIZER=ADDRESS
./mkbt.sh btshell --build-dir build/btshell-source --fragment config/crypto_source.conf
```

协议 command handlers 只完成编译/链接适配，本轮没有执行初始化、广播、扫描、连接、配对或 BR 命令。
导入 command source 仍有 unused parameter/signedness 等告警；这些不代表无线功能已验证。
Linux HCI USER 的旧 EPERM 边界未被本轮修改。

日志保存在 `/tmp/embt-btshell-smoke2.log`、`/tmp/embt-btshell-asan.log`、
`/tmp/embt-btshell-source.log`、`/tmp/embt-btshell-socket-trace.log`。
三个已提交 PR 的状态与 SHA 核验保存在 `/tmp/embt-submit-btshell/remote-verified.json`。
