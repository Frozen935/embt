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

# Kconfig 与配置头实际改造清单

状态：已实现，按清理PR #4与独立构建Draft PR拆分，文档归PR #2，未合入。新增最外层 EMBT_STACK、EMBT_64BIT 与互斥大小端配置；
旧架构/内核条件已进一步清理，PSA/Mbed TLS实际接入见 [CRYPTO_PROVIDER_DESIGN.md](CRYPTO_PROVIDER_DESIGN.md)。实施分支 build/standalone-cmake-kconfig；构建入口、完整验证
和已知编译错误见 [BUILD_SYSTEM_IMPLEMENTATION.md](BUILD_SYSTEM_IMPLEMENTATION.md)。
下文引用指向当前文件，删除项仅记录配置名与所属文件，不保留失效的历史行号。

## 1. 已完成的改造

| 编号 | 实际变化 |
| --- | --- |
| K-01/K-02 | 根 Kconfig 集中提供目标、运行与第三方需求，补齐 workqueue 类型/默认值；完整解析无缺失符号 |
| K-03 | 移除 BT_HAS_HCI_VS 的 DT default 和专用辅助变量；根配置给默认 n |
| K-04 | 删除旧日志配置文件、模板入口、模块变量、日志 backend 和数据日志选项；不补 LOG 占位 |
| K-05 | DIS 版本改用根 EMBT_APP_VERSION，保留原 fallback |
| K-06 | 通用 shell 未接入前通过 EMBT_PORT_HAS_SHELL=n 门控；SMP 测试使用 BT_TESTING && BT_SMP |
| K-07 | 根定义 BT_HCI_SET_PUBLIC_ADDR；CMake 去掉 MBEDTLS_BUILTIN、mbedtls_iface 和 TF-M 注入 |
| K-08 | BT → EMBT，驱动依赖、CMake 和一处 API 文档引用同步改名 |
| K-09 | EXPERIMENTAL → EMBT_EXPERIMENTAL，54 处 select 已更新，不保留旧别名 |
| K-10 | 删除 8 个完整 deprecated config 块及其专用条件，保留支持的 GATT 替代项和默认安全要求 |
| H-01 | 108 处原配置头 include 保留；主头和旧路径转发头由同一 driver 自动生成 |

日志/命名清理的已有 Kconfig 修改继续保留；本轮进一步删除 Controller/旧平台配置，
仅为保留原同步保护而删除 settings.c 的 MULTITHREADING 条件包围。
整个模型目前 1010 个定义符号。下表的基线值表达配置选择或接口需求，不代表对应 OS/provider 已实现。

当前进一步清理记录见 [CONTROLLER_CONFIG_CLEANUP.md](CONTROLLER_CONFIG_CLEANUP.md)，
记录上一轮50项删除；最新根总开关/ABI及内核残留清理见
[ABI_CONFIG_CLEANUP.md](ABI_CONFIG_CLEANUP.md)，当前表只列实际保留定义。Mbed TLS七个相关配置已恢复到模块入口，
其中五个是实际SOURCE选项；原AES/p256-m自动建议以第三方条件default恢复。

## 2. 当前根配置与第三方模块配置

