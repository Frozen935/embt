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
"""Manual, review-driven Zephyr patch preparation and validation."""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[2]
BASELINE = 'f8da8ea370508f274685a25ff19dbe5a9ff54790'
PREFIXES = ('subsys/bluetooth/', 'tests/bluetooth/', 'include/zephyr/bluetooth/',
            'lib/net_buf/', 'subsys/settings/')


class SyncError(RuntimeError):
    """A preparation or validation failure, never implicit adoption."""


def git(repo, *args, data=None, check=True, env=None):
    result = subprocess.run(['git', *args], cwd=repo, input=data,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    if check and result.returncode:
        raise SyncError(result.stderr.decode().strip())
    return result


def text(repo, *args):
    return git(repo, *args).stdout.decode().strip()


def fingerprint(repo):
    """Hash current executable/configuration inputs, including untracked adaptations."""
    names = git(repo, 'ls-files', '-z', '--cached', '--others', '--exclude-standard').stdout
    digest = hashlib.sha256()
    for raw in sorted(set(names.split(b'\0'))):
        if not raw:
            continue
        name = raw.decode()
        path = Path(repo) / name
        if name.startswith('docs/') or not (path.suffix in ('.c', '.h', '.py', '.cmake', '.conf', '.json')
                or path.name == 'CMakeLists.txt' or path.name.startswith('Kconfig')):
            continue
        digest.update(raw + b'\0')
        digest.update(path.read_bytes() if path.exists() else b'DELETED')
    return digest.hexdigest()


def map_path(path):
    """Map retained files; board/provider retirement is explicit."""
    if PurePosixPath(path).is_absolute() or '..' in PurePosixPath(path).parts:
        raise SyncError('Unsafe upstream path: ' + path)
    if path.startswith(('subsys/bluetooth/controller/', 'tests/bluetooth/controller/',
                        'tests/bluetooth/df/connectionless_cte_chains/')):
        return None, 'excluded', 'Controller implementation/tests are retired'
    if path in ('subsys/bluetooth/audio/shell/bap_usb.c',
                'subsys/bluetooth/audio/shell/bap_usb_backend.h',
                'subsys/bluetooth/mesh/blob_io_flash.c',
                'include/zephyr/bluetooth/mesh/blob_io_flash.h'):
        return None, 'excluded', 'This platform-specific provider was explicitly retired'
    if path.endswith(('.overlay', '.dts', '.dtsi')):
        return None, 'excluded', 'Device-tree inputs are retired'
    for source, target in [('subsys/bluetooth/', 'bluetooth/'),
                           ('tests/bluetooth/', 'test/bluetooth/'),
                           ('include/zephyr/bluetooth/', 'include/embt/bluetooth/'),
                           ('lib/net_buf/', 'lib/net_buf/'),
                           ('subsys/settings/', 'lib/settings/')]:
        if path.startswith(source):
            mapped = target + path[len(source):]
            if source == 'tests/bluetooth/':
                if path.endswith('/tests.yaml'):
                    mapped = mapped[:-len('tests.yaml')] + 'test.json'
                elif path.endswith('/prj.conf'):
                    mapped = mapped[:-len('prj.conf')] + 'config.conf'
            if path.endswith(('.yaml', '.yml')) and not path.endswith('/tests.yaml'):
                return None, 'review', 'Non-runner YAML requires a scope decision'
            return mapped, 'mapped', 'Path mapping only; semantics require review'
    return None, 'review', 'Outside the initial mapping; inspect dependencies'


def scan(repo, start, end):
    start, end = text(repo, 'rev-parse', start), text(repo, 'rev-parse', end)
    if git(repo, 'merge-base', '--is-ancestor', start, end, check=False).returncode:
        raise SyncError('Upstream start is not an ancestor of the end')
    shas = text(repo, 'rev-list', '--reverse', '--topo-order', '--no-merges',
                start + '..' + end, '--', *PREFIXES).splitlines()
    commits = []
    for sha in shas:
        paths = []
        data = git(repo, 'diff-tree', '--no-commit-id', '--no-renames', '-r',
                   '--name-only', '-z', sha).stdout
        for raw in data.split(b'\0'):
            if not raw:
                continue
            source = raw.decode()
            target, decision, reason = map_path(source)
            paths.append({'source': source, 'target': target,
                          'decision': decision, 'reason': reason})
        meta = text(repo, 'show', '-s', '--format=%an%n%ae%n%aI', sha).splitlines()
        patch = git(repo, 'show', '--format=', '--no-renames', sha).stdout
        patch_id = git(repo, 'patch-id', '--stable', data=patch).stdout.decode().split()
        commits.append({'sha': sha, 'title': text(repo, 'show', '-s', '--format=%s', sha),
                        'message': git(repo, 'show', '-s', '--format=%B', sha).stdout.decode(),
                        'author': meta[0], 'email': meta[1], 'author_date': meta[2],
                        'patch_id': patch_id[0] if patch_id else None, 'paths': paths})
    return {'schema': 1, 'upstream_from': start, 'upstream_to': end, 'commits': commits}


def prepare(repo, inventory, selected):
    """Select real scan entries without making an adoption decision."""
    if inventory.get('schema') != 1:
        raise SyncError('Unknown scan schema')
    if not selected or len(set(selected)) != len(selected):
        raise SyncError('Select a nonempty unique commit list')
    entries = {p['sha']: p for p in inventory['commits']}
    if set(selected) - entries.keys():
        raise SyncError('Selected commit is not in the frozen scan')
    return {'schema': 1, 'embt_base': text(repo, 'rev-parse', 'HEAD'),
            'upstream_from': inventory['upstream_from'],
            'upstream_to': inventory['upstream_to'],
            'commits': [dict(entries[sha], state='prepared') for sha in selected]}


def patch_paths(repo, patch):
    data = git(repo, 'apply', '--numstat', '-z', str(patch)).stdout
    result = set()
    for row in data.split(b'\0'):
        if row:
            fields = row.split(b'\t', 2)
            if len(fields) != 3 or not fields[2]:
                raise SyncError('Rename encoding is not supported in the manual seed')
            path = fields[2].decode()
            if PurePosixPath(path).is_absolute() or '..' in PurePosixPath(path).parts:
                raise SyncError('Unsafe adapted patch path')
            result.add(path)
    return result


def write_json(path, payload):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + '\n')


