#!/usr/bin/env python3
"""Repair PTDE EnvDiffuse post-filter RGB/A channel layout at exact DSR shader sites.

Source facts:
- Verified original DSR DXBC t11/t13 source token 0x00107e46 is xyzw.
- The reused PTDE divide is r12.yzw / r12.x and thus requires wxyz.
- On PTDE RGBA8 sidecar, leaving xyzw calculates G/R,B/R,A/R, not R/A,G/A,B/A.

Change only SOURCE operand swizzle, preserving t11/t13 resource/sampler/coordinates,
the shader's destination, all EnvSpec t12/t14, SpecRGB, material response and fog.
Fail closed if source offsets or original swizzle drift. Diagnostic build only:
the production v2.3.0 DSR-stock-EnvDiffuse guard stays untouched.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
STABLE=ROOT/"renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp"
LERP=ROOT/"renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"

STABLE_OLD="""    // Keep the exact coordinate/resource/sampler operands but capture all four
    // filtered channels in r12 so alpha survives until the post-filter divide.
    ptde_sample[3] = 0x001000f2u;
    ptde_sample[4] = 12u;"""
STABLE_NEW="""    // Verified stock DXBC t11 source is texture.xyzw, not texture.wxyz.
    // The PTDE decode below divides r12.yzw by r12.x: alpha must be X.
    // Correct ONLY the copied SAMPLE source swizzle, with fail-open exact gate.
    if (ptde_sample[7] != 0x00107e46u)
        return false;
    ptde_sample[7] = 0x00107936u; // wxyz = alpha,R,G,B
    ptde_sample[3] = 0x001000f2u;
    ptde_sample[4] = 12u;"""
LERP_OLD="""    // Preserve exact coordinate/resource/sampler identity but keep alpha
    // through filtering. PTDE decodes filtered RGBA as RGB/A.
    sample_a[3] = 0x001000f2u;
    sample_a[4] = 12u;
    sample_b[3] = 0x001000f2u;
    sample_b[4] = 12u;"""
LERP_NEW="""    // Exact original DSR t11/t13 SAMPLE texture source swizzle is xyzw.
    // The PTDE RGB/A decode reads r12.yzw / r12.x, requiring wxyz.
    // A and B must be fixed identically BEFORE the post-filter divide.
    if (sample_a[7] != 0x00107e46u ||
        sample_b[7] != 0x00107e46u)
        return false;
    sample_a[7] = 0x00107936u; // alpha,R,G,B
    sample_b[7] = 0x00107936u;
    sample_a[3] = 0x001000f2u;
    sample_a[4] = 12u;
    sample_b[3] = 0x001000f2u;
    sample_b[4] = 12u;"""

def apply_once(path,original,replacement):
    data=path.read_text(encoding="utf-8")
    if original not in data or data.count(original)!=1 or replacement in data:
        raise RuntimeError("Exact EnvDiffuse DXBC source drift: "+str(path))
    return data.replace(original,replacement,1)

def main():
    # Verify all patterns before mutating either shader source.
    stable=apply_once(STABLE,STABLE_OLD,STABLE_NEW)
    lerp=apply_once(LERP,LERP_OLD,LERP_NEW)
    STABLE.write_text(stable,encoding="utf-8")
    LERP.write_text(lerp,encoding="utf-8")
    print("V230 PTDE EnvDiffuse SOURCE channel-order correction: stable1 + lerp A/B2, gated original XYZW => target WXYZ")

if __name__=="__main__":
    main()
