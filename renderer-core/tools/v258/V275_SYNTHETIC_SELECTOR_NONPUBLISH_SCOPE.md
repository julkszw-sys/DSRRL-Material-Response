# V275 — exact native synthetic selector source nonpublication

## Why this change (canonical prerequisites)

Before construction, Supabase revision 12964 was checked. The following
findings were already **CONFIRMED** and are *not* rediscovered here:

- `renderer.pmetal.re_alternate_builder_synthetic_zero_m10_misclassification_20261009_v1`, revision 12179: original retail DSR alternate builders at `0x140217370` and `0x140217970` produce a zero-packed A/B/beta descriptor without the authentic LightBank producer, and the shared `0x14020E9D0` callback may arrive at `0x20EB7F`. A return RVA alone is NOT a reason to reject every draw; actual area0 is legitimate.
- `renderer.cross.envspec_classic_part_selector_algorithm_homology_v1`, revision 4237: ordinary Classic selector encoding across PTDE↔DSR is already proven homologous at the algorithm level; authored data equality remains separately open.
- `renderer.hemenvlerp.v273_ptde_c84w_c85w_common_selector_t_valid_interior_v1`, revision 12961: PTDE c84.w/c85.w share t for the verified valid interior blend. Do not split beta arbitrarily.
- `renderer.hemenvlerp.v272_owner_rx33_positive_beta_draw_route_v1`, revision 12958: the active old R19 transport reaches eight interior beta bands and stages one host beta into both b12 weights. New RX shader still dormant.

## Actually new in V275

The production `pmetal_selector_policy.hpp` already declares
`is_synthetic_zero_descriptor(ret_rva, raw_a, raw_b, beta)`, but the
original `pmetal_env_source_selector_event` does **not call it**.
Therefore V275 applies a temporary, deterministic source overlay **inside
the existing single .addon64 build**, immediately after the actual owner and
character validation and before exact selector-state publication.

Only the following complete predicate is suppressed:

```
return_site == 0x20EB7F
raw_selector_A == 0
raw_selector_B == 0
beta == 0.0f
```

The real `0x20E019` full-builder path is **not** excluded even for a valid
area0/row0/zero-beta LightBank. Nonzero A, B, or beta through `0x20EB7F`
is also not excluded, and the other return `0x20FB9E` remains unmodified.

The selector callback has already called `pmetal_producer_state_begin`;
the synthetic candidate therefore does not publish a new
`pmetal_producer_state_publish`. Crucially this is **NOT** a claim of total
draw fail-open: `pmetal_env_source_runtime::latest()` may still use the
independently hooked `latest_hook_source` fallback. That source must
be separately proven or scoped in the next step before claiming no false
LightBank reaches the consumer.

## Reproducibility

- Build V275 from the current branch, using the inherited exact V258–V272
  GPU shaders, native Win64 ReShade addon compiler and host state transactions.
- `apply_v275_nonpublish_synthetic_selector.py --apply` is called only from
  the generated temporary builder; the overlay's source idempotency checker
  must pass, and the original tracked source file is restored in `finally`.
- `test_v275_synthetic_selector_policy.cpp` compiles against the real
  production policy header and checks eight positive and negative cases.
- GitHub CI refuses successful construction unless the one-addon binary
  contains `[DSRRL V275 SYNTHETIC ZERO SELECTOR NONPUBLISH]`, and all
  original modified source files return to exact tracked bytes.

## Runtime falsification

A user runtime hit should include
`[DSRRL V275 SYNTHETIC ZERO SELECTOR NONPUBLISH]`.
It confirms that this one owner+return+tuple is suppressed.
A missing hit does not mean the synthetic case never occurs (the exact
material and event must co-occur during gameplay). Check the existing
`P_Metal` receiver and `R19` draw telemetry separately. Assess
`latest_hook_source` fallback and actual source bank/row identity before
claiming elimination of the m10-on-m18 symptom.

Do NOT activate dormant V270 RX pixel shaders, globally overwrite
`source.beta`, touch executable bytes, rewrite PTDE k135, or infer
pixel equivalence from this builder-level input policy.

**Construction ≠ runtime liveness ≠ bridge activation ≠ PTDE-visible pixels.**
