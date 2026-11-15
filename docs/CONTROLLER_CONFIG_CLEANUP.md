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

# Controller 与旧平台配置清理记录

状态：已在本地 build/standalone-cmake-kconfig 完成，未暂存、提交或推送。
本次删除重复的 bluetooth/LICENSE，项目继续使用根 LICENSE；存活源码的许可声明不变。

## 1. 删除范围

从上一轮本地构建模型删除 50 个完整配置定义：39 个点名项、4 个随关联失去用途的
根标志，以及 7 个强制依赖 USB device stack 的 Audio shell 配置。模型由 1064 变为
1014 个定义，根 Kconfig 当前 53 个定义。

| 类别 | 已删除定义 |
| --- | --- |
| 点名删除 | `ARM_MPU`, `BOARD_NATIVE_SIM`, `BOARD_QEMU_CORTEX_M3`, `BOARD_QEMU_X86`, `BT_CTLR_ADV_EXT`, `BT_CTLR_ADV_EXT_SUPPORT`, `BT_CTLR_ADV_ISO_SUPPORT`, `BT_CTLR_CENTRAL_ISO_SUPPORT`, `BT_CTLR_CHANNEL_SOUNDING_SUPPORT`, `BT_CTLR_CRYPTO`, `BT_CTLR_DATA_LEN_UPDATE_SUPPORT`, `BT_CTLR_DF`, `BT_CTLR_DF_CONN_CTE_REQ`, `BT_CTLR_DF_CTE_RX_SUPPORT`, `BT_CTLR_DF_CTE_TX_SUPPORT`, `BT_CTLR_DF_SUPPORT`, `BT_CTLR_EXTENDED_FEAT_SET_SUPPORT`, `BT_CTLR_EXT_SCAN_FP`, `BT_CTLR_FRAME_SPACE_UPDATE_SUPPORT`, `BT_CTLR_LE_PATH_LOSS_MONITORING_SUPPORT`, `BT_CTLR_LE_POWER_CONTROL_SUPPORT`, `BT_CTLR_PERIPHERAL_ISO_SUPPORT`, `BT_CTLR_PHY_CODED`, `BT_CTLR_PHY_UPDATE_SUPPORT`, `BT_CTLR_SCA_UPDATE_SUPPORT`, `BT_CTLR_SHORTER_CONNECTION_INTERVALS_SUPPORT`, `BT_CTLR_SUBRATING_SUPPORT`, `BT_CTLR_SYNC_ISO_SUPPORT`, `BT_CTLR_SYNC_TRANSFER_RECEIVER_SUPPORT`, `BT_CTLR_SYNC_TRANSFER_SENDER_SUPPORT`, `BT_ESP32`, `BT_LL_SW_SPLIT`, `BT_STM32WBA`, `BT_STM32_IPM`, `MODULES`, `MULTITHREADING`, `UART_BT`, `USBD_AUDIO2_CLASS`, `USBD_BOS_SUPPORT` |
| 关联孤立根标志 | `BT_TICKER_NEXT_SLOT_GET`, `HAS_BT_CTLR`, `MPU_ALLOW_FLASH_WRITE`, `SOC_COMPATIBLE_NRF` |
| 强制依赖 USB 的配置 | `BT_BAP_SHELL_USB_PRODUCT`, `BT_BAP_SHELL_USB_VID`, `BT_BAP_SHELL_USB_PID`, `BT_BAP_SHELL_USB_SELF_POWERED`, `BT_BAP_SHELL_USB_REMOTE_WAKEUP`, `BT_BAP_SHELL_USB_MAX_POWER`, `BT_BAP_SHELL_USB_20_EXTENSION_DESC` |

不再用隐藏 bool/default n 保留这些名字。当前 Kconfig 和 CMake 不引用 BT_CTLR_*、
MODULES、MULTITHREADING、指定的 STM32/ESP32/board/MPU/UART/USB 标志。

## 2. 依赖解除评估

