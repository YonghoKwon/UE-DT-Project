"""STYLE-02: prove a formatting change touched only whitespace.

Usage: python -X utf8 -I token_compare.py <git-rev-before> <git-rev-after> [paths...]
Compares every changed .h/.cpp between two revisions as C++ token streams. String/char
literals and preprocessor tokens must match exactly; comments match after collapsing
whitespace. Exit 1 on any difference.
"""
import re
import subprocess
import sys

# Alternatives are joined without re.VERBOSE so '#' (stringizing/token pasting) stays literal.
TOKEN = re.compile('|'.join([
    r'(?P<ws>\s+)',
    r'(?P<lcomment>//[^\n]*)',
    r'(?P<bcomment>/\*.*?\*/)',
    r'(?P<rawstr>(?:u8|u|U|L)?R"(?P<delim>[^(\s]*)\(.*?\)(?P=delim)")',
    r'(?P<str>(?:u8|u|U|L)?"(?:\\.|[^"\\\n])*")',
    r"(?P<chr>(?:u8|u|U|L)?'(?:\\.|[^'\\\n])+')",
    r'(?P<cont>\\\r?\n)',
    r'(?P<id>[A-Za-z_]\w*)',
    r"(?P<num>\.?\d(?:[\w.']|[eEpP][+-])*)",
    r'(?P<punct>::|->\*?|\.\.\.|<<=|>>=|<=>|[-+*/%&|^!=<>]=|&&|\|\||\+\+|--|<<|##|.)',
]), re.S)


def tokens(text):
    out = []
    for m in TOKEN.finditer(text.lstrip('﻿')):
        kind = m.lastgroup if m.lastgroup != 'delim' else 'rawstr'
        if kind in ('ws', 'cont'):
            continue
        value = m.group(0)
        if kind in ('lcomment', 'bcomment'):
            value = ' '.join(value.split())
        out.append(value)
    return out


def show(rev, path):
    result = subprocess.run(['git', 'show', f'{rev}:{path}'], capture_output=True)
    return result.stdout.decode('utf-8', 'replace') if result.returncode == 0 else None


before, after = sys.argv[1], sys.argv[2]
changed = subprocess.run(['git', 'diff', '--name-only', before, after, '--', *(sys.argv[3:] or ['.'])],
                         capture_output=True, text=True, check=True).stdout.split()
checked = bad = 0
for path in changed:
    if not path.endswith(('.h', '.cpp', '.inl')):
        print(f'NON-SOURCE CHANGE: {path}')
        bad += 1
        continue
    a, b = show(before, path), show(after, path)
    if a is None or b is None:
        print(f'ADDED/REMOVED: {path}')
        bad += 1
        continue
    checked += 1
    ta, tb = tokens(a), tokens(b)
    if ta != tb:
        i = next((k for k, (x, y) in enumerate(zip(ta, tb)) if x != y), min(len(ta), len(tb)))
        print(f'TOKEN DIFF: {path} at token {i}: {ta[i:i + 5]} != {tb[i:i + 5]}')
        bad += 1
print(f'checked {checked} files, differences {bad}')
sys.exit(1 if bad else 0)
