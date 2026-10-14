# V276 — actual materialized R19 HemEnvLerp, second material gamma live-out

## Why this is not the V264/V265 stock-original-only audit

Original owner SHA-pinned DXBC from the DSR retail binder was fed to **the
actual production** `materialize_pmetal_rgba_lerp_receiver` C++ chain,
including `materialize_ptde_diffuse_response_v1`, EnvSpec/EnvDiffuse
reconstruction, R15/R16/R17 MaterialWorkflow, SpecRGB and terminal.

The complete source + generated R19 manifest headers were extracted from
the branch's authenticated Windows CI artifact and compiled locally with
the **identical R19 CMakeCache ON defines** to the V272 one-addon baseline.
No renderer/EXE bytes are modified; all produced game-shader DXBC stays
in the user's local private directory, never in GitHub or a public artifact.

The standalone native Windows exporter has separately **compiled and
self-tested successfully** in GitHub Actions run
[38493609880](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/38493609880)
(`V276_REAL_R19_MATERIALIZER_LINKED_SELFTEST_PASS`).
The user-original full materialization was executed locally, not merely
compiled in public CI. D3DReflect/WARP acceptance on the actual resulting
owner binaries is **not claimed** from the offline Linux run.

## Three original to final exact SHA identities

| RX | Final R19 DXBC SHA-256 | bytes | final stock Fog start | final stock Glow RGB |
|--|--|--:|--:|--:|
|33| `1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab` |22512|2579|2922|
|34| `8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97` |22212|2498|2841|
|35| `b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70` |20584|2151|2494|

Exactly **+27 SHEX DWORD** shift across all three relative to original
donors. The original **212-DWORD stock DSR Fog/LightScattering** block and
both output1 Glow writes remain unique and identical, proving boundaries
needed for any subsequent patch. This alone is not PTDE-visible pixel pass.

## Actual generated final shader second-material operator

The original DSR ordinary ELSE second material response
`M=pow(saturate(SpecTex.rgb * cb0[10].rgb), 2.2)`
**still physically exists unchanged**, independently of R19's first
diffuse gamma being already remapped to exponent 1 and PTDE c100.
`cb0[85]` MaterialWorkflow controls which branch executes; actual draw
CB`[85]` values and debug override `cb0[102]` are NOT yet measured.

| RX | second EXP | early consumer | MAD #1 | MAD #2 | r5.xyz preFog | conditional r1.xyz |
|--|--:|--:|--:|--:|--:|--:|
|33|1372|1505|2139|2370|2525|2546|
|34|1281|1414|2058|2289|2444|2465|
|35|941|1074|1711|1942|2097|2118|

All listed opcodes are **byte-identical** between retail original and true
final R19 and match uniquely at exactly +27. Receiver-specific output
gamma results are r11/r10/r9.xyz.

Per-component conservative def-use was run on EACH ACTUAL FINAL DXBC,
not the original stock blobs, reusing the V265 original SHA-anchored
register-component checker adapted to the final hashes and offsets.
The certified **eight-instruction** dependency chains after the second
MAD are:

- RX33: `2370,2456,2492,2504,2511,2518,2525,2546`
- RX34: `2289,2375,2411,2423,2430,2437,2444,2465`
- RX35: `1942,2028,2064,2076,2083,2090,2097,2118`

The final `r5.xyz` receives a possible contribution from the original
second gamma → first/second MAD before the untouched stock Fog; a
conditional `r1.xyz` can consume it. This is **a proven static
component-level may-flow on final R19**, *not* runtime execution or a
PTDE delta. Each wrong SHA/input fails the oracle.

## What actually remains to FINISH

1. Identify, at exact equipment FLVER/MTD/slot and actual PTDE sidecar
   t10/t1/t12/t14 ownership, the true material branch (`cb0[85]` and
   `cb0[102]`) and the downstream first mismatch with the PTDE original
   material operator.
2. Port only that DSR-only **material response operator**, not all stock
   DX11 second gamma or a blanket RX shader, using native R19 shared
   PS + material-selective draw binding. Do not alter unrelated DSR
   PBL/PointLight/fog branches.
3. Preserve stock DSR Fog/LightScattering/Glow and original independent
   `latest_hook_source`. Exact RDEF/ABI and physical PTDE sidecar proofs
   must fail-open. Do not bind V270 fixture shaders with artificial RT1.
4. Validate operator-local downstream and PTDE-visible pixels for actual
   P_Metal and nonmetal armor, while leaving unmatched/world/body materials
   stock DSR.

**STATUS:** Original→true-final-R19 **CONSTRUCTION and static shader
operator reachability CONFIRMED**. Actual equipment branch, material-local
replacement, active receiver draw, WARP-on-owner-final and pixel parity
remain OPEN. No user test addon issued from this stage.

**Repo lineage:** PR #364; starts at V272 and intentionally omits
previously REJECTED-as-visual-fix V275 source policy. R43 active control
plane not modified.
