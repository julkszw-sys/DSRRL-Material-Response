# Runtime Core V2

Status: development / pixel-inert infrastructure

This branch introduces a clean-room runtime state layer for Material Response. It was designed after auditing a separate Dark Souls renderer mod as an architectural reference. No implementation code was copied from that binary.

## Why this exists

Material Response already has working operator-local code, but the historical development path accumulated several independent routing mechanisms. That makes these states easy to confuse:

1. add-on loaded;
2. hook/event active;
3. receiver matched;
4. logical resource matched;
5. exact material matched;
6. bridge activated;
7. original D3D state restored;
8. visible operator changed;
9. PTDE-visible result improved.

Runtime Core V2 makes steps 3-7 explicit and machine-countable.

Dark Souls Remastered also creates multiple D3D11 deferred contexts. Runtime state therefore cannot be treated as one process-global draw state. The core tracks command lists independently.

## Core rules

### Per-command-list state

Every ReShade `api::command_list *` gets an independent record:

- bound pixel pipeline identity;
- pipeline generation;
- draw serial;
- active draw transaction, if any.

This is intended to cover the implicit immediate command list and D3D11 deferred contexts without TLS guessing.

### Generation-safe resource identity

A raw D3D/ReShade handle is not a permanent identity. Handles may be released and reused.

The tracker stores a generation for:

- resources;
- pipelines.

A resource view has its own generation and also records the resource generation it was created against. If either a view handle or resource handle is destroyed and later reused, an old command-list binding no longer resolves to the new object.

The ReShade adapter now fingerprints the semantic `resource_desc` fields at `init_resource` time, before any draw. It deliberately hashes descriptor fields rather than raw struct bytes, so padding is not part of the identity.

Logical resource identity is attached separately only after an operator-specific route proves it. The core maintains a generation-safe reverse index from `logical_hash` to live resources. A lookup succeeds only when exactly one live resource owns that logical identity. Missing or ambiguous logical identities fail open and are counted separately; the core never guesses between candidates.

Pixel-shader SRV slots are tracked as `(view handle, view generation)`, then resolved through the live resource generation and logical identity. This prevents accidental cross-resource activation caused by either view-handle or resource-handle reuse.

### Route contracts

An operator declares the evidence required before it may modify a draw:

- `evidence_shader`
- `evidence_receiver`
- `evidence_material`
- `evidence_resource`
- `evidence_logical_id`
- `evidence_format`
- `evidence_state`

The current draw provides the subset actually verified.

If:

```
required & ~verified != 0
```

the transaction is rejected and counted as fail-open. Stock DSR remains active.

This is deliberately stricter than matching a shared shader body. It is compatible with the current P_Metal PROTECT, which forbids a visible shared-HemEnv patch without exact material gating.

### Draw transaction invariant

A bridge activation is a transaction:

```
verified route
    -> save relevant D3D state
    -> bridge active
    -> draw
    -> restore complete relevant state
```

Every activation must produce one restore.

At a frame boundary, an active transaction is treated as a restore invariant violation:

- `unrestored += 1`

It is deliberately **not** counted as fail-open. Fail-open means the bridge refused to mutate and stock DSR remained intact. A missing restore happens after activation and is therefore a distinct integrity fault. The tracker clears only its bookkeeping marker; operator-specific state restoration is still mandatory.

The state-restoration implementation remains operator-specific. Runtime Core V2 records the invariant; it does not pretend that all operators touch the same D3D state.

## Per-operator proof counters

Counters are separated for:

- SpecRGB
- Diffuse
- Normal
- EnvSpec
- Subsurf

Each operator exposes:

- `receiver_matches`
- `resource_matches`
- `activations`
- `restores`
- `fail_open`
- `rejected`
- `stale_view`
- `unrestored`
- `restore_faults`
- `logical_miss`
- `logical_ambiguous`

The snapshot also contains source-level staged routing counters for each operator:

`capture -> receiver -> resource_lookup -> route_gate -> sidecar_ready -> final_bind -> restore`

