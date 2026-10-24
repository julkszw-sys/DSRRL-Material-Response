#!/usr/bin/env python3
"""Read-only parser for bounded Warrior 9360 ReShade first-cut telemetry."""
import re
import sys
from pathlib import Path
from collections import defaultdict

FIRST = re.compile(r'\[DSRRL WARRIOR 9360 FIRST CUT\]\s+(.*)')
FIELD = re.compile(r'([a-z][a-z0-9_]*)=([^\s]+)')
STAGES = ('selector_invalid','selector_not_authenticated','selector_published',
          'pre_route','no_pipeline_route','no_dynamic_route','identity_reject',
          'identity_ready','prepare_reject','prepared',
          'parser_registered','selector_flver_lookup')

def parse(source):
    rows=[]
    for line in source.splitlines():
        result=FIRST.search(line)
        if result:
            fields=dict(FIELD.findall(result.group(1)))
            if fields.get('model','').endswith('_9360'):
                rows.append(fields)
    return rows

def main():
    if len(sys.argv) != 2:
        print('Usage: python parse_warrior_9360_runtime.py ReShade.log')
        return 2
    path=Path(sys.argv[1])
    if not path.is_file():
        print('Log not found:',path)
        return 2
    rows=parse(path.read_text(encoding='utf-8',errors='replace'))
    print('Warrior 9360 bounded trace rows:',len(rows))
    if not rows:
        print('No positive receiver/owner inference from missing telemetry.')
        return 1
    records=defaultdict(dict)
    for row in rows:
        records[(row['model'],row.get('slot','?'))][row.get('stage','?')]=row
    for (model,slot),by_stage in sorted(records.items()):
        print('\n'+model+' / slot '+slot)
        for stage in STAGES:
            if stage in by_stage:
                r=by_stage[stage]
                print('  %-30s RX=%s MTD_route=%s route_mask=%s reason=%s'%(
                    stage,r.get('rx','?'),r.get('mtd_route','?'),
                    r.get('route_mask','?'),r.get('decision','?')))
    print('\nRuntime hits do not establish PTDE pixel equivalence.')
    return 0

if __name__=='__main__':
    sys.exit(main())
