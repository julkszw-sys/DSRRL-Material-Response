#!/usr/bin/env python3
"""Transiently insert exact DXBC pair capture after V275 source overlay.

Apply before building V276, after V275's --apply. Fails closed on source drift.
Only diagnostics: two calls after *successful* exact SHA materializers.
"""
from pathlib import Path
import argparse

SOURCE=Path("renderer-core/integrated/integrated_addon.cpp")
INC='#include "dsrrl/runtime/v275_material_draw_trace.hpp"'
NEW=INC+'\n#include "../tools/v277/v277_exact_stable_lerp_dxbc_capture.hpp"'
STABLE='''            if (envspec.result ==
                envspec_result::applied) {
                const bool registered ='''
STABLE_NEW='''            if (envspec.result ==
                envspec_result::applied) {
                dsrrl::runtime::v277::capture(
                    source,pixel_shader->code_size,envspec_payload,
                    envspec.receiver_id,false);
                const bool registered ='''
LERP='''            if (envspec_lerp.result ==
                    envspec_lerp_result::applied) {
                if (g_pmetal_envspec.'''
LERP_NEW='''            if (envspec_lerp.result ==
                    envspec_lerp_result::applied) {
                dsrrl::runtime::v277::capture(
                    source,pixel_shader->code_size,envspec_lerp_payload,
                    envspec_lerp.semantic_receiver_id,true);
                if (g_pmetal_envspec.'''
# No file change ever reaches git: nested V274B/V275 builder restores source.
def patch(s:str)->str:
    for old,new in ((INC,NEW),(STABLE,STABLE_NEW),(LERP,LERP_NEW)):
        if s.count(old)!=1:raise AssertionError("V277 exact source preimage failed")
        s=s.replace(old,new,1)
    if s.count("dsrrl::runtime::v277::capture(")!=2:
        raise AssertionError("unexpected DXBC capture call count")
    return s

def main():
    p=argparse.ArgumentParser();p.add_argument("--apply",action="store_true")
    args=p.parse_args();s=SOURCE.read_text(encoding="utf8")
    if args.apply:
        SOURCE.write_text(patch(s),encoding="utf8",newline="\n")
    else:
        assert NEW in s and STABLE_NEW in s and LERP_NEW in s
        assert s.count("dsrrl::runtime::v277::capture(")==2
    print("V277_EXACT_STABLE_LERP_DXBC_CREATE_TIME_HOOK_PASS")
    print("V277_NO_GPU_PIXEL_OR_R5_CORRECTION_CLAIM")
if __name__=="__main__":main()
