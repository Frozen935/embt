#!/usr/bin/env python3
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

"""Evaluate EmBT configuration once for the menu, headers and build graph."""

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_VERSION = 1


def fail(message):
    raise ValueError(message)


def project_path(value):
    path = Path(value)
    return (path if path.is_absolute() else ROOT / path).resolve()


def write_if_changed(path, data):
    path = Path(path)
    if isinstance(data, str):
        data = data.encode()
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def json_text(data):
    return json.dumps(data, indent=2, sort_keys=True, ensure_ascii=True) + "\n"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_model(conf, lib):
    # Kconfiglib creates MODULES internally even in a boolean-only model.
    # Ignore only that unreferenced parser sentinel, not actual dependencies.
    module_referenced = any(conf.modules in node.referenced for node in conf.node_iter())
    missing = [name for name, sym in conf.syms.items()
               if not sym.nodes and not sym.is_constant
               and not (sym is conf.modules and not module_referenced)
               and not re.fullmatch(r"-?(?:[0-9]+|0[xX][0-9a-fA-F]+)", name)]
    untyped = [sym.name for sym in conf.unique_defined_syms if sym.type == lib.UNKNOWN]
    if missing or untyped or conf.warnings:
        details = conf.warnings + ["Undefined symbols: " + ", ".join(sorted(missing))] * bool(missing)
        details += ["Untyped symbols: " + ", ".join(sorted(untyped))] * bool(untyped)
        fail("Invalid Kconfig model:\n" + "\n".join(details))


def load_checked(conf, path, replace=True):
    if not path.is_file():
        fail(f"Configuration input does not exist: {path}")
    before = len(conf.warnings)
    conf.load_config(str(path), replace=replace)
    if conf.missing_syms:
        fail("Unknown or retired configuration options: "
             + ", ".join(sorted({name for name, _ in conf.missing_syms})))
    if len(conf.warnings) != before:
        fail("Invalid configuration input:\n" + "\n".join(conf.warnings[before:]))


def values(conf, lib):
    result = {}
    for sym in conf.unique_defined_syms:
        value = sym.str_value
        kind = lib.TYPE_TO_STR[sym.type]
        if sym.type in (lib.INT, lib.HEX) and sym.user_value is not None and sym.visibility:
            base = 10 if sym.type == lib.INT else 16
            requested = int(sym.user_value, base)
            for low, high, condition in sym.ranges:
                if lib.expr_value(condition):
                    if not int(low.str_value, base) <= requested <= int(high.str_value, base):
                        fail(f"CONFIG_{sym.name}={sym.user_value} is outside "
                             f"the range {low.str_value}..{high.str_value}")
                    break
        if sym.type in (lib.BOOL, lib.TRISTATE):
            if value == "m":
                fail(f"CONFIG_{sym.name}=m is not supported")
            value = value == "y"
            kind = "bool"
        result["CONFIG_" + sym.name] = {"type": kind, "value": value}
    if conf.syms["EMBT_STACK"].tri_value:
        little = conf.syms["EMBT_LITTLE_ENDIAN"].tri_value
        big = conf.syms["EMBT_BIG_ENDIAN"].tri_value
        if bool(little) == bool(big):
            fail("Select exactly one of EMBT_LITTLE_ENDIAN and EMBT_BIG_ENDIAN")
    return result


def emit(conf, lib, build, provenance):
    settings = values(conf, lib)
    generated = build / "generated/include"
    metadata = build / "kconfig"
    with tempfile.TemporaryDirectory(prefix=".evaluate-", dir=metadata) as temporary:
        temp = Path(temporary)
        conf.write_config(str(temp / ".config"), save_old=False)
        conf.write_autoconf(str(temp / "autoconf.h"))
        config_data = (temp / ".config").read_bytes()
        config_hash = hashlib.sha256(config_data).hexdigest()
        header = ("/* Generated by EmBT. Do not edit. */\n"
                  "#ifndef EMBT_GENERATED_AUTOCONF_H_\n"
                  "#define EMBT_GENERATED_AUTOCONF_H_\n"
                  + (temp / "autoconf.h").read_text()
                  + "\n#endif /* EMBT_GENERATED_AUTOCONF_H_ */\n")
        forwarding = ("/* Generated compatibility path for imported sources. */\n"
                      "#ifndef EMBT_GENERATED_ZEPHYR_AUTOCONF_H_\n"
                      "#define EMBT_GENERATED_ZEPHYR_AUTOCONF_H_\n"
                      '#include "../embt/autoconf.h"\n'
                      "#endif /* EMBT_GENERATED_ZEPHYR_AUTOCONF_H_ */\n")
        dependencies = sorted({str(project_path(name)) for name in conf.kconfig_filenames}
                              | {str(Path(__file__).resolve()), str(metadata / "inputs.json")}
                              | {item["path"] for item in provenance["files"]})
        payload = {"schema_version": SCHEMA_VERSION, "config_sha256": config_hash,
                   "symbols": settings, "inputs": provenance, "dependencies": dependencies}
        write_if_changed(build / ".config", config_data)
        write_if_changed(generated / "embt/autoconf.h", header)
        write_if_changed(generated / "zephyr/autoconf.h", forwarding)
        write_if_changed(metadata / "config.json", json_text(payload))
        write_if_changed(metadata / "dependencies.json", json_text(dependencies))
    for sym in conf.unique_defined_syms:
        if sym.user_value is not None and sym.type == lib.BOOL and sym.user_value != sym.tri_value:
            print(f"Adjusted CONFIG_{sym.name}: requested {lib.TRI_TO_STR[sym.user_value]}, "
                  f"effective {sym.str_value} (dependencies or selections)", file=sys.stderr)
    print(f"Configuration: {build / '.config'} ({config_hash[:12]})")