These replace the old binary-only V8/V12 one-shot stage flags. Operator code records a stage only after that stage has actually succeeded. The first stage that remains at zero therefore identifies the current activation boundary without changing rendering.

This allows a runtime report to answer a concrete question such as:

> EnvSpec receiver was seen 412 times, the exact logical resource was seen 397 times, the bridge activated 397 times, restored 397 times, and failed open 15 times.

That is substantially more useful than a single “addon loaded” or “shader count 24/24” line.

## ReShade API 20 integration map

The shipping addon already targets ReShade API 20. The intended adapter uses the public event surface:

- `init_device / destroy_device`
- `init_command_list / destroy_command_list`
- `init_resource / destroy_resource`
- `init_resource_view / destroy_resource_view`
- `init_pipeline / destroy_pipeline`
- `bind_pipeline`
- `push_descriptors` for D3D11 pixel-shader SRV slots
- descriptor-binding events already used by the bridge
- draw events already used by the bridge
- `present` as a bounded proof/report boundary

For D3D11, ReShade documents that `init_command_list` is called both for created deferred contexts and for the implicit immediate command list. This is the correct ownership unit for draw state.

## Integration order

1. Merge Runtime Core V2 as pixel-inert tracking only.
2. Feed the existing 24 confirmed HemEnv receiver identities into the pipeline registry.
3. Move SpecRGB routing to the route-contract gate.
4. Move Diffuse and Normal routing.
5. Move EnvSpec last, preserving the exact P_Metal material gate and full-state restore requirement.
6. Move Subsurf.
7. Remove legacy per-feature duplicate state trackers only after counter equivalence is demonstrated from existing evidence.

## What this deliberately does not do

Runtime Core V2 does not:

- change shader math;
- change PTDE/DSR material equations;
- add global lighting gains;
- replace PBL/TAA/SSS globally;
- classify a material from shader identity alone;
- make a pixel-equivalence claim;
- authorize an operator merely because its resource or receiver looks similar.

It is control-plane infrastructure for existing and future operator islands.

## Tests

The pure C++ core has no ReShade dependency and can be tested independently:

```text
g++ -std=c++17 -Wall -Wextra -Werror \
    src/runtime_core_v2.cpp \
    tests/runtime_core_v2_tests.cpp \
    -o runtime_core_v2_tests

./runtime_core_v2_tests
```

The initial test set covers:

- repeated resource init without false generation change;
- resource handle reuse;
- resource-view handle reuse;
- stale command-list SRV rejection;
- two independent command-list states;
- route rejection when exact material evidence is missing;
- successful activation + restore;
- frame-boundary detection of an unrestored transaction.



## Resource-routing stage migration contract

The legacy Normal/Diffuse V12 runtime path established the useful diagnostic sequence:

```text
capture
  -> verified receiver
  -> exact logical SRV/resource lookup
  -> route gate
  -> sidecar/replacement ready
  -> final slot bind
  -> restore
```

Runtime Core V2 exposes the same semantic stages generically through `route_stage` and `tracker::note_stage`. This is intentionally not tied to old RVAs or binary flag storage.

Recommended migration pattern for an operator:

```cpp
auto &rt = global_tracker();

rt.note_stage(op, route_stage::capture);          // exact logical identity captured
rt.note_receiver_match(op);                       // certified consumer/receiver
rt.note_stage(op, route_stage::receiver);

const auto host = rt.resolve_bound_pixel_shader_resource(command, slot, op);
if (!host || host->logical_hash == 0) {
    rt.note_fail_open(op);
    return;
}
rt.note_resource_match(op);
rt.note_stage(op, route_stage::resource_lookup);

// Exact operator-specific pair/triple/material gate passed.
rt.note_stage(op, route_stage::route_gate);

// Replacement exists, has exact identity and is safe to bind.
rt.note_stage(op, route_stage::sidecar_ready);

// Perform the operator-local D3D transaction, then:
rt.note_stage(op, route_stage::final_bind);

// Restore complete relevant state, then:
rt.note_stage(op, route_stage::restore);
```

The example is ordering guidance, not authorization to activate a bridge. Each operator still needs its own verified receiver/material/resource contract.
