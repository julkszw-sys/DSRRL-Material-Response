#!/usr/bin/env python3
"""V312 owner-local admission: prepare denied, authorize only after exact 15-file WARP."""
import argparse, hashlib, json, shutil, sys
from pathlib import Path
V297={33:"a591a3181fb9953f8fb59305f5901a66c4cfd0a16652e5871a8ebb1ac01313cc",34:"96fb2897db71426380c834703a69205f02e9306506ba7385b1ed5999a5b584dd",35:"20af2d760a39a7e1d5b8903a451eb8bf061ce1da5e0ac8dfa5c4ff367eed513b"}
R19={33:"1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab",34:"8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97",35:"b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70"}
def j(p):return json.loads(p.read_text(encoding="utf-8-sig"))
def h(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("mode",choices=("prepare","authorize","direct-game"))
    ap.add_argument("owner",type=Path)
    ap.add_argument("out",type=Path)
    ap.add_argument("--warp",type=Path)
    ap.add_argument("--owner-direct-game",action="store_true")
    a=ap.parse_args()
    if a.out.exists():raise ValueError("do not overwrite owner evidence")
    receipt=j(a.owner/"V300_OWNER_LOCAL_RECEIPT.json")
    if receipt.get("schema")!="V300_OWNER_LOCAL_SHADER_TRIPLET_VERIFIED" or receipt.get("ready_for_runtime_approval") is not False or receipt.get("pixel_equivalent") is not False:raise ValueError("unsafe receipt")
    rec={x["path"].replace("\\","/"):x for x in receipt["files"]}
    manifest=j(a.owner/"v297_postls/V297_OWNER_POST_LS_MANIFEST.json")
    if manifest.get("schema")!="DSRRL_V297_LERP_PTDE_POSTLS_V1" or manifest.get("runtime_bind") is not False or [x.get("rx") for x in manifest.get("receivers",[])]!=[33,34,35]:raise ValueError("unsafe V297")
    if len(rec)!=12 or len(receipt["files"])!=12:raise ValueError("12 unique receipt files required")
    for rel,row in rec.items():
        p=a.owner/rel
        if h(p)!=row["sha256"] or p.stat().st_size!=row["size"]:raise ValueError("owner receipt SHA mismatch "+rel)
    for rx in (33,34,35):
        src=a.owner/f"lerp_r19/final_r19_RX{rx}.dxbc"
        side=a.owner/f"v301/HemEnvLerp/v297_RX{rx}.dxbc"
        stage=a.owner/f"v297_postls/v286_R5_R11_R7_LerpRX{rx}.dxbc"
        m=manifest["receivers"][rx-33]
        if h(src)!=R19[rx] or h(side)!=V297[rx] or side.read_bytes()!=stage.read_bytes() or m.get("sha256")!=V297[rx]:raise ValueError("unknown RX source")
    enabled=a.mode in ("authorize","direct-game")
    if a.mode=="direct-game" and not a.owner_direct_game:raise ValueError("explicit --owner-direct-game required")
    if enabled:
        if a.warp is None:raise ValueError("WARP report required")
        report=j(a.warp)
        if report.get("schema")!="DSRRL_V312_OWNER_WINDOWS_WARP_V1" or report.get("result")!="ALL_15_REAL_DXBC_WARP_PASS":raise ValueError("no real owner WARP PASS")
        rows=report.get("files")
        if not isinstance(rows,list) or len(rows)!=15:raise ValueError("15 owner shader tests mandatory")
        expected=dict(rec)
        for rx in (33,34,35):
            rel=f"v301/HemEnvLerp/v297_RX{rx}.dxbc"
            p=a.owner/rel
            expected[rel]={"sha256":V297[rx],"size":p.stat().st_size}
        seen=set()
        for row in rows:
            k=row.get("relative_path")
            if k not in expected or k in seen or row.get("sha256")!=expected[k]["sha256"] or row.get("warp_d3dreflect")!="PASS" or row.get("warp_create_pixel_shader")!="PASS":raise ValueError("WARP rejected/duplicate/source mismatch")
            if row.get("b12_declared_bytes")!=(64 if k.startswith("stable_r17/") else 96):raise ValueError("WARP ABI mismatch")
            seen.add(k)
        if len(seen)!=15:raise ValueError("incomplete WARP")
    dst=a.out/"DSR/DSRRL/EnvSpec/HemEnvLerp"
    dst.mkdir(parents=True)
    try:
        lines=["#pragma once","#include <array>","#include <cstddef>","#include <cstdint>","namespace dsrrl::runtime::hemenvlerp_v301 {","struct owner_rx { std::uint32_t receiver_id; std::size_t size; const char* sha256; };",f"inline constexpr std::array<owner_rx,{3 if enabled else 0}> approved{{{{"]
        if enabled:
            for rx in (33,34,35):
                size=(a.owner/f"v301/HemEnvLerp/v297_RX{rx}.dxbc").stat().st_size
                lines.append(f'  {{{rx}u,{size}u,"{V297[rx]}"}},')
        lines.extend(["}};","}"])
        inc=a.out/"include";inc.mkdir()
        (inc/"hemenvlerp_v301_owner_approval.hpp").write_text("\n".join(lines)+"\n")
        for rx in (33,34,35):
            shutil.copyfile(a.owner/f"v301/HemEnvLerp/v297_RX{rx}.dxbc",dst/f"v297_RX{rx}.dxbc")
        (a.out/"V312_ADMISSION.json").write_text(json.dumps(dict(phase="DIRECT_GAME_DIAGNOSTIC" if a.mode=="direct-game" else ("DIAGNOSTIC_ADMISSION" if enabled else "PREPARED_FAIL_OPEN"),windows_warp="SKIPPED_BY_OWNER" if a.mode=="direct-game" else ("PASS" if enabled else "OPEN"),admission_basis="OWNER_DIRECT_GAME" if a.mode=="direct-game" else ("WARP" if enabled else "NONE"),pixel="OPEN",addon_runtime="OPEN"),indent=2)+"\n")
        print("V313_"+("DIRECT_GAME_DIAGNOSTIC" if a.mode=="direct-game" else ("DIAGNOSTIC_ADMISSION" if enabled else "PREPARED_FAIL_OPEN")))
    except Exception:
        shutil.rmtree(a.out,ignore_errors=True);raise
if __name__=="__main__":
    try:main()
    except (OSError,ValueError,KeyError) as e:
        print("V312_FAIL_OPEN:",e,file=sys.stderr);sys.exit(2)
