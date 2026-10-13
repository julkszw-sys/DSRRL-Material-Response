# V260 — material-workflow consumer fork in real DSR HemEnvLerp

**Status: targeted reverse engineering. No V260 playable addon and no shader patch.**

The goal is PTDE-visible HemEnvLerp on sidecar-backed armor/equipment
material slots, **not** only P_Metal. Stock DSR Fog and LightScattering
remain untouched. This investigation deliberately does not edit
`latest_hook_source`.

## Verified original DSR DXBC

The stock RX33/RX34/RX35 HemEnvLerp shaders are each authenticated by
their exact SHA256 in `verify_workflow_branch_dxbc.py`. Each has a unique
`if cb0[85]` with two executable material-response arms:

| receiver | IF start | ELSE start | ENDIF | IF instructions | ELSE instructions |
|---|---:|---:|---:|---:|---:|
| RX33 | 1081 | 1292 | 1394 | 26 | 14 |
| RX34 | 990 | 1201 | 1303 | 26 | 14 |
| RX35 | 650 | 861 | 963 | 26 | 14 |

These are original **SHEX DWORD offsets**, not byte or runtime virtual
addresses. Both arms contain their own references to `cb0[9]`,
`cb0[10]`, and `cb0[100]`.

## Confirmed code asymmetry

In `pmetal_rgba_lerp_materializer.cpp`,
`apply_exact_ptde_lerp_materialworkflow_r15_r16_r17` stops modifying
the instructions when it reaches top-level ELSE
(`in_else=true`).
It modifies R15/R16/R17 sites in IF and a separate post-merge gate
located downstream of ENDIF. **It does not rewrite the 14-instruction
ELSE material operator.** This is source-level proof of incomplete
branch-wide operator reconstruction, not proof of a rendered regression.

## Original DSR executable producer fact

The UTF16 `g_MaterialWorkflow` name is at file offset 19936120 of
the uploaded exe. Native xref `0x140293538` loads that exact string
into material property lookup call `0x1402928b0` with type 8 and
stores the decoded material value at offset `material+0x220`.
**The graph from that CPU property to the actual live GPU `cb0[85]`
has not been established.** We do not assume MTD default equals GPU
branch selector, and must not force this branch.

Authored PTDE MTD lacks g_MaterialWorkflow for P_Leather and
Ps_Body; vanilla DSR adds the property with default 0. P_RoughCloth
has DSR-only value 1. These MTD differences are confirmed source
facts, NOT a live shader branch proof.

## Required semantic cut before an authorized fix

1. Trace the DSR per-draw producer: MTD effective value/FLVER override
   to cb0 byte offset 85*16 and confirmed RX33–35 consumer selection.
2. Verify separate IF vs ELSE live-out register semantics and
   compare against the exact PTDE DifSpcBmp HemEnvLerp material
   operator (SpecTex RGB, c101, DiffuseMap+c156, c100).
3. Only then implement receiver+MTD+slot+sidecar-qualified DXBC patch
   for the mismatching consumer arm. Preserve original stock DSR
   Fog/LightScattering 212-DWORD opcode block (V259 hash) and
   all independent DSR systems.
4. Fail-open on unknown producer, CB/ISGN, material FLVER identity,
   sidecar asset checksum, or resource slot. Separate build,
   active-draw and pixel-behavior results.

## Reproduce

```sh
python renderer-core/tools/v260/verify_workflow_branch_dxbc.py \
    --donors <directory-with-RX33.dxbc-RX34.dxbc-RX35.dxbc> \
    --exe <read-only-original-DarkSoulsRemastered.exe> \
    --out v260_branch_audit.json
```

No original game DXBC/EXE is committed in this PR. An independently
executed verifier on the owner-supplied stock DXBC produced a PASS for
all three RX and wrote the audit JSON. Neither build nor code analysis
can establish PTDE pixel equivalence without operator-local GPU
signals and visual comparison.

**Do not create another dummy addon to confirm dormancy.**
