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

"""Configuration CLI contracts for platform defaults, generated imports and retired options."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / 'tools/kconfig/configure.py'


class PlatformConfigTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='embt-platform-contract-')
        self.directory = Path(self.temporary.name)
        self.build = self.directory / 'build'

    def tearDown(self):
        self.temporary.cleanup()

    def configure(self, text=''):
        fragment = self.directory / 'fragment.conf'
        fragment.write_text(text)
        return subprocess.run([sys.executable, str(DRIVER), 'configure',
                               '--build-dir', str(self.build), '--fragment', str(fragment)],
                              capture_output=True, text=True)

    def values(self):
        return json.loads((self.build / 'kconfig/config.json').read_text())['symbols']

    def test_linux_defaults_and_rng_capability(self):
        result = self.configure('CONFIG_UUID_V4=y\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        values = self.values()
        for name in ('HAS_RANDOM_GENERATOR', 'UUID_V4'):
            self.assertTrue(values['CONFIG_' + name]['value'])
        self.assertEqual(values['CONFIG_BT_RX_STACK_SIZE']['value'], '4096')
        self.assertEqual(values['CONFIG_BT_RX_PRIO']['value'], '8')
        self.assertFalse((self.build / 'kconfig/dependencies.json').exists())

    def test_shared_posix_defaults(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stderr)
        values = self.values()
        self.assertNotIn('CONFIG_EMBT_PLATFORM_NUTTX', values)
        self.assertNotIn('CONFIG_EMBT_PLATFORM_LINUX', values)
        self.assertEqual(values['CONFIG_EMBT_THREAD_PRIORITY_BASE']['value'], '150')
        self.assertEqual(values['CONFIG_BT_RX_STACK_SIZE']['value'], '4096')
        self.assertEqual(values['CONFIG_BT_DRIVER_RX_HIGH_PRIO']['value'], '6')
        self.assertEqual(values['CONFIG_BT_HCI_TX_PRIO']['value'], '7')

    def test_stack_overrides_and_init_order(self):
        result = self.configure('CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=1048576\n'
                                'CONFIG_BT_RX_STACK_SIZE=2097152\n'
                                'CONFIG_BT_LONG_WQ=y\nCONFIG_BT_LONG_WQ_INIT_PRIO=10\n'
                                'CONFIG_BT_CONN_TX_NOTIFY_WQ=y\n'
                                'CONFIG_BT_CONN_TX_NOTIFY_WQ_INIT_PRIORITY=20\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        values = self.values()
        expected = {'BT_RX_STACK_SIZE': '2097152', 'BT_LONG_WQ_STACK_SIZE': '4096',
                    'BT_LONG_WQ_INIT_PRIO': '10', 'BT_CONN_TX_NOTIFY_WQ_INIT_PRIORITY': '20'}
        for name, value in expected.items():
            self.assertEqual(values['CONFIG_' + name]['value'], value)

    def test_nuttx_reference_override(self):
        result = self.configure('CONFIG_EMBT_THREAD_PRIORITY_BASE=160\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.values()['CONFIG_EMBT_THREAD_PRIORITY_BASE']['value'], '160')

    def test_retired_requirements_rejected(self):
        for name in ('POLL', 'SHELL', 'SHELL_GETOPT', 'GETOPT_LONG', 'EMBT_PORT_HAS_SHELL',
                     'EMBT_THREAD_STACK_DEFAULT', 'EMBT_THREAD_SCHED_FIFO', 'BT_ASSERT_PANIC',
                     'EMBT_PLATFORM_LINUX', 'EMBT_PLATFORM_NUTTX'):
            with self.subTest(name=name):
                result = self.configure('CONFIG_' + name + '=y\n')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Unknown or retired', result.stderr)
                self.assertFalse((self.build / '.config').exists())

    def test_rtp_version_and_range(self):
        result = self.configure('CONFIG_BT_A2DP=y\nCONFIG_BT_AVDTP_RTP_VERSION=1\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.values()['CONFIG_BT_AVDTP_RTP_VERSION']['value'], '1')
        with tempfile.TemporaryDirectory(dir=self.directory) as other:
            self.build = Path(other) / 'build'
            result = self.configure('CONFIG_BT_A2DP=y\nCONFIG_BT_AVDTP_RTP_VERSION=4\n')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('outside the range', result.stderr)

    def test_literal_export_and_canonical_header_recovery(self):
        value = 'literal ${UNDEFINED}; closing ]] and ]=] and backslash \\ end'
        result = self.configure('CONFIG_EMBT_APP_VERSION=' + json.dumps(value) + '\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        script = self.directory / 'import.cmake'
        output = self.directory / 'imported.txt'
        script.write_text('function(import_config)\n'
                          '  include("' + str(self.build / 'kconfig/config.cmake') + '")\n'
                          'endfunction()\nimport_config()\n'
                          'file(WRITE "' + str(output) + '" "${CONFIG_EMBT_APP_VERSION}")\n')
        run = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(output.read_text(), value)
        before = (self.build / '.config').read_bytes()
        header = self.build / 'generated/include/embt/autoconf.h'
        expected_header = header.read_bytes()
        header.unlink()
        result = subprocess.run([sys.executable, str(DRIVER), 'configure', '--build-dir', str(self.build)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(header.read_bytes(), expected_header)
        self.assertEqual((self.build / '.config').read_bytes(), before)
        metadata = json.loads((self.build / 'kconfig/config.json').read_text())
        self.assertEqual(metadata['config_sha256'], hashlib.sha256(before).hexdigest())


if __name__ == '__main__':
    unittest.main()
