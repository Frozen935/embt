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
"""Build locally declared test units and record actual configuration/execution outcomes."""
import argparse
import concurrent.futures
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'test/bluetooth'


def discover_units():
    return {str(path.parent.relative_to(TESTS)): json.loads(path.read_text())
            for path in sorted(TESTS.rglob('test.json'))}


def check_configuration(build, options):
    """Compare scenario intent with the canonical Kconfig driver's typed output."""
    payload = json.loads((build / 'kconfig/config.json').read_text())
    report = {'config_sha256': payload['config_sha256'], 'checks': [], 'errors': []}
    actual_hash = hashlib.sha256((build / '.config').read_bytes()).hexdigest()
    if actual_hash != payload['config_sha256']:
        report['errors'].append('Canonical configuration metadata does not match .config')
        return report
    for key, requested in dict(option.split('=', 1) for option in options).items():
        symbol = payload['symbols'].get(key)
        if symbol is None:
            report['errors'].append('Unknown expected symbol: ' + key)
            continue
        actual = symbol['value']
        if symbol['type'] == 'bool':
            if requested not in ('y', 'n'):
                raise ValueError('Expected y or n for ' + key)
            expected = requested == 'y'
        elif symbol['type'] in ('int', 'hex'):
            base = 10 if symbol['type'] == 'int' else 16
            expected, actual = int(requested, base), int(actual, base)
        elif symbol['type'] == 'string':
            expected = json.loads(requested)
        else:
            raise ValueError('Unsupported configuration type for ' + key)
        report['checks'].append({'symbol': key, 'requested': requested,
                                 'effective': symbol['value'], 'matched': actual == expected})
        if actual != expected:
            report['errors'].append(f'{key}: requested {requested}, effective {symbol["value"]}')
    return report


def check_required_cases(compiled, required):
    """Require a nonempty compiled inventory and any explicitly required cases."""
    cases = {case for suite in compiled['suites'] for case in suite['cases']}
    errors = ['No active compiled cases'] if not cases else []
    errors += ['Required case not compiled: ' + case for case in required if case not in cases]
    return {'compiled_count': len(cases), 'required_cases': required, 'errors': errors}


def check_execution(results_xml, name, required, execution):
    """Reject empty results or required cases that did not actually pass."""
    tests = list(ET.parse(results_xml).iter('testcase'))
    errors = [] if tests else ['No test cases executed']
    executed = [test for test in tests if test.find('skipped') is None and
                test.get('status', 'run') == 'run']
    if tests and not executed:
        errors.append('No test cases actually ran')
    if any(test.find('failure') is not None or test.find('error') is not None for test in tests):
        errors.append('Test results contain failures')
    for case in required:
        test_name = f'bluetooth.{name}.' + ('ordered_suite' if execution == 'suite' else case)
        matches = [test for test in tests if test.get('name') == test_name]
        passed = len(matches) == 1 and matches[0].get('status', 'run') == 'run' and all(
            matches[0].find(tag) is None for tag in ('failure', 'error', 'skipped'))
        if passed and execution == 'suite':
            passed = re.search(r'^\[\s*OK\s*\]\s+' + re.escape(case) + r'\s*$',
                               matches[0].findtext('system-out', ''), re.M) is not None
        if not passed:
            errors.append('Required case did not pass: ' + case)
    return {'test_entries': len(tests), 'executed_entries': len(executed),
            'required_cases': required, 'errors': errors}


