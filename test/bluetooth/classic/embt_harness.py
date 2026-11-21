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
"""POSIX process and PTY adapter for the imported Classic pytest programs."""
import os
import pty
import queue
import re
import select
import signal
import subprocess
import threading
import time
from pathlib import Path


class DeviceAdapter:
    """Read actual shell output, preserving asynchronous event lines."""

    def __init__(self, command, log_path):
        self.command = list(command)
        self.log_path = Path(log_path)
        self.master, slave = pty.openpty()
        self.process = subprocess.Popen(command, stdin=slave, stdout=slave, stderr=slave,
                                        start_new_session=True, close_fds=True, cwd=Path(log_path).parent)
        os.close(slave)
        self.lines = queue.Queue()
        self.prompt_generation = 0
        self.log = Path(log_path).open('w')
        self.stopped = False
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        pending = b''
        while not self.stopped:
            if not select.select([self.master], [], [], 0.1)[0]:
                continue
            try:
                data = os.read(self.master, 65536)
            except OSError:
                break
            if not data:
                break
            self.log.write(data.decode(errors='replace'))
            self.log.flush()
            pending += data
            while pending:
                newline = pending.find(b'\n')
                prompt = pending.find(b'btshell> ')
                if prompt >= 0 and (newline < 0 or prompt < newline):
                    if prompt:
                        self.lines.put(pending[:prompt].decode(errors='replace').strip('\r'))
                    self.prompt_generation += 1
                    self.lines.put(('prompt', self.prompt_generation))
                    pending = pending[prompt + len(b'btshell> '):]
                elif newline >= 0:
                    line, pending = pending.split(b'\n', 1)
                    self.lines.put(line.decode(errors='replace').strip('\r'))
                else:
                    break

    def readlines(self, print_output=False):
        result = []
        while True:
            try:
                line = self.lines.get_nowait()
                if isinstance(line, str):
                    result.append(line)
            except queue.Empty:
                return result

    def readlines_until(self, regex, timeout=20, **kwargs):
        result = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=min(0.2, deadline - time.monotonic()))
            except queue.Empty:
                continue
            if not isinstance(line, str):
                continue
            result.append(line)
            if re.search(regex, line):
                return result
        raise TimeoutError(f'Shell response not found: {regex}; lines={result}')

    def wait_for_prompt(self, timeout=20, after=0):
        """Collect actual command output until a newer shell prompt arrives."""
        result = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                event = self.lines.get(timeout=max(0.001, deadline - time.monotonic()))
            except queue.Empty:
                break
            if isinstance(event, tuple):
                if event[1] > after:
                    return result
            else:
                result.append(event)
            if self.process.poll() is not None:
                raise RuntimeError(f'Shell exited with status {self.process.returncode}')
        raise TimeoutError(f'Shell command did not finish; output={result}')

    def write(self, command):
        os.write(self.master, command.encode() if isinstance(command, str) else command)

    def clear_buffer(self):
        self.readlines()

    def restart(self):
        """Cold-restart this actual process while preserving its endpoint and store directory."""
        command = self.command
        log_path = self.log_path
        self.write('test_smp reboot\n')
        status = self.process.wait(timeout=10)
        if status != 75:
            raise RuntimeError(f'Restart request exited with unexpected status {status}')
        self.close()
        self.__init__(command, log_path.with_name(log_path.stem + '-restart.log'))
        self.readlines_until('Bluetooth is disabled', timeout=10)
        self.wait_for_prompt(timeout=10)

    def close(self):
        if self.process.poll() is None:
            self.write('quit\n')
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGTERM)
                self.process.wait(timeout=3)
        self.stopped = True
        self.reader.join(timeout=1)
        os.close(self.master)
        self.log.close()
        return self.process.returncode


class Shell:
    """Issue commands to one real EmBT shell; the DUT adapter owns event reads."""

    def __init__(self, dut):
        self.dut = dut

    def exec_command(self, command, **kwargs):
        generation = self.dut.prompt_generation
        self.dut.write(command + '\n')
        return self.dut.wait_for_prompt(timeout=kwargs.get('timeout', 20), after=generation)
