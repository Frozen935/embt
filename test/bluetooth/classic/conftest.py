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
# SPDX-License-Identifier: Apache-2.0
"""Linked Bumble Controllers and fresh EmBT subprocesses for Classic tests."""
import os
import re
import selectors
import signal
import subprocess
import sys
import time
from pathlib import Path
import pytest

DIRECTORY = Path(__file__).resolve().parent
ROOT = DIRECTORY.parents[2]
sys.path.insert(0, str(DIRECTORY))
from embt_harness import DeviceAdapter, Shell


def pytest_addoption(parser):
    parser.addoption('--hci-transport', default=None)
    parser.addoption('--embt-build', default=None)


@pytest.fixture
def controller_pair(tmp_path):
    log = (tmp_path / 'controllers.log').open('w')
    command = [sys.executable, str(DIRECTORY / 'bumble/common/controllers.py'),
               'tcp-server:127.0.0.1:0@00:00:01:00:00:01',
               'tcp-server:127.0.0.1:0@00:00:01:00:00:02']
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=log,
                               start_new_session=True)
    ports = []
    try:
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        pending = b''
        deadline = time.monotonic() + 15
        while len(ports) < 2:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not selector.select(remaining):
                raise RuntimeError('Bumble Controller startup timed out')
            data = os.read(process.stdout.fileno(), 4096)
            if not data:
                raise RuntimeError('Bumble Controller exited during startup')
            pending += data
            while b'\n' in pending:
                line, pending = pending.split(b'\n', 1)
                if line.startswith(b'HCI'):
                    ports.append(int(line.split()[-1]))
        selector.close()
        yield ports
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
        process.wait(timeout=5)
        log.close()


@pytest.fixture
def dut(request, controller_pair, tmp_path, monkeypatch):
    name = request.node.path.parent.parent.name
    if name in ('rfcomm_c', 'rfcomm_s'):
        request.module.rfcomm_utility.install(monkeypatch)
    build = request.config.getoption('--embt-build')
    executable = Path(build) if build else ROOT / 'build' / ('upstream-classic_' + name) / 'test/bluetooth/classic' / ('test_bt_classic_' + name)
    if not executable.is_file():
        pytest.fail(f'Classic test executable is unavailable: {executable}')
    device = DeviceAdapter([str(executable), '--tcp', '127.0.0.1', str(controller_pair[0])],
                           tmp_path / 'iut.log')
    try:
        device.readlines_until('Bluetooth is disabled', timeout=10)
        device.wait_for_prompt(timeout=10)
        yield device
    finally:
        status = device.close()
        if status not in (0, 75):
            pytest.fail(f'EmBT subprocess exited with status {status}')


@pytest.fixture
def shell(dut):
    return Shell(dut)


@pytest.fixture
def initialize(request, shell, dut, controller_pair):
    def command_until(command, pattern, timeout=20):
        lines = shell.exec_command(command, timeout=timeout)
        if not any(re.search(pattern, line) for line in lines):
            lines.extend(dut.readlines_until(pattern, timeout=timeout))
        return lines

    lines = command_until('bt init', 'Bluetooth initialized')
    address = None
    for line in lines:
        match = re.search(r'Identity: *[PR]:(([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})', line)
        if match:
            address = match.group(1)
    if address is None:
        pytest.fail('Controller identity not printed during bt init')
    name = request.node.path.parent.parent.name
    if name not in ('gap_c', 'gap_s', 'rfcomm_c', 'rfcomm_s'):
        command_until('br pscan on', 'BR/EDR set/reset connectable done', timeout=10)
        command_until('br iscan on', 'BR/EDR set/reset discoverable done', timeout=10)
    if name in ('a2dp_sink', 'a2dp_source') and 'register_ep' not in request.node.name:
        # Upstream cases inherit this state from the first session-wide registration case.
        # Fresh POSIX processes recreate that precondition explicitly.
        role = 'sink' if name == 'a2dp_sink' else 'source'
        command_until(f'{name} register_cb', 'success', timeout=10)
        command_until(f'{name} register_ep {role} sbc',
                      f'SBC {role} endpoint is registered', timeout=10)
    if name in ('l2cap_c', 'l2cap_s'):
        command_until('l2cap_br register 1001', 'registered', timeout=10)
    return f'tcp-client:127.0.0.1:{controller_pair[1]}', address


@pytest.fixture
def device_under_test(initialize):
    return initialize


@pytest.fixture
def a2dp_sink_dut(initialize):
    return initialize


@pytest.fixture
def a2dp_source_dut(initialize):
    return initialize


@pytest.fixture
def l2cap_br_dut(initialize):
    return initialize


@pytest.fixture
def sdp_client_dut(initialize):
    return initialize


@pytest.fixture
def sdp_server_dut(initialize):
    return initialize


@pytest.fixture
def smp_initiator_dut(initialize):
    return initialize


@pytest.fixture(autouse=True)
def isolate_test_artifacts(monkeypatch, tmp_path):
    """Keep peer snoops and stores inside the current test's private directory."""
    monkeypatch.chdir(tmp_path)
