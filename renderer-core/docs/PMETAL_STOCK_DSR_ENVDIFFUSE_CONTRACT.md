# P_Metal stock DSR EnvDiffuse candidate

Status: **CONSTRUCTION CANDIDATE — runtime activation and pixel behavior OPEN**

## Intent and receiver selection

This candidate is derived from the pinned V359-RC1 source lineage. It keeps separate create-time payloads from the same verified receiver shader, but the runtime scope of this candidate is **exact P_Metal only**:

- **Exact P_Metal** — route 345, authenticated P_Metal MTD identity, receivers 33–35: stock DSR EnvDiffuse consumer instructions and native t11/t13 resources/samplers remain untouched.
- **All non-P_Metal aliases fail open for this bridge** — the candidate builder explicitly disables the experimental V285/V289/V293 nonmetal alias gates. `exact_pmetal_envspec_candidate` therefore rejects these draws before the P_Metal EnvSpec sidecar/source transaction, leaving this bridge fail-open to stock DSR. The dormant default PTDE-EnvDiffuse payload is not reachable through nonmetal aliases in this build.

The runtime selects the stock payload only after the exact P_Metal material and route decision both pass. If the stock payload is absent or any identity/receiver/resource/transaction check fails, that draw fails open to stock DSR. This scope is local to the P_Metal EnvSpec/EnvDiffuse bridge; unrelated renderer operators retain their own independent routing.

## Operator changes

- **EnvDiffuse on exact P_Metal:** no PTDE EnvDiffuse consumer rewrite; no PTDE EnvDiffuse SRV/sampler binding at t11/t13; no EnvDiffuse resource or transaction ownership. Stable b12 stays 64 bytes. HemEnvLerp retains the original t11/t13 A/B+beta block byte-for-byte and keeps b12 at 64 bytes rather than extending it solely for EnvDiffuse endpoints.
- **EnvSpec:** keep the existing PTDE EnvSpec sidecar at t12/t14 and the existing source A/B+beta bridge.
- **Material Response / SpecRGB / MaterialWorkflow / terminal SAT:** preserve the existing exact material/owner gates and cumulative operator islands.
- **Other operators:** keep R3/R5/R6/R7/R11 and the existing independent PointLight, Upper/Lower, Subsurface and atmosphere/fog behavior unchanged. No global renderer rollback is introduced.

The stock P_Metal path consumes only the authenticated EnvSpec source. In this build, the PTDE EnvDiffuse donor is not loaded, the EnvDiffuse linear carrier is explicitly invalid, and neither hook publication nor selector publication waits for EnvDiffuse endpoint decode. EnvSpec source-cache identity excludes EnvDiffuse fields in stock mode, so an unrelated EnvDiffuse change cannot invalidate the EnvSpec carrier. Lerp remains guarded by exact receiver identity, feature gates, EnvSpec source/probe validity, and registered replacement availability. Missing or mismatched EnvSpec state fails open. Non-P_Metal alias routes are disabled in this candidate rather than being granted a broad exception.

## Pipeline and remaining semantic limit

At the P_Metal EnvDiffuse semantic cut, the candidate leaves DSR's t11/t13 consumer instructions, resources and samplers unchanged. It does **not** claim that the final EnvDiffuse pixel is bit-identical to vanilla DSR: the shared R5 normal-basis bridge remains an input to the environment consumers, and existing R3/R11 changes remain downstream. Those operators are intentionally retained to preserve the rest of the mod; splitting the shared normal basis so stock EnvDiffuse also receives the unmodified DSR normal would be a separate shader-operator change requiring its own exact census and proof.

## Validation status

The source-contract test and native Windows single-addon build establish construction and compile-time invariants only. The builder additionally asserts that V285/V289/V293 nonmetal alias islands remain disabled for this P_Metal-only candidate. Runtime liveness, receiver hits, bridge activation, and PTDE-visible pixel behavior are separate states. Pixel equivalence remains OPEN until existing project evidence can prove it; no new user runtime capture is assumed or required for this construction step.
