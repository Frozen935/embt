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

# OSI 接口契约测试

测试只依据公开给协议栈内部使用的 osi_* 合同，辅助协调也使用 OSI、标准 C 与 cmocka。
不调用 pthread、系统调用、native 字段或其他原子 API 来证明 OSI 自己的行为。
具体 API/storage 见 [OSI 参考](reference/OSI_API.md)，运行命令见 [TESTING](TESTING.md)。

| 模块 | 项数 | 验证合同 |
| --- | ---: | --- |
| types | 1 | 签名、存储声明、timeout 宽度与特殊值，不断言 native layout |
| time | 3 | 单调 ms/us、sleep、单位转换边界与输出保留 |
| memory | 2 | 分配/清零/对齐/释放、零大小及乘法溢出 |
| mutex | 2 | 递归、互斥、不等待/有限 timeout、有效错误 |
| sem | 3 | count/limit、饱和 give、静止 reset、生产/消费 |
| cond | 3 | predicate、signal/broadcast、timeout 与返回持锁 |
| thread | 5 | create/start、创建/借用/复制句柄、身份、命名、yield、join 与 cancel 请求 |
| timer | 3 | 一次性通知、rearm/stop/remaining/参数错误 |
| io | 2 | console_write 状态/written 与有效格式化调用 |

共 24 项。这不是全部 branch/API 功能覆盖证明；`osi_fatal` 的不返回路径仅核查签名/链接，
`osi_vprint` 不检查 sink 内容，native OOM/系统调用故障/实际调度与栈属性不在纯合同 suite 中。

## 生命周期与断言

创建 worker 前初始化它使用的对象。ready/release 握手使用 OSI，结果存入其 context，
join 后由主测试线程断言。worker/callback 不做 cmocka longjmp。
等待协调设有限 timeout，runner 设独立应用上限；不可安全回收的协调失败终止隔离应用。

cancel 成功只表示请求提交；随后仍需 join，允许正常或取消退出，不断言 native cancellation
point/cleanup 的平台细节。身份在 live/unjoined 时比较，不使用 join 后已失效的身份。
使用两个同时存活、由 OSI gate 控制的 worker 验证统一句柄：create 与 self 相等，
复制仍指向同一线程，不同线程不相等，借用的当前句柄可被识别；只 join 原拥有者一次。

timer 使用每 case 独立 static context。delete 后保留 callback/semaphore/context 到应用结束，
不在后续 case 清零或复用；delete 只释放调度资源，不证明 callback 已完成。

未知选择、真实 suppression 和零执行必须失败；listing 不运行 fixture。
OSI-only build 不引入 filesystem、socket、真实 audio server 或工具链故障注入测试。
这些集成组有自己的 platform/linux/filesystem label，不能用标签或主机模型宣布新目标通过。
最近结果见 [STATUS](STATUS.md)，过程记录与接口审计材料仅在本地保留。
