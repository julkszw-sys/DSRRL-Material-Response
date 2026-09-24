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

## Operator-window homology invariant

An island is not integration-ready merely because its PTDE input/resource can be fed to
a DSR receiver. Resource substitution is not operator substitution.

Every enabled island MUST define and verify an explicit replacement window:

    replacement_entry
        -> PTDE-owned intermediate operators
        -> replacement_exit
        -> first homologous downstream DSR operation

The window is valid only when all of the following hold:

1. `replacement_entry` is the earliest point at which the island intentionally diverges
   from stock DSR semantics.
2. Every operation between entry and exit has been classified as PTDE-owned,
   semantically shared/homologous, or explicitly suppressed.
3. No active DSR-only transform, selector, BRDF term, decode/encode, exponent/root,
   roughness response, visibility response, cluster filter, host-light contribution,
   or other non-homologous operator survives inside the replacement window.
4. `replacement_exit` reconnects only at the first downstream operation whose semantics
   are proven homologous between PTDE and DSR.
5. A new PTDE resource/value MUST NOT be consumed by an unverified stock DSR tail.
6. If any intermediate operation is unknown, ambiguous, or only inferred, homology is
   not verified.

Accordingly, `downstream_homology_verified` means proof of the complete operator window,
not merely proof that a compatible downstream consumer exists.

For each island the integration record SHOULD carry at least:

- `replacement_entry`;
- `replacement_exit`;
- `first_homologous_downstream`;
- `owned_intermediate_operators`;
- `suppressed_dsr_only_operators`;
- `shared_homologous_operators`;
- `unresolved_intermediate_operators`;
- `downstream_homology_verified`.

Activation rule:

    island_active = feature_enabled
                 && receiver_identity_verified
                 && replacement_entry_verified
                 && replacement_exit_verified
                 && downstream_homology_verified
                 && unresolved_intermediate_operators.empty()

If this rule is not satisfied, the island MUST fail open to the native DSR path for that
receiver/draw. It must not partially inject PTDE state into a stock DSR consumer.

This is the anti-hybrid invariant: a draw may execute the verified PTDE island or the
native DSR island, but must never accidentally execute a PTDE input followed by
unaccounted DSR-only operators. Intentional composition with a proven homologous DSR
suffix is allowed only after `replacement_exit`.

RE/audit work for an island is therefore incomplete until the full entry-to-exit
operator chain has been classified. Known high-risk examples include EnvSpec and
PointLight specular, where resource-only substitution can leave DSR PBL/roughness or
microfacet tails active; the same rule applies to every current and future island.

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
