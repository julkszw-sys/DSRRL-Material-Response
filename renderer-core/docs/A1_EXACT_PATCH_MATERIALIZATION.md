# A1 exact patch materialization

This layer turns the recovered P2.2 exact recipe corpus into per-island DXBC
patch recipes. It complements the target-branch provenance verifier: that
verifier proves the 144-plan/312-op corpus against the identity index; this
layer makes those operations directly consumable by Renderer Core.

## Source

- source artifact 253:
  `DSRRL_RENDERER_P2_2_HDR_OFF_CAUSAL_CONTROL_2026-09-07.zip`
- source member:
  `data/PORT_PLAN.json`
- source member SHA-256:
  `5cc15f8084fb75cb33c27be3f0e94be7fd186f07c16274b33ee09735b747a1ec`
- exact selected corpus in Git:
  `renderer-core/data/provenance/a1_exact_recipes_v1.part1.tsv`
  through `part3.tsv`
- aggregate exact recipe corpus SHA-256:
  `c38d5560e7dfc99f149da0908d754400e8bf654fe7e0a0469e5f08f47d7575b0`

## Exact owner decomposition

The generated table contains 144 selected shader bodies and 312 exact DWORD
operations, partitioned by semantic owner:

- `terminal_sat_rgb`: 144 ops
- `diffuse_material_domain`: 108 ops
- `pointlight_pnts_attenuation`: 24 ops
- `envspec_nospc_delete`: 12 ops
- `fixed_postfog_identity`: 24 ops

Owner assignment is reconstructed from the exact DWORD translation itself and
cross-checked against the owners declared by the identity index. No rejected or
nonclosed P2.2 mask bit is materialized by this table.

## Fail-open transaction

`apply_a1_exact_owner_patch` requires exact original shader SHA-256 and exact
code size. For the requested owner, every target DWORD is checked before any
write occurs.

The function fails open when the shader identity is unknown, code size differs,
the plan has no operation for the requested owner, an offset is invalid, an
unexpected word is observed, or the owner-specific recipe is already partially
applied.

A fully old state is patched atomically at owner scope. A fully new state is
reported as already applied.

This closes the missing exact byte-recipe materialization layer for the five
already-closed A1 islands. It does not itself activate them. Receiver/material
routing, feature gates, DXBC checksum/replacement-container handling, runtime
liveness and PTDE-visible pixel validation remain separate layers.
