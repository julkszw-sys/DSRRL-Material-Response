# DSRRL Renderer Core v1 — Phase 0

This directory is the future stable runtime foundation for DSRRL Renderer Edition.

## Phase 0 goal

Phase 0 is deliberately pixel-inert. It does not restore Upper/Lower, PointLight,
EnvSpec or any other PTDE renderer operator. Its job is to prove that the architecture
itself is stable before operator islands are integrated.

Default invariants:

- every operator feature gate is OFF;
- no DarkSoulsRemastered.exe hook is installed by Renderer Core;
- no shader is replaced;
- no resource is replaced;
- no draw is replayed;
- no D3D state is mutated;
- no semantic snapshot stores a raw game pointer;
- the optional ReShade probe only registers the addon and logs READY;
- with all islands disabled, Renderer Core is pass-through by construction.

The accepted behavior baseline for integration is Material Response 1.45
SHA-256 e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342.

## Architecture

The stable core owns infrastructure, not PTDE equations:

- feature_registry: independent island gates, default OFF;
- hook_registry: one semantic owner per EXE hook site;
- receiver_registry: exact shader identity plus capability mask;
- snapshot_bus: immutable copied semantic values, never borrowed game pointers;
- carrier_abi: versioned GPU carrier layout;
- shader_registry: create-time replacement recipes;
- draw_transaction_manager: one transaction per native draw and mandatory restore;
- renderer_core: plan composition and pass-through invariant.

Operator implementations will live outside the core and request services through these
interfaces. An operator may fail open without disabling unrelated islands.

## Carrier ABI v1

The first eight float4 lanes are frozen:

0 D1 direction
1 D2 direction
2 D3 direction
3 D1 PTDE color
4 D2 PTDE color
5 D3 PTDE color
6 Upper PTDE
7 Lower PTDE

ABI v1 is exactly 128 bytes. Existing slots must never be repurposed. Future payload
growth requires a new carrier or an explicit ABI version.

## Hook policy

A new EXE hook is allowed only when reverse engineering proves that the required
semantic state is irrecoverably lost downstream and no exact narrower carrier exists.

One hook site has one owner. Multiple operator islands may consume a core-published
semantic event; they may not install competing detours on the same address.

## Build

Pure core and tests are platform-neutral:

    cmake -S renderer-core -B build/renderer-core
    cmake --build build/renderer-core
    ctest --test-dir build/renderer-core --output-on-failure

The optional Phase 0 ReShade API20 probe is Windows-only:

    cmake -S renderer-core -B build/renderer-core-p0 \
      -DDSRRL_BUILD_PHASE0_PROBE=ON \
      -DRESHade_INCLUDE_DIR=PATH/TO/PINNED/RESHade/include

The probe is diagnostic only. Final product remains one integrated DSRRL addon.
