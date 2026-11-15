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

# C/C++ 注释规范

新写或实质修改的内部/公开接口使用 Doxygen；源码、头文件和 agent-authored review
comments 使用英文。保留原上游版权及 comments，不为规范化批量重写导入文件。
风格仅作用于本次改动，不混入无关空行、注释位置或格式变化。

## 文件与接口

新文件使用 [完整 Apache 模板](../LICENSE_HEADER_TEMPLATE.md)，按真实年份和版权主体填写。
文件说明使用 `@file`、`@brief`；实现中的理由/约束使用普通 `/* ... */`。
不要把已有 header notice 换成示例公司或把短说明充当完整许可。

函数说明先写一句功能，再给真实合同：

| 标签 | 内容 |
| --- | --- |
| `@brief` | 功能，而不是复述函数名称 |
| `@param[in/out/in,out]` | 方向、单位、有效范围、NULL 与存储所有权 |
| `@retval` / `@return` | 成功、错误或返回值；区分状态、count、旧值和指针 |
| `@pre` / `@post` | 初始化、持锁、调用先后与可核实状态 |
| `@par Context` | 线程/callback/ISR、是否阻塞和调用限制 |
| `@par Lifetime` / `Ownership` | 借用、引用转移、归还、回收及完成屏障 |
| `@note` / `@warning` | 不直观且真实的限制，不能代替正常合同 |

只写适用的内容，不给每个简单 helper 堆空标签或重复实现步骤。

```c
/**
 * @brief Wait for termination and release joinable resources.
 * @param[in,out] thread Valid created and unjoined handle.
 * @retval 0 Termination was confirmed.
 * @return A negative error code on failure.
 * @pre Serialize lifecycle operations and do not join the calling thread.
 * @par Lifetime
 * Release the entry context only after a successful join.
 */
int osi_thread_join(os_thread_t *thread);
```

## 类型、回调与实现

struct/enum/callback 说明用途和合同；字段注释可用 `/**< ... */`，尤其单位、所有权和限值。
宏注明参数求值、副作用、类型要求与可用上下文；无需为每个常量重复数字。
回调明确由谁调用、可否重入/阻塞，以及参数能保留多久。
并发相关接口注明调用方的锁/顺序要求，不能用“线程安全”覆盖条件性的保证。

实现注释解释为什么、协议约束或不变量，避免逐行翻译代码。
修改行为同步维护受影响注释，不留下错误的 tick、owner、init 或释放说明。
检查声明和实现合同一致；日志/注释不会让未验证范围自动变成完成。
