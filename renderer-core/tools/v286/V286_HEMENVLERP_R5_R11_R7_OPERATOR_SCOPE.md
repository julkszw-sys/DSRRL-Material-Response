# V286 HemEnvLerp R5+R11+R7 source construction

Priority: PTDE-visible HemEnvLerp continuity. This is source-only, not a runtime or pixel release.

**CONFIRMED stock input:** RX33 Csd and RX34 Sdw original Stable/Lerp share identical 176-DWORD nine-SAMPLE_C t7 visibility kernels; Lerp +14 DWORD layout shift, same s7 comparison sampler, same source register (r6/r5), and dcl_temps=13. RX35 plain has no corresponding R7 shadow operator. Original stock t8 SAO and material-workflow gates are independent from this shadow source.

**Exact owner-local V286:** read six full-SHA pinned **final** shaders: Stable R17 RX33/34/35 followed by HemEnvLerp R19 RX33/34/35. Reconstruct SHA-pinned R5 normal operator and length-preserving R11 whole-surface SAO scalar fix, then for RX33/34 extract exactly 559 DWORD from authorized Stable R17 PTDE R7 donor, require 16 regular t7/s7 samples and terminal SAT, and replace authenticated 176-DWORD Lerp R7 host nine-SAMPLE_C shadow operator. Extend dcl_temps from 13 to 18, comparison s7 to regular s7, and update exactly one sampler s7 RDEF binding flags 3 to 1 (keep t7 resource and independent operators). Rebuild SHEX and container checksum. RX35 forwards verified R5+R11 only.

**Key guard:** source original stock R7 fragment SHA RX33 682a1d4cb910d5720f96427386f034970f3bc9435d44b6b45b558e5eb668b254, RX34 0cbd2ab2cc5e8037e43c59e037d63dca81c98983d3db94ae5fb660eb98ddcf54. Entire final donor hashes are authenticated by V281; mismatched source/ABI/chunk/cut fails open without any output. No copyrighted shader bytes shipped.

**R3 already present:** Lerp R19 R15–R17 material workflow neutralizes the late SpecTex alpha common-surface gate equivalent to Stable R3; do not duplicate it.

**CI:** native Win64 /W4 /WX compilation and invalid-source reject test added to the actively running V258 Lerp workflow at source commit d3abf4519d1431bd6ee7141e539846e0377c3a0d. CI result must be checked; compile is not pixel proof.

**OFF/OPEN:** No shader bound in game, no material-wide enable, no one-addon runtime activation, no WARP/driver CreatePixelShader proof and no owner-final V286 candidate SHA or pixel result yet. Active R43 RC1 preserved. Exact FLVER/MTD/slot, physical SpecRGB sidecar, t1/t10, shadow SRV and CB source require verified draw-specific gate, state restoration and fail-open.

**Next objective:** After owner-final source result, verify PTDE pre-Fog output at beta=0 and beta=1 against same-material Stable endpoints; check interior beta transitions, independent PBL/PointLight, Fog/LightScattering and Glow. Only PTDE-visible pixel comparisons can qualify the target.

Implementation: renderer-core/tools/v286/reconstruct_lerp_r5_r11_r7_from_final.cpp in PR #364 commit 413c930227441f82d3f5f8f28280a86316a14e23.