#!/usr/bin/env bash
# /****************************************************************************
#  *  Copyright (C) 2026 EmBT contributors
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  ***************************************************************************/

set -euo pipefail

embt_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat <<'HELP'
Usage: ./mkbt.sh [build|configure|menuconfig|olddefconfig|defconfig|savedefconfig|clean] [options]

  --build-dir DIR       Build directory (default: build/posix)
  --defconfig FILE      Initial configuration; use defconfig to reset an existing one
  --fragment FILE       Initial/reset overlay, repeatable in override order
  --output FILE         Required output for savedefconfig
  --force               Replace an existing savedefconfig output
  -j, --jobs N          Parallel build jobs (default: 8)
  --verbose             Show compiler commands
  --cmake-arg ARG       Extra CMake configure argument, repeatable
  -h, --help            Show this help

All relative paths are resolved from the EmBT source root.
Menu operations use Python directly and do not require a configured CMake build.
EMBT_PYTHON selects the Python interpreter (default: python3).
Configuration headers are generated automatically; existing includes are retained.
HELP
}

fail() { printf 'mkbt: %s\n' "$*" >&2; exit 2; }
need_value() { (($# >= 2)) || fail "Missing value for $1"; }
absolute() {
  [[ "$1" != *';'* && "$1" != *$'\n'* ]] || fail 'Paths with semicolons/newlines are not supported by this CMake entry point'
  if [[ "$1" = /* ]]; then printf '%s\n' "$1"; else printf '%s/%s\n' "$embt_root" "$1"; fi
}

action=build
if (($#)) && [[ "$1" != -* ]]; then action="$1"; shift; fi
case "$action" in
  build|configure|menuconfig|olddefconfig|defconfig|savedefconfig|clean) ;;
  help) usage; exit 0 ;;
  *) fail "Unknown action: $action" ;;
esac
build_dir="$embt_root/build/posix"
defconfig=""
fragments=()
config_args=()
cmake_args=()
output=""
force=0
verbose=0
jobs=8
while (($#)); do
  case "$1" in
    --build-dir) need_value "$@"; build_dir="$(absolute "$2")"; shift 2 ;;
    --defconfig) need_value "$@"; defconfig="$(absolute "$2")"; shift 2 ;;
    --fragment) need_value "$@"; fragments+=("$(absolute "$2")"); shift 2 ;;
    --output) need_value "$@"; output="$(absolute "$2")"; shift 2 ;;
    --force) force=1; shift ;;
    -j|--jobs) need_value "$@"; jobs="$2"; [[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail 'Jobs must be a positive integer'; shift 2 ;;
    --verbose) verbose=1; shift ;;
    --cmake-arg) need_value "$@"; cmake_args+=("$2"); shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) fail "Unknown option: $1" ;;
  esac
done

if [[ "$action" != savedefconfig && ( -n "$output" || "$force" != 0 ) ]]; then
  fail 'Output/force options apply only to savedefconfig'
fi
if ((${#cmake_args[@]})) && [[ "$action" != configure && "$action" != build && "$action" != defconfig ]]; then
  fail 'CMake arguments apply only to configure, build or defconfig'
fi

python_cmd="${EMBT_PYTHON:-python3}"
python_path="$(command -v "$python_cmd")" || fail "Python interpreter not found: $python_cmd"
config_args=(--build-dir "$build_dir")
if [[ -n "$defconfig" ]]; then config_args+=(--defconfig "$defconfig"); fi
fragment_list=""
for fragment in "${fragments[@]}"; do
  config_args+=(--fragment "$fragment")
  if [[ -n "$fragment_list" ]]; then fragment_list+=';'; fi
  fragment_list+="$fragment"
done

configure_build() {
  local command=(cmake -S "$embt_root" -B "$build_dir" -G Ninja "-DPython3_EXECUTABLE=$python_path")
  if [[ -n "$defconfig" ]]; then command+=("-DEMBT_DEFCONFIG:FILEPATH=$defconfig"); fi
  if ((${#fragments[@]})) || [[ "$action" = defconfig ]]; then
    command+=("-DEMBT_CONFIG_FRAGMENTS:STRING=$fragment_list")
  fi
  command+=("${cmake_args[@]}")
  "${command[@]}"
}

case "$action" in
  menuconfig|olddefconfig)
    [[ -z "$output" && "$force" = 0 && ${#cmake_args[@]} = 0 ]] || fail 'Options are not applicable to this configuration action'
    exec "$python_path" "$embt_root/tools/kconfig/configure.py" "$action" "${config_args[@]}"
    ;;
  savedefconfig)
    [[ -n "$output" ]] || fail 'savedefconfig requires --output'
    config_args+=(--output "$output")
    if ((force)); then config_args+=(--force); fi
    exec "$python_path" "$embt_root/tools/kconfig/configure.py" "$action" "${config_args[@]}"
    ;;
  defconfig)
    if [[ -z "$defconfig" ]]; then defconfig="$embt_root/config/posix_host_defconfig"; fi
    "$python_path" "$embt_root/tools/kconfig/configure.py" defconfig "${config_args[@]}"
    if [[ -f "$build_dir/CMakeCache.txt" ]]; then configure_build; fi
    ;;
  configure|build)
    [[ -z "$output" && "$force" = 0 ]] || fail 'Output/force options apply only to savedefconfig'
    configure_build
    if [[ "$action" = build ]]; then
      command=(cmake --build "$build_dir" --target embt --parallel "$jobs")
      if ((verbose)); then command+=(--verbose); fi
      "${command[@]}"
    fi
    ;;
  clean)
    if [[ -f "$build_dir/CMakeCache.txt" ]]; then
      cmake --build "$build_dir" --target clean
    else
      printf 'No configured build directory to clean: %s\n' "$build_dir"
    fi
    ;;
esac