| 情况 | 本次处理 | 影响 |
| --- | --- | --- |
| !HAS_BT_CTLR 或 BT_CTLR_* 的平台能力门控 | 删除整条平台门控表达式，同时删除 HAS_BT_CTLR | 之前 HAS_BT_CTLR=n 时条件恒成立；Host 功能保留，外部能力仍由运行期 HCI 确认 |
| 周期广播同步传输的混合依赖 | 删除旧混合行，分别保留 BT_PER_ADV_SYNC/BT_PER_ADV 和 BT_CONN 的独立依赖 | 不使没有 ACL 连接或周期广播基础的功能变为可选 |
| Frame Space 的混合依赖 | 删除旧混合行，分别保留 BT_CONN 与 BT_LE_EXTENDED_FEAT_SET | 保持 Host 前置能力约束 |
| Mesh HCI 扩展与 LL_SW_SPLIT 条件 | 删除旧混合行，保留 broadcaster/observer 的独立依赖 | 不恢复进程内软件 Controller；保留真正的 Host 角色要求 |
| BT_RPA 依赖 Host 或 Controller AES | 改为仅依赖 BT_HOST_CRYPTO | 不让无 AES 的 RPA 功能启用；保留独立 Host 实现 |
| BT_HOST_CRYPTO 的 !BT_CTLR_CRYPTO | 删除否定 Controller 条件 | Host 仍默认提供 AES；PSA/key/algorithm 需求保留 |
| BT_SETTINGS 的条件 MPU select | 删除整行 select，并删除无其他消费者的 MPU_ALLOW_FLASH_WRITE | 不失去 settings 的存储接口；POSIX 不依赖 ARM MPU Flash 放行 |
| BT_SHELL 的软件 Controller ticker select | 删除整行 select，并删除 BT_TICKER_NEXT_SLOT_GET | 没有旧 LL 实现；其他 shell 配置继续保留 |
| LL/STM32 专用 TX 栈大小 default | 删除整行平台默认值 | userchan 的 512 和通用 fallback 保留，首期值不变 |
| Controller DF 专用命令 buffer default | 删除整行 default 及只解释该行的注释 | 首期 Classic 的 255 不变，独立通用 fallback 保留 |
| Board 专用 vendor-extension heuristics default | 删除整行 default | 首期已为 n，普通选项仍可手工选择 |
| Audio shell USB 配置块 | 删除 7 个完整 config 和其包围的强制 if 条件，移除 bap_usb.c 的 CMake选择 | 它们强制要求已移除 USB device stack；保留 Audio 协议与现存 C 文件 |
| MULTITHREADING 的 settings C 条件 | 删除三个条件包围，保留 mutex 声明、lock 与 unlock 主体 | 清理前固定 y；不因删配置意外去掉同步保护，也不代表已经改单线程 |
| MODULES 内建解析状态 | 删除 config；driver 只忽略无文本依赖的 Kconfiglib 内建对象 | 不新增 m 支持；实际 MODULES 依赖仍视为未定义错误 |

解除强制条件时保留的是确有非平台实现的 Host 功能；不能使用外部 Controller 的
编译开关声称硬件支持功能。保留的运行能力仍需后续 HCI/POSIX/PSA bring-up 验证。
本次没有无法评估而等待用户决定的强制关联。

### 2.1 默认值变化

- 删除 BT_HCI_ACL_FLOW_CONTROL 的平台 default 后，其一般默认值可能成为 n。
  config/posix_host_defconfig 显式加入 CONFIG_BT_HCI_ACL_FLOW_CONTROL=y，保持原接收流控
  与 buffer 需求。与清理前基线逐符号比较，所有保留配置的求值结果一致。
- 删除 NUS 的 default y if !UART_BT 后，启用 NUS 时 BT_ZEPHYR_NUS_DEFAULT_INSTANCE
  的默认值从 y 变为 n。以后需要默认实例时显式选择，选项及自定义实例 API仍保留。
- 普通源代码仍有以前的 Controller/USB条件、注释和旧日志引用；当前此次任务清理的是
  配置模型及实际构建入口，不进行全量协议 C 分支重构。settings 锁是本次必要同步处理。

## 3. 实际表达式变更

每条引用指向当前仍存在的文件。删除内容仅描述表达式，不引用已删除文件的历史行号。

