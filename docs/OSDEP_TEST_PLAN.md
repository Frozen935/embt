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

# OSI POSIX 后端与 cmocka 测试规划

状态：本地 review 草案，2026-11-15。用户要求删除 demo、采用 cmocka，并让所有 OS 模块通过测试。
本规划的 T0～T5 已实施：接口后端、test/、构建与 CI 配置已建立。当前 29 个用例通过，
具体运行结果及 TSan/远端 CI 限制见 [OSDEP_TEST_RESULTS.md](OSDEP_TEST_RESULTS.md)。
所有改动未暂存、提交或推送。

API 基线见 [OSDEP_API_DESIGN.md](OSDEP_API_DESIGN.md)，注释使用
[CODE_COMMENT_STYLE.md](CODE_COMMENT_STYLE.md)。沿用轻量 POSIX 类型和已确认的九个 thread API。

## 1. 目标和当前事实

目标是在 Linux 上实现真实 OSI 后端，并由 cmocka 驱动功能、边界、并发和错误路径测试。
全部模块完成后才能报告“OS 模块测试通过”；头文件编译、cmocka 自身探测或某个模块通过不能代替此结果。

当前事实：

- osdep 保留轻量接口头，并已增加 6 个 POSIX backend .c 文件。
- 39 个 OSI API：mutex 4、sem 6、cond 5、thread 9、timer 5、time 4、memory 4、io 2。
- 本机 pkg-config 报告 cmocka 1.1.7；CMake 包提供 cmocka::cmocka。
- 本机 cmocka.h 提供 cmocka_set_test_filter，可用于按用例运行。
- 根 CMake 已接入 CTest 与 OSI 范围，mkbt.sh test 编译专用 osi_tests target。
- 四个 demo 只做过编译，没有 runtime 结果，已删除而不保留为测试入口。

本轮授权运行未来的 OSI 单元测试。协议栈的运行目标与验收仍按原 bring-up 计划推进。
cmocka 测试通过不解除 Draft PR 的运行前集成限制，最终合入仍由人决定。

## 2. cmocka 依赖选择

首期建议使用本机已有的 1.1.7 作为兼容基线，优先 SYSTEM 接入。
官方已发布 2.0.0，但本轮不同时推进 2.x API 升级，避免框架版本变化影响后端验证。
cmocka 使用 Apache-2.0，保留上游版权和许可，不改为项目版权。

| 模式 | 方案 |
| --- | --- |
| SYSTEM，默认 | find_package(cmocka 1.1.7 EXACT CONFIG REQUIRED)，使用已安装包；记录实际版本 |
| SOURCE，可复现构建 | 明确准备固定版本 1.1.7 的源码，校验锁定 SHA256，再从源码构建 |

首期验证版本为 1.1.7。SYSTEM 当前要求 EXACT 版本匹配；其他版本明确拒绝，
后续升级需独立验证兼容性，不通过静默切换 provider 解决。
找不到 SYSTEM 包时明确失败，不自动转 SOURCE，不在 configure 时下载安装包。

SOURCE 已增加 third_party/cmocka/CMakeLists.txt、dependencies.lock.json 的版本/URL/SHA256，
以及 tools/dependencies/prepare_cmocka.py。上游源码放 third_party/cmocka/upstream/ 并忽略生成内容，
下载由准备步骤显式执行，CMake 验证已准备的输入；准备脚本与已校验的 archive 哈希已实现，SOURCE 模式通过测试。
只保留 SYSTEM/SOURCE 两种选择，不先增加 EXTERNAL 或多层通用 provider 框架。

cmocka 只参与测试目标。产品 libembt.a 不链接它，配置头和 EmBT 编译选项不传入 cmocka 的源码构建。

## 3. 目录和目标

以下布局已建立；所有 OSI 功能由真实 backend 实现，测试不使用成功占位：

