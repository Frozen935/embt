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

# USB Controller 初始化验证

最新输出调整：btshell 的初始化成功回调已补充 HCI/LMP 版本、revision/subversion 和 manufacturer，
直接使用 Read Local Version Information 已缓存的字段，不增加 HCI 命令。新代码已编译并通过 CLI smoke；
验证新输出的后续尝试因 hci0 已被另一个交互式 btshell 占用而返回 EBUSY，未计为成功初始化记录。
已有运行实例需要退出并重新启动，才能加载新的输出代码。

状态：2026-11-15，本地 `build/btshell-posix`。用户已批准 sudo 真机测试，并明确要求通过驱动
HCIDEVDOWN 释放 Controller，不停止 BlueZ。当前改动未暂存、未提交、未推送。
本轮确认初始化和关闭可执行；完整无线运行基线尚未验收，Draft PR 的集成冻结不变。

## 1. 设备与入口

- USB dongle：33fa:0001，Linux hci0，Barrot Technology Limited（Manufacturer 2279）。
- HCI/LMP 5.1，revision/subversion 0x0089；Controller ACL buffer 1021 bytes × 9，SCO buffer 255 bytes × 4。
- 测试前确认无活动连接。BlueZ 服务始终保持 active。

```sh
./mkbt.sh btshell
sudo ./build/btshell/bin/btshell --hci 0 --no-flow-control
```

同步初始化和关闭：

```text
btshell> bt init sync
Bluetooth initialized
btshell> bt disable
btshell> quit
```

异步初始化使用 `bt init`，等待 `Bluetooth initialized` 完成回调后再调用 `bt disable`。
本轮在同一进程中连续执行两轮异步 enable/disable，均成功。

## 2. 驱动与公共头改动

`socket_io.c` 的 Linux user endpoint 在 socket 创建后、bind 前执行 HCIDEVDOWN。
Controller index 按 ioctl ABI 以整数值传入；已经 down 的 EALREADY 允许继续。
真实错误返回负 errno 并关闭 fd，避免在设备释放失败时继续 bind。
TCP 和 Unix endpoint 不执行这个 ioctl。driver close 不停止 BlueZ，也不自动恢复 powered 状态。
测试拥有者负责关闭后把原本 UP 的 hci0 恢复为 UP；本轮还执行了原 name/class 的恢复命令。

HCI 公共头补上自身使用的 errno.h，避免单独 include 该头时 ENOSYS/ENOTCONN 未声明；
补齐 driver data 的显式指针转换，使公开 HCI 头可独立用于 C++11。

## 3. Controller 兼容性限制

默认配置的第一次真机调用已经完成 Reset、Read Local Features、Read Local Version 和
Read Supported Commands，随后在 Host Buffer Size（0x0c33）失败：

```text
Host ACL MTU: 200, ACL packet count: 10
Host SCO MTU: 0, SCO packet count: 0
Status: Unsupported Feature or Parameter Value (0x11)
```

Controller 的 supported commands bitmap 声称支持相关流控命令，但拒绝当前 Host Buffer Size。
本轮没有断言它对所有参数组合都不支持，也没有修改所有 Controller 的默认行为。
使用既有 BT_HCI_QUIRK_NO_FLOW_CONTROL，通过 btshell 显式 `--no-flow-control` 参数选择，
只影响该次 endpoint 的 Controller-to-Host flow-control 行为，不关闭 Host-to-Controller command/ACL credit 管理。
这是兼容模式的初始化成功，不能据此宣称该 dongle 的 Controller-to-Host 流控可用。

## 4. 成功的初始化路径

单轮成功日志有 25 条初始化 HCI 命令及 1 条关闭 Reset，命令完成 status 均为 0x00：

| 阶段 | 已执行的主要操作 |
| --- | --- |
| Common | Reset、Read Local Features、Read Local Version、Read Supported Commands、连接接受超时 |
| LE | Read LE Features、Read LE Buffer Size、Write LE Host Support、Read Supported States |
| LE 数据与安全基础 | Read Maximum Data Length、Write Default Data Length、Read Resolving List Size、Set LE Event Mask |
| BR/EDR | Read Extended Features、Read BR Buffer Size、Write SSP Mode、Write Inquiry Mode、Write Local Name/Class/Page Timeout |
| BR 安全与公共状态 | Write Secure Connections Host Support、Read Default Link Policy、Set Event Mask、Read BD_ADDR |
| Host 初始化完成 | bt_conn_init 及其 ATT/GATT、SMP、L2CAP 初始化返回成功，bt_enable 返回 0，并发布 READY |
| Close | bt_disable 返回成功，Reset 成功，HCI user channel 关闭并 join RX |

没有执行广播启用、扫描启用、Inquiry、Create Connection、配对或音频收发。
公钥生成等 Host 后台工作已经可以运行，但本轮未把安全协议交互或密码学覆盖率作为验收结果。

## 5. 验证结果

| 验证 | 结果 |
| --- | --- |
| 普通二进制同步 enable/disable | 成功，1 个完整周期 |
| 普通二进制异步 enable/disable | 成功，同一进程 2 个完整周期 |
| ASan/UBSan 二进制真机 enable/disable | 成功，1 个完整周期，无 sanitizer 报错 |
| shell-enabled ASan/UBSan 回归 | 52/52，通过新增 ioctl 顺序及释放失败回收验证 |
| 设备恢复 | 每次 hci0 恢复 UP，BlueZ 始终 active；测试前后无活动连接 |

52 项回归中包含：原 OSI 29 项、base/transport 22 项、CLI integration 1 项。
增加了 modeled HCIDEVDOWN/bind 顺序与 EPERM 失败后的 fd 回收、再次打开验证。
CLI smoke 验证兼容参数只保存配置，未执行 Bluetooth handler 时不会打开或改变 Controller。

原始日志、btsnoop 和 JSON 记录保存在 `/tmp/embt-controller-init/`：

- `attempt1.*`：默认流控路径的 HCI 0x11 失败及关闭。
- `attempt3.*`：成功的同步初始化和关闭。
- `async-repeat.*`：同一进程两次异步初始化和关闭。
- `asan-hardware.*`：sanitizer 二进制的真机结果。
- `final-regression-52.log`：最终 52 项回归结果。
- `validation.json`：命令序列、恢复状态和源码校验值。

早期 `attempt2` 是新参数尚未构建成功时的 CLI 参数拒绝，不算作 HCI 初始化尝试。
所有无线运行结论以这里的实际作用范围为限；Controller 初始化成功没有解除 runtime integration hold。
