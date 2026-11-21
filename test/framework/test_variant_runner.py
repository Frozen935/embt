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

"""Verify scenario gates against real Kconfig output and controlled process results."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
RUNNER = Path(sys.argv.pop(1)) if len(sys.argv) > 1 else ROOT / 'tools/tests/run_bluetooth_tests.py'
spec = importlib.util.spec_from_file_location('variant_runner', RUNNER)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
runner.ROOT = ROOT


class VariantRunnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.environment = tempfile.TemporaryDirectory(prefix='embt-variant-contract-')
        cls.root = Path(cls.environment.name)
        cls.fragment = cls.root / 'valid.conf'
        cls.fragment.write_text((ROOT / 'test/bluetooth/host_id/config.conf').read_text() +
                                'CONFIG_BT_PERIPHERAL=n\nCONFIG_BT_CENTRAL=n\n'
                                'CONFIG_BT_BROADCASTER=n\nCONFIG_BT_OBSERVER=y\n'
                                'CONFIG_BT_SMP=n\nCONFIG_BT_PRIVACY=y\n'
                                'CONFIG_BT_RPA_TIMEOUT_DYNAMIC=y\nCONFIG_BT_RPA_TIMEOUT=900\n'
                                'CONFIG_BT_DEVICE_NAME="variant-probe"\n')
        cls.valid_build = cls.root / 'valid-build'
        subprocess.run(['python3', str(ROOT / 'tools/kconfig/configure.py'), 'defconfig',
                        '--build-dir', str(cls.valid_build), '--fragment', str(cls.fragment)],
                       check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    @classmethod
    def tearDownClass(cls):
        cls.environment.cleanup()

    def test_real_configuration_matches_bool_integer_and_string(self):
        report = runner.check_configuration(self.valid_build, [
            'CONFIG_BT_PRIVACY=y', 'CONFIG_BT_BROADCASTER=n', 'CONFIG_BT_SMP=n',
            'CONFIG_BT_RPA_TIMEOUT=0900', 'CONFIG_BT_DEVICE_NAME="variant-probe"'])
        self.assertEqual(report['errors'], [])

    def test_configuration_adjustment_is_rejected(self):
        report = runner.check_configuration(self.valid_build, ['CONFIG_BT_BROADCASTER=y'])
        self.assertTrue(report['errors'])

    def test_unknown_configuration_is_rejected(self):
        report = runner.check_configuration(self.valid_build, ['CONFIG_RETIRED_TEST_KEY=y'])
        self.assertTrue(report['errors'])

    def test_stale_configuration_metadata_is_rejected(self):
        build = self.root / 'stale-build'
        (build / 'kconfig').mkdir(parents=True)
        (build / 'kconfig/config.json').write_bytes(
            (self.valid_build / 'kconfig/config.json').read_bytes())
        (build / '.config').write_bytes((self.valid_build / '.config').read_bytes() + b'\n')
        self.assertTrue(runner.check_configuration(build, [])['errors'])

    def test_required_compiled_case_is_checked(self):
        inventory = {'suites': [{'name': 'probe', 'cases': ['probe__goal']}]}
        self.assertEqual(runner.check_required_cases(inventory, ['probe__goal'])['errors'], [])
        self.assertTrue(runner.check_required_cases(inventory, ['probe__missing'])['errors'])
        self.assertTrue(runner.check_required_cases({'suites': []}, [])['errors'])

    def run_pipeline(self, label, invalid_config=False, required=None, skipped=False):
        """Use the real Kconfig driver; model only CMake, discovery and CTest processes."""
        base = self.root / label
        source = base / 'source/host_id'
        source.mkdir(parents=True)
        source.joinpath('config.conf').write_text(
            (ROOT / 'test/bluetooth/host_id/config.conf').read_text() if invalid_config
            else self.fragment.read_text())
        required = required or []
        options = []
        if invalid_config:
            options = ['CONFIG_BT_PRIVACY=y', 'CONFIG_BT_SMP=n', 'CONFIG_BT_BROADCASTER=n',
                       'CONFIG_BT_OBSERVER=y', 'CONFIG_BT_RPA_TIMEOUT_DYNAMIC=y']
        variant = {'scenario': label, 'options': options, 'required_cases': required}
        metadata = {'runner': 'cmocka', 'execution': 'case'}
        real_run = subprocess.run
        builds = []

        def process(command, **kwargs):
            if command[:2] == ['cmake', '-S']:
                build = command[command.index('-B') + 1]
                fragment = next(arg.split('=', 1)[1] for arg in command
                                if arg.startswith('-DEMBT_CONFIG_FRAGMENTS='))
                return real_run(['python3', str(ROOT / 'tools/kconfig/configure.py'),
                                 'defconfig', '--build-dir', build, '--fragment', fragment],
                                **kwargs)
            if command[:2] == ['cmake', '--build']:
                builds.append(command)
            elif command[-1] == '--list-json':
                return subprocess.CompletedProcess(command, 0, json.dumps({
                    'suites': [{'name': 'probe', 'cases': ['probe__case']}]}), '')
            elif command[0] == 'ctest':
                path = Path(command[command.index('--output-junit') + 1])
                mark = '<skipped/>' if skipped else ''
                path.write_text('<testsuite><testcase name="bluetooth.host_id.probe__case">' +
                                mark + '</testcase></testsuite>')
            return subprocess.CompletedProcess(command, 0)

        with mock.patch.object(runner, 'TESTS', source.parent), \
             mock.patch.object(runner.subprocess, 'run', side_effect=process):
            result = runner.run_suite('host_id', base / 'build', base / 'output', metadata, variant)
        return result, builds

    def test_config_mismatch_stops_before_build(self):
        result, builds = self.run_pipeline('invalid-options', invalid_config=True)
        self.assertEqual(result['status'], 'configuration-check-error')
        self.assertEqual(builds, [])
        self.assertNotIn('test', result)

    def test_missing_required_case_stops_before_ctest(self):
        result, _ = self.run_pipeline('missing-case', required=['probe__goal'])
        self.assertEqual(result['status'], 'case-check-error')
        self.assertIsNone(result['test'])
        self.assertFalse((Path(result['logs']) / 'test.log').exists())

    def test_skipped_required_case_is_not_success(self):
        result, _ = self.run_pipeline('skipped-case', required=['probe__case'], skipped=True)
        self.assertEqual(result['status'], 'execution-check-error')
        self.assertNotEqual(result['test'], 0)

    def test_success_requires_all_gates(self):
        result, builds = self.run_pipeline('valid-case', required=['probe__case'])
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(len(builds), 1)
        for phase in ('configuration_check', 'case_check', 'execution_check', 'test'):
            self.assertEqual(result[phase], 0)

    def test_non_cmocka_execution_requires_fresh_passing_results(self):
        for kind in ('pytest', 'linked'):
            for mode in ('empty', 'skipped', 'failed', 'missing', 'stale', 'passed'):
                with self.subTest(runner=kind, report=mode):
                    base = self.root / (kind + '-' + mode)
                    source = base / 'source/probe'
                    source.mkdir(parents=True)
                    source.joinpath('config.conf').write_text('')
                    logs = base / 'output/probe'
                    logs.mkdir(parents=True)
                    if mode == 'stale':
                        logs.joinpath('results.xml').write_text(
                            '<testsuite><testcase name="old"/></testsuite>')

                    def process(command, **kwargs):
                        if '-m' in command and 'pytest' in command:
                            if mode not in ('missing', 'stale'):
                                path = Path(next(arg.split('=', 1)[1] for arg in command
                                                 if arg.startswith('--junitxml=')))
                                if mode == 'empty':
                                    text = '<testsuite/>'
                                else:
                                    mark = {'skipped': '<skipped/>', 'failed': '<failure/>',
                                            'passed': ''}[mode]
                                    text = '<testsuite><testcase name="probe">' + mark + \
                                           '</testcase></testsuite>'
                                path.write_text(text)
                        return subprocess.CompletedProcess(command, 0)

                    metadata = {'runner': kind, 'pytest': 'probe/test.py', 'timeout_s': 5}
                    with mock.patch.object(runner, 'TESTS', source.parent), \
                         mock.patch.object(runner, 'check_configuration',
                                           return_value={'errors': []}), \
                         mock.patch.object(runner.subprocess, 'run', side_effect=process):
                        result = runner.run_suite('probe', base / 'build', base / 'output',
                                                  metadata)
                    self.assertEqual(result['status'], 'passed' if mode == 'passed'
                                     else 'execution-check-error')
                    self.assertEqual(result['test'], 0 if mode == 'passed' else 1)

    def test_ordered_suite_checks_native_case_result(self):
        p = self.root / 'ordered.xml'
        p.write_text('<testsuite><testcase name="bluetooth.gatt.ordered_suite">'
                     '<system-out>[       OK ] gatt__goal\n[  SKIPPED ] gatt__other</system-out>'
                     '</testcase></testsuite>')
        self.assertEqual(runner.check_execution(p, 'gatt', ['gatt__goal'], 'suite')['errors'], [])
        self.assertTrue(runner.check_execution(p, 'gatt', ['gatt__other'], 'suite')['errors'])
        p.write_text('<testsuite/>')
        self.assertTrue(runner.check_execution(p, 'gatt', [], 'suite')['errors'])
        p.write_text('<testsuite><testcase name="bluetooth.gatt.ordered_suite" '
                     'status="notrun"/></testsuite>')
        self.assertTrue(runner.check_execution(p, 'gatt', [], 'suite')['errors'])
        p.write_text('<testsuite><testcase name="bluetooth.gatt.ordered_suite">'
                     '<failure/></testcase></testsuite>')
        self.assertTrue(runner.check_execution(p, 'gatt', [], 'suite')['errors'])


if __name__ == '__main__':
    unittest.main()
