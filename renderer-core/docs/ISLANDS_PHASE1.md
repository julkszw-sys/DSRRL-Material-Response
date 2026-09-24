# Renderer Core v1 — Islands Phase 1

This branch starts operator-island integration without activating any island by default.

## First island: Material Response

Material Response is intentionally global in asset scope. It is not restricted to armor,
equipment or player materials.

There are two safe activation classes:

1. global_receiver_safe
   - exact receiver/homolog evidence proves the Material Response operator is safe for
     that shader body independent of the asset that uses it;
   - no equipment/category gate is allowed or required.

2. exact_material_required
   - shared Phn DifSpc/DifSpcBmp hosts may serve semantically different materials;
   - shader identity alone is insufficient;
   - activation requires the existing same-cut actual_material route plus exact material
     identity where route/SHA collisions exist.

This directly preserves the confirmed A1/A2 lesson: broad asset coverage is valid, but
shared specular hosts fail open until exact material routing closes them.

## Operator decomposition

The old Expanded A1 plans sometimes bundled several closed operators in one replacement.
Renderer Core does not repeat that architecture.

Material Response owns only material-response semantics represented by its certified
operation flags. PointLight attenuation, terminal scene clamp, EnvSpec sampling/resource
behavior and other renderer operators belong to separate islands.

## EnvSpec policy

Material Response may carry PTDE EnvSpec presence as semantic metadata, but it never
silently guesses.

- PTDE EnvSpec ABSENT: EnvSpec island may suppress the DSR-only branch.
- PTDE EnvSpec PRESENT + bridge ready: EnvSpec island may activate the PTDE bridge.
- PTDE EnvSpec PRESENT + bridge not ready: preserve host DSR as fail-open.
- UNKNOWN: preserve host DSR.

Therefore broad Material Response does not imply broad EnvSpec replacement or deletion.

## Current evidence basis

- Expanded A1: 144 CONFIRMED+CLOSED shader bodies / 252 aliases, global asset scope,
  EnvSpec deletion only on certified PTDE no-EnvSpec homologs.
- Expanded A2 census: 249 unique PntS/PntSS/PntSSSS bodies / 609 aliases.
- 72 shared Phn DifSpc/DifSpcBmp specular bodies require exact actual-material routing.
- Current Material Response route corpus contains 35 confirmed MTD rows / 34 distinct
  route indices across P_*, C_*, Ps_* and S_* classes.
- route5 contains the known byte-identical raw-MTD collision
  P_Metal[DSB]_Edge.mtd vs S_Metal[DSB]_Edge.mtd, so SHA alone is forbidden there.

## Status

Construction only. Islands remain default OFF. No runtime or pixel claim is made by
this source addition.
