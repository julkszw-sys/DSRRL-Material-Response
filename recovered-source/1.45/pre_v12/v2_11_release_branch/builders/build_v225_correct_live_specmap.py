# DSRRL V2.25 reproducibility note
# Built from V2.22B. For shader IDs 894/913/932:
# 1) replace the two redundant fixed-angular bypass MULs (14 dwords total)
#    with a byte-exact copy of the shader's original t1/s1 SAMPLE redirected to r3
#    plus 3 NOP dwords;
# 2) patch injected cb12[0] material MUL to consume r3.yzw (fresh SpecMap RGB);
# 3) restore COLOR0 native xyz swizzle;
# 4) regenerate DXBC checksum.
# Exact per-shader word offsets and before/after SHA256 are in the audit JSON.
# Final production mod remains PARAM-only; this addon is diagnostic only.
