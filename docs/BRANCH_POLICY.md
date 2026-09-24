# Branch and release policy

## Principle

Git history is evidence. Do not delete, squash away or silently rewrite superseded renderer implementations merely to make the repository look cleaner.

## Roles

`main` is release-facing and stable.  
`develop` is integration.  
`recovery/*` preserves or reconstructs historical source/provenance.  
`dev/*` is implementation/experiment work.  
`release/*` is reserved for source-complete RC/release material.

## Source-complete release gate

A renderer/addon build cannot be promoted to RC or RELEASE unless the exact producing commit contains or deterministically materializes:

1. all handwritten C/C++/ASM/Python/PowerShell source;
2. all generators and required small generated tables/headers, or deterministic generators for them;
3. SHA-256 identities for every external binary/data input;
4. a deterministic build command/script;
5. a build audit with output SHA-256 and relevant PE/DXBC metadata;
6. the source commit SHA;
7. parent and rollback lineage;
8. feature/invariant declarations sufficient to distinguish construction, compatibility, runtime liveness, activation and pixel status.

A previous `.addon64` may be retained as historical evidence, a compatibility baseline or an explicitly declared external input. It may not be treated as the canonical implementation of a newly promoted build.

## Recovered historical source

Byte-exact recovered originals belong under a recovery namespace and are immutable. Corrections or modernizations go into a new implementation path; never edit provenance snapshots and then continue to call them the historical originals.

Reconstructed source must be labeled `RECONSTRUCTED` until it is independently validated against the known binary/behavioral evidence.

## Renderer-specific requirements

For each bridge/operator, source promotion also requires verified receiver/material/resource routing and fail-open behavior. Build success alone does not promote pixel equivalence.
