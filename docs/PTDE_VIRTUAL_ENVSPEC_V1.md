# PTDE Virtual EnvSpec V1

## Goal

Test a resource-only carrier:

```text
exact PTDE PackedGI slot2 content
    -> PTDE sampled RGB/alpha decode
    -> DSR-compatible 256x256 cube
    -> 8-mip R11G11B10_FLOAT topology
    -> stock DSR EnvSpec consumer
```

No native DSR cubemap RGB is reused.

This is intentionally a diagnostic bridge, not a PTDE-equivalence claim.

## Why

Current evidence separates two native-DSR cubemap problems:

- broad P_Metal over-energy in locations such as Anor Londo;
- localized blue/cyan hotspot reflections that are visible with native DSR cubemap content.

At the same time, feeding exact PTDE one-mip cubes directly through the current P_Metal path can collapse the armor toward black.

V1 tests whether DSR can consume PTDE-derived content successfully when the resource itself has the topology that DSR expects.

## Exact resource gate

The addon loads:

`DSRRL/EnvSpec/PackedGI/PTDE_GI_ENVSPEC_PACK_RGBA.bin`

and requires canonical SHA-256:

`c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3`

Only runtime-created 32x32x6 one-mip cubemaps that are byte-identical to a canonical PTDE slot2 entry are virtualized.

No approximate probe matching is used.

## Virtual mip construction

For each exact slot2 source cube:

1. Source RGBA is sampled bilinearly.
2. The PTDE post-filter decode `RGB / alpha` is applied.
3. Each of the eight DSR mip levels is generated independently from the same PTDE one-mip source:
   `256, 128, 64, 32, 16, 8, 4, 2`.
4. RGB is stored as `R11G11B10_FLOAT`.
5. No arbitrary gain, chroma correction or native DSR resource content is added.

Generating each mip from the same PTDE source is deliberate: PTDE selects one discrete prefiltered slot and does not have DSR's material-driven eight-level EnvSpec topology. This first diagnostic therefore minimizes inherited DSR mip-content semantics while still satisfying the DSR resource ABI.

## Bind replacement

The original exact PTDE sidecar remains the identity carrier.

When its SRV is pushed to PS t12 or t14, the addon immediately pushes the corresponding virtual R11G11B10_FLOAT SRV at the same slot and preserves the existing sampler/layout/binding transaction.

Probe A/B routing remains the one already selected by the existing bridge; the virtualizer does not infer or replace probe assignment.

## Scope

V1 virtualizes the exact PTDE **slot2 resource class**, not only P_Metal. This is diagnostic because Runtime Core V2 does not yet expose the legacy exact actual-material route to this standalone addon.

Therefore:

- construction/activation can be tested;
- blue-hotspot and black-collapse behavior can be falsified;
- production P_Metal promotion remains prohibited until exact-material gating is integrated.

## Expected log

Startup:

`[DSRRL VIRTUAL ENVSPEC V1] canonical PackedGI PASS; slot2 virtualization armed`

Heartbeat:

`[DSRRL VIRTUAL ENVSPEC V1] ... slot2_exact=<n> virtual=<n> create_fail=<n> t12=<n> t14=<n>`

Useful runtime PASS requires:

- `PACK=1`
- `slot2_exact > 0`
- `virtual > 0`
- `create_fail = 0`
- `t12 > 0` on active slot2 draws
- `t14 > 0` when the B endpoint is active

Pixel behavior remains a separate result.
