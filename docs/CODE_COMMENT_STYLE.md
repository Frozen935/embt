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

# C/C++ 代码与头文件注释规范

状态：本地 review 草案，2026-11-15。本规范定义后续新增和修改注释的统一格式。
该格式已应用于 osdep 内部接口与 POSIX 类型头；未批量改写导入 Zephyr 源码。
后续 cmocka 测试源码同样遵循本规范，原 demo 已按要求删除。

采用 Zephyr 的 C 注释习惯与标准 Doxygen 标签。说明文档可以使用中文，
所有新增的源码注释、头文件注释和本文代码示例使用英文。

## 1. 适用范围与基本原则

- 适用于项目自行维护的 C/C++ 源码和头文件，包括 osdep 内部接口；内部接口同样需要说明契约。
- 新增接口、类型和修改了行为的接口按本规范记录。保留未改变的上游注释、copyright 和来源信息，
  不为了格式统一批量重写导入代码或第三方代码。
- API 声明处是接口契约的权威位置；实现处解释算法、限制或操作顺序，避免复制整份契约。
- 注释解释用途、约束及原因，不逐句翻译代码，不描述已经删除的行为。
- 注释不能承诺实现没有提供的检查、同步、资源回收或执行顺序。
  未实现的 API 草案必须由设计文档说明状态，不能把计划当作已经验证的行为。
- 按需要记录阻塞、线程上下文、资源寿命和回调语义，不要求每个简单函数填写全部章节。

## 2. 语法与排版

| 用途 | 统一形式 |
| --- | --- |
| 文件、函数、类型、宏的文档注释 | `/** ... */` |
| 简短字段、枚举值的尾随文档注释 | `/**< ... */` |
| 函数内部的实现说明 | `/* ... */` |
| Doxygen 标签前缀 | 统一使用 `@`，不在新增注释中混用反斜杠标签 |

函数及其他多行文档块使用下面的排版：

```c
/**
 * @brief Perform a clearly described operation.
 *
 * Explain additional constraints only when they help the caller.
 */
```

- 每行最多 100 列，按 8 列 tab 计算；代码缩进沿用 Zephyr。
- 注释块和被注释的声明保持相同缩进；`*` 对齐，`*` 后留一个空格。
- 标签与参数名、说明之间使用单个空格，不靠大量空格把描述对齐成表格。
- 长描述换行时在 `*` 后比普通文本多缩进两个空格；段落之间使用空白 `*` 行。
- 简短字段可以使用一行 `/**< ... */`；超过行宽时改用字段前的多行块。
- 英文说明使用清楚的句子并以句号结束；`@brief` 简述用途，不重复函数名字。
- 不新增 `//`、`///`、`//!`、`/*!` 等另一套注释形式，不使用装饰性分隔线代替接口说明。
- 使用标准标签，不引入尚未定义的 Zephyr 自定义标签，如 `@kconfig`。
  配置条件可以用普通文字与宏名称描述。现有上游自定义标签按来源保留。

## 3. 文件说明与许可证

新 C/C++ 文件和头文件首先使用[统一许可证头模板](../LICENSE_HEADER_TEMPLATE.md)，
紧接着放文件级 Doxygen 说明。头文件说明位于 include guard 之前：

```c
/**
 * @file
 * @brief OSI thread operations for internal stack components.
 *
 * Provide the thread lifecycle and identity operations used inside EmBT.
 */
```

该示例只展示文件说明，不替代完整 Apache-2.0 banner。
许可证通知和 Doxygen 文件说明分开；不在 Doxygen 中重复 copyright、license、作者、日期或 Git SHA。
文件级描述说明本文件职责，不罗列每个函数，也不把内部头文件称为应用公共接口。

## 4. 函数文档

### 4.1 哪些函数需要文档

- 头文件中的项目 API 声明必须有紧邻的 Doxygen 注释，包括 osdep 内部 API。
- 非 static 函数如果没有头文件声明，在定义处写完整 Doxygen 注释。
- 行为非直观的 static 函数，在声明或定义处记录必要的用途、参数和结果。
  极短且用途直接的局部 helper 不强制套完整 API 模板。
- 同一函数在声明和定义处不重复维护契约。实现需要关联说明时可用 `@copydoc`，
  但通常只补普通实现注释即可；不要求每个定义都有重复的文档块。

