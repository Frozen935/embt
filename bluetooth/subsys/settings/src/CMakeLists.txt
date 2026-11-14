# SPDX-License-Identifier: Apache-2.0

zephyr_sources(
  settings_store.c
  settings.c
  settings_line.c
)

zephyr_sources_ifdef(CONFIG_SETTINGS_RUNTIME settings_runtime.c)