def validate_support_target(repo, target, reason):
    path = PurePosixPath(target)
    if not reason or path.is_absolute() or '..' in path.parts:
        raise SyncError('Invalid additional support target')
    if target.startswith('test/bluetooth/'):
        if (target.endswith('/PROVENANCE.md') or '/controller/' in target or
                map_path('tests/' + target[len('test/'):])[1] == 'excluded'):
            raise SyncError('Retired test support cannot be restored')
        return
    if (not target.startswith(('lib/', 'base/', 'osdep/', 'drivers/', 'include/embt/')) or
            path.suffix not in ('.c', '.h', '.cmake', '.conf', '.json')):
        raise SyncError('Additional code needs a retained provider scope')
    if (not (Path(repo) / target).is_file() or (Path(repo) / target).is_symlink() or
            git(repo, 'ls-files', '--error-unmatch', '--', target, check=False).returncode):
        raise SyncError('Additional provider must be an existing tracked file')


def apply(repo, plan, patches, output, dry_run=False):
    if dry_run:
        with tempfile.TemporaryDirectory(prefix='embt-upstream-index-') as directory:
            env = dict(os.environ, GIT_INDEX_FILE=str(Path(directory) / 'index'))
            git(repo, 'read-tree', 'HEAD', env=env)
            return apply_plan(repo, plan, patches, output, dry_run=True, env=env)
    return apply_plan(repo, plan, patches, output)


