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

# EmBT

[English](README.md)

EmBT 是一个独立维护的项目，用于从 [Zephyr](https://github.com/zephyrproject-rtos/zephyr)
中提取蓝牙 Host 协议栈，并进行移植和维护。

## 代码来源

初始代码来自 Zephyr 的 `main` 分支，基线提交为
`f8da8ea370508f274685a25ff19dbe5a9ff54790`。原始 Git 历史和上游版权声明均保留。
详细来源见 [初始来源记录](docs/upstream/INIT.md)。

后续选择性引入有价值的社区蓝牙补丁，并记录原始提交说明和 SHA。

## 当前状态

目录提取、头文件迁移、独立构建与 POSIX 适配已在本地工作分支实现，完整 M1 无线验收仍待完成。
当前先整理文档与架构，代码收尾和合入暂缓。阅读入口见 [文档索引](docs/README.md)，
最新状态与待办见 [STATUS](docs/STATUS.md)。

## 许可证

[Apache-2.0](LICENSE)。导入文件保留原始版权和许可声明。
