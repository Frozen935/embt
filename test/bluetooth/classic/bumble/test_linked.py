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
"""Original two-role C regressions over linked Bumble Controllers."""
import os
import selectors
import signal
import subprocess
import time
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[4]


def run_pair(executable, ports, role_case, ready_marker, tmp_path, timeout):
    processes = []
    try:
        peripheral_args = [str(executable), str(ports[1]), '00:00:01:00:00:01', 'peripheral']
        central_args = [str(executable), str(ports[0]), '00:00:01:00:00:02', 'central']
        if role_case:
            peripheral_args.append('gap_peripheral__' + role_case)
            central_args.append('gap_central__' + role_case)
        peripheral = subprocess.Popen(peripheral_args, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, text=True, start_new_session=True)
        processes.append(peripheral)
        def read_until(marker, wait_timeout=20):
            selector = selectors.DefaultSelector()
            selector.register(peripheral.stdout, selectors.EVENT_READ)
            captured = b''
            deadline = time.monotonic() + wait_timeout
            try:
                while time.monotonic() < deadline:
                    if not selector.select(0.2):
                        continue
                    data = os.read(peripheral.stdout.fileno(), 4096)
                    if not data:
                        pytest.fail('Peripheral exited before ' + marker + ': ' + captured.decode())
                    captured += data
                    if marker.encode() in captured:
                        return captured.decode()
                pytest.fail('Peripheral did not reach ' + marker + ': ' + captured.decode())
            finally:
                selector.close()

        lines = [read_until(ready_marker)]
        central = subprocess.Popen(central_args, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, start_new_session=True,
                                   stdin=subprocess.PIPE,
                                   env=dict(os.environ, EMBT_TEST_PEER_SYNC="1"))
        processes.append(central)
        lines.append(read_until("PEER_CONNECTED", timeout))
        central_out, _ = central.communicate(input="x\n", timeout=timeout)
        peripheral_out, _ = peripheral.communicate(timeout=timeout)
        (tmp_path / 'central.log').write_text(central_out)
        (tmp_path / 'peripheral.log').write_text(''.join(lines) + peripheral_out)
        assert central.returncode == 0, central_out
        assert peripheral.returncode == 0, ''.join(lines) + peripheral_out
        assert '[  PASSED  ] 1 test(s).' in central_out
        assert '[  PASSED  ] 1 test(s).' in peripheral_out
    finally:
        for process in processes:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=5)


def test_acl_connect_disconnect(controller_pair, tmp_path, request):
    override = request.config.getoption('--embt-build')
    executable = Path(override) if override else ROOT / 'build/classic-acl-tests/test/bluetooth/classic/bumble/test_bt_classic_acl'
    run_pair(executable, controller_pair, None, 'CONNECTABLE', tmp_path, 60)


@pytest.mark.parametrize('case', ['test_01_general_discovery', 'test_02_limited_discovery'])
def test_gap_discovery(controller_pair, tmp_path, case, request):
    override = request.config.getoption('--embt-build')
    executable = Path(override) if override else ROOT / 'build/classic-gap-tests/test/bluetooth/classic/bumble/test_bt_classic_gap'
    run_pair(executable, controller_pair, case, 'DISCOVERABLE', tmp_path, 120)
