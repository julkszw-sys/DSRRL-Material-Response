#!/usr/bin/env python3
"""V317 build-only overlay on EXACT V272: replace *only* legacy R19 P_Metal
Lerp shader registration with V286 exact-owner DXBC sidecar. Do not touch
equipment SpecRGB resource lookup, material-response banks, old V270 dormant
islands, source/LightBank hooks, PointLight, stock Fog/LS or game EXE.
MUST run after V272's own source overlays and before native CMake configure.
"""
import argparse
from pathlib import Path

ROOT=Path("renderer-core")
P=ROOT/"src/runtime/pmetal_envspec_draw_runtime.cpp"
C=ROOT/"integrated/CMakeLists.txt"
I=ROOT/"include/dsrrl/runtime"
T=ROOT/"tools/v317"
TAG="[DSRRL V317 OWNER V286 LERP]"
BEGIN="bool pmetal_envspec_draw_runtime::\nregister_lerp_replacement("
END="bool pmetal_envspec_draw_runtime::prepare("
SHA_H="hemenvlerp_v301_owner_approval.hpp"
LOAD_H="hemenvlerp_v301_owner_sidecar.hpp"
def need(cond,msg):
    if not cond: raise RuntimeError("V317_FAIL_OPEN: "+msg)
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    args=ap.parse_args()
    s=P.read_text(encoding="utf-8").replace("\r\n","\n")
    cm=C.read_text(encoding="utf-8").replace("\r\n","\n")
    ready=(TAG in s and "DSRRL_V317_OWNER_V286_LERP" in cm and
           (I/SHA_H).exists() and (I/LOAD_H).exists())
    if ready:
        need(s.count(TAG)==1,"duplicate/ambiguous V317 owner Lerp patch")
        print("V317_V272_OWNER_V286_LERP_OVERLAY_ORACLE_PASS")
        return
    need(TAG not in s,"partial V317 patch")
    need("DSRRL_V317_OWNER_V286_LERP" not in cm,"partial V317 CMake")
    need(not (I/SHA_H).exists() and not (I/LOAD_H).exists(),
         "unexpected existing owner allowlist/loader; refuse overwrite")
    need(s.count(BEGIN)==1 and s.count(END)>=1,
         "V272 R19 exact create-time registration site changed")
    begin=s.index(BEGIN)
    end=s.index(END,begin)
    fn=s[begin:end]
    source=r'''    // V317: owner-authorized exact V286 sidecar replaces obsolete V272
    // P_Metal HemEnvLerp pixel shader. No old Lerp shader fallback.
    const void *admitted_dxbc = dxbc;
    std::size_t admitted_size = dxbc_size;
#if defined(DSRRL_V317_OWNER_V286_LERP)
    std::vector<std::uint8_t> owner_v286_dxbc;
    if (!hemenvlerp_v301::load(
            outcome.semantic_receiver_id,
            dxbc,dxbc_size,owner_v286_dxbc)) {
        ++lerp_replacement_register_fail_;
        return false; // stock DSR, never the obsolete V272 candidate
    }
    admitted_dxbc=owner_v286_dxbc.data();
    admitted_size=owner_v286_dxbc.size();
    reshade::log::message(reshade::log::level::info,
        "[DSRRL V317 OWNER V286 LERP] exact RX/source/sidecar SHA admitted");
#endif

'''
    anchor="    const auto payload_sha256 =\n        hashing::sha256(\n            static_cast<const std::uint8_t *>(\n                dxbc),\n            dxbc_size);"
    need(fn.count(anchor)==1,"R19 candidate SHA location missing")
    fn=fn.replace(anchor,source+anchor.replace("dxbc),","admitted_dxbc),").replace("dxbc_size);","admitted_size);"),1)
    old="            size_it->second ==\n                dxbc_size;"
    need(fn.count(old)==1,"R19 existing payload ABI comparison missing")
    fn=fn.replace(old,old.replace("dxbc_size;","admitted_size;"),1)
    old="            device_->CreatePixelShader(\n                dxbc,\n                dxbc_size,\n                nullptr,\n                &shader)"
    need(fn.count(old)==1,"R19 legacy shader creation site missing")
    fn=fn.replace(old,old.replace("                dxbc,","                admitted_dxbc,").replace("                dxbc_size,","                admitted_size,"),1)
    old="        dxbc_size;\n\n    ++lerp_replacement_register_ok_;"
    need(fn.count(old)==1,"R19 stored byte-size missing")
    fn=fn.replace(old,old.replace("dxbc_size;","admitted_size;"),1)
    includes='#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"'
    need(s.count(includes)==1,"unique owner runtime header anchor missing")
    # Header is only created while the V272 builder runs (never committed
    # in renderer-core/include, preserving exact V272 equipment source).
    s=s.replace(includes,includes+'''
#if defined(DSRRL_V317_OWNER_V286_LERP)
#include "dsrrl/runtime/hemenvlerp_v301_owner_sidecar.hpp"
#endif''',1)
    begin=s.index(BEGIN);end=s.index(END,begin)
    s=s[:begin]+fn+s[end:]
    cm+="\n# V317 private owner Lerp, no generic equipment SpecRGB alteration.\n"
    cm+="target_compile_definitions(dsrrl_renderer_core_integrated PRIVATE DSRRL_V317_OWNER_V286_LERP)\n"
    if not args.apply:
        print("V317_V272_OVERLAY_PREIMAGE_PASS_NOT_APPLIED")
        return
    approval=(T/"owner_v286_approval.hpp.txt").read_text(encoding="utf-8")
    loader=(T/"owner_v286_loader.hpp.txt").read_text(encoding="utf-8")
    need("v286_RX" in loader and 'L"DSRRL"' in loader and "v297_RX" not in loader,
         "wrong loader sidecar identity")
    need("array<owner_rx,3>" in approval,"wrong owner allowlist")
    P.write_text(s,encoding="utf-8",newline="\n")
    C.write_text(cm,encoding="utf-8",newline="\n")
    (I/SHA_H).write_text(approval,encoding="utf-8",newline="\n")
    (I/LOAD_H).write_text(loader,encoding="utf-8",newline="\n")
    print("V317_V272_OWNER_V286_LERP_OVERLAY_APPLIED")
if __name__=="__main__":
    main()