```text
osdep/
    CMakeLists.txt
    include/osi/                  已有内部接口
    posix/
        CMakeLists.txt
        include/osi/native_types.h
        sync.c                    mutex/sem/cond 的 POSIX 实现
        thread.c
        timer.c
        time.c
        memory.c
        io.c

test/
    CMakeLists.txt
    osdep/
        CMakeLists.txt
        test_osi_types.c
        test_osi_mutex.c
        test_osi_sem.c
        test_osi_cond.c
        test_osi_thread.c
        test_osi_timer.c
        test_osi_time.c
        test_osi_memory.c
        test_osi_io.c
        support/                  必要的等待协调、子进程和故障注入辅助

third_party/cmocka/
    CMakeLists.txt
    upstream/                     SOURCE 模式显式准备

tools/cmake/embt_testing.cmake
tools/dependencies/prepare_cmocka.py
```

一个真实 embt_osi OBJECT library，内部别名 EmBT::osi，供机制实现和测试直接使用。
同一构建中正常测试和产品复用这份后端；FULL 范围按既有组件机制将 object 聚合进 libembt.a，
不产生一个需要应用额外处理的 OSI archive，也不复制另一套测试专用“成功实现”。
测试目标不链接完整 EmBT::embt，因此 Host 的待移植源文件不会阻塞 OSI 验证。

OSI include 路径和 POSIX feature 定义供内部消费者使用；产品聚合通过 PRIVATE 依赖接入，
不向应用安装或导出 OSI 头文件。OSI 及测试使用同一生成配置；cmocka 保持自己的配置。
故障注入可另设带 linker wrap 的测试 executable，但复用真实 backend object。

测试 support 按实际用例增加，不先写新的 scheduler、通用资源管理框架或生产 OSI 状态字段。

## 4. Kconfig、CMake 和 mkbt.sh 接入

1. 继续使用项目唯一 Kconfig driver，生成 canonical autoconf 和 forwarding header；保持原 include 规则。
2. 测试开关用 CMake 的 BUILD_TESTING，项目默认 OFF，在 include(CTest) 前声明默认值。
   它是开发工具开关，不新增 CONFIG_CMOCKA，不改变产品 runtime feature 的 Kconfig 管理。
3. 增加一个开发构建范围 EMBT_BUILD_SCOPE：FULL 默认构建协议栈；OSI 只配置 OSI 后端及测试依赖。
   OSI 范围保留同一 Kconfig/ABI 检查，跳过 Host 和未使用的 crypto/codec provider 接入。
   这样不依赖完整 Bluetooth 或 Mbed TLS 已经配置/编译成功，也不增加第二套配置解析。
4. BUILD_TESTING=ON 时接入 CTest 和 cmocka；OFF 时不查找 cmocka。
   OSI 测试范围要求 EMBT_STACK 和 EMBT 配置启用，并必须启用测试；
   不能在根开关关闭后提前 return，却将“测试范围但零测试”报告为成功。
5. 定义 osi_tests 构建聚合目标，只依赖 OSI 测试 executable。
6. mkbt.sh 增加 test 动作：设置 OSI 范围及 BUILD_TESTING=ON，configure，构建 osi_tests，再运行 CTest。
   保留原 build/menuconfig 行为，不用原固定 embt target 去执行测试动作。
7. 后端未实现的模块明确保持 pending；测试目标不能自动丢弃未实现模块后报告全量通过。

已实现的统一入口：

```sh
./mkbt.sh test --build-dir build/osi-tests
```

其内部执行对应：

```sh
cmake -S . -B build/osi-tests -G Ninja \
  -DBUILD_TESTING=ON -DEMBT_BUILD_SCOPE=OSI \
  -DEMBT_CMOCKA_PROVIDER=SYSTEM
cmake --build build/osi-tests --target osi_tests
ctest --test-dir build/osi-tests -L '^osi$' --output-on-failure --no-tests=error
```

CTest 为每个用例设置有限 TIMEOUT；构建输出和测试结果在 build-dir 保存，不提交生成物。
框架 bootstrap 可以先运行类型/常量测试，但这一步只说明测试入口可用，不算 OS 函数通过。

## 5. 完整 API 覆盖清单

