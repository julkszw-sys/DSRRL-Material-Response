# Renderer Core v1 — HemDir3 semantic mode2 boundary

Status: **HIGH CONFIDENCE carrier/routing boundary; exact producer cut OPEN**  
Canonical: `renderer.core.islands.hemdir3_semantic_mode2_source_boundary_v1` (rev 8936)

## Scope

This contract belongs only to the Renderer Core v1 `lightbank.hemdir3` island. It does not authorize ordinary-player activation, whole-shader equivalence, runtime liveness, or pixel equivalence.

## Capture-free source -> downstream chain

1. **Source / producer** — the island requires the *effective* lighting semantic mode `2` (HemDir3). The current supported DSR selector observer at retail RVA `0x22BA20` observes selector-entry state; it does not by itself recover the effective selector result. The exact producer/observation cut for eligible mode2 draws remains OPEN.
2. **Negative routing boundary** — ordinary PlayerIns traced routing is already known to produce only selector values `{0,1}`. Therefore ordinary player armor must not be promoted to HemDir3 merely from material `g_LightingType`, retained HemDir3 payload identity, or D123 availability.
3. **Transport** — once exact mode2 is proven, carry it as an immutable semantic-mode snapshot. U/L + D1/D2/D3 continue to use the existing Renderer Core `b13[0..7]` semantic carrier; mode identity is not inferred from those values.
4. **Consumer** — activate only an exact compatible Phn receiver route and suppress/replace the host HemEnv EnvDiffuse source branch. HemDir3 and EnvDiffuse are mutually exclusive source modes at this join.
5. **Composition** — the local PTDE source join remains `H + sum_i max(-dot(N_final,L_i),0) * C_i`, followed by the existing material continuation.
6. **Downstream** — material-domain, post-Fog and atmosphere compatibility remain independent readiness gates. Existing terminal behavior is not evidence of source-mode routing.
7. **Fail-open** — missing/ambiguous effective mode2 identity, incomplete b13 carrier, unverified receiver, unsuppressed host EnvDiffuse, or incomplete downstream continuation preserves the host route.

## Narrowest carrier

`exact effective-mode2 observation -> immutable semantic-mode snapshot + existing b13[0..7] -> draw-local HemDir3 source-mode transaction`

Do not infer mode2 from shader name, retained binder payload, material family, `g_LightingType`, or nonzero D123 values.

## Remaining blocker

Recover the exact effective selector-mode2 producer/observation point for eligible non-player/special draws on the supported executable, without requiring new runtime capture. Until then the island remains feature-OFF / fail-open. Runtime activation and PTDE-visible pixel equivalence remain OPEN.
