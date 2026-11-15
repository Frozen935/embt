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

# Settings 原设计与 POSIX File Backend 适配

实现起点：`feat/settings-posix-file`，基于原构建 PR #5 的 `8854edad6b9`。
本轮纳入 [构建 Patch Series](BUILD_PATCH_SERIES.md) 的 settings commit，文档通过 PR #2 更新。
先前本地目录、btshell、Controller 改动已经分别更新到文档 PR #2（08c9d44e2de）、
清理 PR #4（211b3367c7f）、构建 PR #5（8854edad6b9）。三个 PR 都是 Draft、未合入，main 未改变。
本文件及 settings 新代码是在该次提交之后产生的工作；本轮按职责拆分后提交到原 Draft PR。

日志修正：本轮已恢复 settings 核心及 Bluetooth settings 的原 LOG 调用，使用内部 base/log.h。
文件后端及 handler/store 设计保持不变；只退役 Zephyr 日志实现，日志调用与错误诊断继续保留。

## 1. 设计来源与保留范围

只参考并移植同一 Zephyr 导入基线 `f8da8ea370508f274685a25ff19dbe5a9ff54790` 的 settings 实现，
没有沿用 OpenBlue 的 settings 方案。

新增/移植来源包括：

- `subsys/settings/src/settings_init.c`
- `subsys/settings/src/settings_file.c`
- `subsys/settings/include/settings/settings_file.h`

原 `settings.c`、`settings_store.c`、`settings_line.c` 和 `settings_runtime.c` 在现有 lib/settings 中适配。
保留导入版权、许可、公共 handler/store 类型和 API 名称；新文件保留完整项目 Apache banner。

继续使用原来的分层：

| 层 | 职责 |
| --- | --- |
| settings_handler/static descriptor | h_get/h_set/h_commit/h_export、分层 name 和 commit priority |
| settings core | 最长子树匹配、静态/动态 handler 登记及 commit 顺序 |
| settings_store + csi_* | 多个读取 source、一个保存 destination，load/save/export 接口 |
| settings_line | 原长度前缀、name=value 二进制记录和 read callback |
| settings_file | POSIX 文件操作、追加、去重、最新值选择、压缩与持久化 |

没有在 core 中建立另一套 key/value 内存数据库；RAM 状态继续由 handler 管理。
FILE 是唯一保留的持久化 backend，原 flash/NVS/FCB/ZMS/retention 等来源没有重新引入。
`settings_runtime` 保留为直接访问 handler 的 get/set/commit API，它不实现 settings_store，也不保存数据。
因此将其 Kconfig 提示改为“Runtime access to settings handlers”，避免误称持久化 backend。

## 2. 初始化和注册

`settings_subsys_init()` 显式创建进程生命周期的递归 OSI mutex，初始化 core/store，并挂入默认 FILE source/destination。
它不依赖 Zephyr SYS_INIT，不启动工作线程，不与 Controller 初始化绑定。
失败可以重试；已经初始化的 core registry 不会在 backend 重试时被清空。

动态 handler 继续使用 settings_register/settings_register_with_cprio，保留原 intrusive 节点和公共布局。
静态宏继续定义只读 descriptor，登记改为明确调用 settings_register_static；同一 descriptor 重复登记幂等，
不同对象使用同名 subtree 返回 EEXIST。descriptor、dynamic handler 和被借用的路径均需保持进程生命周期。
不创建空 section 遍历宏或恢复旧 linker 自动初始化机制。

BT_SETTINGS 的 bt、keys、CCC、SC、CF/hash（按配置）及 BR link_key descriptor 在 bt_settings_init 中明确登记，
原 priority 0/1 与配置 guard 保留。
`bt/` 的数据恢复仍要求 bt_enable 后 HCI 已打开；未打开时原 Host handler 延后应用数据。
测试没有通过设置假 OPEN 标志来绕过该条件。

## 3. File Backend 与格式

保留 Zephyr FILE backend 的追加记录设计、同值去重、删除标记、最新记录优先和 max-lines 压缩逻辑。

```text
uint16 native-endian length | name bytes | '=' | raw value bytes
```

length 是 name、分隔符和值的总字节数，不包括自身两字节。值可以包含 NUL、换行和任意二进制字节。
native-endian 长度字段沿用原 settings_line，不声明跨端序格式兼容；也没有新增 CRC 或加密格式。
值长度 0 表示删除，读取时忽略删除记录及其更早版本；无法用零长度值表示另一个独立的空 blob。

压缩阈值表示何时整理 superseded records，不是可保存 key 数量的上限。
压缩输出有效最新值和新记录，保持原记录编码；没有改为 JSON、INI 或全量 RAM cache。

POSIX 适配包括：

- 正确处理 write 的短写和 EINTR，不把部分写入当成成功。
- 检查文件类型、name/value 长度、记录边界，read_cb 限定在当前 value 内；SIZE_MAX 请求不会读到下一条记录。
- 加载前验证完整记录。未完成的最后一条 append 被忽略，下一次实际写入截去该尾部；完整但畸形的记录返回 EBADMSG。
- append 写入失败尽量恢复原有效文件长度；已有完整记录不因新的 partial append 被替换。
- 写入和新目录/文件创建使用 fsync；同值重试也执行同步，避免一次 fsync 失败之后返回虚假的持久化成功。
- 压缩写入同目录临时文件，关闭前同步，再 rename 发布并同步 parent directory。
- companion .lock 文件通过 flock 保护读取、追加和压缩，避免 rename 后换 inode 造成跨进程锁失效。
- 新目录 0700、数据和锁文件 0600；不跟随数据文件/锁文件 symlink。
- load callback 可以使用提供的 read_cb；从 callback 再次进入 FILE backend 返回 EDEADLK，避免递归 flock 死锁。

