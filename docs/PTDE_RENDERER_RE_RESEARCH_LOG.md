# PTDE Renderer Full Reconstruction — RE Research Log

Branch: `dev/ptde-renderer-full-reconstruction`

## Persistence policy

Primary knowledge store: Supabase `dsrrl`.

If Supabase bootstrap, evidence submission, finding promotion, schema access, or control-plane access is unavailable or fails in a way that blocks research, **research must continue** and be persisted here immediately.

Fallback protocol:

1. Append raw RE evidence here first.
2. Preserve exact provenance: binary/artifact, SHA-256 when known, addresses/callsites, shader/resource identities, method, and confidence.
3. Separate observation from interpretation.
4. Use project statuses only: CONFIRMED / HIGH CONFIDENCE / HYPOTHESIS / OPEN / REJECTED.
5. Do not overwrite or delete superseded results; append falsifiers and mark old models REJECTED/SUPERSEDED.
6. Once Supabase is available again, backfill in order:
   - `dsrrl.submit_evidence(...)`
   - `dsrrl.propose_finding(...)`
   - `dsrrl.promote_finding(...)` when justified
   - producer/operator mapping tables
   - revision/bootstrap verification
   - `dsrrl.knowledge_hygiene_audit()`
7. GitHub fallback notes remain as audit history after backfill.

## Scope

Target: reconstruct PTDE renderer producers/operators on the DSR host, excluding DSR-added SFX that are intentionally retained.

Required model for each operator:

`source -> producer -> transport -> consumer -> composition -> postprocess -> pixel`

Producer/CB/resource equivalence is not pixel equivalence.

## Current branch state

Created from `nexus-review/material-response-1.45-source`.

### CONFIRMED — PTDE ShaderConstant producer ABI census

Direct binary RE of canonical PTDE `DATA.exe` recovered registered `ShaderConstant_*Entity` factory/getter/constructor/vtable chains and entity-defined managed-block footprints:

| Producer | Size |
|---|---:|
| CameraMtx | 0xA0 |
| ClipPlane | 0xC0 |
| DirLight | 0x3B0 |
| DofParam | 0x460 |
| Fog | 0x90 |
| LightScattering | 0xF0 |
| LocalWorldMtx | 0x780 |
| LocalWorldMtx_WithPrev | 0xEE0 |
| Normal2AlphaParam | 0x70 |
| PointLight | 0xE0 |
| ToneMap | 0x70 |
| Water | 0x1B0 |
| WorldViewClipMtx | 0xE0 |

Important: these values are **not yet claimed as GPU CB sizes**. RE already shows the virtual slot does not describe the object instance size itself. Its exact managed-block semantics remain under investigation.

### CONFIRMED — producer-class set divergence

Normalized class census:

PTDE-only:
- `ShaderConstant_ClipPlaneEntity`

DSR-only:
- `ShaderConstant_DeferredLightingEntity`
- `ShaderConstant_GlowEntity`
- `ShaderConstant_WindParamEntity`

This proves compiled producer-class divergence only. Runtime activation and visible responsibility are still OPEN until consumer/routing RE closes them.

### CONFIRMED — HemDir3 stock DSR execution graph issue

DSR retains HemDir3 shader payloads, but current stock shader registration/binding evidence indicates the family is orphaned/unreachable in the audited ordinary stock PHN execution graph. Therefore restoring PTDE HemDir3 behavior may require restoring the execution/receiver path as an operator island, not merely reproducing LightBank producer values.

## Current RE target

Resolve the semantic meaning and downstream consumer of the producer virtual slot that returns large PTDE blocks (e.g. PointLight 0xE0, DirLight 0x3B0, ToneMap 0x70) but often 0x20 in DSR. Do not label it constant-buffer size until its allocator/copy/upload callsites prove that interpretation.
