# Architectural reference audit: dxgi(7).dll

This document records architectural observations only. It is not a source-code port.

## Identity

Reference file SHA-256:

`42f2b9039a5b416416db9869ab019f8f86811428e8a285905e559d186cff64bc`

Size:

`20,746,240 bytes`

Static strings identify it as:

- DarkSouls2 Addon
- DarkSouls2 Lighting Engine
- version 0.9.42
- target executable DarkSoulsII.exe

The binary imports `D3DReflect` and `D3DCompile` from `D3DCOMPILER_47.dll`.

## Useful architectural patterns

The relevant lesson is not its visual feature set. It is the separation of renderer control-plane concerns.

The binary contains independently named systems for:

- engine core initialization;
- texture replacement;
- atmosphere;
- renderer state;
- debug drawing;
- game-code integration.

It also contains explicit messages for:

- immediate command queue/context initialization;
- deferred command queue/context initialization;
- device mismatch checks;
- shader reflection failures;
- texture lifetime/replacement lifetime;
- resource/view tracking;
- render-pass classification;
- texture cache and override metadata.

Examples of render-pass labels visible in the binary include GBuffer, reflection, sun shadow map, shadow map, light accumulation, SSAO, screen-space shadow generation and SSR.

Examples of texture-routing infrastructure visible in the binary include:

- a texture override INI;
- shader hashes;
- shader reflection;
- texture fingerprints/cache;
- DDS loading;
- lifetime diagnostics when the game releases a texture while a replacement is active.

## What we are adopting

Clean-room equivalents of the following concepts are useful for DSRRL:

1. one runtime state owner instead of feature-local ad-hoc state;
2. explicit per-command-list state, including deferred contexts;
3. generation-safe resource/view identity;
4. strong shader/receiver identity;
5. separate logical resource identity from raw resource handles;
6. bounded per-operator activation/restore/fail-open counters;
7. explicit route contracts before any visible state change;
8. central lifetime handling for replacement resources.

## What we are not adopting

The reference also contains a large number of renderer features that are not part of the DSRRL target, including its own atmosphere, AO/SSR/SSGI/upscaling and other renderer systems.

Those are not a justification for replacing Remastered-native systems.

DSR remains the host renderer. A DSRRL bridge still needs:

- a proven PTDE↔DSR mismatch;
- the actual semantic input and consumer;
- the narrowest valid carrier;
- exact receiver/material/resource routing;
- full relevant state restoration;
- fail-open on identity mismatch;
- independent PTDE-visible validation.

## Why this matters for current Material Response

The public Material Response 1.45 release already has operator-specific logic for PTDE SpecRGB, diffuse, normal, Subsurf and EnvSpec. The missing common layer is a single proofable runtime state model.

Runtime Core V2 is therefore deliberately infrastructure-first. It does not copy the reference binary's renderer features or implementation. It uses only independently observed architectural ideas and implements them against the public ReShade API and our own DSRRL routing rules.
