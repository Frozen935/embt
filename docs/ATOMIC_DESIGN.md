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

# 内部 Atomic 适配设计

状态：2026-11-15，按用户最新要求修订的本地设计。atomic 使用 bt_atomic 前缀，属于内部基础工具，
不再将旧 atomic_* API 作为项目对外接口。私有头及协议调用迁移已在本地实现，原公开 atomic 头已退役。

## 1. 命名与位置

| 项目 | 设计 |
| --- | --- |
| 函数 | bt_atomic_get/set/inc/dec/cas/ptr_cas 等 bt_atomic_* |
| 类型 | bt_atomic_t、bt_atomic_val_t、bt_atomic_ptr_t、bt_atomic_ptr_val_t |
| 宏 | BT_ATOMIC_INIT、BT_ATOMIC_PTR_INIT、BT_ATOMIC_DEFINE 等 |
| 内部头 | base/include/base/bt_atomic.h，内部使用 <base/bt_atomic.h> |
| 实现 | 优先单个 header 的 static inline 与 GCC/Clang __atomic builtin |
| 测试 | test/base/test_atomic.c，接入现有 cmocka/CTest |

首期不拆多套 backend，不增加 atomic.c 或空 base/atomic 目录，不建立原 atomic_* 名字的兼容别名。
编译器 intrinsic 的 __atomic_* 名字是 compiler API，不重命名；重新定义的是项目的接口层。
如借鉴导入实现，保留原版权和许可，新注释符合项目 Doxygen 规范。

## 2. 内部可见性

base/include 仅向库内部实现和测试 target 提供，不放进对外 include/embt，不安装、不向
EmBT::embt 消费者导出。函数保持 static inline，不提供应用调用的 atomic 动态/全局符号。
public 头不能 include <base/bt_atomic.h>，也不能通过 umbrella 间接带出该头或宏。

改前缀、改变 include 可见性和处理 public 头暴露点是同一内部化目标的不同步骤。
只重命名函数或隐藏 linker symbol，不能证明应用已经不依赖 atomic。

## 3. 现有公开依赖与处理边界

| 当前暴露点 | 内部化方向 |
| --- | --- |
| conn.h 的 inline bt_conn_take 调用 atomic_ptr_clear | 保留业务 API 名称和行为，将 atomic 操作移入库内实现，public 头只保留业务声明 |
| net_buf.h 的 inline pool/指针 helper 调用 atomic | 保留业务能力，通过库内实现完成读取/交换，不把私有 atomic 头带给应用 |
| net_buf 的 ref_word、pool avail_count 等 atomic_t 字段 | 保持 native long 的大小、对齐和位布局；对外可保留底层存储，标明内部使用，不公开 atomic 类型/操作 |
| l2cap.h 的 atomic 字段和 ATOMIC_DEFINE 数组 | 保持必要的存储大小/对齐，字段与计算不依赖私有 atomic 宏；具体修改按实际消费者核对 |
| l2cap ops.status 的 atomic_t * 参数 | 需要单独确认外部业务读取方式，如状态快照或业务读取接口；不能只改成 long * 后允许并发普通读取 |

公开数据存储不等于公开原子接口。禁止应用直接读写内部同步/引用计数字段；状态、引用和
buffer 生命周期通过相应业务 API 管理。无需为此引入带 mutex、owner 或状态机的 atomic 对象。

L2CAP 状态回调可能涉及业务接口契约，不能在 atomic 改名时自动改变参数类型、快照语义或 ABI。
本设计将它列为需要确认的边界；实现内部化时必须明确解决，而不是用 exported include 路径兜底。

## 4. 保留底层语义

- bt_atomic_t 的底层类型先保持 long，pointer 类型保持 void *；类型名更换不改变存储宽度。
- inc/dec/add/sub/set 和位运算返回旧值；CAS 返回成功状态并使用 strong CAS。
- 位图 test-and-set/clear 返回原状态，保留跨 native word 的操作。
- 首期继续使用 __ATOMIC_SEQ_CST，不因未来单线程目标减少 fence 或改成普通读写。
- 初始化宏只初始化数据，不创建 OS 资源，不需要新增运行期 atomic init 对象管理。
- 检查 target 的自然对齐和 native-width lock-free 能力，不静默引入 libatomic 锁或 pthread fallback。
- 不把 atomic_t 改成固定 atomic_int：net_buf 的现有 ref_word 明确依赖 long 及其 endian 布局。

## 5. 迁移与测试

1. 建立带前缀的单一私有 header，去掉旧 kernel/syscall/arch provider 选择和未定义的 CONFIG 分发。
2. 明确上述公开依赖的业务边界后，按模块迁移类型、宏和调用点，不保留原名 alias。
3. 在库内部使用目标依赖提供 base/include；public header 使用测试确认不会引入私有路径或名字。
4. 验证旧值、strong CAS、pointer、位图最高位/跨 word、并发计数/唯一 claim、数据发布和 C/C++ 编译。
5. 验证 public header 可在应用 target 条件下使用，不依赖 atomic 私有头；结构 sizeof/align/offset
   不因移除公开 typedef/宏发生变化。涉及业务 callback 的变化另行审查和测试。

当前已完成实现、native lock-free 检查和并发/位图测试，公开 C/C++ consumer 不依赖私有头。
L2CAP status 保留原指针 ABI，并增加 bt_l2cap_chan_status_get；快照回调签名仍待用户确认。
详细验证见 BRINGUP_IMPLEMENTATION_REVIEW.md；所有修改未提交或推送。