| 模块 | 必须覆盖的 API/对象 | 核心验证 |
| --- | --- | --- |
| common/types | timeout、NO_WAIT/FOREVER、storage 宏、native 类型 | 类型与常量正确；storage 初始化不代替 native init |
| mutex，4 | init/deinit/lock/unlock | normal/recursive、NO_WAIT、有限/无穷等待、匹配释放 |
| sem，6 | init/deinit/take/give/count_get/reset | native 限额、并发 give、不丢 credit、等待、quiescent reset |
| cond，5 | init/deinit/wait/signal/broadcast | predicate 锁、等待时释放/返回重取锁、signal/broadcast、超时 |
| thread，9 | create/start/cancel/join/self/is_current/yield/name_set/id_equal | 直接参数、立即启动、start 空操作、cancel+join、身份、属性和失败回滚 |
| timer，5 | create/start/stop/delete/remaining_ms | 一次性到期、重设/停止、剩余时间、native callback 和 storage 寿命 |
| time，4 | time_get_ms/sleep_ms/from_us/from_seconds | 单调时间、中断续睡、边界与溢出、失败输出约定 |
| memory，4 | malloc/calloc/aligned_alloc/free | 分配访问、零初始化、对齐、零大小、overflow、失败与配对释放 |
| io，2 | console_write/fatal | 原始字节、partial write、错误数量、非返回终止和诊断 |

每个 API 至少有正常路径；已有明确契约的错误、边界和并发路径必须覆盖。
不能为提高覆盖率故意调用 undefined 的 native 生命周期操作。

## 6. 各模块用例重点

### 6.1 common/types、time 和 memory

- 编译检查原生 mutex/cond 别名、thread 只含 pthread_t、九个线程签名、宏和 int64 timeout。
  C++ header smoke 可单独检查，不让普通 C 测试构建强制依赖 C++ 编译器。
- time_get_ms 非递减，不能要求返回 UTC 或特定起点；clock_gettime 失败正确转换。
- us 转换测试 0、1、999、1000、1001 和 INT64_MAX，验证向上取整及中间算术安全。
  非负 int64 us 转为 int64 ms 本身不会越界，不构造不可达的 overflow 情形。
- seconds 测试 INT64_MAX/1000 及再加一，溢出失败时输出 sentinel 不改变。
- sleep 测试 0、有限值、负值和中断后继续；用 monotonic 测量，不要求精确毫秒或调度实时性。
- malloc 正常读写与释放；calloc 内容全零及乘法 overflow；aligned_alloc 测试合法对齐、
  非 alignment 倍数的 size、非法 alignment、零大小；free(NULL) 不出错。
- 分配失败通过受控注入验证，不申请巨量内存赌系统会不会 OOM。

### 6.2 mutex、sem 和 cond

- mutex 由另一个线程持有时测试 NO_WAIT、有限超时和释放后成功；递归取得次数与释放匹配。
  不对 normal mutex 递归锁、非 owner unlock、销毁活动对象强行要求固定错误码。
- sem 多线程同时 give 到小 limit（尤其 limit=1），确认 count 不超过 limit；增加 take/give 交错，
  所有 worker join 后再 reset/deinit。count_get 只做诊断快照，不充当 reservation。
- sem 空计数下 NO_WAIT/有限超时/唤醒，reset 在 quiescent 状态清零，不期望 waiter 取消。
- cond waiter 在同一个 mutex 下检查 predicate；控制线程取得该锁后修改并通知，
  验证等待过程释放锁、返回重取锁。signal 不断言具体 waiter 或 FIFO；broadcast 验证全部完成。
- 有限等待不应提前成功或退化为无限；允许调度导致的延后，由外层 timeout 限制挂死。

### 6.3 thread

- 创建前完成 context 初始化；没有调用 start 时，entry 也能运行。start 不创建第二个执行或重启线程。
- 正常返回后 join；阻塞 worker 只有被明确唤醒后才能完成 join。join 无 timeout，不增加 timedjoin API。
- self/id_equal/is_current 同时验证本线程和另一有效线程，避免 pthread_t 整数假设。
- name_set 验证支持的名称及其错误；create 的 name 作为诊断提示，命名失败不冒充创建失败。
- stack_size 使用 native 默认和合法显式尺寸，非法尺寸按已定义错误处理。
  首期 priority=0 成功，非零 -ENOTSUP；不恢复 silent ignore。
- cancel 测试 deferred cancellation point，随后 join；worker 跨取消点持有资源时设置 native cleanup，
  验证资源确实释放。cancel 返回本身不被当作退出确认。
