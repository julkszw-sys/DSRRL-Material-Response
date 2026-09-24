# Phase 0 acceptance gate

Renderer Core v1 Phase 0 is PASS only when all of the following are true.

## Construction
- pure core builds with warnings as errors;
- unit tests pass;
- carrier ABI v1 is exactly 128 bytes;
- all features default OFF;
- hook registry starts empty;
- transaction registry starts empty;
- source is committed and reproducible.

## Compatibility
- Material Response 1.45 behavior is unchanged;
- no Phase 0 code patches DarkSoulsRemastered.exe;
- no Phase 0 code modifies a shader/resource/PARAM;
- optional probe uses ReShade API 20 only.

## Runtime liveness
Owner runtime must survive startup/menu, loading a save, area loading, bonfire/warp transition, fullscreen/windowed transition, ResizeBuffers/runtime recreation, deferred-context creation/use/destruction, and normal game exit.

## Bridge activation
Not applicable in Phase 0. No PTDE operator is permitted to activate.

## Pixel behavior
Expected result: no Renderer Core pixel delta. Material Response 1.45 remains the visual baseline.

Failure of any Phase 0 criterion blocks Upper/Lower integration.
