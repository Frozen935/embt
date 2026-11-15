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

# Mbed TLS / PSA 接入设计与实施

状态：已在 build/standalone-cmake-kconfig 实现并验证；代码通过独立构建Draft PR跟踪，文档归PR #2，未合入。
PSA需求由一套 Mbed TLS实现提供；SYSTEM、SOURCE、EXTERNAL只表示库的取得方式，不并行
构建两个 PSA提供方。协议组件统一使用 EmBT::psa。

当前完整协议栈仍因旧日志/zephyr头引用失败；本记录的成功是密码库取得、编译、链接和算法
能力验证，不是蓝牙运行。已有 bt_crypto_init 仍负责运行时 psa_crypto_init，不新增重复初始化。

## 1. 当前配置及来源

| 配置 | 实际用途/来源 |
| --- | --- |
| PSA_CRYPTO | 模块Kconfig中的EmBT需求标志，由Host安全、crypto、UUID等消费者select；不是第二个密码库 |
| PSA_WANT_* | 对应第三方PSA配置宏的需求包装；SOURCE转换成库的native宏，SYSTEM/EXTERNAL保留原库配置 |
| EMBT_CRYPTO_SYSTEM | 默认方式，查找已安装的 Mbed TLS crypto target |
| EMBT_CRYPTO_SOURCE | 构建显式准备、固定校验的 Mbed TLS源码 |
| EMBT_CRYPTO_EXTERNAL | 使用宿主提供的现有库target |
| EMBT_MBEDTLS_HELPERS | OBEX需要mbedtls_ct_memcmp/constant_time.h的独立需求，同时select PSA_CRYPTO |

PSA与Mbed TLS配置集中在 [third_party/mbedtls/Kconfig](../third_party/mbedtls/Kconfig)，根Kconfig
仅通过rsource引用。该文件是EmBT为依赖维护的配置入口，不是从Mbed TLS上游复制的Kconfig。
模块目前27个定义：20个密码需求/接入项，以及以下7个恢复的Mbed TLS相关配置。

| 恢复项 | 当前语义 | 是否改变预编译库 |
| --- | --- | --- |
| MBEDTLS | PSA需求存在时请求使用Mbed TLS，实现由CMake验证 | 否 |
| PSA_CRYPTO_PROVIDER_MBEDTLS | 标记当前请求的PSA实现为Mbed TLS，不新增第二套库 | 否 |
| MBEDTLS_AES_ROM_TABLES | SOURCE模式按Host crypto/CCM、crypto toolbox、GATT caching、Mesh需求默认y；允许关闭 | 仅改变本项目SOURCE构建 |
| MBEDTLS_AES_FEWER_TABLES | 要求ROM tables；沿用上游默认y，以更多算术运算换取较小表空间 | 仅改变本项目SOURCE构建 |
| MBEDTLS_PSA_P256M_DRIVER_ENABLED | SOURCE模式按BT_ECC需求默认y；要求P-256且无key-pair DERIVE需求；允许关闭 | 仅改变本项目SOURCE构建 |
| MBEDTLS_SHA256_SMALLER | 要求PSA SHA-256需求；沿用上游默认y，使用较小但较慢的实现 | 仅改变本项目SOURCE构建 |
| MBEDTLS_ECP_NIST_OPTIM | 要求P-256需求；沿用上游默认y，优化通用ECP实现的模约减 | 仅改变本项目SOURCE构建 |

五个源码选项在锁定的上游Mbed TLS 3.6.5中均有原生宏和实现；P256M并非仅属于Zephyr。
它的源码/链接由原生Mbed TLS CMake提供，不创建空driver。源码模板直接生成同名库宏，
上述源码选项已完成SOURCE编译和PSA能力验证。系统/外部库保持原配置，这些选项不可选。

Host/crypto/mesh/gatt的旧优化/driver建议已恢复为第三方Kconfig中的条件default，
不改协议Kconfig；保留用户显式n覆盖默认值的能力。p256-m的imply PSA_WANT_ALG_SHA_256也已恢复。
PSA需求、所有PSA_WANT_*、三个取得方式和helper需求均在模块Kconfig；LIBSBC/LIBLC3和
通用随机接口可用性仍在根Kconfig。UUID_V5仍主动select PSA/SHA-1需求。

上游 Mbed TLS/TF-PSA-Crypto没有Kconfig，当前定义由EmBT管理。Mbed TLS 2/3含PSA实现，
4.x将实现拆到TF-PSA-Crypto；这是版本内部组织差异，不是本项目必须接两套密码库。
SOURCE当前锁定并验证3.6.5；SYSTEM/EXTERNAL版本由实际库与能力检查决定，4.x未实测。

## 1.1 加密关联核查与恢复记录

