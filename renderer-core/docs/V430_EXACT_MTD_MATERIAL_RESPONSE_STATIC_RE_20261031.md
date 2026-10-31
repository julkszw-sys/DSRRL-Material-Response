# V430 — Exact RX34 MTD and PTDE↔DSR material-response static census (2026-10-31)

Read-only source: owner archive `DSRRL_FINAL_EVIDENCE_PART02(2).zip` SHA-256 `57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076`. Decompressed DSR `Mtd.mtdbnd.dcx`, `MtdPatch.mtdbnd.dcx` and original PTDE MTD BND3. No game binary changes, no runtime or addon claims.

## Direct RX34 material join

DSR `P_Metal[DSB].mtd` SHA256 **`ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b`**, byte-identical to historical RX34 route345 Source1 MTD SHA in V421/V413. PTDE original `P_Metal[DSB].mtd` SHA256 `eefeefa39946779ab2a45fde28a8d7aa67e2b8ccc749e7b48b51e002059c0c92`. DSR MtdPatch contains no P_Metal override.

Both embed the same material authoring source `N:\\FRPG\\data\\Material\\spx\\FRPG_Phn_ColDifSpcBmp.spx`. This establishes *MTD source family*, not exact runtime DXBC shader variant or shader semantics. The original DXBC BND contains no compiled filename with literal `ColDifSpcBmp`; different `DifSpcBmp...` permutations remain unjoined to this RX34 draw.

Exact named **MTD defaults** in original source (NOT attested runtime uniforms):

| MTD | Parameter | PTDE | DSR |
|---|---|---:|---:|
| P_Metal[DSB] | g_SpecularMapColorPower | 2.5 | 1.0 |
| P_Metal[DSB] | g_SpecularPower | 8.5 | 1.0 |
| C_Metal[DSB] | g_SpecularMapColorPower | 2.5 | 1.0 |
| C_Metal[DSB] | g_SpecularPower | 8.5 | 1.0 |
| M_7Metal[DSB] | g_SpecularMapColorPower | 2.5 | 1.0 |
| M_7Metal[DSB] | g_SpecularPower | 8.5 | 1.0 |
| C_DullLeather[DSB] | g_DiffuseMapColorPower | 0.5 | float32(0.7) |
| C_DullLeather[DSB] | g_SpecularMapColorPower | 1.0 | float32(1.2) |
| C_DullLeather[DSB] | g_SpecularPower | 2.0 | 1.0 |

P_Metal's other parsed typed defaults agree: diffuse RGB 1,1,1; diffuse power 0.5; specular RGB 1,1,1; shadow scale float32(0.8); tex scroll zero, detail bump zero. Its two numeric specular defaults differ; PTDE and DSR raw MTD files otherwise have additional incidental padding/metadata differences, not automatically operator changes.

## Full native BND3 census

PTDE **435** unique MTD filenames, DSR **580**, **435 shared**, DSR-only **145**, PTDE-only zero. Among 435 shared, **300 same embedded .spx path**, **135 different .spx path**. Named typed default parser detects **226** shared materials with at least one comparable default changed: **187** with unchanged .spx family, 39 with changed source family. Per-parameter affected materials: g_SpecularMapColorPower 173; g_SpecularPower 146; g_DiffuseMapColorPower 26; g_DiffuseMapColor 17; g_SpecularMapColor 11. These are subsets of parseable named scalars/vectors, not full material physics or pixel equivalence.

The owner-facing handoff `DSRRL_V430_MTD_MATERIAL_RESPONSE_STATIC_RE_FULL_HANDOFF_2026-10-31.zip` contains the SHA-pinned full offline stdlib verifier, 435-name hash/SPX index CSV, named default-difference CSV (420 changed named entries), JSON summary and report. No proprietary game MTD or DXBC bytes included.

**Operator consequence**: independently verified MTD default material-response differences are not the same as V425/V428 LightBank type5 gamma transport; do not compensate one with the other. Copying PTDE's numerical defaults into DSR is NOT established as the correct inverse because the DSR material/shader operator and per-FLVER overrides are unresolved. Next static work: MTD parameters -> native CB writer -> exact active P_Metal DXBC hash and PS def-use; separate nonmetal family/asset and pass classification. Receiver guard must fail-open on any unverified identity.

**Status**: original MTD identities and comparable named defaults CONFIRMED STATIC, source SPX family CONFIRMED, exact compiled RX34 shader association OPEN, active uniform/consumer OPEN, pixel OPEN. Frozen V359-RC1 and R43 Source2 PROTECT intact.