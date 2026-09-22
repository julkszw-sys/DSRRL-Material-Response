# Contributing

Development targets faithful PTDE-visible operator behaviour while keeping Dark Souls Remastered as the host renderer.

Read [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) before changing renderer code.

## Required engineering rules

- isolate the responsible operator before changing output;
- use the narrowest valid carrier: PARAM, shader, constant buffer, resource, material or asset;
- require exact receiver/material/resource identity where the operator needs it;
- fail open to stock DSR when identity or state cannot be verified;
- do not use arbitrary gain/exposure/tone-map compensation for a different operator;
- do not generalize from one shader family without a census;
- keep draw-specific state scoped to the command list/context;
- restore every mutated D3D state component after the draw;
- distinguish construction, compatibility, runtime liveness, bridge activation and pixel behaviour.

## Branching

- `main`: release/reproduction only.
- `develop`: canonical source-first integration.
- `exp/<operator>/<purpose>`: short-lived experiments.

New long-lived development branches should not be created when the work belongs on `develop`.

## Validation

At minimum, source changes should pass:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

ReShade adapter and addon changes should also compile against the pinned ReShade API revision used by CI.

Runtime or pixel claims require their own evidence; successful compilation is not sufficient.