def run(args):
    try:
        import kconfiglib as lib
    except ImportError:
        fail("Install build dependencies: python3 -m pip install -r tools/requirements-build.txt")
    if lib.VERSION != (14, 1, 0):
        fail("EmBT requires Kconfiglib 14.1.0; install tools/requirements-build.txt")
    if args.action == "menuconfig":
        if not sys.stdin.isatty() or not sys.stdout.isatty() or os.environ.get("TERM", "dumb") == "dumb":
            fail("menuconfig requires an interactive terminal with TERM set")
        import curses
        import menuconfig
        try:
            curses.setupterm()
        except curses.error as error:
            fail(f"Cannot initialize menuconfig terminal: {error}")

    build = project_path(args.build_dir)
    if build == ROOT or (ROOT / "Kconfig").is_relative_to(build):
        fail("Use a separate build directory, not the source root or its parent")
    build.mkdir(parents=True, exist_ok=True)
    metadata = build / "kconfig"
    metadata.mkdir(exist_ok=True)
    with (metadata / ".lock").open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            fail(f"Another configuration process is using {build}")
        config = build / ".config"
        inputs_file = metadata / "inputs.json"
        existing_inputs = json.loads(inputs_file.read_text()) if inputs_file.exists() else None
        os.environ["srctree"] = str(ROOT)
        os.environ["KCONFIG_CONFIG"] = str(config)
        os.environ.pop("KCONFIG_FUNCTIONS", None)
        try:
            conf = lib.Kconfig(str(ROOT / "Kconfig"), warn_to_stderr=False)
        except lib.KconfigError as error:
            fail(str(error))
        conf.warn_assign_undef = True
        conf.warn_assign_override = False
        conf.warn_assign_redun = False
        check_model(conf, lib)
        reset = args.action == "defconfig"
        if args.action in ("olddefconfig", "savedefconfig") and not config.exists():
            fail(f"No existing configuration: {config}")
        chosen_base = str(project_path(args.defconfig)) if args.defconfig else None
        chosen_fragments = [str(project_path(p)) for p in args.fragment] if args.fragment is not None else None
        if config.exists() and not reset:
            if existing_inputs:
                if chosen_base and chosen_base != existing_inputs["defconfig"]:
                    fail("Different defconfig requested; use mkbt.sh defconfig to reset explicitly")
                if chosen_fragments is not None and chosen_fragments != existing_inputs["fragments"]:
                    fail("Different fragments requested; use mkbt.sh defconfig to reset explicitly")
            elif chosen_base or chosen_fragments:
                fail("Existing .config has no input record; use defconfig for an explicit reset")
            provenance = existing_inputs or {"defconfig": None, "fragments": [], "files": []}
            load_checked(conf, config)
            for item in provenance["files"]:
                path = Path(item["path"])
                if not path.exists() or digest(path) != item["sha256"]:
                    print(f"Initial input changed; preserving .config: {path}", file=sys.stderr)
        else:
            base = Path(chosen_base) if chosen_base else ROOT / "config/posix_host_defconfig"
            fragments = chosen_fragments or []
            load_checked(conf, base)
            for name in fragments:
                load_checked(conf, Path(name), replace=False)
            provenance = {"defconfig": str(base), "fragments": fragments,
                          "files": [{"path": str(path), "sha256": digest(path)}
                                    for path in [base, *map(Path, fragments)]]}
        values(conf, lib)
        if args.action == "savedefconfig":
            if not args.output:
                fail("savedefconfig requires --output")
            output = project_path(args.output)
            if output == config:
                fail("savedefconfig output must not overwrite the active .config")
            if output.exists() and not args.force:
                fail(f"Output exists: {output}; use --force to replace it")
            with tempfile.TemporaryDirectory(dir=metadata) as temporary:
                temp = Path(temporary) / "defconfig"
                conf.write_min_config(str(temp))
                write_if_changed(output, temp.read_bytes())
            print(f"Saved minimal configuration: {output}")
            return
        if reset and config.exists():
            write_if_changed(build / ".config.old", config.read_bytes())
        write_if_changed(inputs_file, json_text(provenance))
        if args.action == "menuconfig":
            if not config.exists():
                emit(conf, lib, build, provenance)
                print("Initialized the configuration before opening menuconfig.")
            before = config.read_bytes()
            previous_cwd = Path.cwd()
            try:
                os.chdir(build)
                try:
                    menuconfig.menuconfig(conf)
                except curses.error as error:
                    fail(f"menuconfig failed: {error}")
            finally:
                os.chdir(previous_cwd)
            conf.warn = True
            conf.warnings.clear()
            # Discard unsaved in-memory choices after the UI exits.
            load_checked(conf, config)
            print("Menu changes saved." if before != config.read_bytes() else "Configuration unchanged by menu.")
        emit(conf, lib, build, provenance)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("configure", "menuconfig", "defconfig", "olddefconfig", "savedefconfig"))
    parser.add_argument("--build-dir", default="build/posix")
    parser.add_argument("--defconfig")
    parser.add_argument("--fragment", action="append", default=None)
    parser.add_argument("--output")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    try:
        run(args)
    except (ValueError, OSError, ImportError) as error:
        print(f"Configuration failed: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Configuration interrupted.", file=sys.stderr)
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
