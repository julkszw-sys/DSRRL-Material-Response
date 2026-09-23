# Source-completeness policy

This policy is mandatory for Expanded A3 and all later renderer-runtime builds.

## Rule

A DSRRL runtime binary MUST NOT be promoted to RC or RELEASE unless the exact code
and deterministic build path used to produce it are committed to this repository.

A release is source-complete only when the repository contains:

1. all handwritten C/C++/ASM/Python/PowerShell source used by the runtime;
2. all generator source;
3. all small generated headers/tables required by the compiler, or a deterministic
   generator plus immutable input identity that recreates them;
4. a machine-readable input manifest with SHA-256 for every external binary/data
   input;
5. a build script/command that produces the runtime from those inputs;
6. a build audit containing output SHA-256, PE metadata and feature flags;
7. the exact Git commit SHA embedded in or adjacent to the build metadata;
8. rollback/parent lineage.

## Forbidden

- shipping a runtime whose behavior only exists in an uncommitted local file;
- treating a final DLL/addon as the canonical implementation;
- satisfying the implementation-source requirement by listing the shipping 1.45 addon as an external input;
- modifying a binary manually without committing the deterministic patcher;
- depending on a chat attachment without recording its content hash and role;
- promoting a build when generated payloads cannot be reconstructed;
- overwriting history to make a later implementation appear to be the original source.

## Generated data

Large generated shader payloads may be stored as compressed immutable snapshots to
keep the repository reviewable, but the repository must also contain:

- the generator;
- the decompression/materialization step;
- the uncompressed SHA-256;
- provenance of the exact source shader/input artifact.

Generated payload snapshots are never the only source of semantic truth.

## CI / release gate

Future release tooling must fail if any of the following are missing:

- SOURCE_MANIFEST.json
- BUILD_AUDIT.json
- source_commit
- exact input hashes
- deterministic build command
- feature/invariant declaration

## 1.45 historical note

Shipping Material Response 1.45 remains the immutable compatibility basis, but its
public review branch reconstructs the final release from an earlier integrated binary
basis rather than containing the full original implementation source for every bridge.
Expanded A3 must not repeat that source-availability limitation.

Therefore the shipping 1.45 addon may be used as a compatibility oracle, byte-diff reference or behavior baseline, but **not** as the implementation basis that makes A3 source-complete. The current A3 branch remains source-incomplete until the monolithic implementation itself is committed and builds without inheriting opaque renderer behavior from the shipping addon binary.
