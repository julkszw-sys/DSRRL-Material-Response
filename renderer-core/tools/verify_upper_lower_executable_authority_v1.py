#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv
from pathlib import Path

def rows(path:Path):
    with path.open('r',encoding='utf-8',newline='') as fh:
        data=[line for line in fh if not line.startswith('#')]
    return list(csv.DictReader(data,delimiter='\t'))

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--authority',type=Path,action='append',required=True)
    ap.add_argument('--expected-plans',type=int,default=114)
    a=ap.parse_args()

    stock_owner={}
    plan_owner={}
    replacement_owner={}
    total=0
    for path in a.authority:
        for row in rows(path):
            total+=1
            plan=int(row['plan_index'])
            stock=row['stock_sha256'].lower()
            replacement=row['replacement_sha256'].lower()

            if plan in plan_owner:
                raise SystemExit(
                    f'duplicate plan_index {plan}: {plan_owner[plan]} vs {path}')
            plan_owner[plan]=path

            if stock in stock_owner:
                raise SystemExit(
                    f'duplicate stock executable SHA {stock}: '
                    f'{stock_owner[stock]} vs {path}')
            stock_owner[stock]=path

            if replacement in replacement_owner:
                raise SystemExit(
                    f'duplicate replacement SHA {replacement}: '
                    f'{replacement_owner[replacement]} vs {path}')
            replacement_owner[replacement]=path

    if total!=a.expected_plans:
        raise SystemExit(
            f'expected {a.expected_plans} U/L executable plans, got {total}')

    if sorted(plan_owner)!=list(range(a.expected_plans)):
        raise SystemExit('U/L plan_index authority must be contiguous from 0')

    print(
        f'UPPER_LOWER_EXECUTABLE_AUTHORITY_PASS plans={total} '
        f'unique_stock={len(stock_owner)} unique_replacement={len(replacement_owner)}')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
