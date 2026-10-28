from analyze_v411_owner_draw_log import analyze
log='''[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0x1 kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=8 live_readable=0 cache_readback=0
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0x2 kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=9 live_readable=1 live_manager_agrees=0
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0x3 kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=7 matched_run=1 latest_run=2
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0x4 kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=1 historical_snapshot=1 live_readable=1 live_manager_agrees=1 cache_readback=1 cache_seq_matches=0 manager_matches=1
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0x5 kind=2 row=30 serial=3 native_producer_seq=25 stamp_state=255
'''
r=analyze(log)
assert r['gpu_draw_samples']==5
assert r['native_actor_owner_certification']=='OPEN'
assert r['classifications']=={
    'source1_context_unreadable':1,
    'source1_native_manager_disagreement':1,
    'source1_ambiguous_context_identity':1,
    'source1_historical_stamp_cache_advanced_actor_open':1,
    'protected_source2_no_native_owner_join':1},r['classifications']
print('V413 CLASSIFIER PASS: native memory/readback/ABA remain distinct; Source2 protected')
