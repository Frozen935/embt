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

# Zephyr 来源与选择性同步

初始来源固定为 Zephyr `main` 的 `f8da8ea370508f274685a25ff19dbe5a9ff54790`，
保留原始 Git 历史、版权和 [INIT](upstream/INIT.md) 中的原始 message。
EmBT 独立维护，不整体 merge/rebase 到 Zephyr main。
手动同步入口为 `tools/upstream/sync.py`。当前先验证人工选定的小批次，不启用定时任务。

## 筛选与导入

每次先固定 EmBT 起点与上游检查终点 SHA，扫描 BLE/BR、保留协议和实际依赖。
原生 tests.yaml/prj.conf 只作为适配输入，目标为当前 test.json/config.conf；设备树输入排除。
检查适用性、已有修复与前置补丁，分别记录采用、适配、延期和拒绝。

旧路径如 `subsys/bluetooth/host` 对应当前 `bluetooth/host`，公共头对应 `include/embt`；
`lib`、基础机制和 namespace 改造需要逐补丁核对。不能直接拷贝整份文件覆盖改造后的源码。
按冻结扫描清单的原始顺序使用 `cherry-pick -x` 或可追溯的适配导入；单独核对语义和
上下文依赖，保留作者、日期、完整 message、
段落/trailers、原始 SHA 和有效源码 comments。EmBT 适配说明独立追加，保留原文。

检查游标、已应用到任务分支、已合入 main 分别记录。扫描到某个 SHA 不表示全部采用；
失败/延期不推进已应用状态。用 SHA、patch-id 与当前代码效果共同检查重复补丁。

## 验证与时间批次

先得到代码提交的完整 EmBT SHA，再提交 `docs/upstream/YYYY-MM-DD_HHMMSS.md`。
文件名按 Asia/Shanghai，正文包含带时区时间；冲突时增加批次序号。
一次多补丁按实际应用顺序列出，记录提交与代码提交分开。

每批说明必须包含：

- 来源仓库、上游分支、检查范围完整 SHA、EmBT 起点、工作分支和 PR/集成状态。
- 原始完整 SHA、原标题、作者/日期、采用方式、EmBT SHA 及前置依赖列表。
- 每个原始 commit message 的完整原文代码块，包括正文与 trailers。
- 原始源码 comments 和版权保留情况；PR/review 讨论出处链接。
- 原路径到 EmBT 的映射、配置/API/行为差异、未引入项及原因。
- 实际配置、依赖、构建/测试/运行命令和结果；日志/HCI 证据及未验证范围。

原始 comments 指完整 commit message 和源码注释，讨论保留链接。
已发布记录更正通过明确的追加说明，不改写历史结果；尚未集成的本地 SHA 重写后
同步修正其对应列表，避免记录只指向旧提交。

```markdown
# YYYY-MM-DD HH:MM:SS +08:00 - Update batch

Source repository / branch:
Upstream range (full SHA):
EmBT starting SHA / work branch / PR:
Integration state:

| Order | Upstream SHA | Original title | Author/date | Method | EmBT SHA | Dependencies |
| --- | --- | --- | --- | --- | --- | --- |

## Original messages and discussion references
## EmBT adaptation and path mapping
## Validation and limitations
```

新文件使用 [完整许可模板](../LICENSE_HEADER_TEMPLATE.md)。初始源码核验见
[INIT](upstream/INIT.md)，测试基线见 [测试来源](upstream/BLUETOOTH_TEST_BASELINE.md)。

## 发布与试运行边界

只有用户明确授权才发布工作分支/PR。社区补丁同样遵守 [AGENTS.md](../AGENTS.md)：
不直接更新集成分支、不自动合入；最终由用户 review 和操作。
origin 指 EmBT，upstream 只用于获取补丁，不恢复 fork 关系或同步 main。

先手动选择 1～3 个真实补丁，验证来源、路径适配、真实执行、重复拒绝与失败状态。
没有合适补丁时记录空批次，不制造修改。定时调度、自动语义适配和自动发布仍未实现。

## 当前手动入口

```sh
python3 tools/upstream/sync.py scan --output /tmp/embt-sync/scan.json
python3 tools/upstream/sync.py prepare --scan /tmp/embt-sync/scan.json \
  --commit <full-upstream-sha> --output /tmp/embt-sync/plan.json
python3 tools/upstream/sync.py apply --plan /tmp/embt-sync/plan.json \
  --patch <full-upstream-sha>=<reviewed-adapted.patch> --output /tmp/embt-sync/applied.json
python3 tools/upstream/sync.py verify --suite <unit> \
  --build-root /tmp/embt-sync/build --output /tmp/embt-sync/tests
python3 tools/upstream/sync.py report --plan /tmp/embt-sync/applied.json \
  --verification /tmp/embt-sync/tests/verification.json \
  --output docs/upstream/<YYYY-MM-DD_HHMMSS>.md
python3 test/upstream/test_sync.py
```

以上占位参数必须替换为本轮完整 SHA、已 review 的最小适配 diff 和实际测试单元。
先通过 Git fetch 固定 upstream/main；入口本身不下载依赖或调用 agent 改写代码。
scan 只扫描首批映射范围；未映射依赖列为 review，不能自动导入。apply 不在 main 上
写文件，校验冻结的 HEAD、目标路径和补丁上下文；当前输出只记录本地状态，不推进 main
的已合入游标。重复补丁通过反向上下文检查报告 already_present，不能据此替代人工效果审查。

