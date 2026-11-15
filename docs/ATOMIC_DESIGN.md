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

# 内部 atomic 接口

单一内部接口头为 `include/embt/base/atomic.h`，使用 `bt_atomic_*` 类型/函数与
`BT_ATOMIC_*` 宏，不提供旧名称 alias。头位于共享 include，但操作仍是内部合同。
atomic 属于 base 的编译器基础工具，不放入 OSI 或引入带 owner/mutex 的包装对象。

## 保留语义

- 保持 native long/pointer 宽度、自然对齐和现有 buffer/ref word 布局。
- inc/dec/add/sub/set/位运算返回旧值；CAS 为 strong CAS，返回成功状态。
- 位图 test-and-set/clear 返回原状态，跨 native word 的操作保持原语义。
- 排序使用 sequentially consistent，不能因未来单线程目标换成普通读写。
- 常量初始化只设置存储，不创建 OS 资源；检查所需 native lock-free 能力。
- 不静默引入 libatomic/线程锁 fallback，不用固定 atomic_int 代替依赖 long 布局的字段。

## 公开业务边界

公开 buffer/connection/L2CAP 保留必要 storage 大小/对齐，操作在库内实现。
公开存储不是供应用直接原子读写的接口；通过业务 API 管理引用、状态和回收。
当前 L2CAP status 保留原指针 ABI，提供 `bt_l2cap_chan_status_get` 读取接口。
更换快照 callback 或公开参数语义属于单独接口设计，不在改名时顺带实施。

public header 不依赖私有 atomic include 路径。目标切换时核对 sizeof/alignment/offset、
旧值/CAS、指针、位图边界、并发计数和 C/C++ consumer；不能把 64 位主机成功当成目标 ABI 验证。
当前状态见 [STATUS](STATUS.md)。
