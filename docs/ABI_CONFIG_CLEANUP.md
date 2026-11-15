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

# 根总开关、ABI 与旧内核配置清理

状态：已在 build/standalone-cmake-kconfig 实现并验证；清理归PR #4，ABI配置和构建接入归独立构建Draft PR，未合入。
密码库三种接入方式另见 [CRYPTO_PROVIDER_DESIGN.md](CRYPTO_PROVIDER_DESIGN.md)，SYSTEM/SOURCE/EXTERNAL已实现，随独立构建Draft PR跟踪，未合入。
本文数量描述ABI清理当轮的快照；最新密码配置已迁至third_party/mbedtls/Kconfig并恢复7项，
当前完整模型1010项，以[KCONFIG_MIGRATION.md](KCONFIG_MIGRATION.md)为准。

## 1. 当前根配置

根 menuconfig EMBT_STACK 包围所有配置文件与根参数；关闭时只有总开关可开启，所有
其他布尔选项均为 n，CMake 保留配置操作目标，不生成 libembt.a/EmBT::embt。
Bluetooth 内部 EMBT 选项保留，负责实际 Host；EMBT_STACK 是最外层总开关。

| 配置 | 含义 | 当前 Linux 基线 |
| --- | --- | --- |
| EMBT_STACK | 启用整个配置树和构建目标 | y |
| EMBT_64BIT | 编译目标指针 ABI：y=64位，n=32位 | y |
| EMBT_LITTLE_ENDIAN | 小端目标，和 BIG_ENDIAN 构成互斥 choice | y |
| EMBT_BIG_ENDIAN | 大端目标 | n |

生成头提供 CONFIG_EMBT_* 宏；n 不定义。CMake 校验指针宽度与编译器 __BYTE_ORDER__，
不从运行 Python 的宿主架构推断，也不要求目标是 x86_64/aarch64。32位与大端配置可选择，
但本机只验证匹配 x86_64小端的编译；其他 ABI 完整编译/运行仍需实际工具链验证。

位宽/字节序的 C/头引用已精确迁移为新名字，包括 byteorder、HCI/AVDTP 位域、buffer 和
相关通用头；没有恢复旧名别名。工具链头的 AArch64 ISA处理使用 __aarch64__ 编译器预定义
宏，不再依赖 CONFIG_ARM64；没有删除实际 ISA适配代码。原 108 处配置头 include 仍保留。

## 2. 删除与依赖处理

删除十个旧根定义（其中 64BIT 替换为 EMBT_64BIT），并删除五个强依赖具体 Flash/secure-store
实现的配置；新增四个根符号后，完整模型由 1014 变为 1003 个定义，当前根定义为 47 个。

| 类别 | 已删除名称 |
| --- | --- |
| 旧根配置 | `64BIT`, `ARM64`, `BT_SPI`, `BUILD_WITH_TFM`, `EMBT_TARGET_ARCH`, `FLASH_MAP`, `FLASH_PAGE_LAYOUT`, `SECURE_STORAGE`, `SMP`, `SOC_SERIES_NRF51` |
| 强制关联配置 | `BT_MESH_BLOB_IO_FLASH`, `BT_MESH_BLOB_IO_FLASH_WRITE_BLOCK_SIZE_MAX`, `BT_MESH_BLOB_IO_FLASH_BLOCK_CACHE`, `BT_MESH_SECURE_STORAGE`, `BT_MESH_SHELL_BLOB_IO_FLASH` |

| 原依赖 | 实际处理 | 影响 |
| --- | --- | --- |
| EMBT 的 !SMP | 删除条件和相关 OS解释注释，删除内核 SMP 配置 | 不改变现有同步/线程实现；蓝牙协议的 BT_SMP 配置保留 |
| ARM64 的线程栈 default | 删除整行默认表达式 | 使用既有通用默认值；非本机64位目标需在实际 port/profile 核对栈需求 |
| BT_SPI 的 TX 栈 default | 删除整行默认值 | 现有 userchan 的默认值保留 |
| TX processor 的 !SOC_SERIES_NRF51 default | 删除旧行；POSIX profile 显式选择 BT_TX_PROCESSOR_THREAD=y | 保持原基线线程模型；一般默认值从 y 变 n，不暗中恢复Nordic条件 |
| UUID_V5 的 !BUILD_WITH_TFM | 删除整条平台条件和仅说明该条件的注释 | PSA SHA-1 的实际依赖保留，不引入 TF-M |
| Flash BLOB 的 FLASH_MAP/PAGE_LAYOUT | 删除完整 BLOB_IO_FLASH及两个子选项，移除源码选择 | 保留可用内存/其他 BLOB接口与实际协议文件 |
| Mesh secure storage 的 SECURE_STORAGE | 删除选项和仅为该选项存在的 crypto select | 不假定有持久化 key store，其他 Mesh PSA需求保留 |
| Mesh shell Flash 演示 | 删除对应 config，移除组合条件中的该分支 | shell 仍由其他实际选项选择，相关文件未删除 |

保留功能的周期广播/连接/Host crypto 前提沿用上一轮评估；此轮没有需要用户进一步判断的
未决强制依赖。对所有保留的基线配置逐符号比较，求值结果不变。

## 3. 总开关与配置保留

EMBT_STACK 不依赖被它包围的 EMBT，以免形成自依赖。所有根 menu、子文件 source 和
需求定义都在 if EMBT_STACK 内，select 也无法从该树之外开启子功能。

关闭总开关并保存时，Kconfig 的规范 .config 会去掉不可见子配置的选择；再次打开时采用
当前默认值。需要保留复杂的多个profile时，先导出最小 defconfig或使用独立构建目录；本轮
未设计在菜单关闭后自动备份/恢复所有子选项的另一套配置状态。

旧活动 build/posix/.config 已备份后，精确删除失效名字、将64位旧名迁移并开启根总开关，
保留其他已有选择。未知/废弃字段仍由统一配置driver报错，工具不普遍吞掉旧配置。

## 4. 已执行验证

- 根模型与 Linux profile：1003 个定义，解析无告警，51个唯一源码编译任务不变。
- 真正打开菜单，关闭并保存 EMBT_STACK后所有布尔配置均为n；再打开总开关后菜单正常。
- 总开关关闭时 CMake configure/generate正常，menuconfig目标仍有，无embt库目标。
- 本机32位配置与64位编译器不匹配时失败；大端配置与小端编译器不匹配时失败，错误明确。
- 显式恢复profile后 BLE/BR/BT_SMP和密码接口需求与清理前一致。
- 实际构建进入C源码，仍因旧日志/zephyr头引用失败，不声称完整编译、跨ABI或运行成功。

日志与差异保存在 /tmp/embt-abi-crypto-design/，包括 verification.log、menuconfig.log、
configure.log、build.log、model-result.json、config-changes.json 和 before/。
密码库单独的系统接口探测见 system-crypto.log，不是整栈验证。
