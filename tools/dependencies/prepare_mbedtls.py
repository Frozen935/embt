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

"""Prepare the explicitly requested, checksummed Mbed TLS source release."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, help="Use an existing archive instead of downloading")
    args = parser.parse_args()
    lock = json.loads((ROOT / "third_party/dependencies.lock.json").read_text())["mbedtls"]
    destination = ROOT / lock["source_subdirectory"]
    if destination.exists():
        raise SystemExit(f"Destination already exists: {destination}; keep or remove it explicitly")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".prepare-", dir=destination.parent) as temporary:
        temp = Path(temporary)
        if args.archive:
            archive = args.archive.resolve()
        else:
            archive = temp / "release.tar.bz2"
            request = urllib.request.Request(lock["archive_url"], headers={"User-Agent": "EmBT-dependency-preparation"})
            with urllib.request.urlopen(request, timeout=30) as response, archive.open("wb") as output:
                shutil.copyfileobj(response, output)
        checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
        if checksum != lock["archive_sha256"]:
            raise SystemExit("Mbed TLS archive checksum does not match dependencies.lock.json")
        stage = temp / "source"
        stage.mkdir()
        with tarfile.open(archive) as bundle:
            for member in bundle:
                name = PurePosixPath(member.name)
                if name.is_absolute() or ".." in name.parts or name.parts[0] != lock["archive_root"]:
                    raise SystemExit(f"Unexpected archive path: {member.name}")
                relative = Path(*name.parts[1:])
                if not relative.parts:
                    continue
                target = stage / relative
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                elif member.isfile():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with bundle.extractfile(member) as source, target.open("wb") as output:
                        shutil.copyfileobj(source, output)
                    target.chmod(member.mode & 0o777)
                else:
                    raise SystemExit(f"Unsupported archive entry: {member.name}")
        if not (stage / "CMakeLists.txt").is_file() or not (stage / "framework/CMakeLists.txt").is_file():
            raise SystemExit("The release archive lacks required source/build files")
        (stage / ".embt-source.json").write_text(json.dumps(lock, indent=2) + "\n")
        stage.rename(destination)
    print(f"Prepared Mbed TLS {lock['version']}: {destination}")


if __name__ == "__main__":
    main()
