# A1 create-time materialization

The recovered exact A1 recipes are now consumable through a create-time
materializer without trusting shader names or 64-bit fast hashes.

## Identity cut

The public entrypoint first rejects non-candidate code sizes, validates the
DXBC container shape, computes SHA-256 over the complete source blob and
selects only an exact original identity from the certified 144-plan corpus.

A caller-provided shader name, receiver guess or approximate hash cannot
authorize a patch.

## Independent island gates

Only the five closed shader islands represented by the exact recipe corpus are
eligible:

- terminal RGB SAT;
- diffuse material-domain;
- PntS attenuation;
- certified no-Spc EnvSpec deletion;
- fixed post-Fog identity.

Each island remains independently controlled by the Renderer Core
`feature_registry`. Disabled owners leave their DWORDs untouched.

## Transaction and checksum

The source DXBC buffer is immutable. Before allocating/exposing a replacement,
the materializer validates every DWORD belonging to every enabled owner.
Unexpected words fail open before any replacement is returned.

A successful subset is copied into a separate replacement buffer, patched,
and passed through the audited P2.2 legacy DXBC checksum algorithm.

If all operations of a historical plan are enabled, the final replacement
SHA-256 must also equal that plan's certified `replacement_sha256`; otherwise
the replacement is discarded and the path fails open.

For a partial per-island subset there is intentionally no invented target SHA.
The resulting SHA is returned for cache/telemetry, while correctness remains
defined by exact input identity, exact selected DWORD recipes and checksum.

## Status boundary

This closes the create-time byte-materialization primitive. It does **not** yet
register a ReShade `create_pipeline` callback and therefore does not promote
runtime liveness, receiver-hit telemetry, bridge activation or pixel behavior.
The existing Phase-0 addon remains pixel-inert.
