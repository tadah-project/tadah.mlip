"""The commands of the ``bash`` code blocks of an rst file.

    bash_commands(path) -> [Command(line, argv), ...]
    bash_blocks(path)   -> [Block(line, commands), ...]

Shared by the tests that run what the documentation tells a reader to run:
cli_examples/check_cli_example.py (the CLI examples' README.rst) and
nfp_docs/check_nfp_run.py (the Nested Fitting pages).

A command is a line of the content of a ``.. code-block:: bash`` directive,
joined with the lines that a trailing backslash continues, and split into
words as the shell splits them (shlex; ``#`` starts a comment). The
directive's options (``:caption:``, ...) are not content; blank and
comment-only lines are not commands. The content ends at the first
non-blank line indented no more than the directive. ``line`` is the
1-based line of the file where the command starts.

A Block is one such directive: ``line`` is the 1-based line of the
directive itself (the line Sphinx gives the block it renders, which
docs/tools/nfp_checks.py compares with the blocks of a page), and
``commands`` are its commands. Only this spelling is read: another
directive that renders a bash block (``code``, ``sourcecode``, a
``literalinclude``, other spacing or letter case) is not, and nfp_checks
warns on such a block in a Nested Fitting page.
"""
import re
import shlex
from collections import namedtuple

DIRECTIVE = re.compile(r'^(\s*)\.\. code-block::\s*bash\s*$')
OPTION = re.compile(r'^\s*:[\w-]+:')

Command = namedtuple('Command', 'line argv')
Block = namedtuple('Block', 'line commands')


def bash_blocks(path):
    '''The bash code blocks of the rst file *path*, in order.'''
    with open(path, encoding='utf-8') as fh:
        lines = fh.read().splitlines()
    blocks = []
    i = 0
    while i < len(lines):
        m = DIRECTIVE.match(lines[i])
        i += 1
        if not m:
            continue
        block = Block(i, [])
        blocks.append(block)
        commands = block.commands
        indent = len(m.group(1))
        # The directive's options: indented lines right after it.
        while (i < len(lines) and lines[i].strip()
               and len(lines[i]) - len(lines[i].lstrip()) > indent
               and OPTION.match(lines[i])):
            i += 1
        pending, start = '', None
        while i < len(lines):
            line = lines[i]
            if line.strip() and len(line) - len(line.lstrip()) <= indent:
                break
            i += 1
            text = line.strip()
            if start is None:
                start = i
            if text.endswith('\\'):
                pending += text[:-1] + ' '
                continue
            argv = shlex.split(pending + text, comments=True)
            if argv:
                commands.append(Command(start, argv))
            pending, start = '', None
        if pending.strip():
            argv = shlex.split(pending, comments=True)
            if argv:
                commands.append(Command(start, argv))
    return blocks


def bash_commands(path):
    '''The commands of the bash code blocks of the rst file *path*, in
    order.'''
    return [c for b in bash_blocks(path) for c in b.commands]


def tadah_commands(path):
    '''The argument lists of the ``tadah`` commands of *path*, in order.'''
    return [c.argv for c in bash_commands(path) if c.argv[0] == 'tadah']
