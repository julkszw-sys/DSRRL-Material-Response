from analyze_v411_owner_draw_log import analyze
text='''[DSRRL V411 OWNER SEQ STAMP] owner=0xA seq=25 epoch=3 journaled=1
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xA kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=1 historical_snapshot=1 cache_readback=1 cache_seq_matches=0 manager_matches=1
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xB kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=6 history_evicted=1 cache_readback=0 cache_seq_matches=0
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xC kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=7 pointer_reassignment_seen=1
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xD kind=2 row=30 serial=3 native_producer_seq=25 stamp_state=255
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xE kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=1 historical_snapshot=0 cache_readback=1 cache_seq_matches=0 manager_matches=1
[DSRRL V411 RX34 OWNER SEQ DRAW] owner=0xF kind=1 row=25 serial=3 native_producer_seq=25 stamp_state=1 historical_snapshot=1 cache_readback=1 cache_seq_matches=1 manager_matches=1
'''
result=analyze(text)
assert result['stamp_samples']==1
assert result['gpu_draw_samples']==6
counts=result['classifications']
assert counts['source1_historical_stamp_cache_advanced_actor_open']==1
assert counts['source1_history_evicted']==1
assert counts['source1_ambiguous_context_identity']==1
assert counts['protected_source2_no_native_owner_join']==1
assert counts['source1_cache_sequence_mismatch']==1
assert counts['source1_historical_stamp_consistent_actor_open']==1
assert result['native_actor_owner_certification']=='OPEN'
print('V412 LOG HISTORY CLASSIFICATION PASS; historical cache advance != proven corruption')