核查基线为保留历史中的Zephyr `f8da8ea370508f274685a25ff19dbe5a9ff54790`。
比较保留的Bluetooth、random、settings、UUID的Kconfig/CMake与源码调用，以及原
modules/mbedtls的PSA逻辑和crypto配置。以下记录本地迁移差异，不代表已提交或整栈可运行。

| 原关联/行为 | 当前处理与恢复判断 |
| --- | --- |
| BT_HOST_CRYPTO、BT_HOST_CCM、BT_CRYPTO、BT_GATT_CACHING、BT_MESH_CRYPTO_LIB各自imply AES_ROM_TABLES | 此前5处建议被移除；现在由third_party/mbedtls/Kconfig按这5个消费者条件default y恢复，仍受SOURCE/AES需求约束 |
| BT_ECC imply P256M_DRIVER | 由第三方Kconfig的default y if BT_ECC恢复；P-256和无DERIVE需求约束保留 |
| P256M_DRIVER imply PSA_WANT_ALG_SHA_256 | 原crypto模块中的关联此前未迁入；现已恢复，SOURCE生成PSA_WANT_ALG_SHA_256并启用相应校验 |
| AES_FEWER_TABLES默认y且依赖AES_ROM_TABLES | 恢复Kconfig、CMake变量映射和原生宏；手动关闭仍有效 |
| SHA256_SMALLER默认y且依赖PSA SHA-256 | 恢复Kconfig、CMake变量映射和原生宏；没有SHA-256需求时不可选 |
| ECP_NIST_OPTIM默认y | 恢复当前P-256场景的Kconfig、映射和原生宏；当前适配由P-256需求派生key-pair BASIC，因此以P-256为约束；仅影响通用ECP实现，不能据此声称p256-m代码体积已最小化 |
| Host AES、CCM、crypto toolbox、GATT、Mesh、BT_ECC的PSA算法/key需求 | 原有select保留，包括Mesh的key-pair DERIVE；不为启用p256-m删除实际需求 |
| BT_OBEX_AUTH select MBEDTLS | 改成select EMBT_MBEDTLS_HELPERS，后者select PSA_CRYPTO；MD5需求、真实Mbed TLS链接与constant-time helper校验保留，不恢复旧Zephyr target |
| UUID_V5 depends on隐藏的PSA SHA-1且排除TF-M | 改成主动select PSA_CRYPTO和SHA-1；SHA-1能力已验证，已删除的TF-M平台依赖不恢复 |
| BT_RPA允许Controller AES，BT_HOST_CRYPTO排除Controller AES | 改成保留Host AES前提；项目删除Controller实现，不恢复Controller配置 |
| BT_SMP_SELFTEST、BT_SMP_FORCE_BREDR依赖旧日志等级 | 改成BT_TESTING && BT_SMP，功能与默认关闭状态保留；不恢复日志系统 |
| BT_SIGNING、旧DEVICE_NAME_GATT_WRITABLE_*安全别名、BT_LOG_SNIFFER_INFO | 按已批准的deprecated/日志清理继续删除；支持的GATT安全选项与加密默认值保留 |
| MBEDTLS_INIT/Zephyr启动期初始化 | 不恢复静态启动钩子；现有common_init调用bt_crypto_init，后者调用psa_crypto_init。当前调用受BT_HOST_CRYPTO控制；关闭Host AES但只使用CCM/OBEX/其他PSA消费者的显式初始化覆盖仍需在动态初始化阶段处理，不能用独立probe通过代替 |
| 原硬件/ARM64/设备树/测试random选择 | 不恢复没有实现的旧后端；PSA_CSPRNG_GENERATOR及random_psa.c保留，POSIX profile显式启用，BT_HOST_CRYPTO_PRNG默认y。通用sys_rand_get接口仍等待POSIX适配 |
| SECURE_STORAGE、BT_MESH_SECURE_STORAGE和SETTINGS_TFM_PSA* | 保持已批准删除；密码算法没有丢失。当前没有持久化settings/PSA key store实现，Linux存储适配另行实施 |
| MBEDTLS_MEMORY_BUFFER_ALLOC_C、ENABLE_HEAP及其大小/section配置 | 原生buffer allocator有实现，但原Zephyr初始化和共享堆所有权已移除；当前SOURCE用libc分配。暂不恢复缺少显式init/生命周期的开关，后续在第三方目录设计独立静态内存池，不能把net_buf静态池等同于密码库内存池 |
| 原TLS/X.509、全算法选择、TF-PSA 4.x、P256M_DRIVER_RAW和其他无调用者的库选项 | 本次不恢复；当前Host通过PSA调用所需算法。源码锁定Mbed TLS 3.6.5，不复制无适配实现的Zephyr菜单 |

