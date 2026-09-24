# Renderer Core v1 architecture contract

## Core vs operator islands

Renderer Core is infrastructure. PTDE restoration logic belongs to isolated operator modules.

Planned islands:
- Material Response
- Upper/Lower
- HemDir3 / D1-D3
- SpecRGB
- EnvSpec
- PointLight
- Subsurface
- Diffuse
- Normal

An island owns only its verified semantic source, producer/consumer transform, carrier slice and receiver scope.

## Draw path

The future active path is:

source capture -> immutable semantic snapshot -> receiver gate -> RenderPatchPlan -> PRE -> one native draw -> POST/restore

There is never one native draw per island. Core composes compatible island requests into one plan. Carrier slot overlap is rejected before activation.

## Safety

Fail-open is local to the island. Unknown receiver, stale identity, missing semantic snapshot, ABI mismatch, resource mismatch, context mismatch or hook ownership conflict must preserve stock DSR for that island.

Immediate and deferred contexts must be treated as distinct command owners. No island may assume one global current D3D11 context.

## Phase 0

Phase 0 carries no renderer behavior. It exists to validate lifecycle and source architecture before the first real island is migrated.

The first island after Phase 0 is Upper/Lower, using the already-confirmed Renderer Edition/M1.2 producer route rather than the Expanded A3 monolith.

## Release governance

An RC/release requires all handwritten source, every generator, immutable external input hashes, deterministic build instructions, build audit, exact source commit, parent/rollback lineage, and independent construction/compatibility/runtime/activation/pixel status.

A binary is an artifact, never the canonical implementation.
