# DSRRL v2.3.0 — cross-operator mapping audit (2026-10-21)

Source baseline: `457d8d93ec981b891ed3f30fa05660f46f768740` (experimental PR482 descendant of owner-uploaded v2.3.0 EnvSpec170 / BC1 T1 Fingerprint). Audit-only: **NO shader, PARAM, MTD, executable, addon, or resource changes**.

## Most important correction: withdraw the 21-slot "fix"

**REJECTED.** Earlier audit `DSRRL_ENVSPC_ATLAS_MAPPING_AUDIT_2026-10-21.md` and the draft branch `agent/ptde-envspec-slot21-repair-20261021` treated the V320 table as proven PTDE targets. This was **wrong**. Preexisting more authoritative V322 original-BND3 evidence (canonical revision 13420) proves that old parser started entries at byte `0x2C`, not the true `0x20`. Re-reading retail PTDE/DSR records yielded **zero differences** between the two parsed `g_EnvSpcSlotNo` integers on shared names; two records had no PTDE value. V319 and V320 were subsequently **REJECTED** (canonical revisions 13448–13452), including the bogus `C_RoughCloth[DSB]_Edge` shift from 0 to 3. `P_DullLeather[DSB]_Edge` and `P_Metal[DSB]_Edge` forced changes are also unauthorized. Older revision 13402 HIGH CONFIDENCE edge-slot inference is superseded by the later corrected binary parser.

The separate slot21 build CI run [39514220613](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/39514220613) failed before producing an addon (PowerShell `Resolve-Path` in builder). The erroneous source branch ref has been reset to its verified baseline `457d8d93` with a GitHub force-with-lease update. **Do not install, promote or reconstruct that 21-slot patch.**

## Cross-operator result matrix

The [deterministic static audit](../tools/audit_v230_cross_operator_mapping.py) checks 9 source contracts. **9/9 PASS is SOURCE CONSTRUCTION ONLY, not nine pixel PASSes.**

| Operator / routing layer | Audit result | Error or remaining gap | Next evidence |
|---|---|---|---|
| MTD / Material Response | **False-positive correction REJECTED**; 325 unique original DSR MTD identities, 213 PRESENT / 87 EXPLICIT_NONE / 25 NOSPC_HOST | Do not import V319/V320 "PTDE slots" again. Renderer material slots are not global gains and unsupported owners must fail-open | Original retail MTD pairwise proof if a *new* suspect appears |
| GI EnvSpec atlas | Index **PASS**: 342 ordinal × four 32²×6 RGBA cubes, `probe*4+slot`, full pack 33,619,968 bytes | Static lookup consistency does not prove a GPU-selected cube or a PTDE equivalent pixel | Exact stock t12/t14 SRV → ordinal → PTDE cube SHA → issued draw witness |
| Additional native DSR cubemaps | Source experimental gate **PASS** for three observed SHA | 15 V359 creation SHA not in canonical 342 and unknown resource family; no original PTDE mapping. Three-SHA test prepared DSR t12/t14 SRV but final draw may fail | Per-resource original filename/producer, per-SRV issued draw |
| SpecRGB / t1 | Fail-open semantics **PASS**; **CONFIRMED runtime gap** `logical_known=0 companion_ready=0` | BC1 t1 SRV exists but PTDE SpecRGB resource identity/companion not proven; PTDE replacement transaction is rejected | Exact native t1 source→name→t10 PTDE material/owner join |
| EnvDiffuse | **Intentional stock DSR** isolated in v2.3.0 via `DSRRL_V230_STOCK_DSR_ENVDIFFUSE` | No PTDE operator parity claim; Lerp CB12 carries EnvDiffuse A/B but independent consumer remains stock | Compare independent EnvDiffuse operator only, not via EnvSpec gain |
| LightBank source / CB | **PASS** source ABI: A/B and beta transported into b12; owner log row5→row6 beta 0.033333 | Actual GPU CB12 consumption and exact per-draw provenance not attested by logs alone | Read actual CB at draw and first consuming shader instruction |
| HemEnvLerp shader + composition | **PASS** structural presence of A/B interpolation MAD and t12/t14; historical V338/V349 swizzle audit wired | **CONFIRMED owner phenotype:** immediate mode-dependent shiny/glossy film with little visible beta change, root operator **OPEN**; separate Stable vs Lerp shader paths | Exact same material/location stable(A) vs lerp(A,B, beta=0), before SAT and after composition; then beta=1/A=B |
| PointLight | **Intentional addon bypass** with full stock DSR host preserved in this baseline | PTDE spatial Light L(d,N,M) is not reconstructed by this build; not an EnvSpec layer or a slot-index defect | Isolated original PTDE PointLight producer/consumer probe |
| GPU/pixel level | **NOT AUDITED** by source CI | No PTDE pixel equivalence claim | PTDE/DSR matched scenes, operator-local GPU captures |

## Concrete runtime evidence

Owner `ReShade(20261020-223504).log` shows `NATIVE UNKNOWN DIAG state=PREPARED_NOT_PIXEL` for RX34 Stable route 160 and route 88. For route160, an immediately following `SPECRGB ROOT` shows `spec_request=0 logical_known=0 companion_ready=0`; the following `pmetal_spec_rgb_reject` aborts the replacement draw. Thus the experimental native cubemap cannot be considered visible from PREPARED alone. Meanwhile RX33 HemEnvLerp for route 345/232 sees beta≈0.03333, but the stock cubemap diagnostic has no witnessed Lerp hit. It cannot be assigned as the Lerp snap root.

## Safe next actions

1. **Do not alter slot numbers or PackedGI bytes** without new, original PTDE and DSR producer evidence. The old 21-row CSV/markdown are superseded and not patch authority.
2. First trace the actual missing SpecRGB logical t1 → PTDE t10 join for route160, with full raw resource and model/MTD identities. Preserve fail-open.
3. Separately instrument **draw-specific Stable↔Lerp continuity** under the same RX, material, t12/t14, LightBank A/B and explicitly controlled shader beta=0. Test at the first consumer before global postprocess, never change the LightBank to compensate.
4. Maintain isolated PointLight, fog and host renderer; do not promote source/CI success into pixel equivalence.

## Reproduction

`python3 renderer-core/tools/audit_v230_cross_operator_mapping.py` writes `operator_mapping_audit_v230.json`. CI: [cross-operator audit run #39514985838](https://github.com/julkszw-sys/DSRRL-Material-Response/actions/runs/39514985838); PASS 9/9. This source audit does NOT rerun retail BND3 binary extraction or GPU screenshots. Historical V322 provenance (latest trusted retail binary comparison) remains the authority for rejecting the 21-slot false-positive.