| 文件 | 删除/替换前表达式 | 实际处理 |
| --- | --- | --- |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on BT_PER_ADV_SYNC && BT_CONN && (!HAS_BT_CTLR &#124;&#124; BT_CTLR_SYNC_TRANSFER_RECEIVER_SUPPORT)` | `depends on BT_PER_ADV_SYNC / 	depends on BT_CONN` |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on (BT_PER_ADV_SYNC &#124;&#124; BT_PER_ADV) && BT_CONN && (!HAS_BT_CTLR &#124;&#124; BT_CTLR_SYNC_TRANSFER_SENDER_SUPPORT)` | `depends on BT_PER_ADV_SYNC &#124;&#124; BT_PER_ADV / 	depends on BT_CONN` |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on (!HAS_BT_CTLR &#124;&#124; BT_CTLR_FRAME_SPACE_UPDATE_SUPPORT) && BT_CONN && BT_LE_EXTENDED_FEAT_SET` | `depends on BT_CONN / 	depends on BT_LE_EXTENDED_FEAT_SET` |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `depends on BT_BROADCASTER && BT_OBSERVER && !BT_LL_SW_SPLIT` | `depends on BT_BROADCASTER / 	depends on BT_OBSERVER` |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `depends on BT_HOST_CRYPTO &#124;&#124; BT_CTLR_CRYPTO` | `depends on BT_HOST_CRYPTO` |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `depends on BT_HCI_VS && !HAS_BT_CTLR` | `depends on BT_HCI_VS` |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `int "Number of HCI command buffers" if !BT_HCI_RAW &#124;&#124; !HAS_BT_CTLR` | `int "Number of HCI command buffers"` |
| [bluetooth/Kconfig.adv](../bluetooth/Kconfig.adv) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_ADV_EXT_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig.adv](../bluetooth/Kconfig.adv) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_PHY_CODED` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_EXTENDED_FEAT_SET_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `default y if !HAS_BT_CTLR && !BT_STM32_IPM && !BT_ESP32  && !BT_STM32WBA` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_PHY_UPDATE_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_DATA_LEN_UPDATE_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_SCA_UPDATE_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_LE_POWER_CONTROL_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_LE_PATH_LOSS_MONITORING_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_SUBRATING_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_CHANNEL_SOUNDING_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_SHORTER_CONNECTION_INTERVALS_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_PERIPHERAL_ISO_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_CENTRAL_ISO_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_ADV_ISO_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_SYNC_ISO_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/Kconfig](../bluetooth/Kconfig) | `select BT_TICKER_NEXT_SLOT_GET if BT_LL_SW_SPLIT` | 删除整行/专用分支 |
| [bluetooth/mesh/Kconfig](../bluetooth/mesh/Kconfig) | `depends on BT_CTLR_ADV_EXT &#124;&#124; !HAS_BT_CTLR` | 删除整行/专用分支 |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `default 83 if (!BT_EXT_ADV && !BT_CLASSIC && BT_CTLR_DF && BT_CTLR_DF_CONN_CTE_REQ && !SOC_COMPATIBLE_NRF)` | 删除整行/专用分支 |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `depends on !BT_HCI_RAW &#124;&#124; !HAS_BT_CTLR &#124;&#124; !BT_HCI_ACL_FLOW_CONTROL` | 删除整行/专用分支 |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `depends on BT_HAS_HCI_VS &#124;&#124; !HAS_BT_CTLR` | 删除整行/专用分支 |
| [bluetooth/common/Kconfig](../bluetooth/common/Kconfig) | `default y if BOARD_QEMU_X86 &#124;&#124; BOARD_QEMU_CORTEX_M3 &#124;&#124; BOARD_NATIVE_SIM` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `default 1280 if BT_LL_SW_SPLIT && (NO_OPTIMIZATIONS &#124;&#124; BT_ISO_BROADCAST)` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `default 1024 if BT_LL_SW_SPLIT && BT_CENTRAL` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `default 768 if BT_LL_SW_SPLIT` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `default 640 if BT_STM32_IPM` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `depends on !BT_CTLR_CRYPTO` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `select MPU_ALLOW_FLASH_WRITE if ARM_MPU` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_EXT_SCAN_FP` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_DF_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_DF_CTE_RX_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_DF_CTE_TX_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_DF_CTE_RX_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/host/Kconfig](../bluetooth/host/Kconfig) | `depends on !HAS_BT_CTLR &#124;&#124; BT_CTLR_DF_CTE_TX_SUPPORT` | 删除整行/专用分支 |
| [bluetooth/services/nus/Kconfig.nus](../bluetooth/services/nus/Kconfig.nus) | `default y if !UART_BT` | 删除整行/专用分支 |
| [bluetooth/audio/shell/CMakeLists.txt](../bluetooth/audio/shell/CMakeLists.txt) | `CONFIG_BT_PBP OR /    (CONFIG_LIBLC3 AND CONFIG_USBD_AUDIO2_CLASS))` | `CONFIG_BT_PBP)` |
| [bluetooth/audio/shell/CMakeLists.txt](../bluetooth/audio/shell/CMakeLists.txt) | `if(CONFIG_LIBLC3 AND CONFIG_USBD_AUDIO2_CLASS) /     embt_library_sources(bap_usb.c) /   endif()` | 删除整行/专用分支 |

## 4. 验证与未完成工作

- 根模型和 POSIX defconfig 解析无警告/缺失类型，生成 1014 项 typed JSON；被删除定义
  不进入 JSON 和配置头。Kconfiglib 的无引用 MODULES 内建状态不会被误算成缺失依赖。
- 在真实 PTY 打开并退出 menuconfig；CMake configure/generate 通过。
- 编译图仍包含同一批 51 个唯一 C 源文件。实际编译继续进入协议源码，并因旧日志/zephyr
  头引用返回非零，未宣称完整编译/运行成功。
- 比较清理前后 POSIX 基线，保留项全部等值；NUS 场景的默认实例变化已单独记录。
- 原配置头 include 继续保留；根 LICENSE 内容不变，duplicate LICENSE 已删除。
- 活动 build/posix/.config 已备份后仅移除已废弃名字，再求值保留其他用户选择。
  其他旧构建目录可能仍含已删除选项，工具会报告需迁移，不会隐式复活配置。

原始 diff、决策与日志在 /tmp/embt-controller-config-retirement/；包括 before/、
result.json、decisions.json、menuconfig.log、cmake-configure.log、build.log。
头文件 namespace、C 层日志、POSIX transport/provider 和动态初始化仍未完成。
