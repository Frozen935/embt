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

# 测试与验证

本页维护测试分类和实际命令；最近结果、已知问题与延期见 [STATUS](STATUS.md)。
测试来自 Zephyr `f8da8ea370508f274685a25ff19dbe5a9ff54790`，原件历史、目录迁移和适配
记录保留。移植不保留 board/YAML/overlay、Controller、原驱动/ISR 或 Flash provider 的测试。
Monitor 使用真实 producer 和内存/文件 sink，不依赖原 UART。

## 测试层次

| 类别 | 验证对象 | 不能推断的范围 |
| --- | --- | --- |
| OSI 契约 | real backend 是否满足 osi_* 行为、时间与生命周期 | native 故障分支、实时调度、所有目标平台 |
| base/transport/settings/audio/framework | 机制、I/O、资源与 runner 的边界 | 全协议功能、真实无线/扬声器 |
| Bluetooth C 默认 profile | 每单元选择的源、fake/fixture 与协议断言 | 默认关闭的功能分支、完整 Controller |
| 配置变体 | 已声明选项、实际编译和必需 case 的执行 | 所有合法 Kconfig 组合 |
| Classic/Bumble/pytest | 真实 EmBT 程序对接模型 Controller/peer | 真实设备无线互通、Controller 实现或 SIG 认证 |
| 物理 Controller | user socket 与实际无线行为 | 没有执行过的 profile/peer |

各项 CTest、cmocka case、pytest 场景和配置 profile 的总数不能直接相加称为 API 覆盖率。
skip 不计通过，模型扩展与生产代码修复分开记录；测试来源见
[测试基线](upstream/BLUETOOTH_TEST_BASELINE.md)，当前结果见 [STATUS](STATUS.md)。

## OSI 和机制

```sh
./mkbt.sh test --build-dir build/osi-tests
./mkbt.sh test --build-dir build/osi-clang \
  --cmake-arg -DCMAKE_C_COMPILER=/usr/bin/clang-18
./mkbt.sh test-port --build-dir build/mechanism-tests \
  --fragment config/full_port.conf --fragment config/audio_playback.conf \
  --fragment config/audio_lc3.conf \
  --cmake-arg -DEMBT_BT_TEST_FILTER=framework
```

Clang 路径按实际安装调整。新 build 默认 ASan/UBSan，关闭用
`--cmake-arg -DEMBT_OSI_SANITIZER=NONE`。TSan 另建目录，不与 ADDRESS 合用。
默认 core test-port 会通过同一 Kconfig driver 加载 `test/bluetooth/core.conf`，为 CCM 和
AT 单元提供 Host CCM/HFP 依赖；普通非测试构建不加载该 fragment。已有测试 build 若配置
不匹配，先用 `./mkbt.sh defconfig --build-dir <dir> --fragment test/bluetooth/core.conf`
明确重置。framework profile 仍单独验证机制与测试框架。

## Bluetooth C 与变体

```sh
python3 tools/tests/run_bluetooth_tests.py --suite addr --output build/test-results/addr
python3 tools/tests/run_bluetooth_tests.py --jobs 3 --output build/test-results/c-default
python3 tools/tests/run_bluetooth_tests.py --variants --jobs 3 --output build/test-results/variants
```

每单元保留自己的 CMake 和 `test.json`：CMake 声明 source/fake/link；JSON 声明 runner、tags、
timeout 和功能场景。`config.conf` 是该单元初始 profile。变体检查选项最终值、配置 hash、
编译发现的 case 和 required-case 实际执行。改变 profile 不靠全局 CONFIG defines。

`BT_TEST_SUITE` 是常量 descriptor，经显式生成的引用表交给共有 cmocka runner。
二进制 `--list` / `--list-json` 不运行 fixture；`--suite` / `--case` 做实际选择。
ordered suite 保持同一进程顺序，不能把每个 CTest 项与其内部 case 数混为一谈。

## Classic 与 Bumble

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r test/bluetooth/classic/requirements.txt
.venv/bin/python tools/tests/run_bluetooth_tests.py \
  --suite classic/gap_c --suite classic/gap_s --jobs 2 \
  --output build/test-results/classic-gap
```

默认全 C 命令不会自动执行 Classic pytest。要逐个选择 `runner=pytest/linked` 的单元，
不能仅加 `--tag classic` 就把未选中的 pytest 自动纳入。
每个场景启动真实 EmBT 程序及 linked Bumble Controller，隔离 settings、snoop 和退出状态。

L2CAP server 元数据已指向真实 server 文件，规范 runner 会执行其 16 个场景：

```sh
.venv/bin/python tools/tests/run_bluetooth_tests.py \
  --suite classic/l2cap_s --output build/test-results/l2cap-server-entry
.venv/bin/python -m pytest --import-mode=importlib \
  test/bluetooth/classic/l2cap_s/pytest/test_l2cap_server.py \
  --embt-build="$PWD/build/upstream-classic_l2cap_s/test/bluetooth/classic/test_bt_classic_l2cap_s" \
  --junitxml="$PWD/build/test-results/l2cap-server-direct.xml"
```

第一条构建并执行已修正的单元，第二条可直接验证同一文件。直接 pytest 命令必须用绝对
executable 路径，因为 fixture 会切换到独立工作目录。近期结果见 STATUS，原失败记录保留。

## 结果可信度与延期

cmocka 主测试线程负责断言；worker 发布结果，在 join 后检查，不能跨线程 longjmp。
回调/worker assertion 使用受限的 test-only boundary 或终止隔离进程，不能吞错误。
未知 suite/case、发现失败、required-case skip、零执行都应失败。cmocka、pytest 和 linked
均检查本次生成的 JUnit，拒绝零执行、全跳过和失败结果；启动前删除旧 XML，不能以旧报告
或 collect-only 代替执行。GAP/SMP helper 合并命令返回内容与后续异步响应进行匹配。

三个 Mesh 调度契约 case、`bt_disable__test_command_in_flight_at_close` 和
`bt_disable__test_data_sent_during_disable` 明确延期。关闭/重启的新增锁区、测试内部锁
屏障和 FIFO 调度 wrapper 已撤回；这些应用生命周期测试不依赖内部 Host 锁制造通过。
skip 不证明生产关闭/重启安全，其余条件性 skip 用变体记录实际执行范围。

## 实机与证据

`adv` profile 是原五分钟连续广播数据更新测试，需要独占 hci0 和 user channel 权限。
它与普通软件回归分开，不能默默加入默认命令。完整 M1 还需 BLE/BR 的连接、安全、
数据和重启恢复矩阵；目前尚未全部验收。

保存实际 SHA、配置、依赖版本、命令、case 清单、JUnit/status/失败日志，以及实机
Controller/peer 和 quirk。build 与 /tmp 是本地证据，不进 Git；历史记录说明证据是否仍可取得。
不要覆盖原失败或用重复成功改写先前全量结果。本轮文档整理只核对命令与入口，没有重跑测试。
