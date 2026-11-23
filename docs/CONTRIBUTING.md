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

# 开发与贡献

仓库政策以 [AGENTS.md](../AGENTS.md) 为准。本页只提供入口，不复制一套相互漂移的规则。
当前优先文档整理；代码收尾、验收与发布待办见 [STATUS](STATUS.md)。

## 范围与规范

保留源自 Zephyr `subsys/bluetooth` 的非 Controller 内容，明确删除例外按已确认范围处理。
feature 未进入首期运行不授权删除源码。导入保留版权、许可、原始消息和来源。
新文件使用 [完整许可模板](../LICENSE_HEADER_TEMPLATE.md)，按实际年份和主体填写；
项目根 [LICENSE](../LICENSE) 为 Apache-2.0，三方依赖保留自己的许可，不批量改写上游头。

C/C++ 沿用 Zephyr 风格；注释使用 [Doxygen 规范](CODE_COMMENT_STYLE.md)。
引用、配置注入、类型归属和内部接口合同见 [头文件规则](HEADER_INCLUDE_RULES.md)，atomic 见
[内部接口规范](ATOMIC_DESIGN.md)，平台接口见 [PORTING](PORTING.md)。
只改任务需要的语义与必要依赖，不混入格式、空行和日志位置的无关改动。

## 本地工作与交付

使用任务分支。新的改动默认留本地，不暂存、commit 或推送，直到用户明确授权该范围。
同一未合入板块继续更新原 patch/PR；文档和政策归文档板块，代码保持单一职责和有序依赖。
已合入历史不再改写；后续修复使用新的任务提交。

commit/PR 的标题、正文及 agent-authored code/review comments 使用英文。
提交说明解释目的、改动/依赖、实际验证和未验证范围。项目说明文档可以使用中文。
提交前检查路径与 scope，显式 stage；构建产物、下载源码、本地 store 和 IDE 文件不提交。

所有 bring-up PR 保持 draft，直到已约定运行基线验证并由用户决定解除 hold。
Agent 不直接更新集成分支、不合入、不启用 auto-merge，也不以提交授权代替集成授权。
最后 review 和 merge 由用户操作；当前文档任务不准备代码合入或更新远端 PR。

验证选择实际受影响的配置和行为，遵循 [TESTING](TESTING.md) 的执行口径。
源码 presence、静态库生成、模型运行、skip 和未执行均不能写成完整功能验收。

## 社区补丁

每周选择性评估 BLE/BR 与实际依赖，不整体跟随上游 main。补丁 provenance、
适配差异和时间批次见 [UPSTREAM](UPSTREAM.md)。实际工具和首轮同步仍待用户安排。
