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

# EmBT 文档

当前工作是文档与架构整理；代码收尾、运行验收、合入准备和社区同步已记入
[当前状态与待办](STATUS.md)，暂不执行。源码基线、最近测试和未完成范围均在状态页集中维护。

| 阅读目的 | 入口 |
| --- | --- |
| 理解目录、模块、线程和资源所有权 | [架构](ARCHITECTURE.md) |
| 配置、构建库或启动 shell | [构建指南](BUILDING.md) |
| 理解 OSI/base、时间、内存与平台适配 | [移植指南](PORTING.md) |
| 运行测试、理解 skip 和模拟/实机边界 | [测试指南](TESTING.md) |
| 查看完成度、已知问题和延期任务 | [状态与待办](STATUS.md) |
| 选择性引入 Zephyr 补丁并保存来源 | [上游维护](UPSTREAM.md) |
| 了解开发、许可和人工提交/合入规则 | [贡献指南](CONTRIBUTING.md) |

具体规范：[头文件](HEADER_INCLUDE_RULES.md)、[atomic](ATOMIC_DESIGN.md)、
[OSI API](reference/OSI_API.md)、[OSI 契约测试](OSI_CONTRACT_TESTS.md)、
[代码注释](CODE_COMMENT_STYLE.md)。

源码来源见 [INIT](upstream/INIT.md)，测试来源和保留范围集中记录在
[测试基线](upstream/BLUETOOTH_TEST_BASELINE.md) 与
[测试来源清单](upstream/BLUETOOTH_TEST_CASES_F8DA8EA.json)。移植过程归档和每目录
PROVENANCE 不进入交付；当前状态和规范由上述主文档维护。
