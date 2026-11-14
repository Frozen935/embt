# SPDX-License-Identifier: Apache-2.0

embt_library(embt_settings)

embt_library_sources(
  settings_store.c
  settings.c
  settings_line.c
)

embt_library_sources_ifdef(CONFIG_SETTINGS_RUNTIME settings_runtime.c)