当前SYSTEM首期profile的既有配置值与本轮修改前一致；新增的3个优化开关均为n。
SOURCE首期profile会默认开启5个源码选项，并由p256-m建议开启PSA SHA-256；原有手动保存的n
继续优先于新default。需要采用新默认值时，用户可在menuconfig中选择，或显式重置defconfig。
没有因这次恢复重置现有build/posix/.config。

## 2. 三种取得方式

| 模式 | 已实现处理 | 验证情况 |
| --- | --- | --- |
| SYSTEM | 优先MbedTLS导出target，次选pkg-config mbedcrypto；找不到直接失败 | 本机libmbedtls-dev 2.28.8-1通过 |
| SOURCE | 读取固定来源/校验描述，生成独立provider配置头，调用原生CMake，只构建所需crypto库 | Mbed TLS 3.6.5完整release通过 |
| EXTERNAL | 必须提供已存在的EMBT_PSA_EXTERNAL_TARGET，使用它的头/配置/链接依赖 | 以真实现有Mbed TLS target验证；RTOS硬件未验证 |

不会在缺少系统/外部库时自动下载或切换方式。菜单使用同一配置driver，与库查找和编译
解耦。未选择PSA时不创建EmBT::psa/能力检查目标。

SOURCE生成头位于build-dir/generated/crypto/，包含MBEDTLS/PSA原生宏；它们与EmBT的
CONFIG_*头分开，第三方不强制包含EmBT autoconf。库target把同一MBEDTLS_CONFIG_FILE
传播给真正的PSA消费者，避免头与库配置不一致。

算法/key类型需求映射到psa_config.h；key pair BASIC/public-key能力按P-256需求派生。
Mbed TLS源码配置启用PSA和对应ECB/CMAC/CCM/ECDH/HMAC/哈希实现，平台熵源、RNG和
pthread同步用于当前Linux源码验证。SOURCE不是已完成的跨RTOS配置；RTOS需提供其平台实现。

## 3. 源码准备与版本锁

固定来源见 [dependencies.lock.json](../third_party/dependencies.lock.json)：

- release：Mbed TLS 3.6.5。
- URL：https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.5/mbedtls-3.6.5.tar.bz2
- SHA256：4a11f1777bb95bf4ad96721cac945a26e04bf19f57d905f241fe77ebeddf46d8。

使用正式release而非只包含gitlink的GitHub自动tar包，保证framework和生成文件完整。
prepare脚本先核对归档SHA，拒绝越界路径/链接等条目，完整提取后才移动至upstream目录。
原始许可和源码声明保留；不会在已有目录上覆盖或隐式重下载。

```sh
python3 tools/dependencies/prepare_mbedtls.py
# Offline alternative:
python3 tools/dependencies/prepare_mbedtls.py --archive /path/to/mbedtls-3.6.5.tar.bz2
```

本轮已执行显式准备，源码在third_party/mbedtls/upstream，目录由.gitignore排除。
没有将第三方源码暂存或提交，系统模式仍是默认。SOURCE不会根据floating main拉库。

## 4. 文件和target合同

| 文件/目标 | 实际职责 |
| --- | --- |
| [third_party/mbedtls/Kconfig](../third_party/mbedtls/Kconfig) | 集中维护需求、实现标志、接入方式和源码选项 |
| [third_party/mbedtls/CMakeLists.txt](../third_party/mbedtls/CMakeLists.txt) | 三种模式、唯一真实库target、源码配置与能力检查 |
| [mbedtls_config.h.in](../third_party/mbedtls/mbedtls_config.h.in)、[psa_config.h.in](../third_party/mbedtls/psa_config.h.in) | SOURCE独立配置模板；不改下载源码默认头 |
| [requirements.h.in](../third_party/mbedtls/requirements.h.in) | 当前算法验证需求，与库配置头分开 |
| [validate.c](../third_party/mbedtls/validate.c) | build-time编译/链接和算法/RNG探测，错误非零返回，不输出keys |
| [tools/cmake/embt_dependencies.cmake](../tools/cmake/embt_dependencies.cmake) | 调用adapter，把库链接依赖与检查挂入最终embt |
| [prepare_mbedtls.py](../tools/dependencies/prepare_mbedtls.py) | 显式固定release取得和校验 |
| EmBT::psa | 链接真实crypto库的usage target，没有独立算法实现或空provider |
| embt_crypto_probe / embt_crypto_check | 构建/执行能力检查；embt构建依赖check |

Host、crypto、random、Mesh、UUID及OBEX消费者按所选功能PRIVATE链接EmBT::psa；最终静态库
保留真实库链接依赖。第三方编译不获得EmBT的-include、CONFIG_*或-O0私有策略。
EXTERNAL可以由宿主先定义target，再add_subdirectory本项目，无需复建/下载另一份库。

