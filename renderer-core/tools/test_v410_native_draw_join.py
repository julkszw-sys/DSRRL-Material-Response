#!/usr/bin/env python3
"""V410 synthetic fixtures: row-only and serial-only joins are not certified."""
import importlib.util,io,sys,tempfile,zipfile
from pathlib import Path
module=Path(__file__).with_name('analyze_v410_native_producer_draw_join.py')
spec=importlib.util.spec_from_file_location('v410',module)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp); z=root/'sample.zip'
    # Minimal real-format CSV fields only. Source provenance remains absent.
    gpu=('stage,rx,actor_owner,route,slot,sem,shader,flver_sha,mtd_sha,'
         'kind,path,bank,row,diff_probe,c86,serial,time\n'
         'indexed_issued,34,00000000219A4D80,345,1,5,A,F,M,2,COMBINED,B,30,244,"1,1,1",49,15:05:45:856\n'
         'indexed_issued,34,00000000219A4D80,345,1,5,A,F,M,1,COMBINED,B,25,244,"0,0,0",3,15:05:48:187\n')
    native=('serial,decoded_rowA,model_join,time\n'
            '3,20,OPEN,15:05:45:811\n'
            '25,25,OPEN,15:05:45:828\n')
    remo=('owner,resolved_descriptor_a\n'
          '00000000219A4D80,1310\n')
    with zipfile.ZipFile(z,'w') as archive:
        archive.writestr('handoff/03_GPU_ISSUED_SAMPLES.csv',gpu)
        archive.writestr('handoff/04_NATIVE_PACKER_SAMPLES.csv',native)
        archive.writestr('handoff/05_REMO_MODEL_SELECTOR_SAMPLES.csv',remo)
    v=m.historical(z)
    assert v['gpu_samples']==2 and v['sampled_transitions']==1
    assert v['source1_serial_collision'][0]['would_mismatch']
    assert v['source1_serial_collision'][0]['native_serial_rows']==['20']
    assert v['row25']['owner_native_producer_join']=='OPEN'
    log=root/'run.log'
    log.write_text('\n'.join([
        '[DSRRL V383 NATIVE LIGHTBANK] serial=25 decoded_rowA=25',
        '[DSRRL V408 SOURCE1 PACKER JOIN] owner=AA native_producer_seq=25 cache_readback=1 same_producer=1',
        '[DSRRL V409 RX34 GPU TRANSITION] owner=AA kind=1 row=25 native_producer_seq=25',
        '[DSRRL V409 RX34 GPU TRANSITION] owner=BB kind=1 row=25 native_producer_seq=25',
        '[DSRRL V409 RX34 GPU TRANSITION] owner=AA kind=1 row=25 native_producer_seq=0',
        '[DSRRL V409 RX34 GPU TRANSITION] owner=AA kind=2 row=30 native_producer_seq=25',
    ]))
    actual=m.classify_log(log)
    states=[e['stage'] for e in actual['classified']]
    assert states==['CACHE_READBACK_AND_NATIVE_SEQ_MATCH',
                    'NATIVE_SEQ_ONLY_NO_OWNER_JOIN',
                    'NO_NATIVE_SEQUENCE','SOURCE2_UNOWNED_FALLBACK'],states
    assert all(e['native_actor_ownership'].startswith('OPEN') for e in actual['classified'])
print('V410 OFFLINE CLASSIFIER PASS: source epoch collision, row-only rejection, 4 provenance stages')
