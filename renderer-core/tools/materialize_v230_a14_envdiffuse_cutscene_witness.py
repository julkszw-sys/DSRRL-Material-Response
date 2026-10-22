#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'src/runtime/pmetal_envspec_draw_runtime.cpp'
s=p.read_text('utf8')
anchor='    effect_latch(effect_source_ready_);'
if s.count(anchor)!=1: raise RuntimeError('A14 source cut drift')
insert=r"""
#if defined(DSRRL_V230_A14_ENVDIFFUSE_WITNESS)
    constexpr std::uint64_t m14=0xc9b879a8c5e624d0ull;
    constexpr std::uint64_t s14=0xe447e0e74af8d1d3ull;
    static std::atomic<std::uint64_t> seen[4]{};
    for(unsigned e=0u;e<2u;++e) {
        const auto sig=e ? source.bank_signature_b : source.bank_signature_a;
        const auto id=e ? source.row_id_b : source.row_id_a;
        const int bank=sig==m14 ? 0 : sig==s14 ? 1 : -1;
        if(bank<0 || id>=64u) continue;
        const unsigned bucket=static_cast<unsigned>(bank)*2u+e;
        const std::uint64_t bit=1ull<<id;
        if(seen[bucket].fetch_or(bit,std::memory_order_relaxed)&bit)continue;
        char line[700]{};
        std::snprintf(line,sizeof(line),
          "[DSRRL A14 ENVDIFFUSE SOURCE] bank=%s endpoint=%c row=%u"
          " family=%u rx=%u route=%u flver=%016llx slot=%u"
          " beta=%.9g valid=%u"
          " diffA=%.9g,%.9g,%.9g diffB=%.9g,%.9g,%.9g"
          " specA=%.9g,%.9g,%.9g specB=%.9g,%.9g,%.9g"
          " SOURCE_ONLY pixel=OPEN",
          bank==0?"m14":"s14",e?'B':'A',static_cast<unsigned>(id),
          static_cast<unsigned>(family),static_cast<unsigned>(decision.receiver_id),
          static_cast<unsigned>(decision.route_index),
          static_cast<unsigned long long>(material.flver_identity_hash),
          static_cast<unsigned>(material.material_slot),
          static_cast<double>(source.beta),source.envdiffuse_linear_valid?1u:0u,
          static_cast<double>(source.envdiffuse_a[0]),
          static_cast<double>(source.envdiffuse_a[1]),
          static_cast<double>(source.envdiffuse_a[2]),
          static_cast<double>(source.envdiffuse_b[0]),
          static_cast<double>(source.envdiffuse_b[1]),
          static_cast<double>(source.envdiffuse_b[2]),
          static_cast<double>(source.a[0]),static_cast<double>(source.a[1]),
          static_cast<double>(source.a[2]),static_cast<double>(source.b[0]),
          static_cast<double>(source.b[1]),static_cast<double>(source.b[2]));
        reshade::log::message(reshade::log::level::warning,line);
    }
#endif
"""
p.write_text(s.replace(anchor,insert+anchor),'utf8')
print('A14 SOURCE CUTSCENE TRACE PASS')
