<!--
/****************************************************************************
 *  Copyright (C) 2026 <COPYRIGHT HOLDER>
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

# Zephyr 来源与社区补丁维护

状态：Review v1，作为开发中记录通过草稿 PR 跟踪。源码基线及 init 已建立；运行基线验证前，后续研发 PR 均保持未合入。

## 1. 初始来源

| 字段 | 值 |
| --- | --- |
| 来源仓库 | https://github.com/zephyrproject-rtos/zephyr |
| 基于分支 | main |
| 基线完整 SHA | f8da8ea370508f274685a25ff19dbe5a9ff54790 |
| 基线提交时间 | 2026-11-12T19:13:43Z |
| EmBT 仓库 | https://github.com/Frozen935/embt |
| 当前仓库关系 | 独立仓库，fork=false，parent/source 为空 |
| 建立方式 | GitHub 服务端 fork，随后解除关系并保留 Git 历史 |
| 解除核验时间 | 2026-11-13T10:08:18Z |
| 历史校验 | main、tree、parents、author、committer 与解除前一致 |

初始记录见 [upstream/INIT.md](upstream/INIT.md)。移植前先提交该说明；不将源码裁剪或编译成功写成已经完成的运行验证。

## 2. 独立维护原则

EmBT main 按自身架构演进，不整体同步 Zephyr main。每周只筛选 BLE、BR/EDR 及必要依赖的有价值补丁，排查前置依赖和当前代码是否已覆盖同一问题。

直接引入保留原始作者与 message，并使用 cherry-pick -x 记录原始 SHA。需要适配时保留原始说明，在独立段落描述 EmBT 调整。原始代码注释在纯迁移中保持；逻辑改造涉及失效注释时更新其说明，并保留原始版本和改动来源。

本版的 comments 范围包括原始 commit message（正文、空行、trailers）和代码注释；PR/review 讨论保留链接。是否要求全量评论快照归档，待 review 确认。

## 3. 文件组织

~~~text
docs/upstream/
├── INIT.md
├── YYYY-MM-DD_HHMMSS.md
└── ...
~~~

- INIT.md 是首份源码来源说明。
- 每批实际引入的补丁对应一个时间文件，使用 Asia/Shanghai 时间；正文记录带时区时间。
- 一次多补丁按原应用顺序列出。文件名秒级时间仍有冲突时增加批次序号。
- 扫描范围和处理状态单独维护；扫描到某个上游 SHA 不表示该范围全部补丁已引入。
- 已发布记录保持可追溯；更正通过明确的追加或更正说明完成。

## 4. 每批操作顺序

1. 冻结 EmBT 起点和本轮上游检查终点。
2. 筛选相关补丁，核对依赖、当前代码效果和改造后的适用性。
3. 按依赖顺序 cherry-pick 或适配移植，保留原始说明和版权。
4. 完成当前阶段适用的配置、构建和运行验证；未验证项目标记清楚。
5. 在本地工作分支提交实际代码补丁，获得对应的 EmBT SHA，记录分支及尚未集成的状态。
6. 创建时间文件，附完整原始消息和 SHA 列表，记录适配及验证结果；用文档提交保存说明。
7. 推送工作分支并创建或更新增量草稿 PR，核对远端分支及记录。当前阶段不合入主线；已约定的运行基线验证后，由人决定并执行最终合入。

记录提交与代码提交分开，因此可以填写已知的 EmBT SHA，无需预知记录提交自身的 SHA。多个相关提交可以作为同一批次 review。

Agent 禁止直接更新默认或集成分支、通过 API 写入主线、合并 PR 或启用 auto-merge。
该规则同样适用于社区补丁和更新说明文档；“提交”“提上去”只授权工作分支交付，最终合入由人操作。运行前所有 PR 保持草稿、未合入，本地可以从依赖的未合入分支继续开发。

检查游标、已应用到工作分支的补丁和已合入主线的补丁分别记录，不能把本地实现或 PR 提交状态描述成主线已集成。

## 5. 时间文件模板

~~~markdown
# YYYY-MM-DD HH:MM:SS +08:00 - 本批更新说明

## 基本信息
- 来源仓库：
- 基于分支：
- 上游检查范围：<完整起点 SHA>..<完整终点 SHA>
- EmBT 起点 SHA：
- 工作分支和 PR：
- 集成状态：当前未合入，运行基线验证前保持草稿
- 批次范围和背景：
- 配置、平台、外部依赖版本：

## 补丁列表
| 序号 | 原始完整 SHA | 原标题 | 原作者/日期 | 处理方式 | EmBT SHA | 依赖 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | <40 位 SHA> | <原文> | <原文> | 直接 CP / 适配 CP | <40 位 SHA> | <前置补丁> |

## 原始 comments
逐补丁附完整原始 commit message 的原文代码块，包含正文和 trailers。
保留原始提交、PR/review 的链接；按约定附评论快照或关键讨论出处。

## EmBT 适配说明
- 原始路径 → EmBT 路径：
- 因平台或架构变化作出的调整：
- 对接口、行为和依赖的影响：
- 未引入或暂缓内容及原因：

## 验证
- 实际执行的配置、构建命令：
- 实际执行的运行流程、控制器及 peer：
- 日志/抓包证据和结果：
- 未完成的验证：
~~~

新记录使用 [license 头模板](templates/LICENSE_HEADERS.md) 的 Markdown 形式。

## 6. 获取补丁的命令形态

以下是在实施阶段配置本地来源并获取补丁的示例，当前规划阶段不执行：

~~~sh
git remote add upstream https://github.com/zephyrproject-rtos/zephyr.git
git fetch --no-tags upstream main
git cherry-pick -x <upstream-commit>
~~~

origin 指向 EmBT。upstream 只作为获取和评估补丁的来源，不恢复 GitHub fork 关系，也不自动更新 EmBT main。

## 7. 初始核验记录

main 为 f8da8ea370508f274685a25ff19dbe5a9ff54790；tree 为 3dfaeb6628ab061e0a441013ab4b6d3caeb78551；直接父提交为 e516578ef7c2c7339b1ec506910593aca7945946。作者、提交者与解除前一致。

导入的 Zephyr 基线为非浅克隆，包含 159,172 个提交、70,681 个受版本管理文件。目录迁移已完成，源码尚未裁剪或改造；没有新增的社区补丁批次、独立构建或运行验收记录。