### 4.2 标签顺序和必需内容

按以下顺序组织一个函数文档；不适用的部分省略：

1. `@brief`：必需，一句话说明操作。
2. 详细描述：补充行为，简单函数可以省略；`@details` 非必需。
3. `@param`：每个参数都记录，顺序和函数声明一致。
4. `@retval` / `@return`：非 void 函数必需，描述结果或错误。
5. `@pre` / `@post`：重要的调用前提或成功后保证。
6. `@par`：按需说明上下文、阻塞、所有权和回调。
7. `@note` / `@warning`：补充说明或具体的危险使用方式。

参数方向统一使用 `@param[in]`、`@param[out]`、`@param[in,out]`：

- 描述实际被读取或修改的数据，不根据参数是否带 const 机械判断。
- 参数名必须与声明完全一致。`void` 参数列表没有 `@param`。
- 数值参数写清单位、范围及特殊值；pointer 参数写清是否允许 NULL。
- 输出参数说明在失败时保持不变、清零，还是可能已经产生部分结果。
- callback/context 参数按需要说明数据借用及寿命，不能只写“callback”或“argument”。

### 4.3 标准状态函数示例

下面按当前 mutex API 草案展示格式，不代表 POSIX 后端已实现或测试：

```c
/**
 * @brief Acquire a mutex.
 *
 * @param[in,out] mutex Initialized mutex to acquire; must not be NULL.
 * @param[in] timeout_ms Relative timeout in milliseconds. Use OS_TIMEOUT_NO_WAIT
 *   for a nonblocking attempt or OS_TIMEOUT_FOREVER for an indefinite wait.
 *
 * @retval 0 The mutex was acquired.
 * @retval -EAGAIN The mutex was unavailable during a nonblocking attempt.
 * @retval -ETIMEDOUT A finite wait expired.
 * @return A negative error code for other failures.
 *
 * @pre The caller must not recursively acquire a normal mutex.
 * @post On success, the caller owns the mutex and must unlock it.
 *
 * @par Context
 * Thread context. This operation may block according to timeout_ms.
 */
int osi_mutex_lock(os_mutex_t *mutex, os_timeout_t timeout_ms);
```

### 4.4 返回值要求

- 状态函数用 `@retval 0` 和已经定义的主要错误值，必要时用 `@return` 补充其他 backend 错误。
  不机械复制所有 errno，不承诺对非法生命周期一定返回某个错误。
- 返回数值、pointer 或 bool 的函数用 `@return` 说明含义；离散特殊值可以补 `@retval`。
- 返回 pointer 的函数说明 NULL 情况以及释放方式；返回 count/时间的函数写清单位和快照性质。
- void 函数不写 `@return void` 或“always succeeds”。不返回的函数明确注明不会返回。
- 提交异步操作或取消请求时，区分“请求被接受”和“操作已完成”。
- 不声称 join 有 timeout，不声称 cancel 成功意味着线程已退出，
  不声称 native timer delete 成功意味着 callback 已结束。

pointer 返回示例：

```c
/**
 * @brief Allocate memory from the OSI system heap.
 *
 * @param[in] size Number of bytes to allocate.
 *
 * @return Allocated memory on success, or NULL for a zero size or allocation failure.
 *
 * @par Ownership
 * The caller releases the returned allocation with osi_free().
 */
void *osi_malloc(size_t size);
```

## 5. 上下文、生命周期与回调

只有接口涉及对应约束时才增加这些标准 `@par` 段落：

| 段落 | 记录什么 |
| --- | --- |
| `@par Context` | 允许的执行上下文，能否阻塞或从指定回调调用 |
| `@par Concurrency` | 同对象并发调用、序列化责任及 callback 是否可能重叠 |
| `@par Ownership` | 参数和结果是借用还是转移，谁分配、谁释放 |
| `@par Lifetime` | storage/context 必须活到什么事件，是否允许复用 |
| `@par Callback` | callback 执行位置、触发条件及完成含义 |

这些是标准 `@par` 的标题，不是自定义 Doxygen 标签；不要求每个函数都写五段。

明确区分调用前提与实现保证：例如“caller 在销毁前确保没有 waiter”是前提，
不能改写成“函数会检查所有 waiter”。完整类型对内部源码可见，也不意味着 caller 能任意读写 native 字段。

