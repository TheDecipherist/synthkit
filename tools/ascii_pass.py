#!/usr/bin/env python3
"""Make a Markdown document ASCII, the way tim-writing requires, without
touching evidence.

    python3 ascii_pass.py <file> [...]

The em dash is the rule's cleanest discriminator - 0 to 0.5 percent of human
writing against 97 percent of generated - so it is the one worth being strict
about. It is replaced BY CONTEXT, not by one substitution:

  | - |      a table cell meaning "none"
  - at BOL   a list marker
  , ... ,    a paired parenthetical becomes commas
  ,          anything left

Digits, hex addresses and provenance tags cannot be touched by any rule here,
which the caller should verify by counting them before and after.
"""
import re, sys
SIMPLE = {'§':'section ', '·':'-', '…':'...', '×':'x',
          '→':' -> ', '±':'+/-', 'Â':'', '–':'-',
          '‘':"'", '’':"'", '“':'"', '”':'"'}
def convert(s):
    for k,v in SIMPLE.items(): s = s.replace(k,v)
    s = re.sub(r'\|\s*—\s*\|', '| - |', s)
    s = re.sub(r'(?m)^—\s+', '- ', s)
    s = re.sub(r'\s—\s([^—\n]{1,80}?)\s—\s', r', \1, ', s)
    s = re.sub(r'\s*—\s*', ', ', s)
    s = re.sub(r',\s*,', ',', s)
    return s
if __name__ == '__main__':
    for f in sys.argv[1:]:
        o = open(f, encoding='utf-8').read(); n = convert(o)
        if n != o:
            open(f,'w',encoding='utf-8').write(n)
            print(f"  rewrote {f}")
