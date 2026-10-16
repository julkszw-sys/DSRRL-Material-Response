#!/usr/bin/env python3
"""Exact source-grounded DSR direct 749 name gate, no assumed low alias."""
from pathlib import Path
import importlib.util, tempfile, subprocess, sys
root=Path(__file__).resolve().parents[1]
path=root/"tools/generate_sidecar_spec_whitelist_20261010.py"
spec=importlib.util.spec_from_file_location("oct10",path)
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
names=mod.load_names(root/"data/provenance/sidecar_specular_20261010_names.b85")
assert len(names)==1295 and len(set(names))==1295
low=[n for n in names if n.endswith("_l_s")]
unknown={"wp_a_0911_s","wp_a_1004_s"}
direct=[n for n in names if n not in unknown and not n.endswith("_l_s")]
assert len(low)==544 and len(direct)==749 and unknown.issubset(names)
header=mod.render_header(names)
assert "std::array<std::uint64_t,1295u> k_spec_hashes" in header
assert "std::array<std::uint64_t,749u> k_dsr_direct_spec_hashes" in header
cpp=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text("utf-8")
assert "DSRRL_V305_DSR_DIRECT_SPEC_ROUTE" in cpp
assert cpp.index("!sidecar_20261010::dsr_direct_spec_allowed(hash)")<cpp.index("generated::spec_equipment_name_hash_allowed_v12(hash)")
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/"generated_spec_whitelist_20261010.hpp"
    p.write_text(header,"utf-8")
    t=Path(d)/"test.cpp"
    t.write_text("""
#include "generated_spec_whitelist_20261010.hpp"
#include <cassert>
#include <cstdint>
constexpr std::uint64_t fnv(const char*s){std::uint64_t h=14695981039346656037ull;for(;*s;++s){h^=std::uint8_t(*s);h*=1099511628211ull;}return h;}
int main(){namespace c=dsrrl::runtime::sidecar_20261010;
assert(c::k_spec_hashes.size()==1295);
assert(c::k_dsr_direct_spec_hashes.size()==749);
assert(c::spec_allowed(fnv("am_a_1000_l_s")));
assert(!c::dsr_direct_spec_allowed(fnv("am_a_1000_l_s")));
assert(!c::dsr_direct_spec_allowed(fnv("wp_a_0911_s")));
assert(!c::dsr_direct_spec_allowed(fnv("wp_a_1004_s")));
assert(c::dsr_direct_spec_allowed(fnv("wp_a_0100_s")));
}
""","utf-8")
    if sys.platform!="win32":
        subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-Werror","-I",d,str(t),"-o",str(Path(d)/"test")],check=True)
        subprocess.run([str(Path(d)/"test")],check=True)
print("V305_DSR_DIRECT_749_PTDE_LOW_544_UNRESOLVED_2_PASS; PIXEL_OPEN")
