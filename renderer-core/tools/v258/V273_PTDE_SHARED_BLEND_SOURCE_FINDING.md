# V273 — PTDE HemEnvLerp source-operator cut (owner original)

**TARGET:** PTDE-visible operator, not separate-register equality as an end state.

**Original owner:** `PTDE/DATA.exe`, SHA-256 `88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d`. Original `ps_3_0` DifSpcBmp HemEnvLerp consumes two distinct registers, `c84.w` (EnvDiffuse) and `c85.w` (EnvSpec). Previously established by V272: independent setters `0x424C60` and `0x424D70` and independent constant uploads. This report DOES NOT repeat that result; it resolves their **joint upstream blend-scalar dependency** for a narrower original PTDE path.

## New owner-original x86 graph

```
selector descriptor+8                  ORIGINAL PTDE
    | 0xF95223 / 0xF95228             scalar t
    v
0xF95515 -> 0xF95529 -> 0xF95534
    | call 0xFF1850: four float4s
    +--> output[0]: decoded endpoint A, alpha=0
    +--> output[1]: decoded endpoint A, alpha=0
    +--> output[2]: decoded endpoint B, alpha=t
    +--> output[3]: decoded endpoint B, alpha=t
                |
                | 0x4BBF70 writes passed alpha into output+0x0C
                v
output[2].w -> 0xF9567D -> setter 0x424C60 -> c84.w
output[3].w -> 0xF956F4 -> setter 0x424D70 -> c85.w
```

`0xFF1850` issues two `0x4BBF70` calls after `FLDZ` (`0xFF1923`, `0xFF1935`) and two calls after loading the **SAME** `[ebp+0x10]` at `0xFF1958` / `0xFF197D`. The `0x4BBFE7` + `0x4BBFEC` sequence copies that exact floating-point argument into the vector's `.w`, while decoding RGB from the three short channels. Thus, on this *authenticated selector-mismatch interior blend branch*:

**`c84.w == c85.w == t`**, conditional on the `0xFF1850` path accepting **both valid type-4 source records** and resolving both selectors (checks `0xFF18A5..0xFF191D`). With invalid or unavailable records the helper branches to a zero-weight fallback; no claim that every invocation is an interior blend.

At the selector-stable `0xFF1600` path, the decoded vector `.w` is zero for both selected endpoint vectors. These paths do not imply a globally shared physical `c84`/`c85` storage location; their independent CPU setters and registers remain intact. RGB for both constants is multiplied by a different per-scene scalar `[ebp+0x14]`, but the alpha components are copied without that gain (`0xF9567D/83` and `0xF956F4/FE`).

## Falsification and reachability

The owner-original local executable was **not patched**. `audit_v273_ptde_common_blend_scalar.py` SHA-pins the owner file, verifies 33 critical original instruction sequences and exact call targets and, with `--falsify`, rejects mutations to each of 33 pinned sites. Re-run locally:

```
python renderer-core/tools/v258/audit_v273_ptde_common_blend_scalar.py --exe <path-to-owner-PTDE-DATA.exe> --out v273_result.json --falsify
```

Result is **STATIC RE / CONFIRMED scoped equality**, not measured PTDE runtime values or pixel validation. No original executable, original shader or PTDE asset is distributed.

## Effect on V272 and decisions

- V272 owner runtime proved R19 DSR **also** writes a single `source.beta` to `b12[3].w` and `b12[5].w` across eight positive-beta samples.
- The hypothesis “PTDE necessarily requires different EnvDiffuse and EnvSpec blend weights because it has two registers” is therefore **FALSIFIED for this original PTDE interior blend operator with two valid records**.
- The **remaining semantic question** is whether DSR `source.beta` represents the *same actual selector-relative t* as original PTDE `selector+8` on an exactly matched profile transition, not whether the two internal DSR b12 weights should be forcibly split.
- Continue checking RGB endpoints, cube probes, material/normal homology, PHN `c110.w` and `c135`, exact receiver/downstream and pixels. The existing R19/V272 shared-beta transport is not changed, and three separate V270 DXBC replacements remain dormant (`PS_bind=OFF`). No unsupported PTDE beta override, blanket gamma/exposure change, or new test addon is justified by this proof.

**Status:** Producer equal-weight relationship CONFIRMED *on this branch*. DSR/PTDE scalar semantic equivalence OPEN. Bridge activation unchanged, PTDE pixel OPEN. Stable independent systems (Fog/LightScattering/Glow, PBL outside exact material consumer, PointLight) remain stock DSR.
