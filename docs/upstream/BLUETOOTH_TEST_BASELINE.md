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

# Bluetooth 测试基线

测试基线和来源信息集中维护在本目录，不在各测试目录新增 PROVENANCE 文件。
移植过程归档仅在本地保存，不作为项目交付文档。

| 字段 | 值 |
|---|---|
| 来源仓库 | https://github.com/zephyrproject-rtos/zephyr |
| 来源分支 | main |
| 原始完整 SHA | f8da8ea370508f274685a25ff19dbe5a9ff54790 |
| 原始测试目录 | tests/bluetooth/ |
| 原始导入后的路径 | bluetooth/tests/bluetooth/ |
| 当前测试目录 | test/bluetooth/ |

原始源码起点及完整 commit message 见 [INIT](INIT.md)。既有测试来源清单见
[BLUETOOTH_TEST_CASES_F8DA8EA.json](BLUETOOTH_TEST_CASES_F8DA8EA.json)；该清单用于追溯
来源及 case 标识，实际编译/执行清单由测试程序和当前 runner 输出确定。原始目录的保留
决定见 [测试范围清单](BLUETOOTH_TEST_SCOPE.json)；该清单记录范围，不表示当前 case 已通过。

## 保留和适配边界

先保留选定的原始测试，再通过目录提交迁移，随后适配 cmocka、配置和 provider，
使源码差异能通过 Git 历史 review。原生描述、数据、断言、注释和适用版权应保留；
新增模型、断言边界与测试接口改动单独说明，不能因原始文件存在而宣称 case 通过。

Controller、原始 driver/ISR、board/YAML/overlay 和已退休 Flash provider 测试按已确认
范围排除。Classic 使用真实 EmBT 测试程序配合 Bumble Controller/peer 模型；它不证明
实机无线互通。当前命令、执行检查和延期范围见 [测试指南](../TESTING.md)，结果和待办见
[STATUS](../STATUS.md)。

两处需要保留的特别说明：

- `tests/bluetooth/hci`：原始十个 driver extended-advertising report case 及复制的 driver
  UUT 已按用户要求删除，Host HCI error-string case 保留；原始内容仍可在上游 Git 中查看。
- `tests/bluetooth/monitor`：原始三个 case 保留短名称和 payload 检查，suite 从
  `bt_monitor_irq` 改为 `bt_monitor`。测试使用二进制内存/文件 sink，生产编码和丢包计数
  仍是被测实现；新增检查包括重复 sink 拒绝、超过 255 的丢包计数和临时文件输出。

后续社区导入按 [上游维护规则](../UPSTREAM.md) 记录完整 SHA、原始 message、路径映射、
适配差异和实际验证；原始 case 说明及注释不因集中来源记录而删除。

## 测试依赖来源

FFF 来自同一 Zephyr SHA 的 `subsys/testsuite/include/zephyr/fff.h`，用于提供 typed fake。
保留其原始源码和 MIT 许可；cmocka 负责执行用例、断言和结果报告。该依赖不进入生产库。
