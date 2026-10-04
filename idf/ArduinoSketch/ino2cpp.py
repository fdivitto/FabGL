#!/usr/bin/env python3
"""
Converts an Arduino sketch to C++ like the Arduino IDE does: includes Arduino.h and adds forward
declarations of the sketch functions just before the first one.

usage: ino2cpp.py sketch.ino output.cpp
"""
import re, sys

src = open(sys.argv[1], encoding='utf-8', errors='replace').read()

# blank out comments and string/char literals, keeping line structure
def blank(m):
    return re.sub(r'[^\n]', ' ', m.group(0))
clean = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', blank, src, flags=re.S)

# keep only text at brace depth 0
out, depth = [], 0
for ch in clean:
    if ch == '{':
        if depth == 0:
            out.append('{')
        depth += 1
    elif ch == '}':
        depth -= 1
    elif depth == 0 or ch == '\n':
        out.append(ch)
top = ''.join(out)

protos = []
firstPos = None
pat = re.compile(r'(?:^|[;}\n])\s*((?:[A-Za-z_][\w:<>\*&\s]*?)[\s\*&]+([A-Za-z_]\w*)\s*\(([^;{}()]*)\))\s*\{', re.S)
for m in pat.finditer(top):
    sig, name, args = m.group(1), m.group(2), m.group(3)
    first = sig.split()[0]
    if first in ('if', 'else', 'while', 'for', 'switch', 'return', 'class', 'struct', 'enum', 'namespace', 'typedef', 'do', 'using'):
        continue
    if '::' in sig.split('(')[0] or '=' in sig.split('(')[0]:
        continue   # methods and initialisers
    if firstPos is None:
        firstPos = m.start(1)
    args = re.sub(r'\s*=\s*[^,)]+', '', args)   # default arguments stay in the definition
    head = sig[:sig.index('(')]
    protos.append(' '.join(head.split()) + '(' + ' '.join(args.split()) + ');')

protos = list(dict.fromkeys(protos))

# "top" keeps one character per source character only for newlines, so map the first function back by line number
line = top.count('\n', 0, firstPos) if firstPos is not None else 0
lines = src.split('\n')
path = sys.argv[1]
res  = ['#include <Arduino.h>', '#line 1 "%s"' % path]
res += lines[:line]
res += protos
res += ['#line %d "%s"' % (line + 1, path)]
res += lines[line:]
open(sys.argv[2], 'w').write('\n'.join(res) + '\n')
