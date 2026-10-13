# V263 — R19 material response source closure, double-patch falsification

This is a **correction to V262**, not another playable diagnostic addon.

## Actual code path

`pmetal_rgba_lerp_materializer.cpp` always invokes
`materialize_ptde_diffuse_response_v1(...,base,true)` for exact RX33/34/35
before the R19 EnvSpec/EnvDiffuse/SpecRGB operator patches.

The 48-row source manifest `material_response_diffuse_v1.tsv` explicitly
has **both** `c100_site0` (IF) and `c100_site1` (ordinary ELSE) plus
`diffuse_pow_site` for each stock receiver. Its C++ consumer already
rewrites both `cb0[9]` sites to `b12[1]` (PTDE c100) and the first
ordinary diffuse gamma exponent **2.2 -> 1.0**.

| RX | IF cb9 site | ELSE cb9 site | first exponent site |
|---|---:|---:|---:|
| 33 | 1209 | 1299 | 1313 |
| 34 | 1118 | 1208 | 1222 |
| 35 | 778 | 868 | 882 |

The V262 patcher insists on the *original stock* preimages `cb0[9]` and
`2.2`. They cannot be found in a correctly materialized R19 shader.
**Do not apply V262 on top of R19**. The synthetic and original-retail
stock-DXBC V262 opcode tests are still valid as *stock oracle tests*,
but they did **not** test the actual R19-replaced shader.

At exponent=1, the DSR first material path computes `abs(Z)`;
PTDE direct local surface expects `Z`. Whether any particular
sidecar-backed equipment input Z is negative is **not proven**.
We cannot attribute nonmetal overgloss to that residual without an
input-domain witness. PTDE diffuse material also depends on verifying
`t0`, vertex/COLOR, `c156`, and `c139` inputs.

## Remaining operator

The second ordinary ELSE material operator retains its independent
`mul_sat(cb0[10]) -> LOG -> MUL(2.2) -> EXP` in all RX33/34/35.
On original DSR DXBC it writes r11/r10/r9, respectively. A conditional
`gFC_DebugDraw` (RDEF cb0[102]) branch can overwrite those registers
with 0 or 1; the next consumer opcode 16 is at SHEX word
1478/1387/1047 (before EnvSpec R19 replacement start 1610/1519/1179).
This **does not** prove the second lane remains visually effective
after the later R19 materializer patch. Trace postpatch dataflow first.

The exact historical 36-element FLVER SHA + material slot + MTD +
logical gSpec manifest in PR300 provides **24 nonmetal** equipment
candidate tuples and 12 P_Metal reference tuples. These mappings do
not automatically prove physical PTDE DDS content SHA. Stock DSR Fog
and LightScattering and `latest_hook_source` remain untouched.

## Verification and constraints

`audit_r19_material_diffuse_v262_inapplicability.py --root .` reads
the current checked-in source and 48 pinned material receiver rows.
Supply `--donors DIR` only to audit your **own** RX33.dxbc,
RX34.dxbc, RX35.dxbc (exact original retail SHA); no donor bytes are
committed. The script rejects changed preimages. GitHub CI builds
on Windows and Linux and protects all production renderer paths.

**Construction/source PASS != active GPU bridge != PTDE pixel PASS.**

Next: trace the distinct second material and postmerge consumers
on the *actual R19-materialized PS* and bind only via exact per-draw
equipment tuple, real sidecar digest, PS receiver and CB authority.
No shared-receiver blanket gamma patch.
