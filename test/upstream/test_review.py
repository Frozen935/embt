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
"""Exercise full review, partial scope and sequential dry-run contracts with Git."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/upstream'))
import review
import test_sync as fixtures


class ReviewTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.SyncTests('test_apply_dry_run_actual_and_repeat')
        self.fixture.setUp()

    def tearDown(self):
        self.fixture.tearDown()

    def completed(self, inventory=None):
        f = self.fixture
        document = review.template(f.target, inventory or f.inventory)
        for entry in document['entries']:
            entry.update(decision='NEED', rationale='The retained input has the old behavior.',
                         evidence=[f.dest + ':1'], adaptation='Map the retained Host path.',
                         tests=['Real fixture application and execution gate'],
                         selected_targets=[f.dest])
        return document

    def test_complete_review_preserves_original_message(self):
        f = self.fixture
        plan = review.prepare(f.target, f.inventory, self.completed())
        self.assertEqual(plan['schema'], 2)
        self.assertEqual(plan['commits'][0]['message'], f.inventory['commits'][0]['message'])
        result = review.sync.apply(f.target, plan, {f.tip: f.patch}, f.root / 'result.json')
        self.assertEqual(result['commits'][0]['state'], 'applied_uncommitted')

    def test_missing_duplicate_or_unresolved_entries_are_rejected(self):
        f = self.fixture
        for kind in ('missing', 'duplicate', 'unresolved'):
            with self.subTest(kind=kind):
                document = self.completed()
                if kind == 'missing': document['entries'] = []
                elif kind == 'duplicate': document['entries'] *= 2
                else: document['entries'][0]['decision'] = 'REVIEW_REQUIRED'
                with self.assertRaises(review.sync.SyncError):
                    review.prepare(f.target, f.inventory, document)

    def test_changed_target_or_scan_requires_reassessment(self):
        f = self.fixture
        document = self.completed()
        changed = copy.deepcopy(f.inventory)
        changed['upstream_to'] = 'f' * 40
        with self.assertRaises(review.sync.SyncError):
            review.prepare(f.target, changed, document)
        f.write(f.target / f.dest, 'different target\n')
        with self.assertRaises(review.sync.SyncError):
            review.prepare(f.target, f.inventory, document)

    def test_changed_source_after_preparation_is_rejected(self):
        f = self.fixture
        plan = review.prepare(f.target, f.inventory, self.completed())
        f.write(f.target / 'Kconfig', 'config UNREVIEWED\n\tbool\n')
        with self.assertRaises(review.sync.SyncError):
            review.sync.apply(f.target, plan, {f.tip: f.patch}, f.root/'result.json')
        self.assertEqual((f.target/f.dest).read_text(), 'old\n')

    def test_provider_support_requires_existing_tracked_code(self):
        f = self.fixture
        f.write(f.target/'lib/shell/provider.c', 'provider\n')
        f.commit(f.target, 'Retain provider\n\nTest fixture.')
        document = self.completed()
        document['entries'][0]['additional_targets'] = {
            'lib/shell/provider.c': 'Correct the real provider used by this backport.'}
        self.assertEqual(review.prepare(f.target, f.inventory, document)['schema'], 2)
        document['entries'][0]['additional_targets'] = {
            'lib/shell/missing.c': 'Claimed provider'}
        with self.assertRaises(review.sync.SyncError):
            review.prepare(f.target, f.inventory, document)

    def test_partial_selection_requires_omission_reason(self):
        f = self.fixture
        f.write(f.upstream / f.source, 'newer\n')
        f.write(f.upstream / 'subsys/bluetooth/audio/example.c', 'unselected\n')
        tip = f.commit(f.upstream, 'Bluetooth: mixed input\n\nTwo independent effects.')
        inventory = review.sync.scan(f.upstream, f.tip, tip)
        document = self.completed(inventory)
        document['entries'][0]['decision'] = 'PARTIAL'
        with self.assertRaises(review.sync.SyncError):
            review.prepare(f.target, inventory, document)
        document['entries'][0]['omitted_sources'] = {
            'subsys/bluetooth/audio/example.c': 'The optional independent effect is deferred.'}
        plan = review.prepare(f.target, inventory, document)
        self.assertEqual(plan['commits'][0]['reviewed_scope']['selected_targets'], [f.dest])

    def test_controller_internal_cte_and_retired_provider_are_excluded(self):
        for source in ('tests/bluetooth/df/connectionless_cte_chains/src/main.c',
                       'subsys/bluetooth/audio/shell/bap_usb.c',
                       'subsys/bluetooth/mesh/blob_io_flash.c'):
            self.assertEqual(review.sync.map_path(source)[1], 'excluded')
        f = self.fixture
        document = self.completed()
        for target in ('test/bluetooth/controller/src/main.c',
                       'test/bluetooth/df/connectionless_cte_chains/src/main.c',
                       'test/bluetooth/example/PROVENANCE.md', 'docs/unrelated.md'):
            with self.subTest(target=target):
                document['entries'][0]['additional_targets'] = {target: 'Claimed scope'}
                with self.assertRaises(review.sync.SyncError):
                    review.prepare(f.target, f.inventory, document)

    def test_sequential_dry_run_uses_an_isolated_index(self):
        f = self.fixture
        f.write(f.upstream / f.source, 'newer\n')
        second = f.commit(f.upstream, 'Bluetooth: dependent fix\n\nDepends on first input.')
        inventory = review.sync.scan(f.upstream, f.base, second)
        plan = review.sync.prepare(f.target, inventory, [f.tip, second])
        patch = f.root / 'second.patch'
        patch.write_text(f.run_git(f.upstream, 'show', '--format=', second).replace(
            'subsys/bluetooth/', 'bluetooth/') + '\n')
        result = review.sync.apply(f.target, plan, {f.tip:f.patch, second:patch},
                                   f.root/'result.json', dry_run=True)
        self.assertTrue(all(e['state'] == 'checked' for e in result['commits']))
        self.assertEqual((f.target/f.dest).read_text(), 'old\n')
        self.assertEqual(f.run_git(f.target, 'status', '--porcelain'), '')

    def test_empty_periodic_scan_is_not_an_adoption(self):
        f = self.fixture
        result = review.collect(f.upstream, f.tip, f.tip, f.root/'empty')
        self.assertEqual(result['state'], 'no_new_commits')
        self.assertEqual(json.loads((f.root/'empty/review.json').read_text())['entries'], [])


if __name__ == '__main__':
    unittest.main()
