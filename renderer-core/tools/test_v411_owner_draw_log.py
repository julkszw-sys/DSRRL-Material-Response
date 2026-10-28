from analyze_v411_owner_draw_log import analyze
text='''[DSRRL V411 OWNER SEQ STAMP] owner=0xA seq=25 epoch=3 journaled=1
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xA kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=1 cache_readback=1 cache_seq_matches=1 manager_matches=1
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xB kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=2 cache_readback=0 cache_seq_matches=0 manager_matches=0
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xA kind=2 row=30 serial=4 native_producer_seq=25 stamp_state=255
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xC kind=1 row=25 serial=3 native_producer_seq=0 stamp_state=0
'''
result=analyze(text)
assert result['stamp_samples']==1
assert result['gpu_draw_samples']==4
assert result['logged_owner_seq_epoch_matches']==1
assert result['native_actor_owner_certification']=='OPEN'
assert result['classifications']=={
 'source1_snapshot_consistent_native_actor_open':1,
 'source1_producer_changed':1,
 'protected_source2_no_native_owner_join':1,
 'source1_missing_native_sequence':1}
print('V411 LOG CLASSIFIER PASS')
