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

"""Check the imported Classic helpers against the command-output contract."""
import ast
import asyncio
import logging
from pathlib import Path
import re
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


def load_helpers(path, names):
    tree = ast.parse(path.read_text(), filename=str(path))
    selected = [node for node in tree.body if isinstance(node, (ast.FunctionDef,
                ast.AsyncFunctionDef)) and node.name in names]
    namespace = {'asyncio': asyncio, 're': re, 'logger': logging.getLogger('response-test')}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(path), 'exec'), namespace)
    return namespace['send_cmd_to_iut']


class ClassicResponseTests(unittest.TestCase):
    def test_gap_command_output_and_later_response(self):
        for name in ('gap_c/pytest/test_gap_c.py', 'gap_s/pytest/test_gap_s.py'):
            send = load_helpers(ROOT / 'test/bluetooth/classic' / name,
                                {'send_cmd_to_iut', '_wait_for_shell_response'})
            for immediate, later, expected in ((['ready'], [], True),
                    (['ready'], [], False), (['command'], ['ready'], True),
                    (['command'], ['ready'], False), (['command'], [], False)):
                with self.subTest(module=name, immediate=immediate, later=later,
                                  expected=expected):
                    shell = mock.Mock()
                    shell.exec_command.return_value = immediate
                    dut = mock.Mock()
                    dut.readlines.return_value = later
                    with mock.patch.object(asyncio, 'sleep', new=mock.AsyncMock()):
                        task = send(shell, dut, 'probe', response='ready',
                                    expect_to_find_resp=expected, max_wait_sec=1)
                        if not expected and ('ready' in immediate or 'ready' in later):
                            with self.assertRaises(AssertionError):
                                asyncio.run(task)
                        else:
                            lines = asyncio.run(task)
                            self.assertIn(immediate[0], lines)
                            if expected:
                                self.assertIn('ready', lines)

    def test_smp_regex_matches_across_command_and_later_output(self):
        send = load_helpers(ROOT / 'test/bluetooth/classic/smp_sc_only/pytest/test_smp.py',
                            {'send_cmd_to_iut', 'wait_for_shell_response', 'search_messages'})
        shell = mock.Mock()
        shell.exec_command.return_value = ['passkey 123456']
        dut = mock.Mock()
        dut.readlines.return_value = ['paired']
        with mock.patch.object(asyncio, 'sleep', new=mock.AsyncMock()):
            lines = asyncio.run(send(shell, dut, 'probe', parse=[r'passkey \d+', 'paired']))
        self.assertEqual(lines, ['passkey 123456', 'paired'])

    def test_unparsed_command_returns_its_output(self):
        for name in ('gap_c/pytest/test_gap_c.py', 'gap_s/pytest/test_gap_s.py',
                     'smp_sc_only/pytest/test_smp.py'):
            send = load_helpers(ROOT / 'test/bluetooth/classic' / name,
                                {'send_cmd_to_iut', '_wait_for_shell_response',
                                 'wait_for_shell_response', 'search_messages'})
            shell = mock.Mock()
            shell.exec_command.return_value = ['command output']
            self.assertEqual(asyncio.run(send(shell, mock.Mock(), 'probe')), ['command output'])


if __name__ == '__main__':
    unittest.main()
