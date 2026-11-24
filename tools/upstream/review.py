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
"""Require complete semantic review before preparing a partial upstream batch."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import sync

DECISIONS = {'NEED', 'PARTIAL', 'ALREADY', 'PENDING', 'DEFER', 'EXCLUDE'}


def inventory_digest(inventory):
    return hashlib.sha256(json.dumps(inventory, sort_keys=True).encode()).hexdigest()


def template(repo, inventory):
    if inventory.get('schema') != 1 or not isinstance(inventory.get('commits'), list):
        raise sync.SyncError('A frozen scan is required')
    entries = []
    for source in inventory['commits']:
        entries.append({'sha': source['sha'], 'decision': 'REVIEW_REQUIRED',
                        'rationale': '', 'evidence': [], 'adaptation': '', 'tests': [],
                        'dependencies': [], 'selected_targets': [],
                        'additional_targets': {}, 'omitted_sources': {}})
    return {'schema': 1, 'embt_base': sync.text(repo, 'rev-parse', 'HEAD'),
            'source_fingerprint': sync.fingerprint(repo),
            'inventory_sha256': inventory_digest(inventory), 'entries': entries}


def prepare(repo, inventory, review):
    if review.get('schema') != 1 or review.get('inventory_sha256') != inventory_digest(inventory):
        raise sync.SyncError('Review does not match the frozen scan')
    if (review.get('embt_base') != sync.text(repo, 'rev-parse', 'HEAD') or
            review.get('source_fingerprint') != sync.fingerprint(repo)):
        raise sync.SyncError('Target inputs changed after assessment; review again')
    records = review.get('entries', [])
    indexed = {e['sha']: e for e in records}
    expected = {e['sha'] for e in inventory['commits']}
    if set(indexed) != expected or len(indexed) != len(records):
        raise sync.SyncError('Every scanned commit needs exactly one decision')
    selected = []
    for original in inventory['commits']:
        entry = indexed[original['sha']]
        if (entry.get('decision') not in DECISIONS or not entry.get('rationale') or
                not entry.get('evidence')):
            raise sync.SyncError('Unresolved applicability: ' + original['sha'])
        if entry['decision'] not in ('NEED', 'PARTIAL'):
            continue
        if not entry.get('adaptation') or not entry.get('tests'):
            raise sync.SyncError('Adoption needs an adaptation and validation plan')
        targets = set(entry.get('selected_targets', []))
        mapped = {e['target'] for e in original['paths'] if e['target']}
        if not targets or targets - mapped:
            raise sync.SyncError('Select retained mapped targets explicitly')
        if len(targets) != len(entry['selected_targets']):
            raise sync.SyncError('Duplicate selected target')
        for item in original['paths']:
            target, decision, _ = sync.map_path(item['source'])
            if (target, decision) != (item['target'], item['decision']):
                raise sync.SyncError('Rescan with the current retirement mapping')
            if item['target'] not in targets and not entry.get('omitted_sources', {}).get(item['source']):
                raise sync.SyncError('Explain each omitted original source')
        additional = entry.get('additional_targets', {})
        for path, reason in additional.items():
            sync.validate_support_target(repo, path, reason)
        selected.append(original['sha'])
    if not selected:
        raise sync.SyncError('No newly applicable commits; retain the assessment without applying')
    plan = sync.prepare(repo, inventory, selected)
    plan['schema'] = 2
    plan['embt_source_fingerprint'] = review['source_fingerprint']
    plan['inventory_sha256'] = inventory_digest(inventory)
    plan['review_sha256'] = hashlib.sha256(json.dumps(review, sort_keys=True).encode()).hexdigest()
    plan['assessment'] = records
    for entry in plan['commits']:
        entry['reviewed_scope'] = indexed[entry['sha']]
    return plan


def collect(repo, start, end, output):
    output = Path(output)
    inventory = sync.scan(repo, start, end)
    sync.write_json(output / 'scan.json', inventory)
    sync.write_json(output / 'review.json', template(repo, inventory))
    directory = output / 'original-diffs'
    directory.mkdir(parents=True, exist_ok=True)
    for entry in inventory['commits']:
        (directory / (entry['sha'] + '.patch')).write_bytes(
            sync.git(repo, 'show', '--format=fuller', '--no-renames', entry['sha']).stdout)
    (output / 'REVIEW_REQUIRED.md').write_text(
        '# Semantic review required\n\n'
        'This scan is an inventory, not an adoption list. Assess actual retained Host code,\n'
        'dependencies, Controller-internal tests, prior decisions, partial effects and real\n'
        'validation. Complete every entry in review.json before prepare. No patches were\n'
        'applied, committed, published or merged. Follow docs/skills/upstream-sync/SKILL.md.\n')
    return {'state': 'review_required' if inventory['commits'] else 'no_new_commits',
            'commits': len(inventory['commits']),
            'upstream_from': inventory['upstream_from'], 'upstream_to': inventory['upstream_to']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=sync.ROOT)
    commands = parser.add_subparsers(dest='action', required=True)
    scan = commands.add_parser('scan')
    scan.add_argument('--from', dest='start', default=sync.BASELINE)
    scan.add_argument('--to', required=True)
    scan.add_argument('--output', type=Path, required=True)
    draft = commands.add_parser('template')
    draft.add_argument('--scan', type=Path, required=True)
    draft.add_argument('--output', type=Path, required=True)
    plan = commands.add_parser('prepare')
    plan.add_argument('--scan', type=Path, required=True)
    plan.add_argument('--review', type=Path, required=True)
    plan.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == 'scan':
            print(json.dumps(collect(args.repo, args.start, args.to, args.output)))
        elif args.action == 'template':
            sync.write_json(args.output, template(args.repo, json.loads(args.scan.read_text())))
        else:
            sync.write_json(args.output, prepare(args.repo, json.loads(args.scan.read_text()),
                                                json.loads(args.review.read_text())))
    except (sync.SyncError, OSError, ValueError, KeyError) as error:
        parser.exit(1, 'Upstream review stopped: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
