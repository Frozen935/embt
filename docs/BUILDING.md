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

# 配置与构建

当前入口是根 `mkbt.sh`、Kconfig 和 CMake；使用 Ninja、GCC/Clang 与 Python 3.10 以上。
初期构建支持 Linux 和识别真实 `__NuttX__` 的目标；NuttX 的实际目标构建尚未验收。
安装 Kconfiglib 所需版本见 [构建依赖](../tools/requirements-build.txt)。系统集成使用实际
Mbed TLS/PSA、cmocka；启用音频时还需要所选 provider。依赖选择见本文末节。

## 常用命令

在仓库根目录执行，relative path 都由 mkbt.sh 相对源码根解析：

```sh
./mkbt.sh build --build-dir build/posix
./mkbt.sh btshell --build-dir build/btshell
./mkbt.sh menuconfig --build-dir build/posix
./mkbt.sh olddefconfig --build-dir build/posix
./mkbt.sh savedefconfig --build-dir build/posix --output config/local_defconfig
```

`build` 构建 whole-archive consumer `embt_link_check`，不是只打静态库。
`btshell` 启用对应片段并构建 `bin/btshell`，不自动打开 Controller。
`menuconfig` 可以在尚未配置 CMake 的目录运行；不要在无终端环境把失败误认为菜单已验证。

## 初始配置与配置重置

根 `EMBT_STACK` 控制整棵配置树，`EMBT` 控制协议栈；`EMBT_64BIT` 和大小端 choice
必须匹配编译目标。默认片段为 `config/posix_host_defconfig`。

新目录的 `--defconfig`、重复 `--fragment` 按顺序提供初始输入。已有 `.config` 保留菜单
修改，普通 build 不反复覆盖它。要切换已有 profile，明确执行重置：

```sh
./mkbt.sh defconfig --build-dir build/full \
  --fragment config/full_port.conf --fragment config/audio_playback.conf \
  --fragment config/audio_lc3.conf
./mkbt.sh build --build-dir build/full
./mkbt.sh btshell --build-dir build/full
```

保存最小配置时，已有输出需要显式 `--force` 才覆盖。
`clean` 清理对象/构建产物并保留 `.config`，不是 profile 重置。
优化由 Kconfig 选择，不使用 `CMAKE_BUILD_TYPE` 或全局 `-O` 替代项目配置。

## 一份配置与 target

```text
Kconfig + initial fragments / saved .config
    -> tools/kconfig/configure.py
    -> .config + kconfig/config.json + kconfig/config.cmake
    -> generated/include/embt/autoconf.h
    -> embt_config -> project targets and EmBT::embt consumers
```

typed CMake 数据与 JSON 来自同一求值器，不是第二套解析。
CMake 在源码选择前生成配置，以 target-scoped `-include` 注入 canonical 头。
现有显式 `<embt/autoconf.h>` 可以共存。头文件、配置和依赖缺失时重配置恢复；
生成物放在 build 目录，不提交 Git，也不通过 `-DCONFIG_*` 或源码补默认值。

| target / helper | 用途 |
| --- | --- |
| `embt_config` | 同一配置头、生成 include 和自动注入 |
| `embt_internal` | 项目实现的私有头、编译要求 |
| `embt_library` 及 `embt_library_sources*` | 组件与唯一 source ownership |
| `embt_config_enabled` / 条件 helper | 检查真实 bool 配置，避免未知条件静默选错 |
| `EmBT::embt` | 应用使用的完整库与公开使用需求 |
| `EmBT::osi`、`EmBT::psa` 等 | 对应真实组件/provider，不用空 target 冒充实现 |
| `embt_link_check` | whole-archive 验证遗漏的实现或依赖 |

配置/编译可用不证明该 feature 的运行可用；可选配置缺口见 [STATUS](STATUS.md)。

## Sanitizer 与测试

新 build 默认 `EMBT_OSI_SANITIZER=ADDRESS`，项目目标使用 ASan/UBSan。
已有 cache 保留所选值；显式 `NONE` 关闭，`THREAD` 需要支持 TSan 的环境并单独构建。
本项目没有声称 TSan 或不提供 sanitizer runtime 的目标已验证。

```sh
./mkbt.sh test --build-dir build/osi-tests
./mkbt.sh test-port --build-dir build/mechanism-tests \
  --fragment config/full_port.conf --fragment config/audio_playback.conf \
  --fragment config/audio_lc3.conf \
  --cmake-arg -DEMBT_BT_TEST_FILTER=framework
```

第二条是独立 framework/机制 profile。默认 core `test-port` 存在 CCM 配置前提缺失，
目前会链接失败，已记待办；不能把上述 profile 通过当成默认入口已修复。
C 单元、变体和 Classic 各自的 profile/命令见 [TESTING](TESTING.md)。

## 第三方 provider

Mbed TLS/PSA 配置位于 `third_party/mbedtls/Kconfig`。使用一套实际密码实现，
PSA 是 API/能力接口，不再额外链接一套无关密码实现。

| 方式 | 合同 |
| --- | --- |
| SYSTEM | 使用已安装的匹配库/头，验证所需能力；不重新配置系统库 |
| SOURCE | 显式准备锁定源码，用映射到真实宏的生成 header 编译 |
| EXTERNAL | 父工程提供真实 target、匹配 headers/config，保留其配置 |

```sh
python3 tools/dependencies/prepare_mbedtls.py
./mkbt.sh build --build-dir build/crypto-source --fragment config/crypto_source.conf
python3 tools/dependencies/prepare_cmocka.py
./mkbt.sh test --build-dir build/cmocka-source \
  --cmake-arg -DEMBT_CMOCKA_PROVIDER=SOURCE
```

锁定版本和 checksum 见 [dependencies.lock.json](../third_party/dependencies.lock.json)。
SBC/LC3 的默认 archive pin/checksum 在对应 CMake；显式已有源码目录无效时应失败，
不偷偷换成下载或系统库。三方源码不注入 EmBT 的 autoconf/内部头要求。
SOURCE/EXTERNAL 对公开 PSA 类型的传递缺口仍未修复，不应靠系统头偶然存在掩盖。
