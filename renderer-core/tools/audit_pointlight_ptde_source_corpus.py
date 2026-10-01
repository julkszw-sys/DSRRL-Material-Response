"""Compare exact authored PointLight rows; never use historical translations."""
import argparse
import hashlib
import json
import struct
import zipfile
import zlib
from pathlib import Path, PureWindowsPath


def banks(path, ptde):
    result = {}
    with zipfile.ZipFile(path) as archive:
        for name in archive.namelist():
            if not name.endswith('.dcx') or (ptde and '/PTDE_ORIGINAL/' not in name):
                continue
            packed = archive.read(name)
            if packed[0x44:0x48] != b'DCA\0':
                raise ValueError('Unsupported DCX')
            bnd = zlib.decompress(packed[0x4c:])
            if bnd[:4] != b'BND3':
                raise ValueError('Unsupported binder')
            for i in range(struct.unpack_from('<I', bnd, 0x10)[0]):
                size, off, _, noff, _ = struct.unpack_from('<5I', bnd, 0x24 + 24*i)
                entry = bnd[noff:bnd.index(b'\0', noff)].decode('shift_jis')
                if not entry.endswith('PointLightBank.param'):
                    continue
                key = PureWindowsPath(entry).name
                if key in result:
                    raise ValueError('Duplicate bank')
                data = bnd[off:off+size]
                count = struct.unpack_from('<H', data, 0xa)[0]
                rows = []
                for j in range(count):
                    row_id, roff, _ = struct.unpack_from('<III', data, 0x30+12*j)
                    if row_id != j or roff+16 > len(data):
                        raise ValueError('Unproven row layout')
                    rows.append(list(struct.unpack_from('<ffhhhh', data, roff)))
                result[key] = {'sha256': hashlib.sha256(data).hexdigest(), 'rows': rows}
    return result


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--ptde', required=True)
    p.add_argument('--dsr', required=True)
    p.add_argument('--output', required=True)
    a = p.parse_args()
    target, host = banks(a.ptde, True), banks(a.dsr, False)
    counts = dict(rows=0, identical=0, range_diff=0, ptde_end_larger=0, rgbm_diff=0)
    changed = []
    for key, bank in sorted(target.items()):
        for i, row in enumerate(bank['rows']):
            dsr = host[key]['rows'][i]
            counts['rows'] += 1
            counts['identical'] += row == dsr
            counts['range_diff'] += row[:2] != dsr[:2]
            counts['ptde_end_larger'] += row[1] > dsr[1]
            counts['rgbm_diff'] += row[2:] != dsr[2:]
            if row != dsr:
                changed.append({'bank': key, 'row': i, 'range_diff': row[:2] != dsr[:2],
                                'ptde_end_larger': row[1] > dsr[1], 'rgbm_diff': row[2:] != dsr[2:]})
    report = {'schema': 'dsrrl.pointlight.ptde_source_corpus.v1', 'counts': counts,
              'input_sha256': {k: hashlib.sha256(Path(v).read_bytes()).hexdigest()
                               for k, v in [('ptde', a.ptde), ('dsr', a.dsr)]},
              'bank_sha256': {k: {'ptde': v['sha256'], 'dsr': host[k]['sha256']}
                              for k, v in sorted(target.items())},
              'changed_rows': changed,
              'required_transport': ['exact bank and row identity', 'paired rows and beta for LerpBank',
                                     'PTDE q plus Begin/End', 'selection/culling support before consumer'],
              'runtime_activation': 'OPEN', 'pixel_behavior': 'OPEN'}
    Path(a.output).write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(counts))


if __name__ == '__main__':
    main()