def run_suite(name, build_root, output, metadata, variant=None):
    token = name.replace('/', '_')
    record = variant['scenario'] if variant else token
    build = build_root / (('variant-' + record) if variant else ('upstream-' + token))
    logs = output / record
    logs.mkdir(parents=True, exist_ok=True)
    fragment = TESTS / name / 'config.conf'
    if variant:
        configuration = dict(re.findall(r'^CONFIG_(\w+)=(.*)$', fragment.read_text(), re.M))
        for option in variant.get('options', []):
            key, value = option[len('CONFIG_'):].split('=', 1)
            configuration[key] = value
        configuration['EMBT_TEST_CONFIG'] = 'y'
        build.mkdir(parents=True, exist_ok=True)
        fragment = build / 'profile.conf'
        fragment.write_text(''.join('CONFIG_' + key + '=' + value + '\n'
                                    for key, value in configuration.items()))
    target = metadata.get('target', 'test_bt_' + token)
    commands = []
    if (build / '.config').exists():
        commands.append(('reset', ['python3', str(ROOT / 'tools/kconfig/configure.py'),
                                  'defconfig', '--build-dir', str(build),
                                  '--fragment', str(fragment)]))
    commands.extend([
        ('configure', ['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
                       '-DBUILD_TESTING=ON', '-DEMBT_BT_TEST_FILTER=' + name,
                       '-DEMBT_CONFIG_FRAGMENTS=' + str(fragment)]),
        ('build', ['cmake', '--build', str(build), '--target', target, '--parallel', '3']),
    ])
    result = {'suite': name, 'build_dir': str(build), 'logs': str(logs)}
    if variant:
        result['scenario'] = variant['scenario']
    for phase, command in commands:
        with (logs / (phase + '.log')).open('w') as stream:
            completed = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                                       cwd=ROOT, check=False)
        result[phase] = completed.returncode
        if completed.returncode:
            result['status'] = phase + '-error'
            break
        if phase == 'configure':
            try:
                report = check_configuration(build, variant.get('options', []) if variant else [])
            except (OSError, ValueError, KeyError) as error:
                report = {'errors': [str(error)]}
            (logs / 'configuration-check.json').write_text(json.dumps(report, indent=2) + '\n')
            result['configuration_check'] = 1 if report['errors'] else 0
            if report['errors']:
                result['status'] = 'configuration-check-error'
                result['reason'] = '; '.join(report['errors'])
                break
    else:
        runtime = build / 'test/bluetooth'
        required = []
        if metadata['runner'] == 'cmocka':
            executable = runtime / target
            manifest = subprocess.run([str(executable), '--list-json'], text=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            if manifest.returncode:
                result['status'] = 'discovery-error'
                result['test'] = manifest.returncode
                (logs / 'discovery.log').write_text(manifest.stdout + manifest.stderr)
            else:
                compiled = json.loads(manifest.stdout)
                (logs / 'cases.json').write_text(json.dumps(compiled, indent=2) + '\n')
                scenario_required = variant.get('required_cases', []) if variant else []
                required = list(dict.fromkeys(metadata.get('required_cases', []) + scenario_required))
                report = check_required_cases(compiled, required)
                if variant and not required:
                    report['errors'].append('Scenario must declare required_cases')
                (logs / 'case-check.json').write_text(json.dumps(report, indent=2) + '\n')
                result['case_check'] = 1 if report['errors'] else 0
                if report['errors']:
                    result['test'] = None
                    result['status'] = 'case-check-error'
                    result['reason'] = '; '.join(report['errors'])
                command = ['ctest', '--test-dir', str(build), '-L', '^unit$', '--output-on-failure',
                           '--output-junit', str(logs / 'results.xml'),
                           '--test-output-size-passed', '1048576', '--test-output-size-failed', '1048576']
        elif metadata['runner'] in ('pytest', 'linked'):
            executable = runtime / 'classic'
            if metadata['runner'] == 'linked':
                executable /= 'bumble'
            executable /= target
            command = [sys.executable, '-m', 'pytest', '--import-mode=importlib',
                       str(TESTS / metadata['pytest']), '--embt-build=' + str(executable),
                       '--timeout=' + str(metadata['timeout_s']),
                       '--junitxml=' + str(logs / 'results.xml'),
                       '--basetemp=' + str(logs / 'pytest')]
            if metadata.get('selection'):
                command.extend(['-k', metadata['selection']])
        else:
            result['status'] = 'unsupported'
            result['test'] = 1
            result['reason'] = metadata.get('unsupported_reason', 'No execution adapter is defined')
        if 'test' not in result:
            (logs / 'results.xml').unlink(missing_ok=True)
            with (logs / 'test.log').open('w') as stream:
                completed = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                                           cwd=ROOT, check=False)
            result['test'] = completed.returncode
            result['status'] = 'passed' if completed.returncode == 0 else 'test-error'
            if not completed.returncode:
                try:
                    report = check_execution(logs / 'results.xml', name, required,
                                             metadata.get('execution', 'case'))
                except (OSError, ValueError, ET.ParseError) as error:
                    report = {'errors': [str(error)]}
                (logs / 'execution-check.json').write_text(json.dumps(report, indent=2) + '\n')
                result['execution_check'] = 1 if report['errors'] else 0
                if report['errors']:
                    result['status'] = 'execution-check-error'
                    result['test'] = 1
                    result['reason'] = '; '.join(report['errors'])
    (logs / 'status.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', action='append', help='Test unit directory; repeat as needed')
    parser.add_argument('--tag', action='append', help='Require a unit tag; repeat to require all tags')
    parser.add_argument('--variants', action='store_true', help='Run locally declared configuration scenarios')
    parser.add_argument('--scenario', action='append', help='Select a named scenario with --variants')
    parser.add_argument('--include-unsupported', action='store_true', help='Attempt units needing retired providers')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--build-root', type=Path, default=ROOT / 'build')
    parser.add_argument('--output', type=Path, required=True)
    arguments = parser.parse_args()
    if arguments.scenario and not arguments.variants:
        parser.error('--scenario requires --variants')
    if arguments.jobs < 1:
        parser.error('--jobs must be positive')
    units = discover_units()
    suites = arguments.suite or [name for name, unit in units.items()
                                if unit['runner'] == 'cmocka' and
                                (arguments.include_unsupported or not unit.get('unsupported_reason'))]
    unknown = set(suites) - units.keys()
    if unknown:
        parser.error('Unknown units: ' + ', '.join(sorted(unknown)))
    if arguments.tag:
        suites = [name for name in suites if set(arguments.tag) <= set(units[name]['tags'])]
    arguments.output = arguments.output.resolve()
    arguments.output.mkdir(parents=True, exist_ok=True)
    tasks = [(name, None) for name in suites]
    if arguments.variants:
        selected = set(arguments.scenario or [])
        all_scenarios = {s for u in units.values() for s in u['scenarios'] if s != 'default'}
        if selected - all_scenarios:
            parser.error('Unknown scenarios: ' + ', '.join(sorted(selected - all_scenarios)))
        tasks = [(name, {'scenario': scenario, **options})
                 for name in suites for scenario, options in units[name]['scenarios'].items()
                 if scenario != 'default' and (not selected or scenario in selected)]
    if not tasks:
        parser.error('No supported test units or scenarios selected')
    with concurrent.futures.ThreadPoolExecutor(max_workers=arguments.jobs) as executor:
        results = list(executor.map(lambda task: run_suite(task[0], arguments.build_root.resolve(),
                                                          arguments.output, units[task[0]], task[1]), tasks))
    (arguments.output / 'status.json').write_text(json.dumps(results, indent=2) + '\n')
    excluded = [{'suite': name, 'reason': unit['unsupported_reason']}
                for name, unit in units.items() if unit.get('unsupported_reason') and name not in suites]
    (arguments.output / 'excluded.json').write_text(json.dumps(excluded, indent=2) + '\n')
    return 0 if all(result.get('test') == 0 for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
