#!/usr/bin/env python3
"""Embed native-reflection-validated V258 DXBC into the one-addon's build.

Both bytecodes are dormant. No live DSR shader binding is permitted until
receiver ISGN/CB/source and physical per-slot sidecar authority are proven.
"""
from pathlib import Path
import argparse, hashlib
def embed(name,data):
    rows=[]
    for i in range(0,len(data),16):
        rows.append("    "+",".join(f"0x{x:02x}" for x in data[i:i+16])+",")
    return (f"inline constexpr std::array<std::uint8_t,{len(data)}u> "
        f"{name} = {{{{\n" + "\n".join(rows) + "\n}};\n")
def run():
    p=argparse.ArgumentParser()
    p.add_argument("--env",required=True,type=Path)
    p.add_argument("--terminal",required=True,type=Path)
    for receiver in (33,34,35):
        p.add_argument(f"--rx{receiver}",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    a=p.parse_args()
    env=a.env.read_bytes();term=a.terminal.read_bytes()
    native={rx:getattr(a,f"rx{rx}").read_bytes() for rx in (33,34,35)}
    for name,data in [("env",env),("terminal",term)]+[
        (f"native_rx{rx}",native[rx]) for rx in (33,34,35)
    ]:
        if len(data)<128 or data[:4]!=b"DXBC":
            raise RuntimeError(name+" not valid DXBC")
    s=("#pragma once\n"
       "// V258 genuine PS5 DXBC, DORMANT fail-open candidate.\n"
       "// Do not interpret presence as receiver ABI or pixel equivalence.\n"
       "#include <array>\n#include <cstdint>\n"
       "namespace dsrrl::runtime::v258_dxbc {\n"
       f"inline constexpr char k_source_digest_env[] = \"{hashlib.sha256(env).hexdigest()}\";\n"
       f"inline constexpr char k_source_digest_terminal[] = \"{hashlib.sha256(term).hexdigest()}\";\n"
       +embed("k_env_local_subcut_ps",env)
       +embed("k_terminal_subcut_ps",term)
       + "".join(
            f'inline constexpr char k_sha256_native_rx{rx}[] = "{hashlib.sha256(native[rx]).hexdigest()}";\\n'
            +embed(f"k_native_rx{rx}_normal_ptde_dormant_ps",native[rx])
            for rx in (33,34,35))
       +"} // namespace dsrrl::runtime::v258_dxbc\n")
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(s,encoding="utf8",newline="\n")
    assert s.count("k_env_local_subcut_ps")==1 and s.count("k_terminal_subcut_ps")==1
    assert all(s.count(f"k_native_rx{rx}_normal_ptde_dormant_ps")==1 for rx in (33,34,35))
    print("V270_FIVE_REAL_DXBC_ONE_ADDON_EMBEDDED_HEADER_PASS",
          len(env),len(term),*(len(native[x]) for x in (33,34,35)))
if __name__=="__main__":
    run()
