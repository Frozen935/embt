<!--
SPDX-FileCopyrightText: Copyright (c) 2026 ByteDance Ltd.
SPDX-License-Identifier: Apache-2.0
-->

# EmBT 初始源码基线

本文件固定 EmBT 开始独立开发时的源码起点。后续目录裁剪、构建替换和协议栈移植均从该基线展开；本记录不代表这些改造已经完成。

## 来源

| 字段 | 值 |
| --- | --- |
| 来源仓库 | https://github.com/zephyrproject-rtos/zephyr |
| 基于分支 | main |
| 原始 SHA | f8da8ea370508f274685a25ff19dbe5a9ff54790 |
| 原始提交时间 | 2026-11-12T19:13:43Z |
| 提交描述 | tests: bluetooth: tester: Reserve BTP service ID for NLCP |
| 目标仓库 | https://github.com/Frozen935/embt |
| 初始复制方式 | GitHub 服务端 fork，随后解除 fork 关系 |
| 关系核验时间 | 2026-11-13T10:08:18Z |
| 历史核验 | fork=false；main、tree、parents、author、committer 与解除前一致 |
| 基线范围 | 完整 Zephyr 工作树和 main 历史，159,172 个提交、70,681 个受版本管理文件 |

## 原始提交说明

以下保留基线 commit 的原始 message：

~~~text
tests: bluetooth: tester: Reserve BTP service ID for NLCP

auto-pts has added the BTP service for the NLC Profile Test Suites
with ID 0x21 (auto-pts/auto-pts commit 0fd27d3c9476, "autopts: Add
Generic Test module for NLC Profile Test Suites").

Add BTP_SERVICE_ID_NLCP (0x21) to the tester list of BTP services, so
the ID is not used for another service.
The BTP_SERVICE_ID_MAX is also increased.
The service itself is not implemented by this commit.

Signed-off-by: Kostyantyn Komarov <ext-konstantyn.komarov@nordicsemi.no>
~~~

## EmBT 方向

首批提取 BLE 和 BR/EDR Host，先使用 Linux POSIX 和 HCI user socket 运行。首期保留 Zephyr buf 机制和应用 API，移除设备树及无关 OS/板级依赖。

代码及必要依赖将收敛到 bluetooth/，项目文档放 docs/；samples/tests、Mesh、LE Audio 和无关源码不保留。后续 HCI Command 异步化、单线程和应用操作接口异步化分别实施。

EmBT main 独立演进，仅 cherry-pick 或等价移植有价值的社区蓝牙补丁。保留原始 copyright、许可、代码注释和提交来源；字节跳动新增或修改内容使用公司 copyright。

## 更新方式

本 init 记录先于代码裁剪和移植提交。后续每批社区补丁在 docs/upstream/ 下使用 YYYY-MM-DD_HHMMSS.md 文件记录更新，文件名按 Asia/Shanghai 时间生成。

每份更新说明列出原始完整 SHA、原始标题和作者、对应的 EmBT SHA，并保留完整原始 commit message、comments 和 trailers。适配说明、影响范围及实际验证结果单独记录。

EmBT main 不整体同步 Zephyr main。具体移植规划另行 review，本次先建立源码来源和更新记录的起点。

当前尚未应用任何新增的社区补丁，尚未完成独立构建或运行验收。
