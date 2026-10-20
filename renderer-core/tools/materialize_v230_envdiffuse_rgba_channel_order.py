#!/usr/bin/env python3
"""Owner build 235: exact DSR RX33/34/35 EnvDiffuse PTDE RGBA channel repair.

Provenance: original DSR FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx (owner evidence).
Stable raw DXBC SHA:
RX33 35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24
RX34 d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837
RX35 7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701
Lerp:
RX33 7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85
RX34 857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db
RX35 9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8

Their original t11 and t13 texture operand[7] = 0x00107e46 (xyzw).
The existing PTDE consumer divides r12.yzw by r12.x: requires wxyz.
Keep the original DSR host exe and original shaders ON DISK unchanged.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
STABLE=ROOT/"renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp"
LERP=ROOT/"renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"
DRAW=ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

STABLE_OLD="""    // Keep the exact coordinate/resource/sampler operands but capture all four
    // filtered channels in r12 so alpha survives until the post-filter divide.
    ptde_sample[3] = 0x001000f2u;
    ptde_sample[4] = 12u;"""
STABLE_NEW="""    // V230 exact original DXBC: t11 source swizzle is xyzw.
    // The PTDE post-filter DIV consumes r12.yzw / r12.x (RGB / alpha).
    // Move alpha into X and color into YZW while preserving t11/s11/LOD/UVW.
    if (ptde_sample[7] != 0x00107e46u)
        return false;  // receiver shader drift => stock DSR fail-open
    ptde_sample[7] = 0x00107936u; // wxyz = alpha,R,G,B
    ptde_sample[3] = 0x001000f2u;
    ptde_sample[4] = 12u;"""
LERP_OLD="""    // Preserve exact coordinate/resource/sampler identity but keep alpha
    // through filtering. PTDE decodes filtered RGBA as RGB/A.
    sample_a[3] = 0x001000f2u;
    sample_a[4] = 12u;
    sample_b[3] = 0x001000f2u;
    sample_b[4] = 12u;"""
LERP_NEW="""    // V230 exact RX33/34/35 t11/t13 original DXBC source is xyzw.
    // PTDE post-filter decode reads r12.yzw / r12.x. Both A and B need
    // the SAME alpha-first swizzle before filter; never change probe keys.
    if (sample_a[7] != 0x00107e46u ||
        sample_b[7] != 0x00107e46u)
        return false;  // raw shader source drift => stock DSR fail-open
    sample_a[7] = 0x00107936u;
    sample_b[7] = 0x00107936u;
    sample_a[3] = 0x001000f2u;
    sample_a[4] = 12u;
    sample_b[3] = 0x001000f2u;
    sample_b[4] = 12u;"""

# Build identity marker only, existing logging remains one-time and bounded.
LOG_OLD='[DSRRL PMETAL FULL PTDE HEMENV] mode=stable_exact_envdiffuse_rgba_div'
LOG_NEW='[DSRRL V230 ENVDIFFUSE_RGBA_FIXED] mode=stable_exact_envdiffuse_rgba_div'
LOG_LERP_OLD='[DSRRL PMETAL R19 LERP] mode=exact_ptde_envdiffuse_ab_beta'
LOG_LERP_NEW='[DSRRL V230 ENVDIFFUSE_RGBA_FIXED] mode=exact_ptde_envdiffuse_ab_beta'

def replace_exact(text, a, b, label, count=1):
    if text.count(a)!=count or b in text:
        raise RuntimeError(f"V230 owner235 source drift at {label}: {text.count(a)} expected {count}")
    return text.replace(a,b)

def main():
    stable=replace_exact(STABLE.read_text(encoding="utf8"),STABLE_OLD,STABLE_NEW,"RX33-35 Stable",1)
    lerp=replace_exact(LERP.read_text(encoding="utf8"),LERP_OLD,LERP_NEW,"RX33-35 Lerp",1)
    draw=DRAW.read_text(encoding="utf8")
    draw=replace_exact(draw,LOG_OLD,LOG_NEW,"one-time Stable build marker")
    draw=replace_exact(draw,LOG_LERP_OLD,LOG_LERP_NEW,"one-time Lerp build marker")
    # Source atomic prevalidation above; no shader/CB/resource unrelated path touched.
    STABLE.write_text(stable,encoding="utf8")
    LERP.write_text(lerp,encoding="utf8")
    DRAW.write_text(draw,encoding="utf8")
    print("V230_ENV_DIFFUSE_RGBA_SOURCE_PATCH_PASS Stable sample t11=1, Lerp t11/t13=2; exact XYZW->WXYZ")

if __name__=="__main__":
    main()