针对当前 OSI 草案：

- start 的 POSIX 空操作要写明，不能让使用者以为 create 后还不会执行。
- cancel 是请求，join 才确认 native termination；业务资源清理不能省略。
- semaphore reset 只在无并发使用者时清空，不描述成会取消 waiter。
- timer callback 可能重叠；stop/delete 都不是 callback completion barrier。
- 接口属于设计草案时，在设计文档标明状态；后续注释跟随最终实现同步更新。

## 6. 类型、字段、枚举与 callback

- API 类型使用 Doxygen 记录用途；有资源寿命或复制限制时一并说明。
- 每个对调用者有意义的结构字段、枚举值都有简短说明。
- 实现私有字段只说明不直观的用途，不为每个简单局部变量写 Doxygen。
- 注释不能授予 caller 修改内部 native 对象的权利。

结构体及字段示例：

```c
/**
 * @brief Native thread handle used by internal OSI callers.
 *
 * Keep the handle valid until osi_thread_join() succeeds.
 */
typedef struct os_thread {
	pthread_t native; /**< Native POSIX thread identity. */
} os_thread_t;
```

枚举示例：

```c
/** @brief Mutex ownership behavior selected during initialization. */
typedef enum {
	OS_MUTEX_NORMAL, /**< Non-recursive mutex ownership. */
	OS_MUTEX_RECURSIVE, /**< Recursive ownership with matching unlock operations. */
} os_mutex_kind_t;
```

callback typedef 同样记录参数及执行约束：

```c
/**
 * @brief Publish a native timer expiration notification.
 *
 * @param[in] timer Timer that produced this notification; must not be NULL.
 * @param[in,out] argument Borrowed caller context; may be NULL if the callback accepts it.
 *
 * @par Context
 * Native POSIX notification thread. Callback invocations may overlap.
 *
 * @par Lifetime
 * The timer and context outlive every possible notification, including callbacks
 * dispatched before native timer deletion. Only publish a short notification here.
 */
typedef void (*os_timer_cb_t)(os_timer_t *timer, void *argument);
```

## 7. 宏及文档分组

公开或跨模块使用的宏说明用途、单位和特殊值；函数式宏使用实际参数名记录 `@param`，
涉及重复求值、类型约束或副作用时明确说明。简单常量不强制增加不存在的参数或返回值。

```c
/** @brief Request a nonblocking attempt from an OSI wait operation. */
#define OS_TIMEOUT_NO_WAIT INT64_C(0)
```

按实际需要使用 `@defgroup`、`@ingroup`、`@{` 和 `@}`，不强制为每个文件建立分组。
`@ingroup` 和 `@ref` 引用的目标必须存在，分组括号必须配对。
不为了生成文档新增协议行为或 general compatibility layer。

## 8. 实现注释

实现中使用普通 `/* ... */`，放在需要解释的语句或代码块前，重点解释约束和原因：

```c
/* Serialize the value check and post so concurrent givers cannot exceed the limit. */
```

避免仅重复操作名称的注释，例如“Lock the mutex”紧挨着 mutex lock 调用，
以及每行一个无实际信息的注释。复杂并发顺序、错误回滚或资源转移需要写明原因。
TODO/FIXME 只记录实际未完成事项，尽量指出具体条件或已存在的问题链接，
不用泛泛的“improve later”代替设计或错误处理。

## 9. 维护和检查

新增或修改代码时检查：

1. 文件说明、函数/类型文档、实现注释使用了对应形式，源码注释为英文。
2. 参数名称、方向、单位、NULL、输出失败行为与声明及实现一致。
3. 返回值、阻塞、异步完成和资源寿命没有超出实现保证。
4. 声明处契约与实现说明没有复制两份需要同步维护的文本。
5. 行宽、换行、空白、引用和分组符合本规范；上游无关注释没有被批量改写。

有 Doxygen 环境时可检查实际提取结果和警告；编译器通过不能证明 Doxygen 文档正确。
当前环境没有可用的 doxygen 命令，本轮检查 Markdown 链接、代码示例语言和行宽，
未生成 Doxygen HTML/XML，也未新增 Doxyfile 或构建 target。
后续注释整理按明确任务范围进行，文档按现有文档 PR 规则提交；当前改动留在本地。
