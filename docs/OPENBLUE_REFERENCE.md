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

# openblue 参考研究

状态：规划依据，非当前 EmBT 的运行验证报告。本次只读研究源码和已有修改记录，没有运行 openblue 硬件测试。

## 1. 冻结输入

- 仓库：https://github.com/Frozen935/openblue
- 分支：main
- SHA：0012adc908c5d5217c320236c15739d141b2ba8e
- 提交时间：2026-04-12T19:48:33+08:00
- 当前 EmBT 对照基线：f8da8ea370508f274685a25ff19dbe5a9ff54790

以下链接固定到该 openblue commit。

## 2. 可采用的经验

| 机制 | 源码证据 | EmBT 规划 |
| --- | --- | --- |
| 分层及独立库 | [CMakeLists.txt](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/CMakeLists.txt) 分离 bluetooth、base、core、drivers、osdep | 协议与导入依赖放 bluetooth，新运行适配放 port |
| 自有 CMake helper | [helpers.cmake](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/cmake/helpers.cmake) 提供 openblue_library 等 | 使用最小 embt_* 函数集，底层使用标准 target_* |
| 独立 Kconfig | [Kconfig](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/Kconfig)、[genconfig.py](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/tools/kconfig/genconfig.py) 使用 Kconfiglib | 配置解析、最终 .config、头文件及 CMake 源码选择保持一致 |
| 常量解耦 | [Kconfig.constants](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/Kconfig.constants) 定义整数边界 | 保留必要标准变量替换，区别于 Zephyr 的设备树扩展 |
| POSIX OSAL | [os.c](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/osdep/posix/os.c)、[os.h](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/osdep/posix/os.h) 包装 pthread、信号量和定时器 | 按 Zephyr 所需语义实现，覆盖超时、启动、取消和销毁 |
| 显式初始化 | [stack_init.c](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/core/stack_init.c) 依次执行 base、driver、host 表 | 真正执行的注册表，依赖顺序明确，支持失败回滚及安全重试 |
| 固定信道注册 | [LE L2CAP](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/bluetooth/host/l2cap.c)、[BR L2CAP](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/bluetooth/host/classic/l2cap_br.c) 有显式注册 API | 覆盖 LE/BR fixed channels、ATT/SMP 及相关回调 |
| 去设备树的 HCI 接入 | [userchan.c](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/drivers/userchan.c) 使用 transport/user_data | 新 driver 显式配置和注册，首期只接实际 HCI user socket |

## 3. 不能直接复用的部分

### 3.1 空遍历不等于动态注册

[base/utils.h](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/base/utils.h) 中：

~~~c
#define STRUCT_SECTION_FOREACH(struct_type, var) for (struct struct_type *var = NULL; false;)
~~~

[bt_mem_pool.c](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/base/bt_mem_pool.c) 的 mem_pool_list_init() 仍使用该宏，bt_mem_pool_registry_init() 调用这一遍历。因此该遍历路径不会调用 create_mem_pool_list()。

这只证明该路径未执行，不能据此判定所有 buf 分配路径都会失败。EmBT 必须为所用对象提供真实的收集、初始化和查找机制；检查 init 表存在不足以证明注册完成。

### 3.2 初始化表仍需生命周期管理

stack_init.c 的表项含 prio/level，但执行函数按数组顺序调用，并在首次错误时直接返回。EmBT 明确依赖顺序，给已创建的资源配置回滚，避免重试时重复启动线程或注册对象。

### 3.3 POSIX 语义需要重新核对

os.h 仍提供 PTHREAD_MUTEX_INITIALIZER/PTHREAD_COND_INITIALIZER。部分 pthread 初始化错误路径使用 errno，os_thread_start 的声明说明其为 no-op。EmBT 显式初始化运行对象，使用正确的 pthread 返回码，明确线程启动门、时钟和有限超时。

### 3.4 HCI 默认通道不同

userchan.c 的默认 conn_type 是 HCI_UNIX，默认路径为 /tmp/hci_bridge.sock，同时实现 HCI_USERCHAN 分支。EmBT 的目标是 HCI_CHANNEL_USER，不能用 Unix bridge 连通结果代替实际 user socket 验收。

### 3.5 当前 Zephyr 的接口和依赖已变化

openblue 使用 bt_buf 等自身名称，根构建还选择 mbed TLS v2.28.8。当前 EmBT main 的 Host crypto 使用 PSA 接口，HCI driver 依赖设备树和 native_sim 层。EmBT 首期保留 net_buf 和 bt_* 名称，重新确认适配接口与密码学 backend。

### 3.6 构建和配置需收敛

openblue 的 helper 使用 INTERFACE target 传递源码，根库聚合；存在全局 include/编译选项和强制 preinclude。EmBT 明确源码编译归属与 target 作用范围。

Kconfig 生成脚本通过 Kconfiglib 解析，但 warn_to_stderr=False 会抑制终端告警输出。EmBT 将未定义符号、未识别扩展和不一致配置显式呈现，不能通过默认值或无输出规避依赖问题。

## 4. 既有成功记录的边界

[CHANGELOG_OPENBLUE.md](https://github.com/Frozen935/openblue/blob/0012adc908c5d5217c320236c15739d141b2ba8e/CHANGELOG_OPENBLUE.md) 的 2026-04-10 条目记录了显式初始化、LE/BR fixed channel 注册，以及 demo 输出 Bluetooth ready 的验证。还记录了 Debug/BT_SETTINGS 条件保护和独立 menuconfig 的修正。

这些记录支持把相关机制作为参考。它们不等同于本次 EmBT 的硬件实测，也不足以单独证明实际 HCI user socket、BLE 和 BR 数据链路均已验证。EmBT 使用本项目的构建和运行证据验收。