def apply_plan(repo, plan, patches, output, dry_run=False, env=None):
    """Apply only caller-reviewed diffs, preserving per-patch progress on failure."""
    if not dry_run and text(repo, 'branch', '--show-current') in ('main', 'master', ''):
        raise SyncError('Apply changes only on an isolated work branch')
    if plan.get('schema') not in (1, 2):
        raise SyncError('Unknown plan schema')
    shas = [p['sha'] for p in plan['commits']]
    if not shas or len(set(shas)) != len(shas):
        raise SyncError('Invalid selected commit list')
    if text(repo, 'rev-parse', 'HEAD') != plan['embt_base']:
        raise SyncError('The EmBT base changed; prepare and validate again')
    if plan['schema'] == 2 and plan.get('embt_source_fingerprint') != fingerprint(repo):
        raise SyncError('Target source changed after preparation; reassess before applying')
    if set(patches) != {p['sha'] for p in plan['commits']}:
        raise SyncError('Supply exactly one adapted patch for every selected commit')
    result = dict(plan, commits=[dict(p) for p in plan['commits']], mode='dry-run' if dry_run else 'apply')
    for entry in result['commits']:
        try:
            for item in entry['paths']:
                mapped, decision, _ = map_path(item['source'])
                if (item['target'], item['decision']) != (mapped, decision):
                    raise SyncError('Plan mapping differs from the reviewed mapper')
            expected = {p['target'] for p in entry['paths'] if p['target']}
            if plan['schema'] == 2:
                scope = entry['reviewed_scope']
                selected = set(scope['selected_targets'])
                additional = scope['additional_targets']
                omitted = scope['omitted_sources']
                if not selected or selected - expected or not scope['rationale'] or not scope['evidence']:
                    raise SyncError('Incomplete reviewed target selection')
                if any(p['target'] not in selected and not omitted.get(p['source']) for p in entry['paths']):
                    raise SyncError('Every omitted source needs an explicit review reason')
                for target, reason in additional.items():
                    validate_support_target(repo, target, reason)
                expected = selected | set(additional)
            elif any(p['decision'] == 'review' for p in entry['paths']):
                raise SyncError('Unresolved source paths require review')
            patch = Path(patches[entry['sha']]).resolve()
            if patch_paths(repo, patch) != expected:
                raise SyncError('Adapted patch does not match the reviewed target scope')
            entry['adapted_patch_sha256'] = hashlib.sha256(patch.read_bytes()).hexdigest()
            cached = ['--cached'] if dry_run else []
            if git(repo, 'apply', *cached, '--reverse', '--check', str(patch), check=False, env=env).returncode == 0:
                entry['state'] = 'already_present'
            else:
                git(repo, 'apply', *cached, '--check', str(patch), env=env)
                git(repo, 'apply', *cached, str(patch), env=env)
                entry['state'] = 'checked' if dry_run else 'applied_uncommitted'
        except (SyncError, OSError) as error:
            entry['state'], entry['error'] = 'blocked', str(error)
            write_json(output, result)
            raise SyncError('Stopped at ' + entry['sha'] + ': ' + str(error)) from error
        write_json(output, result)
    return result


def junit(path):
    cases = list(ET.parse(path).iter('testcase'))
    failed = sum(c.find('failure') is not None or c.find('error') is not None for c in cases)
    skipped = sum(c.find('skipped') is not None or c.get('status', 'run') != 'run' for c in cases)
    if not cases or len(cases) == skipped or failed:
        raise SyncError('Empty, all-skipped or failing test report: ' + str(path))
    return {'entries': len(cases), 'passed': len(cases) - skipped, 'skipped': skipped, 'failed': failed}


def verify(repo, suites, build_root, output):
    """Use the existing canonical runner and require actual fresh execution."""
    if not suites or len(set(suites)) != len(suites):
        raise SyncError('Select a nonempty unique test suite list')
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    for suite in suites:
        if PurePosixPath(suite).is_absolute() or '..' in PurePosixPath(suite).parts:
            raise SyncError('Unsafe suite selection')
        (output / suite.replace('/', '_') / 'results.xml').unlink(missing_ok=True)
    args = [sys.executable, str(Path(repo) / 'tools/tests/run_bluetooth_tests.py'),
            '--build-root', str(Path(build_root).resolve()), '--output', str(output)]
    for suite in suites:
        args += ['--suite', suite]
    input_fingerprint = fingerprint(repo)
    with (output / 'runner.log').open('w') as log:
        process = subprocess.run(args, cwd=repo, stdout=log, stderr=subprocess.STDOUT)
    result = {'source_fingerprint': input_fingerprint, 'embt_head': text(repo, 'rev-parse', 'HEAD'), 'command': args,
              'exit_code': process.returncode, 'passed': False, 'tests': []}
    try:
        if fingerprint(repo) != input_fingerprint:
            raise SyncError('Source inputs changed while validation was running')
        if process.returncode:
            raise SyncError('Canonical runner failed; see runner.log')
        records = json.loads((output / 'status.json').read_text())
        if {p['suite'] for p in records} != set(suites) or len(records) != len(suites):
            raise SyncError('Runner selection does not match requested suites')
        for record in records:
            if record['status'] != 'passed' or record.get('execution_check') != 0:
                raise SyncError('Execution gate did not pass')
            result['tests'].append(dict(suite=record['suite'], **junit(
                output / record['suite'].replace('/', '_') / 'results.xml')))
        result['passed'] = True
    except (SyncError, OSError, ValueError, ET.ParseError, KeyError) as error:
        result['error'] = str(error)
    write_json(output / 'verification.json', result)
    if not result['passed']:
        raise SyncError(result['error'])
    return result