- 注入 pthread_create 和启动参数分配失败，检查临时参数/属性释放且没有残留用户线程。

### 6.4 timer

- create 后 disarmed、正延迟单次到期、remaining 的毫秒取整、rearm 替换 schedule、stop 解除计划。
- callback 只记录原子状态或通知，不调用 cmocka 断言或 timer 生命周期 API。
- 为每个 timer 用例建立单独 CTest 进程。一个 test_osi_timer executable 可按精确用例名
  调用 cmocka_set_test_filter；对应 timer/context 使用进程寿命的静态数据。
  runner 先校验用例名确实存在；未知过滤条件或零个 cmocka 用例必须失败，不能只看 CTest 注册数。
- delete 后维持 callback 字段与数据，进程结束前不复用、释放或销毁 callback 可能访问的对象。
  原生 delete 不能提供 drain，不写“delete 返回后 callback 不会运行”的错误测试。
- stop 的取消测试只有在受控、未到期的 schedule 上验证；已派发通知允许继续执行。
  不用短 sleep 断言绝无竞态，不把一次重设造成的 callback 重叠判为违反契约。
- 同时覆盖 timer_create/settime/gettime/delete 的失败转换和 remaining 失败时保持输出。

### 6.5 io/fatal

- 小块 pipe 输出验证内容与长度，包括内嵌 NUL；恢复测试环境的输出 fd 后再做 cmocka 断言。
- 受控 write 注入覆盖部分写入、后续失败、written 数量和中断处理。
- fatal 通过 fork+exec 在新的子进程执行，父进程 waitpid 验证明确约定的终止方式；
  子进程先处理专用执行参数，不进入 cmocka runner 或沿用父进程的测试状态。
  不能用 expect_assert_failure 替代任意 fatal，也不能把 SIGSEGV 等任意崩溃当作通过。
- POSIX fatal 的具体退出方式和 console 默认输出目的地需实现前定稿，建议 fatal 使用 abort。

## 7. 并发、失败和隔离规则

cmocka 官方说明框架不是完全 thread safe。因此：

- 所有 cmocka assertions、expect/mock 状态操作由用例控制线程执行。
  worker/callback 保存结果，通过真实 POSIX 同步通知；完成回收后由控制线程断言。
- 用 barrier、mutex/cond 和 semaphore 构造顺序，不靠 sleep 猜测线程到达某个阶段。
  一个 ready flag 不一定证明 native waiter 已登记，协调方式必须与被测语义一致。
- 用例有 monotonic deadline，CTest 有最终 TIMEOUT。错误 backend 导致 join/等待卡住时，
  CTest 将该进程判为 timeout failure，不让后台线程跨越到下一个用例。
- 在仍有 worker 引用 stack/context 时不先断言 longjmp 并释放 fixture。先安排退出、join，再断言。
  无法安全回收的失败由进程隔离处理，不 destroy 活跃 native 对象凑清理。
- 不用 cmocka memory wrappers 的成功结果证明 OSI 分配器正确；真实 OSI 分配和回收必须被测到。

Linux/GCC/Clang 错误注入优先采用测试 target 私有的 linker --wrap，复用真实 backend。
注入只匹配指定调用线程、函数和一次调用序号，完成后撤销，防止破坏框架自己的分配/日志。
pthread 错误注入返回 rc，syscall 错误返回 -1 并设置 errno；不混淆二者。
clock_nanosleep 等直接返回错误码的 POSIX 函数也按 rc 处理，依实际函数约定验证。
测试 wrapper 留在 test/osdep/support，不增加生产 hook、虚假后端或全局 logger。

## 8. 测试前需要明确的小项

实现前只补足实际可测试的契约：

| 项目 | 需要明确的决定 |
| --- | --- |
| init 数值越界 | sem initial_count/limit、无效 mutex kind 的实际错误码；不猜测 native undefined 行为 |
| NULL pointer | 区分明确的合法 NULL、已承诺的参数校验和 caller 前置条件 |
| console_write | 默认输出 fd、长度为零的行为，以及中断后重试/返回约定 |
| fatal | 明确 POSIX 的终止方式和可要求的诊断内容 |
| 超大 timeout | 按目标 time_t/kernel 能力确定可表示范围，不把所有 INT64_MAX 参数都判作 overflow |
| timer 资源寿命 | 保持当前较弱 native delete 契约；动态释放需求另行设计 |

