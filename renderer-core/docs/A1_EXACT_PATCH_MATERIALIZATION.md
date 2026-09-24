# A1 exact patch materialization

This layer turns the recovered P2.2 source plan into exact per-island DXBC
patch recipes. It complements the provenance verifier added in commit
`1bd9b1a`: that verifier proves the recovered source and the 144-plan/312-op
selection; this layer makes those selected operations directly consumable by
Renderer Core.

## Source

- artifact 253:
  `DSRRL_RENDERER_P2_2_HDR_OFF_CAUSAL_CONTROL_2026-09-07.zip`
- archive SHA-256:
  `70b2d91aec619ad2df99a161151d974d8d52269f4040acdd6288ff586a9f8fed`
- source member:
  `data/PORT_PLAN.json`
- source member SHA-256:
  `5cc15f8084fb75cb33c27be3f0e94be7fd186f07c16274b33ee09735b747a1ec`
- repository copy:
  `renderer-core/data/provenance/p2_2_PORT_PLAN.json.gz`

## Exact owner decomposition

The generated table contains 144 selected shader bodies and 312 exact DWORD
operations, partitioned by semantic owner:

- `terminal_sat_rgb`: 144 ops
- `diffuse_material_domain`: 108 ops
- `pointlight_pnts_attenuation`: 24 ops
- `envspec_nospc_delete`: 12 ops
- `fixed_postfog_identity`: 24 ops

No rejected or nonclosed P2.2 mask bit is materialized by this table.

## Fail-open transaction

`apply_a1_exact_owner_patch` requires the caller to select the plan by exact
original shader SHA-256 and exact code size. For the requested owner, every
target DWORD is checked before any write occurs.

The function fails open when:

- the original shader identity is absent;
- code size differs;
- the selected plan has no operations for that owner;
- an offset is out of bounds;
- any DWORD is neither the expected old word nor the exact replacement word;
- the owner-specific recipe is already partially applied.

A fully old state is patched atomically at owner scope. A fully new state is
reported as already applied.

This is a construction/materialization primitive only. Feature activation,
receiver/material/resource routing, DXBC checksum handling, full replacement
hash validation, runtime liveness and PTDE-visible pixel validation remain
separate layers.
