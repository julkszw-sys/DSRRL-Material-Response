#!/usr/bin/env python3
from v438_apply_draw_part_a import patch,patch_last,integrated
patch_last(integrated,'''#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    if (prepared.envspec_in_batch) {''',
'''#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    if(decision.receiver_id>=33u && decision.receiver_id<=35u && prepared.envspec_in_batch &&
       !dsrrl::runtime::draw_tx_issued(dispatch.transaction)) {
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::ENV_FAIL,{
            material.model_owner_instance,
            dsrrl::runtime::v438::p(prepared.envspec.shader),
            prepared.envspec.source_kind,prepared.envspec.stable_envdiffuse_row,
            prepared.envspec.stable_envdiffuse_native_producer_sequence,
            static_cast<unsigned>(dispatch.transaction)});
    }
#endif
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    if (prepared.envspec_in_batch) {''')
patch(integrated,'''    g_clustered_pnts.frame_event(present);''',
'''    g_clustered_pnts.frame_event(present);
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    dsrrl::runtime::v438::on_present();
#endif''')
print('V438 source overlay: typed wrappers, native producer, owner, Source1 and issued RX34 PASS')
