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
"""Exercise the manual sync workflow using real Git repositories and reports."""
import importlib.util
import json
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('manual_sync', ROOT / 'tools/upstream/sync.py')
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


class SyncTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='embt-sync-test-')
        self.root = Path(self.tmp.name)
        self.upstream = self.root / 'upstream'
        self.target = self.root / 'target'
        self.env = dict(os.environ, GIT_AUTHOR_NAME='Test Author', GIT_AUTHOR_EMAIL='test@example.com',
                        GIT_COMMITTER_NAME='Test Author', GIT_COMMITTER_EMAIL='test@example.com')
        for repo in (self.upstream, self.target):
            repo.mkdir()
            self.run_git(repo, 'init', '-b', 'task')
        self.source = 'subsys/bluetooth/host/example.c'
        self.dest = 'bluetooth/host/example.c'
        self.write(self.upstream / self.source, 'old\n')
        self.base = self.commit(self.upstream, 'Initial input\n\nTest fixture.')
        self.write(self.upstream / self.source, 'new\n')
        self.message = 'Bluetooth: fix example\n\nPreserve the original description.\n\nSigned-off-by: Test Author <test@example.com>'
        self.tip = self.commit(self.upstream, self.message)
        self.message = self.run_git(self.upstream, 'show', '-s', '--format=%B', self.tip)
        self.write(self.target / self.dest, 'old\n')
        self.commit(self.target, 'Initial target\n\nTest fixture.')
        self.inventory = sync.scan(self.upstream, self.base, self.tip)
        self.plan = sync.prepare(self.target, self.inventory, [self.tip])
        self.patch = self.root / 'adapted.patch'
        diff = self.run_git(self.upstream, 'show', '--format=', self.tip)
        self.patch.write_text(diff.replace('subsys/bluetooth/', 'bluetooth/') + '\n')

    def tearDown(self):
        self.tmp.cleanup()

    def run_git(self, repo, *args):
        return subprocess.check_output(['git', *args], cwd=repo, env=self.env,
                                       stderr=subprocess.DEVNULL, text=True).strip()

    def commit(self, repo, message):
        self.run_git(repo, 'add', '.')
        self.run_git(repo, 'commit', '-m', message)
        return self.run_git(repo, 'rev-parse', 'HEAD')

    def write(self, path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data)

    def test_mapping_and_retirement(self):
        self.assertEqual(sync.map_path(self.source)[0], self.dest)
        self.assertEqual(sync.map_path('tests/bluetooth/example/tests.yaml')[0], 'test/bluetooth/example/test.json')
        self.assertEqual(sync.map_path('tests/bluetooth/example/prj.conf')[0], 'test/bluetooth/example/config.conf')
        for p in ('subsys/bluetooth/controller/example.c', 'tests/bluetooth/controller/example.c',
                  'tests/bluetooth/example/app.overlay'):
            self.assertEqual(sync.map_path(p)[1], 'excluded')
        self.assertEqual(sync.map_path('kernel/example.c')[1], 'review')
        with self.assertRaises(sync.SyncError): sync.map_path('../../example.c')

    def test_scan_freezes_full_provenance_and_empty_range(self):
        entry = self.inventory['commits'][0]
        self.assertEqual(entry['sha'], self.tip)
        self.assertEqual(entry['message'].strip(), self.message)
        self.assertEqual(entry['author'], 'Test Author')
        self.assertTrue(entry['patch_id'])
        self.assertEqual(sync.scan(self.upstream, self.tip, self.tip)['commits'], [])
        with self.assertRaises(sync.SyncError): sync.scan(self.upstream, self.tip, self.base)

    def test_selection_rejects_empty_duplicate_and_unknown_inputs(self):
        for selected in ([], [self.tip, self.tip], ['0' * 40]):
            with self.assertRaises(sync.SyncError): sync.prepare(self.target, self.inventory, selected)

    def test_apply_dry_run_actual_and_repeat(self):
        output = self.root / 'result.json'
        patches = {self.tip: self.patch}
        self.assertEqual(sync.apply(self.target, self.plan, patches, output, True)['commits'][0]['state'], 'checked')
        self.assertEqual((self.target / self.dest).read_text(), 'old\n')
        self.assertEqual(sync.apply(self.target, self.plan, patches, output)['commits'][0]['state'], 'applied_uncommitted')
        self.assertEqual((self.target / self.dest).read_text(), 'new\n')
        self.assertEqual(sync.apply(self.target, self.plan, patches, output)['commits'][0]['state'], 'already_present')

    def test_scope_or_context_failure_does_not_mutate_target(self):
        self.patch.write_text(self.patch.read_text().replace(self.dest, 'bluetooth/host/unreviewed.c'))
        with self.assertRaises(sync.SyncError): sync.apply(self.target, self.plan, {self.tip: self.patch}, self.root/'result.json')
        self.assertEqual((self.target / self.dest).read_text(), 'old\n')
        self.assertEqual(json.loads((self.root/'result.json').read_text())['commits'][0]['state'], 'blocked')

    def test_changed_base_is_rejected(self):
        self.write(self.target / 'other', 'new input')
        self.commit(self.target, 'Unrelated change\n\nKeep a different base.')
        with self.assertRaises(sync.SyncError): sync.apply(self.target, self.plan, {self.tip: self.patch}, self.root/'result.json')

    def test_junit_requires_actual_success(self):
        path = self.root / 'results.xml'
        for data in ('<testsuite/>', '<testsuite><testcase><skipped/></testcase></testsuite>',
                     '<testsuite><testcase><failure/></testcase></testsuite>'):
            path.write_text(data)
            with self.assertRaises(sync.SyncError): sync.junit(path)
        path.write_text('<testsuite><testcase name="actual"/></testsuite>')
        self.assertEqual(sync.junit(path)['passed'], 1)

    def test_apply_rejects_main_branch(self):
        self.run_git(self.target, 'checkout', '-b', 'main')
        with self.assertRaises(sync.SyncError):
            sync.apply(self.target, self.plan, {self.tip: self.patch}, self.root/'result.json')
        self.assertEqual((self.target / self.dest).read_text(), 'old\n')

    def test_verify_rejects_zero_exit_without_valid_execution(self):
        for mode in ('passed', 'empty', 'skipped', 'failed', 'missing', 'changed'):
            with self.subTest(mode=mode):
                script = self.target/'tools/tests/run_bluetooth_tests.py'
                code = """import json, sys
from pathlib import Path
out = Path(sys.argv[sys.argv.index('--output') + 1])
mode = MODE
(out/'probe').mkdir(parents=True, exist_ok=True)
(out/'status.json').write_text(json.dumps([{'suite':'probe','status':'passed','execution_check':0}]))
if mode != 'missing':
    text = '<testsuite/>' if mode == 'empty' else '<testsuite><testcase name="probe">' + ({'skipped':'<skipped/>','failed':'<failure/>'}.get(mode,'')) + '</testcase></testsuite>'
    (out/'probe/results.xml').write_text(text)
if mode == 'changed':
    (Path.cwd()/'changed.c').write_text('changed executable input')
""".replace('MODE', repr(mode))
                self.write(script, code)
                output = self.root/('verify-'+mode)
                if mode == 'missing':
                    self.write(output/'probe/results.xml', '<testsuite><testcase name="stale"/></testsuite>')
                if mode == 'passed':
                    self.assertTrue(sync.verify(self.target, ['probe'], self.root/'build', output)['passed'])
                else:
                    with self.assertRaises(sync.SyncError):
                        sync.verify(self.target, ['probe'], self.root/'build', output)
                    self.assertFalse(json.loads((output/'verification.json').read_text())['passed'])

    def test_modified_mapping_and_duplicate_patch_arguments_are_rejected(self):
        plan = json.loads(json.dumps(self.plan))
        plan['commits'][0]['paths'][0]['target'] = 'unreviewed.c'
        with self.assertRaises(sync.SyncError):
            sync.apply(self.target, plan, {self.tip: self.patch}, self.root/'result.json')
        with self.assertRaises(sync.SyncError):
            sync.parse_patches([self.tip+'=first.patch', self.tip+'=second.patch'])
        with self.assertRaises(sync.SyncError):
            sync.prepare(self.target, {'schema': 2, 'commits': []}, [self.tip])

    def test_source_fingerprint_and_stale_success_are_checked(self):
        before = sync.fingerprint(self.target)
        self.write(self.target / 'notes.md', 'Documentation does not change executable inputs')
        self.assertEqual(sync.fingerprint(self.target), before)
        self.write(self.target / self.dest, 'modified code')
        self.assertNotEqual(sync.fingerprint(self.target), before)
        with self.assertRaises(sync.SyncError):
            sync.report(self.target, self.plan, {'passed': True, 'source_fingerprint': before},
                        self.root/'stale.md')

    def test_report_preserves_original_message_and_integration_boundary(self):
        out = self.root/'batch.md'
        sync.report(self.target, self.plan, {'passed': False, 'error': 'not tested'}, out)
        self.assertIn(self.message, out.read_text())
        self.assertIn(self.tip, out.read_text())
        self.assertIn('not merged', out.read_text())


if __name__ == '__main__':
    unittest.main()