这些决定同步进 API 注释和设计文档；不为了方便测试新增 thread 状态机或改变 API 参数。

## 9. 实现顺序与阶段交付

| 阶段 | 内容 | 验收 |
| --- | --- | --- |
| T0 框架入口 | SYSTEM cmocka、CTest、OSI 构建范围、mkbt.sh test、types/常量和签名检查 | 框架可运行、用例数量非零，阶段报告明确已实现范围 |
| T1 无线程基础 | time.c、memory.c，正常及错误路径测试 | time/memory 全部 API 通过；分配/转换无泄漏或溢出 |
| T2 同步原语 | sync.c，mutex/sem/cond 功能、超时和并发 | 限额、predicate 与资源释放正确 |
| T3 线程 | thread.c，九个 API、取消与创建失败清理 | 全部 thread API 通过，无残留 worker |
| T4 timer 与 io | timer.c、io.c，进程隔离及 syscall 错误 | timer 契约与输出/终止行为通过 |
| T5 完整回归 | 全部 OS 模块、ASan/UBSan、并发重复运行，SOURCE 可复现及 CI | 全量正常与错误测试均通过，证据归档 |

测试线程协调可以先用 native pthread，而不依赖尚未通过的 OSI thread API，避免验证循环。
正常路径测真实 backend；mock 只用于难稳定触发的失败，不替代同步/时钟/线程/timer 本身。
前几个阶段允许逐模块交付并标明 pending，最后全量门槛不允许跳过模块。

## 10. 最终通过标准和证据

- 39 个函数全部有真实实现，所有模块及其契约用例执行并通过；没有零用例、静默 skip 或 timeout。
- 真实失败注入证明返回码和失败回滚正确，正常调用无错误残留资源。
- 并发重点用例 repeat until-fail 多轮稳定通过，不用 until-pass 重试掩盖 flaky failure。
- ASan/UBSan 单独配置运行通过；TSan 在支持的环境另做并发分析，与 ASan 分开运行，
  不为掩盖失败 blanket suppression。记录系统线程缓存等 native 资源与真实泄漏的区别。
  sanitizer 使用独立 build-dir，同时作用于真实 OSI backend object 和测试 executable；
  只 instrument 测试调用代码不够。编译/链接选项按 target 绑定，不通过全局 C_FLAGS 修改产品构建。
- 测试产物只链接 OSI、Threads、配置及 cmocka，产品 archive 不带 cmocka，应用不获得内部 include。
- 保存 Git SHA、配置、编译器/libc、cmocka 版本、注册/执行用例数、CTest 结果与 sanitizer 日志。
  本机 CTest 支持 JUnit，可输出唯一文件；最低版本环境缺少该功能时保留普通 CTest 日志。
- CI 在本机通过后接入同一命令，SOURCE 模式用锁定依赖；不在 CI 和本机维护两套后端或测试。

## 11. 本轮修改与参考

本轮删除：osdep/demo 下 memory_time_io_demo.c、sync_demo.c、thread_demo.c、timer_demo.c。
同步清理 API/架构/计划文档中的 demo 路径，并更新原来暂不建立 tests 的约定：
新的 test/osdep 是本项目自行维护的测试，不恢复被删除的 Zephyr samples/tests。

本轮已实现以上阶段并进行本机测试，源码准备使用经过 SHA256 校验的 1.1.7 release。
Kconfig 未新增测试 feature，CMake/mkbt.sh、test/ 与 POSIX backend 已接入；工作流只创建未远端运行。
验收证据见测试结果文档，收到提交指令才按文档/构建/实现范围更新对应 Draft PR。

参考与当前核对：

- [cmocka 官方项目、版本与许可证](https://cmocka.org/index.html)
- [cmocka 文档及 Threading 限制](https://api.cmocka.org/)
- 本机 pkg-config cmocka=1.1.7，cmocka-config.cmake 导出 cmocka::cmocka。
- 本机 cmocka.h 的 cmocka_set_test_filter 声明、CTest 的 test-dir/no-tests/repeat/JUnit 选项已核对。