运行时已有bt_crypto_init调用PSA初始化；本次不改key store ownership、不增加全局free，
未加入每个HCI调用重新初始化。完整OS/运行初始化验证仍属于bring-up后续阶段。

## 5. 使用命令

默认SYSTEM：

```sh
./mkbt.sh configure
cmake --build build/posix --target embt_crypto_check
./mkbt.sh build -j 8
```

SOURCE示例：先显式准备源码，再在menuconfig的Mbed TLS integration中选SOURCE；或在明确
的初始fragment加入CONFIG_EMBT_CRYPTO_SOURCE=y并创建独立build目录。已有配置切换使用菜单
或defconfig，不在普通build时重放fragment。

EXTERNAL示例（宿主CMake片段）：

```cmake
# PlatformCrypto must link a real library and publish its matching headers.
set(EMBT_PSA_EXTERNAL_TARGET PlatformCrypto)
add_subdirectory(embt)
```

同时在EmBT配置选择EMBT_CRYPTO_EXTERNAL。本项目根构建当前仍是Linux/GCC/Clang，RTOS
完整构建入口与设备验证未实现；EXTERNAL的target合同已用Linux上的真实现有库验证。

## 6. 已执行验证与限制

| 检查 | 结果 |
| --- | --- |
| SYSTEM | 找到并链接Mbed TLS 2.28.8，基础与扩展算法检查通过 |
| SOURCE | 3.6.5恢复默认优化组合、全部手动关闭优化组合、Mesh DERIVE触发通用ECDH回退的扩展算法组合均编译及能力验证通过 |
| EXTERNAL | 宿主真实target接入成功，consumer可链接运行，未带入EmBT配置宏 |
| 基础算法 | PSA init/random、AES-ECB/CMAC已知向量、P-256 key生成/import/export、ECDH往返通过 |
| 扩展算法 | CCM往返、HMAC-SHA256已知向量、SHA-1/SHA-256/MD5接口、Mbed比较helper通过 |
| 配置隔离 | 第三方编译没有EmBT -include/CONFIG_*和-O0，SOURCE库/consumer使用同一provider配置 |
| 失败路径 | 缺SYSTEM、未准备SOURCE、未给EXTERNAL target均失败；不切换或联网；旧driver配置被拒绝 |
| 菜单 | 真实PTY可选择SOURCE，保存结果有效，菜单不触发第三方查找/下载 |
| 全量EmBT构建 | crypto check先通过；随后在旧日志/头文件引用处失败，未形成完整libembt.a |

SOURCE当前只验证固定release，EXTERNAL/系统其他版本依赖实际配置和能力检查。
交叉编译构建probe但不在宿主执行目标程序，明确提示在目标上运行；RTOS熵源、同步、
allocator和全栈生命周期仍需设备验证，不能把本机成功等同于所有平台成功。

最新模块迁移和源码选项记录在/tmp/embt-mbedtls-kconfig-module/，
原始三种接入方式的验证脚本在/tmp/embt-mbedtls-integration/，包括system/source/external/expanded
configure/check记录、verification.log、menu.log、system-full-build.log与prepare-source.log。
提交分为文档PR #2、清理PR #4及独立构建Draft PR，全部未合入。源代码日志清理、namespace、POSIX/HCI和运行初始化工作继续后续推进。

## 7. 本轮恢复验证

记录目录：`/tmp/embt-crypto-restoration/`，其中`kconfig-audit.diff`保存基线对比，
`before/`与`before-values.json`保存修改前状态，`model-results.json`保存配置组合结果。

- 完整1010项模型无未定义/无类型缺失/告警；SYSTEM、EXTERNAL、SOURCE默认、SOURCE手动关闭、
  SOURCE Mesh回退、RAW+Host CCM、EMBT_STACK关闭共7个组合通过。
- SYSTEM既有配置值与修改前一致；EXTERNAL与SYSTEM的源码选项不可选；总开关关闭所有bool。
- SOURCE默认开启AES ROM/fewer tables、p256-m、SHA256_SMALLER和ECP_NIST_OPTIM，
  原生宏及PSA SHA-256生成结果检查通过；随机数、AES-ECB/CMAC、P-256/ECDH和SHA-256检查通过。
- SOURCE优化全部手动n后，原生宏不定义，通用加密能力检查仍通过。
- SOURCE Mesh的DERIVE需求使p256-m为n；通用ECDH、CCM、HMAC-SHA256、UUID SHA-1和OBEX
  MD5/helper检查通过。这是密码库检查，没有编译或运行Mesh协议，未新增DERIVE操作的运行测试。
- SYSTEM能力检查通过；本轮未再次接入外部RTOS target，也没有进行蓝牙硬件或整栈运行验证。
