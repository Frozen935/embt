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

# OSI POSIX 实现与测试结果

状态：2026-11-15，本地未提交实现。已按用户要求将全部测试代码放在根目录 test/。
29 个 CTest 用例覆盖 39 个 OSI API；后端采用轻量原生 POSIX 类型。

## 1. 本轮实现

- osdep/posix 的 sync.c、thread.c、timer.c、time.c、memory.c、io.c 实现全部 API。
- 一个 embt_osi OBJECT target，正常测试与产品聚合复用，不重复编译后端。
- test/osdep 包含各模块 cmocka 用例及受控 native 错误注入，回调/worker 不使用框架断言。
- BUILD_TESTING 默认 OFF；EMBT_BUILD_SCOPE=OSI 通过同一 Kconfig driver，只配置 OSI/test 依赖。
- mkbt.sh test 默认 build/osi-tests，构建 osi_tests 并执行 CTest，生成用例 manifest 和可用的 JUnit。
- cmocka SYSTEM 固定已验证的 1.1.7；SOURCE 显式准备同版本并校验 archive SHA256。
- CI 文件为 [.github/workflows/osi-tests.yml](../.github/workflows/osi-tests.yml)，包含两种 provider 和
  NONE/ADDRESS 组合、并发重复运行及结果归档；工作流尚未在 GitHub 执行。

## 2. 实际结果

| 配置 | 执行结果 |
| --- | --- |
| SYSTEM，普通构建 | 29/29 通过 |
| SOURCE，固定源码 | 29/29 通过 |
| SYSTEM，ASan+UBSan | 29/29 通过 |
| SOURCE，ASan+UBSan | 29/29 通过 |
| 普通并发/生命周期回归 | 15 个用例各重复 20 次，共 300 次通过 |
| ASan+UBSan 并发/生命周期回归 | 15 个用例各重复 20 次，共 300 次通过 |
| TSan | 编译成功；本机进程启动时出现 unexpected memory mapping，未获得有效全量结果 |
| FULL 范围 | CMake configure 及 embt_osi 的 6 个 backend object 编译通过 |
| C++11 内部头文件与类型 | 编译检查通过 |
| 未知用例 / 空 CTest 筛选 | 返回非零，不能静默通过 |

用例分布：types 1，time 2，memory 2，mutex 2，sem 3，cond 3，thread 4，timer 3，io 3，faults 6。
API 名称在用例源文件中的覆盖检查为 39/39；它说明全部入口被调用，不等同于 100% 行/分支覆盖率。

构建编译记录确认：每个 build-dir 中 6 个 backend 源各编译一次；SOURCE cmocka 的编译不带
EmBT autoconf 强制 include 或 generated/include 路径。产品默认范围没有 cmocka/test targets。

## 3. 契约决定与边界

- 等待使用 CLOCK_MONOTONIC；target libc 需提供 pthread_mutex_clocklock 和 sem_clockwait。
- mutex kind、sem 数值越界及必需 NULL 参数有明确 -EINVAL 校验；不维护原生对象有效性状态机。
- thread create 即运行，start 保持兼容空操作；cancel 提交请求，join 阻塞确认 native termination。
  priority 当前只支持 0，其他值 -ENOTSUP；名字超出 POSIX 限制返回 -ERANGE。
- 启动参数在线程进入用户代码前释放，失败路径释放临时分配/属性；取消测试验证 native cleanup。
- timer 使用原生 SIGEV_THREAD，stop/delete 不 drain callback，测试数据活到独立用例进程结束。
- console 写 stderr，重试 EINTR、处理 partial write，零长度接受 NULL；fatal 输出可选诊断后 abort。
- 此结果验证 OSI 基础能力。Bluetooth Host 的 k_* 接入、基础机制和 HCI/协议栈运行仍需后续工作。

TSan 结果是运行环境限制，未进行 suppression，也未将个别进程成功当作 TSan 全量通过。
GitHub CI 仅完成代码配置；当前没有远端执行记录。

## 4. 复现命令

```sh
./mkbt.sh test
./mkbt.sh test --build-dir build/osi-source --cmake-arg -DEMBT_CMOCKA_PROVIDER=SOURCE
./mkbt.sh test --build-dir build/osi-asan --cmake-arg -DEMBT_OSI_SANITIZER=ADDRESS
./mkbt.sh test --build-dir build/osi-source-asan \
  --cmake-arg -DEMBT_CMOCKA_PROVIDER=SOURCE --cmake-arg -DEMBT_OSI_SANITIZER=ADDRESS
ctest --test-dir build/osi-tests -R 'osi[.](mutex|sem|cond|thread|timer)[.]' \
  --repeat until-fail:20 --output-on-failure --no-tests=error
```

新 checkout 使用 SOURCE 前显式运行 tools/dependencies/prepare_cmocka.py。
测试使用本机 cmocka 1.1.7、GCC 13.3 和 glibc 2.39。HEAD 是
0886297ef3b58590ce3027773cd5bdf16484594b，源码改动未提交；仅该 SHA 不能表示本次测试树。
文件哈希、配置及环境记录在 build/osi-tests/validation.json，测试清单和 JUnit 也在各 build-dir。

原始日志：/tmp/embt-osi-normal-29.log、/tmp/embt-osi-final-source.log、
/tmp/embt-osi-final-asan.log、/tmp/embt-osi-source-asan.log、/tmp/embt-osi-repeat.log、
/tmp/embt-osi-repeat-asan.log、/tmp/embt-osi-tsan.log、/tmp/embt-osi-full-component.log。

本轮没有提交、推送或合入。全部 bring-up PR 的 runtime integration hold 保持原规则。
