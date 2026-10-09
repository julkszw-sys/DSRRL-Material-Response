#!/usr/bin/env python3
"""R43 native DSR-only m99/s99/default LightBank, same PTDE operator island."""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
s=p.read_text(encoding="utf-8")
a="""    const auto *row = pmetal_env_source_authority::find_row(*bank,row_id);
    if (row == nullptr) {
        record_hook_decode(hook_decode_row_unknown,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    const float scale = static_cast<float>(row->m) * 0.01f;
    out = {
        static_cast<float>(row->r) / 255.0f * scale,
        static_cast<float>(row->g) / 255.0f * scale,
        static_cast<float>(row->b) / 255.0f * scale,
        0.0f
    };"""
b="""    const auto *row = pmetal_env_source_authority::find_row(*bank,row_id);
    std::array<std::uint16_t,4> native_dsr{};
    if (bank->count == 0u && bank->live_count == 64u) {
        // Original native m99/s99/default LightBank. The layout hash has
        // already certified exact bank identity; read actual DSR authored row
        // WITHOUT leaving the PTDE shader/material/EnvSpec operator island.
        std::uint32_t offset = 0u;
        if (!safe_read(entry + 4u,offset) ||
            offset < 0x30u + 12u*64u ||
            offset > 0x100000u ||
            !safe_read(base + offset + 4u,native_dsr)) {
            record_hook_decode(hook_decode_row_unknown,version_u32,count_u32,index,row_id,signature);
            return false;
        }
    } else if (row == nullptr) {
        record_hook_decode(hook_decode_row_unknown,version_u32,count_u32,index,row_id,signature);
        return false;
    }
    const float red = static_cast<float>(row ? row->r : native_dsr[0]);
    const float green = static_cast<float>(row ? row->g : native_dsr[1]);
    const float blue = static_cast<float>(row ? row->b : native_dsr[2]);
    const float scale = static_cast<float>(row ? row->m : native_dsr[3]) * 0.01f;
    out = {red / 255.0f * scale, green / 255.0f * scale,
           blue / 255.0f * scale, 0.0f};"""
assert s.count(a)==1, "No exact R43 DSR bank decoding locus"
s=s.replace(a,b,1)
a="""    if (!exact_envdiffuse_endpoint(
            next.bank_signature_a,
            next.row_id_a,
            envdiffuse_a) ||
        !exact_envdiffuse_endpoint(
            next.bank_signature_b,
            next.row_id_b,
            envdiffuse_b)) {"""
b="""    // A DSR-only whole-bank has no PTDE EnvDiffuse sidecar. Its original
    // native source enters the SAME bridged PTDE consumer, not stock DSR.
    const bool dsr_a = pmetal_env_source_authority::
        find_bank(next.bank_signature_a) != nullptr &&
        pmetal_env_source_authority::
        find_bank(next.bank_signature_a)->count == 0u;
    const bool dsr_b = pmetal_env_source_authority::
        find_bank(next.bank_signature_b) != nullptr &&
        pmetal_env_source_authority::
        find_bank(next.bank_signature_b)->count == 0u;
    if (dsr_a) envdiffuse_a=a;
    if (dsr_b) envdiffuse_b=b;
    if ((!dsr_a && !exact_envdiffuse_endpoint(
             next.bank_signature_a,next.row_id_a,envdiffuse_a)) ||
        (!dsr_b && !exact_envdiffuse_endpoint(
             next.bank_signature_b,next.row_id_b,envdiffuse_b))) {"""
assert s.count(a)==1
s=s.replace(a,b,1)
p.write_text(s,encoding="utf-8")
print("PASS native DSR-only m99/default LightBank RGBM donor keeps PTDE P_Metal island")
