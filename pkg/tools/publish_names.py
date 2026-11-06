#!/usr/bin/env python3
"""Copy a names CSV without the example_strings column, which quotes text from the game.
match.py keeps that column for local review; publish this copy instead.
Usage: publish_names.py <in.csv> <out.csv>
"""
import sys, csv

with open(sys.argv[1], encoding='utf8', errors='replace', newline='') as fh:
    rows = list(csv.DictReader(fh))
fields = [f for f in rows[0] if f != 'example_strings']
with open(sys.argv[2], 'w', encoding='utf8', newline='') as fh:
    w = csv.DictWriter(fh, fieldnames=fields, extrasaction='ignore')
    w.writeheader()
    w.writerows(rows)
print('rows', len(rows), 'columns', len(fields))
