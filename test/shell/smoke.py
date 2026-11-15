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
#

"""Exercise btshell transport-free CLI behavior without invoking Bluetooth handlers."""

import argparse
import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import subprocess
import time
import termios


def batch(binary, data=b"", options=(), expected=0):
    result = subprocess.run(
        [str(binary), *options], input=data, capture_output=True, timeout=8
    )
    assert result.returncode == expected, (result.returncode, result.stderr)
    return result.stdout.decode(), result.stderr.decode()


def until(fd, token, process):
    data = bytearray()
    deadline = time.monotonic() + 5
    while token not in data:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise AssertionError(("PTY output timeout", bytes(data)))
        readable, _, _ = select.select([fd], [], [], remaining)
        if readable:
            try:
                chunk = os.read(fd, 4096)
            except OSError as error:
                if error.errno == errno.EIO:
                    chunk = b""
                else:
                    raise
            if not chunk:
                raise AssertionError(("PTY closed before prompt", bytes(data), process.poll()))
            data.extend(chunk)
    return bytes(data)


def own_terminal():
    os.setsid()
    fcntl.ioctl(0, termios.TIOCSCTTY, 0)


def terminal(binary, exit_bytes, expected):
    master, slave = pty.openpty()
    process = subprocess.Popen(
        [str(binary)], stdin=slave, stdout=slave, stderr=slave, close_fds=True, preexec_fn=own_terminal
    )
    os.close(slave)
    try:
        data = until(master, b"btshell> ", process)
        assert b"Bluetooth is disabled" in data
        os.write(master, b"help br l2cap\n")
        data = until(master, b"btshell> ", process)
        assert b"register" in data and b"connect" in data
        os.write(master, exit_bytes)
        assert process.wait(timeout=5) == expected
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        os.close(master)


def run(binary):
    passed = []
    output, errors = batch(binary, options=("--help",))
    assert "Usage: btshell" in output and not errors
    passed.append("process help")
    output, errors = batch(binary, options=("--version",))
    assert output.strip() == "EmBT btshell" and not errors
    passed.append("process version")

    output, errors = batch(binary, b"help\nhelp br l2cap\nhelp bt scan-filter-set\nversion\nquit\n")
    for root in ("bt", "gatt", "l2cap", "br"):
        assert root in output
    assert "rssi" in output and "register" in output and output.endswith("Bye.\n")
    assert not errors
    passed.append("roots and nested help without handlers")

    output, errors = batch(binary, b"help 'br' \"l2cap\"\nhelp b\\r l2cap\n\t  version\t\r\nq\n")
    assert output.count("register") == 2 and not errors
    passed.append("quotes escapes tabs and CRLF")

    output, errors = batch(binary, b"help 'br\nhelp br\\\nversion\nexit extra\nquit\n")
    assert errors.count("Invalid command arguments") == 2
    assert "Exit takes no arguments" in errors and output.endswith("Bye.\n")
    passed.append("malformed input recovery")

    output, errors = batch(binary, b"help " + b"a " * 65 + b"\nversion\nquit\n")
    assert "Invalid command arguments" in errors and "Bye." in output
    passed.append("argument limit")

    output, errors = batch(binary, b"x" * 5000 + b"\nversion\nquit\n")
    assert "Invalid command line" in errors and "EmBT btshell\n" in output
    passed.append("line overflow drains the entire line")

    output, errors = batch(binary, b"help br\x00l2cap\nversion\nquit\n")
    assert "Invalid command line" in errors and output.endswith("Bye.\n")
    passed.append("embedded NUL rejection")

    output, errors = batch(binary, b"unknown-command\nhelp absent\nhelp \"\"\nq\n")
    assert errors.count("Command failed") == 3 and output.endswith("Bye.\n")
    passed.append("unknown commands and empty quoted token")

    output, errors = batch(binary, b"version")
    assert output.endswith("Bye.\n") and not errors
    passed.append("EOF with unterminated last line")
    output, errors = batch(binary)
    assert output.endswith("Bye.\n") and not errors
    passed.append("empty EOF")

    for options in (("--hci", "0"), ("--tcp", "127.0.0.1", "9"), ("--unix", "/tmp/embt-absent.sock"), ("--hci", "0", "--no-flow-control")):
        output, errors = batch(binary, b"help\nquit\n", options=options)
        assert "Bluetooth is disabled" in output and not errors
    passed.append("endpoint selection opens no transport")

    for options in (("--hci", "-1"), ("--hci", "65535"), ("--tcp", "localhost", "0"),
                    ("--unix", "relative"), ("--hci", "0", "--unix", "/tmp/x"), ("--no-flow-control",)):
        output, errors = batch(binary, options=options, expected=2)
        assert "Invalid transport configuration" in errors
    passed.append("invalid endpoint arguments")

    terminal(binary, b"quit\n", 0)
    passed.append("PTY prompt help and quit")
    terminal(binary, b"\x04", 0)
    passed.append("PTY Ctrl-D")
    terminal(binary, b"\x03", -2)
    passed.append("PTY Ctrl-C")
    print(f"Passed {len(passed)} CLI checks; no Bluetooth handler executed.")
    for case in passed:
        print(f"  PASS: {case}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    run(args.binary.resolve())
