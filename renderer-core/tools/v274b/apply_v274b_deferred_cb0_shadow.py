#!/usr/bin/env python3
"""V274B diagnostic-only producer CPU shadow for deferred DX11 b0 CB.
Call after V274 source overlay and before V272 integrated CMake compile.
All modified engine files are restored after building. Fail-closed preimages.
"""
from pathlib import Path
import argparse
ROOT=Path("renderer-core")
INCLUDE=ROOT/"include/dsrrl/runtime/v274b_cb_shadow.hpp"
SRC=ROOT/"tools/v274b/v274b_cb_shadow.hpp"
DRAW=ROOT/"src/runtime/pmetal_envspec_draw_runtime.cpp"
INTEGRATED=ROOT/"integrated/integrated_addon.cpp"
HEADER='#include "dsrrl/runtime/pixel_srv_shadow.hpp"\n'
ADD_HEADER=HEADER+'#include "dsrrl/runtime/v274b_cb_shadow.hpp"\n'
DRAW_CALL='''    if (family == pmetal_envspec_receiver_family::hemenvlerp)
        v274_capture_actual_lerp_cb(
            context, material, decision, prepared,
            source.beta, env_semantics.envspc_slot);
'''
DRAW_ADDED=DRAW_CALL+'''    if (family == pmetal_envspec_receiver_family::hemenvlerp)
        v274b::log_prepared(
            context, decision.receiver_id, material.route_index,
            material.semantic_name_hash, material.flver_identity_hash,
            material.material_slot, source.beta, decision.c101);
'''
REGISTER='''void register_events()
{
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
'''
REGISTER_ADDED='''void register_events()
{
    reshade::register_event<reshade::addon_event::map_buffer_region>(dsrrl::runtime::v274b::map_buffer);
    reshade::register_event<reshade::addon_event::unmap_buffer_region>(dsrrl::runtime::v274b::unmap_buffer);
    reshade::register_event<reshade::addon_event::update_buffer_region>(dsrrl::runtime::v274b::update_immediate);
    reshade::register_event<reshade::addon_event::update_buffer_region_command>(dsrrl::runtime::v274b::update_deferred);
    reshade::register_event<reshade::addon_event::destroy_resource>(dsrrl::runtime::v274b::destroy_resource);
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
'''
UNREGISTER='''void unregister_events()
{
    if (k_drawtime_islands_runtime_enabled &&
'''
UNREGISTER_ADDED='''void unregister_events()
{
    reshade::unregister_event<reshade::addon_event::destroy_resource>(dsrrl::runtime::v274b::destroy_resource);
    reshade::unregister_event<reshade::addon_event::update_buffer_region_command>(dsrrl::runtime::v274b::update_deferred);
    reshade::unregister_event<reshade::addon_event::update_buffer_region>(dsrrl::runtime::v274b::update_immediate);
    reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(dsrrl::runtime::v274b::unmap_buffer);
    reshade::unregister_event<reshade::addon_event::map_buffer_region>(dsrrl::runtime::v274b::map_buffer);
    if (k_drawtime_islands_runtime_enabled &&
'''
MARK='[DSRRL V274B DEFERRED CB0 CPU SHADOW]'

def patch(s:str,mode:str)->str:
    s=s.replace('\r\n','\n')
    if mode=='draw':
        if s.count(DRAW_ADDED)==1 and s.count(ADD_HEADER)==1:return s
        assert s.count(DRAW_CALL)==1,(mode,'V274 draw overlay missing')
        assert s.count(HEADER)==1,(mode,'header preimage mismatch')
        s=s.replace(HEADER,ADD_HEADER,1).replace(DRAW_CALL,DRAW_ADDED,1)
    elif mode=='integrated':
        if s.count(REGISTER_ADDED)==1 and s.count(UNREGISTER_ADDED)==1:return s
        assert s.count(HEADER)==1,(mode,'header preimage missing')
        assert s.count(REGISTER)==1,(mode,'register preimage missing')
        assert s.count(UNREGISTER)==1,(mode,'unregister preimage missing')
        s=s.replace(HEADER,ADD_HEADER,1)
        s=s.replace(REGISTER,REGISTER_ADDED,1)
        s=s.replace(UNREGISTER,UNREGISTER_ADDED,1)
    else: raise ValueError(mode)
    assert s.count(ADD_HEADER)==1
    return s

def main():
    pa=argparse.ArgumentParser()
    pa.add_argument('--apply',action='store_true')
    args=pa.parse_args()
    assert SRC.is_file(), 'V274B actual source missing'
    for p,mode in [(DRAW,'draw'),(INTEGRATED,'integrated')]:
        old=p.read_text(encoding='utf-8')
        new=patch(old,mode)
        if args.apply:
            if old!=new:p.write_text(new,encoding='utf-8',newline='\n')
        else:
            assert new==old,'V274B not applied to '+str(p)
    if args.apply:
        assert not INCLUDE.exists(),'V274B untracked header collision'
        INCLUDE.write_bytes(SRC.read_bytes())
    else:
        assert INCLUDE.read_bytes()==SRC.read_bytes(),'V274B shadow header drift'
    print('V274B_D3D11_CPU_WRITER_CB0_SHADOW_'+('APPLIED' if args.apply else 'VERIFIED'))
    print('V274B_STABLE_SHADER_AND_LERP_OPERATOR_DXBC_UNCHANGED')
    print('V274B_READ_ONLY_MAP_UNMAP_UPDATE_DEFERRED_EVENTS')
    print('V274B_PREPARED_CPU_SHADOW_NOT_PIXEL_PROOF')
if __name__=='__main__':main()
