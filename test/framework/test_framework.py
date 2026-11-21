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

"""Verify the real probe process rather than inspecting runner implementation."""
import json
import subprocess
import sys
import unittest

PROGRAM = sys.argv.pop(1)
BUILD = sys.argv.pop(1)

class FrameworkContract(unittest.TestCase):
    def run_probe(self, *arguments):
        return subprocess.run([PROGRAM, *arguments], text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5)

    def test_listing_has_no_fixture_side_effects(self):
        result = self.run_probe('--list')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(result.stdout.splitlines(), [
            'positive__first', 'positive__second', 'negative__failure',
            'negative__skip', 'negative__worker_failure'])

    def test_json_matches_compiled_suites(self):
        result = self.run_probe('--list-json')
        self.assertEqual(result.returncode, 0, result.stdout)
        suites = json.loads(result.stdout)['suites']
        self.assertEqual([s['name'] for s in suites], ['positive', 'negative'])
        self.assertEqual(suites[0]['cases'], ['positive__first', 'positive__second'])

    def test_native_group_and_case_fixtures(self):
        result = self.run_probe('--suite', 'positive')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(result.stdout.count('case_setup'), 2)
        self.assertEqual(result.stdout.count('case_teardown'), 2)
        self.assertEqual(result.stdout.count('group_setup'), 1)
        self.assertEqual(result.stdout.count('group_teardown'), 1)

    def test_case_selection(self):
        for args in [('--case', 'positive__first'), ('positive__first',),
                     ('--suite', 'positive', '--case', 'first')]:
            result = self.run_probe(*args)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertNotIn('[ RUN      ] positive__second', result.stdout)

    def test_unknown_suite_and_case_are_errors(self):
        for args in [('--suite', 'missing'), ('--case', 'missing')]:
            result = self.run_probe(*args)
            self.assertEqual(result.returncode, 2, result.stdout)
            self.assertNotIn('group_setup', result.stdout)

    def test_invalid_arguments_do_not_execute(self):
        for args in [('--suite',), ('--case',), ('--list', '--list-json'),
                     ('--list', '--case', 'positive__first'), ('--unknown',)]:
            result = self.run_probe(*args)
            self.assertEqual(result.returncode, 2, result.stdout)
            self.assertNotIn('group_setup', result.stdout)

    def test_failure_is_not_a_successful_process(self):
        result = self.run_probe('--case', 'negative__failure')
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn('[  FAILED  ]', result.stdout)

    def test_worker_failure_isolated_and_reported(self):
        result = self.run_probe('--case', 'negative__worker_failure')
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn('Callback assertion failed', result.stdout)

    def test_missing_suite_cannot_link(self):
        result = subprocess.run(['cmake', '--build', BUILD, '--target',
                                 'test_bt_framework_missing'], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('bt_test_suite_missing_descriptor', result.stdout)

    def test_skip_is_reported_separately(self):
        result = self.run_probe('--case', 'negative__skip')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn('[  SKIPPED ]', result.stdout)

if __name__ == '__main__':
    unittest.main()
