# Resource Routing Recovery V1

## Problem

Material Response 1.45 Telemetry V2 proves the Material Response/c101 core and PTDE SpecRGB t10 path are active, while Diffuse t0, Normal t2, EnvSpec t12+t14 and Subsurf route3 remain inactive in the tested runtime.

The project already has a known-good Normal/Diffuse lineage. V12 proved this route end-to-end:

`local receiver ordinal 0..22 -> exact SRV lookup -> safe tuple/pair -> sidecar ready -> t0/t2 bind`.

## Regression isolated in 1.45

At the common Diffuse PREPARE cut (RVA `0x19ABD3` in the Telemetry V2 basis), the current binary no longer applies the V12 ordinary DifSpcBmp gate. Instead it calls a later exact P_Metal gate requiring route 345 / source 9..11 semantics before the shared asset-transport path can continue.

That is the wrong semantic cut for the ordinary Diffuse resource bridge. The P_Metal discriminator is material-local and must not gate the shared equipment Diffuse transport.

## Recovery V1

Recovery V1 restores only the V12 receiver-domain predicate at that cut:

```text
local_receiver = TLS[+0x14]
if local_receiver >= 23:
    fail open
else:
    continue existing exact-SRV / pair / sidecar path
```

No renderer equation, c100/c101 material response, t0/t2 final binder, SpecRGB path, Subsurf route, PointLight, PARAM, or PE layout is changed.

The patch is intentionally small and guarded by the exact Telemetry V2 SHA-256 and exact expected bytes.

## Validation contract

Runtime success for this recovery requires:

- `SPC=1` remains true;
- `DIF` transitions to `1` on ordinary homologous DifSpcBmp equipment;
- `NRM` transitions to `1` when the downstream Normal path is exercised;
- `FAIL=0` remains true;
- normal game exit.

`ENV` and `SUB` are independent and are not claimed fixed by Recovery V1. Runtime activation does not imply PTDE pixel equivalence.