fsync 或 rename 后的 directory sync 失败意味着持久化状态不确定：数据可能已经写入/发布，返回错误并不代表数据必然未出现。
多 key h_export/save_subtree 仍按原设计逐 key 保存，不新增全批次事务或 RAM staging rollback。
如果 load 时发生 I/O 错误，不执行 commit；部分 h_set 的 RAM staging 可能已经发生，调用方可重试。
保留原 core 对单个 handler h_set 校验错误的处理；原生文件 I/O 错误被独立记录并返回，不会被该处理吞掉。

## 4. 配置及使用方式

| 配置 | 设置 |
| --- | --- |
| SETTINGS | 整个模块开关，开启时选择 FILE |
| SETTINGS_FILE | 唯一实际持久化 provider，hidden boolean |
| SETTINGS_ENCODE_LEN | 由 FILE 选择，使用原记录格式 |
| SETTINGS_FILE_PATH | 默认 .embt/settings.bin，相对进程工作目录；可在 Kconfig 修改 |
| SETTINGS_FILE_MAX_LINES | 默认 32，保留原压缩触发语义 |
| SETTINGS_DYNAMIC_HANDLERS | 保留原 default y 和公共 handler 设计 |
| SETTINGS_RUNTIME | 直接 handler API，按需启用 |
| BT_SETTINGS | 独立开启 Bluetooth 持久化，测试 profile 明确启用 |

默认普通编译 profile 继续关闭 SETTINGS；新 `config/settings_file.conf` 用来开启 SETTINGS/FILE/RUNTIME/BT_SETTINGS。
没有恢复 FILE_SYSTEM 或 storage/flash 的根占位配置。

应用调用顺序：

1. 如需运行时选路径，在初始化前调用 settings_file_set_path；该路径字符串被借用，需长期有效且不再修改。
2. 调用 settings_subsys_init。
3. 登记 application handler，使用 settings_register 或 settings_register_static。
4. 使用 settings_load/load_subtree 读取并 commit，或者 settings_load_one/get_val_len 直接查询。
5. 使用 settings_save_one/delete 保存单 key，或 settings_save/save_subtree 通过 h_export 保存当前 RAM 状态。

settings_load_one 返回 value 的实际总长度，最多复制 buf_len 字节；调用方必须核对返回长度，不能把小 buffer 当成完整结果。
settings_delete 只修改持久化记录，不自动修改 handler RAM 状态。

btshell 的 settings profile 在进程启动时准备 settings subsystem，仍不自动调用 bt_enable。
Bluetooth 成功回调检查 settings_load 返回值；失败不打印 Settings Loaded，也不继续报告完整初始化成功。
显式 no-settings-load 模式保留手工加载顺序，并显示 pending 状态。

## 5. 构建与测试

普通 settings+btshell 测试：

```sh
./mkbt.sh test-port --build-dir build/settings-file-normal \
  --fragment config/settings_file.conf --fragment config/btshell.conf
```

ASan/UBSan 的 settings 测试：

```sh
./mkbt.sh test-port --build-dir build/settings-file-tests \
  --fragment config/settings_file.conf --cmake-arg -DEMBT_OSI_SANITIZER=ADDRESS
```

SOURCE crypto 的 settings+btshell 构建：

```sh
./mkbt.sh btshell --build-dir build/settings-file-source \
  --fragment config/settings_file.conf --fragment config/crypto_source.conf
```

18 项 settings 检查：CRUD、handler/priority/runtime、direct load、callback reentry、read bounds、compression、
binary/key/value limits、partial tail、malformed records、short-write/EINTR/failed append、read failure、
rename failure、fsync failure/retry、permissions/symlink、initialization retry、new-process reload、
concurrent process writers、Bluetooth identity store/flush 字节验证及 HCI 未打开时的延后恢复。

| 验证 | 结果 |
| --- | --- |
| settings+btshell 普通 profile | 70/70 CTest，通过 |
| settings ASan/UBSan profile | 69/69 CTest，通过；不包含单独 shell CLI case |
| settings cases repeated | 18 × 10，共 180 次，通过 |
| Mbed TLS 3.6.5 SOURCE + settings/btshell | 编译、链接通过 |
| 公共 settings/settings_file 头 | C/C++11 编译通过，无私有 include 路径 |
| 默认 SETTINGS=n profile | 编译、完整链接通过 |

实际独立进程读写演示使用同一个测试程序，存储放在 /tmp/embt-settings-task/manual-persistence：

```text
child_write: Persisted value=123 and a deletion record.
child_read: Loaded value=123 in a new process; deleted key is absent.
Persistent file: .embt/settings.bin, 80 bytes, mode 0600
```

这是实际文件和新进程读取的结果，不是内存内复用同一个 settings 对象。
没有在本轮占用 USB Controller、进行新配对或保存真实 Link Key/IRK；Bluetooth 配对数据的完整无线恢复仍待后续验证。

## 6. 本轮提交边界

之前的目录/btshell/Controller 变更已更新到原 PR；本轮 settings 代码和测试纳入构建 PR #5
的独立 settings commit，本说明归文档 PR #2。runtime integration hold 仍不解除。

原始日志与基线副本位于 /tmp/embt-settings-task；数据/测试产物由 build/.gitignore 排除。
