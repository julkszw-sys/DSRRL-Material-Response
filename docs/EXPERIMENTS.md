# Renderer experiment index

This file distinguishes active source experiments from retained historical diagnostics.

Experimental success is never a release or pixel-equivalence claim.

## Current experiment

### PTDE Virtual EnvSpec V3

Source:

`src/runtime_core_v2_virtual_envspec_v3.cpp`

Build gate:

`DSRRL_BUILD_EXPERIMENTS=ON`

Purpose:

- identify native DSR EnvSpec probes by exact certified SHA-256 fingerprints;
- map the native probe identity to the corresponding exact PTDE PackedGI probe;
- construct a DSR-compatible virtual R11G11B10_FLOAT cube from the PTDE signal;
- substitute only when the exact source resource identity has been established;
- remain a broad diagnostic until exact operator/material routing authorizes production activation.

V3 caches virtual GPU cubes by exact `(device, packed_index)` identity so repeated DSR resources for the same probe reuse one virtual resource. Cache identity is invalidated on device teardown.

Status:

- CONSTRUCTION: validated by CI when the Windows/ReShade job passes;
- COMPATIBILITY: source/API compile validation only;
- RUNTIME LIVENESS: requires independent runtime evidence;
- BRIDGE ACTIVATION: requires independent runtime evidence;
- PIXEL-BEHAVIOR: OPEN.

## Legacy experiments

### PTDE Virtual EnvSpec V1

`src/runtime_core_v2_virtual_envspec.cpp`

Exact PTDE slot2 sidecar identity was used as the carrier. Retained for historical comparison.

### PTDE Virtual EnvSpec V2

`src/runtime_core_v2_virtual_envspec_v2.cpp`

Added a legacy Material Response companion route using the 1.45 binary/TLS contract. Companion access has been made thread-safe, but the RVA/TLS coupling is not the target architecture.

V1 and V2 build only when:

`DSRRL_BUILD_LEGACY_EXPERIMENTS=ON`

They should not receive new production features. New EnvSpec work belongs in V3 or a successor integrated through the common source-first runtime.

## Promotion rule

A diagnostic may move toward production only after:

1. exact receiver identity is established;
2. exact material/MTD identity is established where required;
3. logical resource identity and format are verified;
4. draw-specific state is transactionally restored;
5. downstream composition is understood;
6. runtime activation is separately proven;
7. visible behaviour improves toward PTDE.

Receiver/resource equality alone is not sufficient.