通用构建/运行参数留在根Kconfig。Mbed TLS与PSA相关的27个定义统一在
third_party/mbedtls/Kconfig，根通过rsource加载；SOURCE源码选项会映射到真实库配置头。
原协议AES/p256-m建议由第三方条件default恢复，并恢复p256-m的SHA-256建议及3个原生优化选项。
完整恢复/保留修改/继续删除清单见[密码库关联核查](CRYPTO_PROVIDER_DESIGN.md#11-加密关联核查与恢复记录)。

### 根配置

| 名称 | 类型 | 首期配置值 | 定义位置 |
| --- | --- | --- | --- |
| `EMBT_STACK` | bool | `y` | [Kconfig:19](../Kconfig#L19) |
| `EMBT_64BIT` | bool | `y` | [Kconfig:29](../Kconfig#L29) |
| `EMBT_LITTLE_ENDIAN` | bool | `y` | [Kconfig:40](../Kconfig#L40) |
| `EMBT_BIG_ENDIAN` | bool | `n` | [Kconfig:43](../Kconfig#L43) |
| `EMBT_APP_VERSION` | string | `""` | [Kconfig:48](../Kconfig#L48) |
| `SOC` | string | `posix` | [Kconfig:54](../Kconfig#L54) |
| `NO_OPTIMIZATIONS` | bool | `y` | [Kconfig:62](../Kconfig#L62) |
| `SIZE_OPTIMIZATIONS` | bool | `n` | [Kconfig:65](../Kconfig#L65) |
| `SPEED_OPTIMIZATIONS` | bool | `n` | [Kconfig:68](../Kconfig#L68) |
| `EMBT_EXPERIMENTAL` | bool | `y` | [Kconfig:73](../Kconfig#L73) |
| `NUM_PREEMPT_PRIORITIES` | int | `15` | [Kconfig:82](../Kconfig#L82) |
| `SYSTEM_WORKQUEUE_PRIORITY` | int | `-1` | [Kconfig:87](../Kconfig#L87) |
| `SYSTEM_WORKQUEUE_STACK_SIZE` | int | `1024` | [Kconfig:94](../Kconfig#L94) |
| `ASSERT` | bool | `y` | [Kconfig:102](../Kconfig#L102) |
| `PRINTK` | bool | `y` | [Kconfig:106](../Kconfig#L106) |
| `POLL` | bool | `n` | [Kconfig:113](../Kconfig#L113) |
| `EMBT_PORT_HAS_SHELL` | bool | `n` | [Kconfig:119](../Kconfig#L119) |
| `SHELL` | bool | `n` | [Kconfig:125](../Kconfig#L125) |
| `SHELL_GETOPT` | bool | `n` | [Kconfig:128](../Kconfig#L128) |
| `GETOPT_LONG` | bool | `n` | [Kconfig:131](../Kconfig#L131) |
| `LIBSBC` | bool | `n` | [Kconfig:140](../Kconfig#L140) |
| `LIBLC3` | bool | `n` | [Kconfig:144](../Kconfig#L144) |
| `HAS_RANDOM_GENERATOR` | bool | `n` | [Kconfig:148](../Kconfig#L148) |
| `BT_H4` | bool | `n` | [Kconfig:158](../Kconfig#L158) |
| `BT_H5` | bool | `n` | [Kconfig:162](../Kconfig#L162) |
| `BT_HCI_SET_PUBLIC_ADDR` | bool | `n` | [Kconfig:166](../Kconfig#L166) |
| `BT_HAS_HCI_VS` | bool | `n` | [Kconfig:172](../Kconfig#L172) |

### Mbed TLS / PSA 模块配置

| 名称 | 类型 | 首期配置值 | 定义位置 |
| --- | --- | --- | --- |
| `PSA_CRYPTO` | bool | `y` | [third_party/mbedtls/Kconfig:19](../third_party/mbedtls/Kconfig#L19) |
| `MBEDTLS` | bool | `y` | [third_party/mbedtls/Kconfig:25](../third_party/mbedtls/Kconfig#L25) |
| `PSA_CRYPTO_PROVIDER_MBEDTLS` | bool | `y` | [third_party/mbedtls/Kconfig:32](../third_party/mbedtls/Kconfig#L32) |
| `EMBT_MBEDTLS_HELPERS` | bool | `n` | [third_party/mbedtls/Kconfig:39](../third_party/mbedtls/Kconfig#L39) |
| `EMBT_CRYPTO_SYSTEM` | bool | `y` | [third_party/mbedtls/Kconfig:53](../third_party/mbedtls/Kconfig#L53) |
| `EMBT_CRYPTO_SOURCE` | bool | `n` | [third_party/mbedtls/Kconfig:56](../third_party/mbedtls/Kconfig#L56) |
| `EMBT_CRYPTO_EXTERNAL` | bool | `n` | [third_party/mbedtls/Kconfig:59](../third_party/mbedtls/Kconfig#L59) |
| `MBEDTLS_AES_ROM_TABLES` | bool | `n` | [third_party/mbedtls/Kconfig:67](../third_party/mbedtls/Kconfig#L67) |
| `MBEDTLS_AES_FEWER_TABLES` | bool | `n` | [third_party/mbedtls/Kconfig:76](../third_party/mbedtls/Kconfig#L76) |
| `MBEDTLS_PSA_P256M_DRIVER_ENABLED` | bool | `n` | [third_party/mbedtls/Kconfig:84](../third_party/mbedtls/Kconfig#L84) |
| `MBEDTLS_SHA256_SMALLER` | bool | `n` | [third_party/mbedtls/Kconfig:96](../third_party/mbedtls/Kconfig#L96) |
| `MBEDTLS_ECP_NIST_OPTIM` | bool | `n` | [third_party/mbedtls/Kconfig:104](../third_party/mbedtls/Kconfig#L104) |
| `PSA_WANT_ALG_CCM` | bool | `n` | [third_party/mbedtls/Kconfig:119](../third_party/mbedtls/Kconfig#L119) |
| `PSA_WANT_ALG_CMAC` | bool | `y` | [third_party/mbedtls/Kconfig:122](../third_party/mbedtls/Kconfig#L122) |
| `PSA_WANT_ALG_ECB_NO_PADDING` | bool | `y` | [third_party/mbedtls/Kconfig:125](../third_party/mbedtls/Kconfig#L125) |
| `PSA_WANT_ALG_ECDH` | bool | `y` | [third_party/mbedtls/Kconfig:128](../third_party/mbedtls/Kconfig#L128) |
| `PSA_WANT_ALG_HMAC` | bool | `n` | [third_party/mbedtls/Kconfig:131](../third_party/mbedtls/Kconfig#L131) |
| `PSA_WANT_ALG_MD5` | bool | `n` | [third_party/mbedtls/Kconfig:134](../third_party/mbedtls/Kconfig#L134) |
| `PSA_WANT_ALG_SHA_1` | bool | `n` | [third_party/mbedtls/Kconfig:137](../third_party/mbedtls/Kconfig#L137) |
| `PSA_WANT_ALG_SHA_256` | bool | `n` | [third_party/mbedtls/Kconfig:140](../third_party/mbedtls/Kconfig#L140) |
| `PSA_WANT_ECC_SECP_R1_256` | bool | `y` | [third_party/mbedtls/Kconfig:143](../third_party/mbedtls/Kconfig#L143) |
| `PSA_WANT_KEY_TYPE_AES` | bool | `y` | [third_party/mbedtls/Kconfig:146](../third_party/mbedtls/Kconfig#L146) |
| `PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_DERIVE` | bool | `n` | [third_party/mbedtls/Kconfig:149](../third_party/mbedtls/Kconfig#L149) |
| `PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_EXPORT` | bool | `y` | [third_party/mbedtls/Kconfig:152](../third_party/mbedtls/Kconfig#L152) |
| `PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_GENERATE` | bool | `y` | [third_party/mbedtls/Kconfig:155](../third_party/mbedtls/Kconfig#L155) |
| `PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_IMPORT` | bool | `y` | [third_party/mbedtls/Kconfig:158](../third_party/mbedtls/Kconfig#L158) |
| `PSA_WANT_KEY_TYPE_HMAC` | bool | `n` | [third_party/mbedtls/Kconfig:161](../third_party/mbedtls/Kconfig#L161) |

## 3. 实验标志改名和 deprecated 配置删除

### 3.1 实验标志

当前54处为select EMBT_EXPERIMENTAL。

| 文件 | 当前行号 |
| --- | --- |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | 214, 220 |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | 284 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | 96, 475, 483, 491, 1017 |
| [bluetooth/host/Kconfig.gatt](../bluetooth/host/Kconfig.gatt) | 10, 74 |
| [bluetooth/host/Kconfig.l2cap](../bluetooth/host/Kconfig.l2cap) | 63, 74 |
| [bluetooth/host/classic/Kconfig](../bluetooth/host/classic/Kconfig) | 17, 103, 109, 119, 125, 131, 137, 147, 154, 198, 224, 232, 499, 507, 557, 564, 593, 617, 626, 635, 645, 655, 667, 697, 704, 741, 804, 922, 930, 939, 947, 973, 980 |
| [bluetooth/mesh/Kconfig](../bluetooth/mesh/Kconfig) | 1907 |
| [bluetooth/services/Kconfig.ans](../bluetooth/services/Kconfig.ans) | 10 |
| [bluetooth/services/Kconfig.dis](../bluetooth/services/Kconfig.dis) | 194 |
| [bluetooth/services/Kconfig.ets](../bluetooth/services/Kconfig.ets) | 8 |
| [bluetooth/services/hids/Kconfig](../bluetooth/services/hids/Kconfig) | 9 |
| [bluetooth/services/ias/Kconfig.ias](../bluetooth/services/ias/Kconfig.ias) | 8, 36 |
| [bluetooth/services/ots/Kconfig](../bluetooth/services/ots/Kconfig) | 11, 78 |

### 3.2 已删除的完整配置项

| 配置名 | 所属文件 | 处理 |
| --- | --- | --- |
| `BT_CUSTOM` | [bluetooth/Kconfig](../bluetooth/Kconfig) | 完整定义已删除；不提供别名或隐藏 n 占位 |
| `BT_SIGNING` | [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | 完整定义已删除；不提供别名或隐藏 n 占位 |
| `BT_MESH_MODEL_VND_MSG_CID_FORCE` | [bluetooth/mesh/Kconfig](../bluetooth/mesh/Kconfig) | 完整定义已删除；不提供别名或隐藏 n 占位 |
| `DEVICE_NAME_GATT_WRITABLE_NONE` | [bluetooth/services/Kconfig.gap_svc](../bluetooth/services/Kconfig.gap_svc) | 完整定义已删除；不提供别名或隐藏 n 占位 |
| `DEVICE_NAME_GATT_WRITABLE_ENCRYPT` | [bluetooth/services/Kconfig.gap_svc](../bluetooth/services/Kconfig.gap_svc) | 完整定义已删除；不提供别名或隐藏 n 占位 |
| `DEVICE_NAME_GATT_WRITABLE_AUTHEN` | [bluetooth/services/Kconfig.gap_svc](../bluetooth/services/Kconfig.gap_svc) | 完整定义已删除；不提供别名或隐藏 n 占位 |
| `DEVICE_APPEARANCE_GATT_WRITABLE_AUTHEN` | [bluetooth/services/Kconfig.gap_svc](../bluetooth/services/Kconfig.gap_svc) | 完整定义已删除；不提供别名或隐藏 n 占位 |
| `RING_BUFFER` | [lib/utils/Kconfig](../lib/utils/Kconfig) | 完整定义已删除；不提供别名或隐藏 n 占位 |

BT_CUSTOM 的驱动空分支和 GATT 旧别名 default 已清除。GATT 当前 BT_* 安全选择及默认值、
RING_BUFFER_LARGE/BASE64/UTF8 等非废弃选项保留。CONFIG_BT_SIGNING 等 C 层条件仍随源码
保留，后续源码适配处理，不重新引入已废弃的 Kconfig。

## 4. 日志配置清理与源码边界

旧 LOG/LOG_*、模块日志等级、日志模板和关联入口已经从 Kconfig/CMake 清掉；没有兼容
bool/default n。Bluetooth 专用日志文件 Kconfig.logging 已删除。

另删除旧 UART/RTT 日志 backend 选择、空余 BT_DEBUG_TYPE/BT_DEBUG_NONE、
BT_DEBUG_ISO_DATA、BT_BAP_DEBUG_STREAM_DATA、BT_BAP_DEBUG_STREAM_SEQ_NUM、
BT_LOG_SNIFFER_INFO、NET_BUF_LOG/NET_BUF_SIMPLE_LOG 和 NET_BUF_WARN_ALLOC_INTERVAL。
SMP 自检/强制 BR 路径仍保留，仅替换日志级别依赖为 BT_TESTING && BT_SMP。

BT_TBS_CLIENT_BEARER_TECHNOLOGY、NET_BUF_POOL_USAGE、NET_BUF_HARDENING、断言及其他非日志
配置保留。C 层 LOG_* 调用和四个旧日志头文件尚未处理，不能把配置清理当成全部日志清理
完成。完整边界见 [LOGGING_REMOVAL_PLAN.md](LOGGING_REMOVAL_PLAN.md)。

## 5. 自动配置头与原引用保留清单

配置头由 CMake 配置阶段调用唯一 Kconfig driver 自动生成，通过 target 的 `-include`
注入。**下列原有 `#include <zephyr/autoconf.h>` 全部保留，不删除、不改名。**
原来的 108 处删除计划撤销；这个表用于核对源码保持不变，新代码不需要再手工添加配置头引用。

构建目录中只有 `generated/include/embt/autoconf.h` 定义配置值；另生成
`generated/include/zephyr/autoconf.h`，通过相对路径 `../embt/autoconf.h` 转发到同一主头。
配置 target 导出 generated/include，并负责强制 include 主头；两个头使用 include guard。
这使强制注入和原显式 include 共存，不生成两套配置值，也不在源码树添加兼容文件。

共 108 处：audio 76、host 20、common 1、services 2、include/embt 9。
消费方通过 `EmBT::embt` 自动获得强制注入与生成头搜索路径；第三方不继承这套编译要求。

| 文件位置 | 拟改动作 |
| --- | --- |
| [bluetooth/audio/aics.c:15](../bluetooth/audio/aics.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/aics_client.c:16](../bluetooth/audio/aics_client.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/aics_internal.h:17](../bluetooth/audio/aics_internal.h#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/ascs.c:18](../bluetooth/audio/ascs.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/audio.c:16](../bluetooth/audio/audio.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/audio_internal.h:14](../bluetooth/audio/audio_internal.h#L14) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_base.c:15](../bluetooth/audio/bap_base.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_broadcast_assistant.c:17](../bluetooth/audio/bap_broadcast_assistant.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_broadcast_sink.c:15](../bluetooth/audio/bap_broadcast_sink.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_broadcast_source.c:15](../bluetooth/audio/bap_broadcast_source.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_endpoint.h:14](../bluetooth/audio/bap_endpoint.h#L14) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_iso.c:15](../bluetooth/audio/bap_iso.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_iso.h:13](../bluetooth/audio/bap_iso.h#L13) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_scan_delegator.c:17](../bluetooth/audio/bap_scan_delegator.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_stream.c:16](../bluetooth/audio/bap_stream.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_unicast_client.c:19](../bluetooth/audio/bap_unicast_client.c#L19) | 保留原配置头 include，不修改 |
| [bluetooth/audio/bap_unicast_server.c:15](../bluetooth/audio/bap_unicast_server.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/cap_acceptor.c:12](../bluetooth/audio/cap_acceptor.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/audio/cap_commander.c:13](../bluetooth/audio/cap_commander.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/audio/cap_common.c:12](../bluetooth/audio/cap_common.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/audio/cap_handover.c:13](../bluetooth/audio/cap_handover.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/audio/cap_initiator.c:15](../bluetooth/audio/cap_initiator.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/cap_internal.h:13](../bluetooth/audio/cap_internal.h#L13) | 保留原配置头 include，不修改 |
| [bluetooth/audio/cap_stream.c:12](../bluetooth/audio/cap_stream.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/audio/ccp_call_control_client.c:14](../bluetooth/audio/ccp_call_control_client.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/audio/ccp_call_control_server.c:15](../bluetooth/audio/ccp_call_control_server.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/codec.c:21](../bluetooth/audio/codec.c#L21) | 保留原配置头 include，不修改 |
| [bluetooth/audio/csip_crypto.c:16](../bluetooth/audio/csip_crypto.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/csip_set_coordinator.c:28](../bluetooth/audio/csip_set_coordinator.c#L28) | 保留原配置头 include，不修改 |
| [bluetooth/audio/csip_set_member.c:17](../bluetooth/audio/csip_set_member.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/gmap_client.c:13](../bluetooth/audio/gmap_client.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/audio/gmap_server.c:13](../bluetooth/audio/gmap_server.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/audio/has.c:14](../bluetooth/audio/has.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/audio/has_client.c:12](../bluetooth/audio/has_client.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/audio/mcc.c:16](../bluetooth/audio/mcc.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/mcc_internal.h:17](../bluetooth/audio/mcc_internal.h#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/mcp_internal.h:16](../bluetooth/audio/mcp_internal.h#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/mcp_media_control_server.c:16](../bluetooth/audio/mcp_media_control_server.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/mcs.c:18](../bluetooth/audio/mcs.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/micp_internal.h:18](../bluetooth/audio/micp_internal.h#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/micp_mic_ctlr.c:16](../bluetooth/audio/micp_mic_ctlr.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/micp_mic_dev.c:15](../bluetooth/audio/micp_mic_dev.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/pacs.c:19](../bluetooth/audio/pacs.c#L19) | 保留原配置头 include，不修改 |
| [bluetooth/audio/pbp.c:10](../bluetooth/audio/pbp.c#L10) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/audio.h:24](../bluetooth/audio/shell/audio.h#L24) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/bap.c:21](../bluetooth/audio/shell/bap.c#L21) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/bap_broadcast_assistant.c:16](../bluetooth/audio/shell/bap_broadcast_assistant.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/bap_scan_delegator.c:16](../bluetooth/audio/shell/bap_scan_delegator.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/bap_usb.c:18](../bluetooth/audio/shell/bap_usb.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/cap_acceptor.c:15](../bluetooth/audio/shell/cap_acceptor.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/cap_commander.c:15](../bluetooth/audio/shell/cap_commander.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/cap_handover.c:17](../bluetooth/audio/shell/cap_handover.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/cap_initiator.c:17](../bluetooth/audio/shell/cap_initiator.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/ccp_call_control_client.c:15](../bluetooth/audio/shell/ccp_call_control_client.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/ccp_call_control_server.c:14](../bluetooth/audio/shell/ccp_call_control_server.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/csip_set_coordinator.c:18](../bluetooth/audio/shell/csip_set_coordinator.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/csip_set_member.c:18](../bluetooth/audio/shell/csip_set_member.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/gmap.c:18](../bluetooth/audio/shell/gmap.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/mcc.c:18](../bluetooth/audio/shell/mcc.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/micp_mic_ctlr.c:16](../bluetooth/audio/shell/micp_mic_ctlr.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/micp_mic_dev.c:17](../bluetooth/audio/shell/micp_mic_dev.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/tbs.c:18](../bluetooth/audio/shell/tbs.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/tbs_client.c:17](../bluetooth/audio/shell/tbs_client.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/tmap.c:15](../bluetooth/audio/shell/tmap.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/vcp_vol_ctlr.c:15](../bluetooth/audio/shell/vcp_vol_ctlr.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/shell/vcp_vol_rend.c:17](../bluetooth/audio/shell/vcp_vol_rend.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/tbs.c:18](../bluetooth/audio/tbs.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/audio/tbs_client.c:14](../bluetooth/audio/tbs_client.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/audio/tbs_internal.h:16](../bluetooth/audio/tbs_internal.h#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/tmap.c:14](../bluetooth/audio/tmap.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/audio/vcp_internal.h:17](../bluetooth/audio/vcp_internal.h#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/vcp_vol_ctlr.c:16](../bluetooth/audio/vcp_vol_ctlr.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/audio/vcp_vol_rend.c:17](../bluetooth/audio/vcp_vol_rend.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/audio/vocs.c:15](../bluetooth/audio/vocs.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/vocs_client.c:15](../bluetooth/audio/vocs_client.c#L15) | 保留原配置头 include，不修改 |
| [bluetooth/audio/vocs_internal.h:16](../bluetooth/audio/vocs_internal.h#L16) | 保留原配置头 include，不修改 |
| [bluetooth/common/long_wq.c:9](../bluetooth/common/long_wq.c#L9) | 保留原配置头 include，不修改 |
| [bluetooth/host/adv.c:13](../bluetooth/host/adv.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/host/aes_ccm.c:11](../bluetooth/host/aes_ccm.c#L11) | 保留原配置头 include，不修改 |
| [bluetooth/host/att.c:14](../bluetooth/host/att.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/host/buf.c:12](../bluetooth/host/buf.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/host/conn.c:16](../bluetooth/host/conn.c#L16) | 保留原配置头 include，不修改 |
| [bluetooth/host/crypto_psa.c:12](../bluetooth/host/crypto_psa.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/host/cs.c:14](../bluetooth/host/cs.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/host/data.c:12](../bluetooth/host/data.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/host/direction.c:11](../bluetooth/host/direction.c#L11) | 保留原配置头 include，不修改 |
| [bluetooth/host/ecc.c:13](../bluetooth/host/ecc.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/host/gatt.c:18](../bluetooth/host/gatt.c#L18) | 保留原配置头 include，不修改 |
| [bluetooth/host/hci_core.c:17](../bluetooth/host/hci_core.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/host/hci_core.h:12](../bluetooth/host/hci_core.h#L12) | 保留原配置头 include，不修改 |
| [bluetooth/host/id.c:14](../bluetooth/host/id.c#L14) | 保留原配置头 include，不修改 |
| [bluetooth/host/iso.c:17](../bluetooth/host/iso.c#L17) | 保留原配置头 include，不修改 |
| [bluetooth/host/scan.c:13](../bluetooth/host/scan.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/host/settings.c:12](../bluetooth/host/settings.c#L12) | 保留原配置头 include，不修改 |
| [bluetooth/host/shell/bt.c:23](../bluetooth/host/shell/bt.c#L23) | 保留原配置头 include，不修改 |
| [bluetooth/host/shell/bt.h:18](../bluetooth/host/shell/bt.h#L18) | 保留原配置头 include，不修改 |
| [bluetooth/host/smp.h:15](../bluetooth/host/smp.h#L15) | 保留原配置头 include，不修改 |
| [bluetooth/services/ots/ots_dir_list.c:13](../bluetooth/services/ots/ots_dir_list.c#L13) | 保留原配置头 include，不修改 |
| [bluetooth/services/ots/ots_dir_list_internal.h:15](../bluetooth/services/ots/ots_dir_list_internal.h#L15) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/audio/aics.h:38](../include/embt/bluetooth/audio/aics.h#L38) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/audio/ascs.h:34](../include/embt/bluetooth/audio/ascs.h#L34) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/audio/audio.h:28](../include/embt/bluetooth/audio/audio.h#L28) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/audio/bap.h:31](../include/embt/bluetooth/audio/bap.h#L31) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/audio/ccp.h:37](../include/embt/bluetooth/audio/ccp.h#L37) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/audio/csip.h:34](../include/embt/bluetooth/audio/csip.h#L34) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/audio/tmap.h:29](../include/embt/bluetooth/audio/tmap.h#L29) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/buf.h:26](../include/embt/bluetooth/buf.h#L26) | 保留原配置头 include，不修改 |
| [include/embt/bluetooth/gatt.h:31](../include/embt/bluetooth/gatt.h#L31) | 保留原配置头 include，不修改 |


## 6. 已执行核对

根模型解析、54 处改名、8 个 deprecated 项删除、日志配置清理和 108 处原配置头引用已核对。
实际 menuconfig、配置类型/持久化/异常、生成头恢复以及 CMake 源码图验证均通过，
见 [实施与验证](BUILD_SYSTEM_IMPLEMENTATION.md)。SYSTEM/SOURCE/EXTERNAL已实际验证，UUID_V5主动声明PSA/SHA-1需求。
真实协议栈编译仍因旧头引用失败，
尚未生成完整静态库或完成运行验证。