def report(repo, plan, verification, destination):
    """Record frozen inputs and original messages without claiming integration."""
    if verification.get('passed') and verification.get('source_fingerprint') != fingerprint(repo):
        raise SyncError('Validation does not match current source inputs')
    now = datetime.now(ZoneInfo('Asia/Shanghai'))
    head = text(repo, 'rev-parse', 'HEAD')
    lines = ['# ' + now.isoformat(timespec='seconds') + ' - Manual upstream batch', '',
             'Source: https://github.com/zephyrproject-rtos/zephyr, main',
             'Upstream range: `' + plan['upstream_from'] + '..' + plan['upstream_to'] + '`',
             'EmBT starting SHA: `' + plan['embt_base'] + '`',
             'EmBT current SHA: `' + head + '`',
             'Integration state: local worktree; not merged.', '']
    if plan.get('assessment'):
        lines += ['## Complete applicability decisions', '',
                  '| Original SHA | Decision | Reason |', '|---|---|---|']
        for item in plan['assessment']:
            reason = item['rationale'].replace('|', '\\|').replace('\n', ' ')
            lines.append('| `' + item['sha'] + '` | ' + item['decision'] + ' | ' + reason + ' |')
        lines.append('')
    lines += ['## Original commits and scope', '']
    for entry in plan['commits']:
        lines += ['### ' + entry['sha'] + ' - ' + entry['title'], '',
                  'Author: ' + entry['author'] + ' <' + entry['email'] + '>, ' + entry['author_date'],
                  'State: ' + entry['state'],
                  'EmBT imported commit: ' + entry.get('embt_commit', 'not assigned'), '', '~~~~text', entry['message'].rstrip('\n'), '~~~~', '']
        scope = entry.get('reviewed_scope')
        if scope:
            lines += ['Adaptation: ' + scope['adaptation'], '',
                      'Planned validation (not an execution claim): ' +
                      json.dumps(scope['tests']), '']
        for p in entry['paths']:
            included = p['target'] and (not scope or p['target'] in scope['selected_targets'])
            target = p['target'] if included else 'not imported'
            reason = p['reason'] if included or not scope else scope['omitted_sources'][p['source']]
            lines.append('- `' + p['source'] + '` -> `' + target + '`: ' + reason)
        if scope:
            for target, reason in scope['additional_targets'].items():
                lines.append('- EmBT test support -> `' + target + '`: ' + reason)
        lines.append('')
    lines += ['## Validation', '', '~~~~json', json.dumps(verification, indent=2), '~~~~', '',
              'This record does not advance an integrated cursor. Uncommitted changes have no',
              'imported EmBT commit SHA. Model/unit tests do not establish radio runtime.', '']
    Path(destination).parent.mkdir(parents=True, exist_ok=True)
    notice = (ROOT / 'docs/UPSTREAM.md').read_text().split('-->')[0] + '-->\n\n'
    Path(destination).write_text(notice + '\n'.join(lines))


def parse_patches(arguments):
    pairs = [p.split('=', 1) for p in arguments]
    if any(len(p) != 2 for p in pairs) or len({p[0] for p in pairs}) != len(pairs):
        raise SyncError('Supply one unique SHA=patch argument per commit')
    return dict(pairs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=ROOT)
    commands = parser.add_subparsers(dest='action', required=True)
    p = commands.add_parser('scan'); p.add_argument('--from', dest='start', default=BASELINE); p.add_argument('--to', default='upstream/main'); p.add_argument('--output', type=Path, required=True)
    p = commands.add_parser('prepare'); p.add_argument('--scan', type=Path, required=True); p.add_argument('--commit', action='append', required=True); p.add_argument('--output', type=Path, required=True)
    p = commands.add_parser('apply'); p.add_argument('--plan', type=Path, required=True); p.add_argument('--patch', action='append', required=True); p.add_argument('--dry-run', action='store_true'); p.add_argument('--output', type=Path, required=True)
    p = commands.add_parser('verify'); p.add_argument('--suite', action='append', required=True); p.add_argument('--build-root', type=Path, required=True); p.add_argument('--output', type=Path, required=True)
    p = commands.add_parser('report'); p.add_argument('--plan', type=Path, required=True); p.add_argument('--verification', type=Path, required=True); p.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == 'scan': write_json(args.output, scan(args.repo, args.start, args.to))
        elif args.action == 'prepare': write_json(args.output, prepare(args.repo, json.loads(args.scan.read_text()), args.commit))
        elif args.action == 'apply': apply(args.repo, json.loads(args.plan.read_text()), parse_patches(args.patch), args.output, args.dry_run)
        elif args.action == 'verify': verify(args.repo, args.suite, args.build_root, args.output)
        else: report(args.repo, json.loads(args.plan.read_text()), json.loads(args.verification.read_text()), args.output)
    except (SyncError, OSError, ValueError, KeyError) as error:
        parser.exit(1, 'Upstream sync stopped: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
