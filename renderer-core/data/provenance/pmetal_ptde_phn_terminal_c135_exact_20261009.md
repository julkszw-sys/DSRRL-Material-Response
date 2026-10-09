# Exact PTDE PHN P_Metal terminal consumer — RX33/RX34 scope (2026-10-09)

**Source:** user-supplied original PTDE compiled PS3 shader assets from
`DSRRL_FINAL_EVIDENCE_PART02(2).zip`, path
`PTDE/PTDE_FRPG_PHN_EXTRACTED/dvdbnd1_dcx_0000631D9390/`.
No shader bytes are redistributed or written into the host EXE.

Four authenticated original PTDE shaders:

| PTDE shader | SHA-256 | terminal DWORD offsets |
|---|---|---|
| FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo | 0c6f2982bc36d56151791acd03caffb63358324d746f3dd408cfeda976fb2daf | 1279,1283,1286 |
| FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo | 9e931f72ca7ebe7ae5bf68f23ff8a7ff3a818f314fd2578f47b1315e9331d9a6 | 1072,1076,1079 |
| FRPG_Phn_DifSpcBmp______Csd_HemEnvLerp.fpo | ab3debbe2f90ccbcc7b70173dc87b4c98c6b6e126b1bd6d93b59685fd38ba72d | 1369,1373,1376 |
| FRPG_Phn_DifSpcBmp______Sdw_HemEnvLerp.fpo | 51451c962c42574a1335a5ebe0eadbc1bc7e3929bcf4af7684cc8229e81d875f | 1162,1166,1169 |

### Actual PTDE consumer operator

Each shader ends with the same executable D3D9 PS3 instructions (last three
instructions before END):

```text
mul r0.xyz, preterminal.rgb, c135.x       ; source operand 0xa0000087
rcp r0.w, c135.y                           ; source operand 0xa0550087
mul_sat oC0.rgb, r0.w, r0.xyz              ; output operand 0x80370800
```

Thus **PTDE output = saturate((c135.x/c135.y) × preterminal_RGB)**.
The 4 authentic shader byte sequences, final instruction forms, full PS3
stream bounds and SHA-256 have been checked locally with
`renderer-core/tools/audit_ptde_pmetal_terminal_consumer.py`
(`--selftest` plus all four input files, PASS).

### Comparison to current DSR/bridge

* The current `pmetal_rgba_materializer.cpp` builds the homologous terminal
  consumer from `b12[0].w` under `DSRRL_PMETAL_FULL_PTDE_HEMENV_R6`.
* `pmetal_envspec_draw_runtime.cpp` sets this to
  `source.phn_k135` **only if** `source.phn_k135_valid`; otherwise **1.0**.
* The current `pmetal_env_source_runtime.cpp` does not establish an exact
  DrawEnv producer of `phn_k135` (no publication of a true, validated
  `phn_k135_valid`), so the active bridge is a unity-surrogate terminal
  rather than **proved PTDE c135**.
* This is a confirmed **consumer/carrier gap**. It is **not proof** that
  c135.x/c135.y differed from 1 at the user's Asylum anchor, nor that the
  bridge's residual pixel error comes from this operator.

**Status:** PTDE terminal consumer CONFIRMED. Exact PTDE dynamic c135 producer
and cut-to-DSR transport OPEN. Pixel-equivalence UNVERIFIED.

**Fix admissibility:** DO NOT invent a constant/gain, globally tune ToneMap,
force the m10/m18 LightBank, or reinterpret RX33/RX34 b0 c61/c62 as the
consumer. The next shader/CB bridge can be activated only with exact
DrawEnv→c135 producer identity and validated receiver family, fail-open
otherwise. Other DSR output processing remains stock.

No new owner-run diagnostic test is requested by this static audit.