每个 patch 的处理结果单独保存；后续 patch 失败时保留已完成的本地前缀，明确 blocked，
不自动重置用户或其他 agent 的内容。schema 2 dry-run 在独立 Git index 中累计应用
所选依赖补丁，真实 index 与工作区保持不变。verify 调用现有配置/执行 gate，要求新生成的非空 JUnit；
源码指纹绑定测试输入，report 拒绝源码已经变化的成功报告。构建、源码 provider 和完整
配置矩阵仍需明确运行、记录；普通 unit 验证不自动代表全部 profile 或硬件已通过。

## 完整评估与部分摘取

新的入口是 `tools/upstream/review.py`。脚本负责固定输入、生成完整清单、校验范围与源码、
累计检查补丁；agent 负责逐条判断实际适用性并适配源码。路径命中或 `git apply` 成功
都不能替代适用性评估。

```sh
python3 tools/upstream/review.py --repo <worktree> scan \
  --from <reviewed-upstream-sha> --to <frozen-upstream-sha> --output <artifacts>
python3 tools/upstream/review.py --repo <worktree> prepare \
  --scan <artifacts>/scan.json --review <artifacts>/review.json --output <plan.json>
```

`review.json` 必须覆盖每一条扫描提交，给出 NEED、PARTIAL、ALREADY、PENDING、DEFER
或 EXCLUDE；需要摘取的条目还必须有当前源码依据、适配与测试计划、明确的目标路径。
每个未摘取的原始路径都要解释原因；附加范围可以包含经过评估的 Host 测试，以及修复直接
依赖的现有、已跟踪 provider 文件。必须记录依赖理由，不能恢复已退役的 provider。
目标 HEAD 或源码 fingerprint 改变后，要重新绑定评估；prepare 之后的源码变更也会在
apply 前被拒绝。

Controller 及依赖其 LLL/radio HAL 的测试都排除，不能只看目录名称；例如
`df/connectionless_cte_chains` 仍属于 Controller 内部测试。已经退役的 USB PCM 与
Flash BLOB adapter 不恢复。保留 Mesh/Audio 源码，同时评估其中实际需要的修复。

部分摘取必须区分行为修复与架构调整；不能因为修复需要同步，就顺带换成全局锁，
也不能因为 shell 拆分补丁包含修复，就整条搬入。work 的取消不代表 GATT 请求/回调
已经结束；released 回调允许释放内存时，也必须检查 worker 的后续状态更新。
拿不定的所有权或设计边界留在 DEFER，并把具体问题交给用户决定。

准备后的 schema 2 plan 使用现有 `sync.py apply`。`--dry-run` 在独立 Git index 中
逐条累计应用依赖补丁，工作区和真实 index 保持不变。实际 apply 后仍只修改本地工作
树，不自动 commit、push、创建 PR 或合入。提交授权与评估结论是不同的事情。

新增配置场景必须声明 required cases。验证必须有本次真实执行的非空 JUnit；退出码 0、
collect-only、全部 skip、旧 XML 或与当前源码不匹配的 fingerprint 都不能通过。
默认全 C、配置变体与 Classic/Bumble 分开执行，OSI/机制与可选功能编译也分别记录。

完整操作规则见 [EmBT upstream skill](skills/upstream-sync/SKILL.md)。定期任务由 agent
按 skill 调用脚本完成评估和适配；脚本单独扫描返回 `review_required` 时，整个同步
周期尚未完成。没有新提交返回 `no_new_commits`，不应生成空的摘取批次。

复用历史记录中的已评估 tip 时，还要重新查看 deferred backlog；已评估 tip 不能当成
全部补丁已经合入的 cursor。原始 baseline、已评估区间、实际导入 SHA 和待办分别保留。
定时任务的频率、起点以及本地提交/发布权限需要明确设置；这轮不自行开启发布或合入。

## 按原顺序重建和所有权判断

本地尚未集成的批次补齐后，按冻结清单的原始序号重建全部已选社区提交；原消息、作者、
日期和 SHA 保留，EmBT 说明追加在原消息之后。部分摘取保留原完整消息并明确子集。
先前试运行的 SCO 提交也放回原位置；工具和独立本地修正分别列出，不混入社区出处。
重建后核对每条实际路径和最终源码指纹，更新同批次记录/LATEST，保持代码与文档分支分离。

基线已有的 RTX 动态释放问题不等于同模块所有独立修复都不可摘取。分别评估新 status
work 的 owner/cancellation、disconnect 重试及 SDU shutdown 的直接影响，并单列遗留
生命周期待办。work 完成不是 GATT/CCC 请求完成；独立参数、conn 引用和真实 terminal/
断链完成边界仍需逐项核对。避免用取消 work 或 native provider 内部 flag 代替请求释放。

测试不能依赖 cooperative priority 自然维持资源耗尽或 FIFO 发布后的完成顺序。
需要持有 transport PDU 的场景显式持有并释放，等待 fixture 自己的工作完成，不对仍有
生产者的整条 queue 做 drain。保留原协议断言、失败日志及实际重复结果，不改生产优先级、
加 wrapper/全局 Host barrier 或伪造成功应答。当前完整批次见 LATEST 指向的 upstream 记录。
