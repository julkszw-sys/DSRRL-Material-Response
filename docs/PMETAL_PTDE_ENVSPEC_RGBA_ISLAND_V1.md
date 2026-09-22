# P_Metal PTDE EnvSpec RGBA Island V1

## Scope

Drop-in successor to `material_response_1_45_resource_routing_stage_telemetry_v2` for the exact ordinary no-PointLight `P_Metal[DSB].mtd` lane.

This is **not** a second ReShade addon and does not use cross-addon TLS. It reuses the current Material Response draw transaction and exact material routing already present in the basis.

Exact identity:

- actual-material route: `345`
- raw DSR MTD SHA-256: `ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b`
- certified source/receiver indices: `9/10/11`
- dedicated embedded DXBC objects: `72/73/74`
- shared generic DifSpcBmp HemEnv receivers `33/34/35` remain byte-identical.

## Repair

The current line loads the canonical raw RGBA PackedGI corpus:

`DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin`

SHA-256:

`c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3`

The existing dedicated P_Metal receivers were still shaped for the historical predecoded R11G11B10_FLOAT carrier. V1 repairs that carrier/consumer mismatch only in DXBC72/73/74.

For A/t12 and B/t14 the dedicated receiver now performs:

```text
SAMPLE_L full RGBA at physical LOD0
RGB / sampled alpha
PTDE A/B endpoint source factors
HemEnvLerp beta when B is active
fresh t10 SpecRGB * cb12[0].rgb (PTDE c101) * COLOR0
terminal RGB SAT
```

The dedicated island retains the PTDE-style direct reflection-vector path and does not consume the stock DSR `t9` BRDF LUT/split-sum EnvSpec tail. The stock DSR roughness/dynamic-LOD EnvSpec hybrid is therefore not the P_Metal EnvSpec consumer in this lane.

## Invariants

Unchanged from the basis:

- complete material route table;
- shared DXBC33/34/35;
- Diffuse bridge;
- Normal bridge;
- SpecRGB resource routing (the dedicated receiver consumes fresh t10);
- EnvDiffuse;
- PointLight;
- Subsurface;
- PARAM/postprocess;
- non-P_Metal materials fail open to their existing basis behavior.

Output addon SHA-256:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

Basis addon SHA-256:

`e15747f4920bd5f40db9019e4e45110ad4655b4defab1ad3027b039fc022d98b`

## Construction audit

- addon size unchanged: `1,803,264` bytes;
- embedded DXBC census: `78`;
- exact changed set: `72/73/74` only;
- all output DXBC checksums valid;
- shared `33/34/35` byte-identical;
- deterministic rebuild is byte-identical to the packaged addon;
- PE exports remain `AddonInit`, `AddonUninit`, `DESCRIPTION`, `NAME`.

## Known residual

The historical V3 exact PTDE `s12/s14` sampler transaction is **not** reintroduced in this current-line V1. The resource/receiver mismatch is repaired, but the exact PTDE mixed `MIRROR/MIRROR/WRAP`, anisotropic, one-mip sampler remains a separate known residual.

Therefore construction is `PASS`; runtime activation and PTDE-visible pixel equivalence remain `OPEN` until observed. No construction result is promoted to pixel PASS.
