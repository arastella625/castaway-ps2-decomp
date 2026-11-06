"""Expand body.cpp by hand: cc1plus has no preprocessor. Usage: expand.py <in.cpp> <out.ii>"""
import re, sys
s = open(sys.argv[1]).read()
s = re.sub(r'//.*', '', s)
s = re.sub(r'#define.*\n', '', s)
s = re.sub(r'V4\((\d+)\)', lambda m: ' '.join('virtual void v%s%s();' % (m.group(1), c) for c in 'abcd'), s)
open(sys.argv[2], 'w', newline='\n').write(s)
