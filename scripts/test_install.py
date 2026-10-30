"""Installer integration checks in temporary directories.

Run: python3 scripts/test_install.py
Uses Bash on macOS/Linux and Windows PowerShell on Windows.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[1]
SKILLS = {p.parent.name: p.parent for p in (REPO / 'skills').glob('*/*/SKILL.md')}
BOARD = 'rp2040-pico'


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='mcu installer ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.codex = self.root / 'codex skills'
        self.claude = self.root / 'claude skills'
        self.env = dict(os.environ, CODEX_SKILLS_DIR=str(self.codex),
                        CLAUDE_SKILLS_DIR=str(self.claude))
        if os.name == 'nt':
            self.command = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                            '-File', str(REPO / 'scripts/install.ps1')]
        else:
            self.command = ['bash', str(REPO / 'scripts/install.sh')]

    def install(self, *args, input='', code=0):
        result = subprocess.run(self.command + list(args), cwd=self.root,
                                env=self.env, input=input, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result.stdout

    def test_list_does_not_install(self):
        output = self.install('--codex', '--list')
        for name in SKILLS:
            self.assertIn(name, output)
        self.assertFalse(self.codex.exists())
        self.assertFalse(self.claude.exists())

    def test_agents_have_separate_destinations(self):
        self.install('--codex', BOARD)
        self.assertTrue((self.codex / BOARD).samefile(SKILLS[BOARD]))
        self.assertFalse(self.claude.exists())
        self.install(BOARD)
        self.assertTrue((self.claude / BOARD).samefile(SKILLS[BOARD]))
        self.install('--codex', '--uninstall', BOARD)
        self.assertFalse((self.codex / BOARD).exists())
        self.assertTrue((self.claude / BOARD / 'SKILL.md').is_file())
        self.assertTrue((SKILLS[BOARD] / 'SKILL.md').is_file())
        self.install('--claude', '--uninstall', BOARD)

    def test_all_links_and_repeat_install(self):
        self.install('--codex', '--all')
        self.assertEqual({p.name for p in self.codex.iterdir()}, set(SKILLS))
        self.install('--codex', '--all')
        for name, source in SKILLS.items():
            self.assertTrue((self.codex / name).samefile(source))
        self.install('--codex', '--uninstall', *SKILLS)
        self.assertEqual(list(self.codex.iterdir()), [])
        self.assertTrue(all((p / 'SKILL.md').is_file() for p in SKILLS.values()))

    def test_copy_includes_resources_and_hidden_files(self):
        self.install('--codex', BOARD, '--copy')
        target = self.codex / BOARD
        self.assertFalse(target.samefile(SKILLS[BOARD]))
        for source in SKILLS[BOARD].rglob('*'):
            if source.is_file():
                self.assertEqual((target / source.relative_to(SKILLS[BOARD])).read_bytes(),
                                 source.read_bytes())
        self.install('--codex', '--uninstall', BOARD)
        self.assertFalse(target.exists())

    def test_replacement_and_switching_modes(self):
        self.install('--codex', BOARD)
        self.install('--codex', BOARD, '--copy', '--yes')
        target = self.codex / BOARD
        self.assertFalse(target.samefile(SKILLS[BOARD]))
        marker = target / 'local-edit.txt'
        marker.write_text('keep unless replacement requested')
        self.install('--codex', BOARD, '--copy', input='n\n')
        self.assertTrue(marker.exists())
        self.install('--codex', BOARD, '--copy', '--yes')
        self.assertFalse(marker.exists())
        self.install('--codex', BOARD, '--link', '--yes')
        self.assertTrue(target.samefile(SKILLS[BOARD]))
        self.install('--codex', '--uninstall', BOARD)

    def test_invalid_requests_do_not_partially_install(self):
        self.install('--codex', BOARD, 'no-such-board', code=1)
        self.assertFalse(self.codex.exists())
        for name in ('../outside', '.', '*', 'rp2040-pico/..'):
            self.install('--codex', '--uninstall', name, code=2)
        self.install('--codex', '--unknown', code=2)
        self.install('--codex', code=2)
        self.assertFalse(self.codex.exists())

    @unittest.skipIf(os.name == 'nt', 'Scaffolder requires Bash')
    def test_installed_template_scaffolds_project(self):
        self.install('--codex', BOARD, '--copy')
        project = self.root / 'new project'
        subprocess.run(['bash', './template/variants/new-project.sh', str(project),
                        '--minimal'], cwd=self.codex / BOARD, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertTrue((project / 'platformio.ini').is_file())
        self.assertTrue((project / 'src/main.cpp').is_file())


if __name__ == '__main__':
    unittest.main()
