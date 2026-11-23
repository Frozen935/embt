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
当前仅保留流程约定，实际同步工具和新增补丁试验在 [STATUS](STATUS.md) 中暂缓。

## 筛选与导入

每周固定一个评估窗口，具体日程待后续确定。本轮不创建任务、fetch 或导入代码。
之后执行时先固定 EmBT 起点与上游检查终点 SHA，扫描 BLE/BR、保留协议和实际依赖。
检查适用性、已有修复与前置补丁，分别记录采用、适配、延期和拒绝。

旧路径如 `subsys/bluetooth/host` 对应当前 `bluetooth/host`，公共头对应 `include/embt`；
`lib`、基础机制和 namespace 改造需要逐补丁核对。不能直接拷贝整份文件覆盖改造后的源码。
按依赖顺序使用 `cherry-pick -x` 或可追溯的适配导入，保留作者、日期、完整 message、
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

后续试运行选择 1～3 个有价值且依赖明确的补丁，验证来源、路径适配、真实执行、
重复拒绝与失败状态。没有合适补丁时记录筛选结果，不为演示制造修改。
试运行通过后再固化 tools/upstream 和每周流程；本次文档整理不实施这些待办。
