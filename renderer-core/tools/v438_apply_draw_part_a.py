#!/usr/bin/env python3
"""V438 read-only issued receiver and Source1/Source2 journal observation."""
from v438_apply_source import patch,patch_last,integrated
patch(integrated,'#include "dsrrl/runtime/runtime_hot_telemetry.hpp"',
'''#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
#include "dsrrl/runtime/v438_one_shot_trace.hpp"
#endif''')
patch(integrated,'''    if (!prepared.ready || !prepared.stable_envdiffuse_trace_valid ||
        decision.receiver_id < 33u || decision.receiver_id > 35u)
        return;''',
'''    if (!prepared.ready || !prepared.stable_envdiffuse_trace_valid ||
        decision.receiver_id < 33u || decision.receiver_id > 35u)
        return;
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    if(decision.receiver_id>=33u && decision.receiver_id<=35u){
        if(dsrrl::runtime::v438::state().active.load())
            dsrrl::runtime::v438::state().env_draws.fetch_add(1);
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::ENV_ISSUED,{
            material.model_owner_instance,
            dsrrl::runtime::v438::p(prepared.shader),
            material.route_index,material.material_slot,prepared.source_kind,
            prepared.stable_envdiffuse_row,prepared.stable_envdiffuse_bank,
            prepared.stable_envdiffuse_source_serial,
            prepared.stable_envdiffuse_native_producer_sequence,
            prepared.stable_envdiffuse_probe,
            dsrrl::runtime::v438::fbits(prepared.stable_envdiffuse_c86[0]),
            dsrrl::runtime::v438::fbits(prepared.stable_envdiffuse_c86[1]),
            dsrrl::runtime::v438::fbits(prepared.stable_envdiffuse_c86[2]),
            prepared.stable_envdiffuse_independent?1u:0u,
            material.owner_tuple_exact?1u:0u,decision.receiver_id});
        std::uint64_t flver[4]{},mtd[4]{};
        std::memcpy(flver,material.flver_sha256.data(),32);
        std::memcpy(mtd,material.raw_mtd_sha256.data(),32);
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::ENV_SHA,{
            material.model_owner_instance,
            prepared.stable_envdiffuse_native_producer_sequence,
            flver[0],flver[1],flver[2],flver[3],
            mtd[0],mtd[1],mtd[2],mtd[3],
            material.route_index,material.material_slot});
#if defined(DSRRL_V411_OWNER_SEQUENCE_DIAG)
        if(prepared.source_kind==1u &&
           prepared.stable_envdiffuse_native_producer_sequence!=0u){
            const auto live_v438=dsrrl::runtime::v413::inspect_owner(
                 material.model_owner_instance);
            const auto joined_v438=dsrrl::runtime::v411::observations().lookup(
                 material.model_owner_instance,
                 prepared.stable_envdiffuse_native_producer_sequence,
                 prepared.stable_envdiffuse_source_serial,
                 live_v438.wrapper,live_v438.manager);
            const auto &w_v438=joined_v438.prior;
            dsrrl::runtime::v438::emit(dsrrl::runtime::v438::ENV_JOURNAL,{
               material.model_owner_instance,
               prepared.stable_envdiffuse_native_producer_sequence,
               prepared.stable_envdiffuse_source_serial,
               static_cast<unsigned>(joined_v438.state),
               static_cast<unsigned>(joined_v438.candidates),
               w_v438.model_this,w_v438.fg_parent,w_v438.fg_registry_id,
               w_v438.model_verified?1u:0u,w_v438.fg_reciprocal?1u:0u,
               w_v438.registry_allocated?1u:0u,
               joined_v438.superseded_by_newer?1u:0u,
               live_v438.readable?1u:0u,live_v438.manager_agrees?1u:0u,
               w_v438.row_a,material.material_slot});
        }
#endif
    }
#endif''')
