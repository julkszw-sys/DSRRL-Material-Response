DSRRL Material Response 1.3 — V12 RECEIVER ORDINAL FIX RC

Purpose
=======
Fix the Normal/Diffuse bridge receiver gate proven wrong by V11 runtime + static RE.

Root cause
==========
TLS block+0x14 is a LOCAL metadata-table ordinal used directly to index the Material Response receiver metadata table. It is not the canonical receiver_registry index 24..47.

The old V8/V10/V11 bridge gate compared this local ordinal against canonical 24..35, so ordinary equipment draws failed open before exact SRV lookup.

Verified local table partition
==============================
  local 0..22  : DifSpcBmp HemEnv/HemEnvLerp ordinary receiver records -> PASS
  local 23..46 : DifSpc non-Bmp records -> REJECT for this bridge scope
  local 47     : inherited gap/non-record -> REJECT
  local 48..50 : separate Subsurf operator-island records -> REJECT here
  local >=51   : invalid -> REJECT

No receiver-family widening is performed. Normal and Diffuse retain the proven ordinary Bmp scope. Exact SRV identity, safe tuple/pair census, sidecar readiness and draw-local bind/restore remain unchanged and fail-open.

Runtime status
==============
CONSTRUCTION: PASS
STATIC COMPATIBILITY: PARTIAL (known appended/JMP-continuation unwind gap retained)
RUNTIME LIVENESS: OPEN
BRIDGE ACTIVATION: OPEN
PIXEL BEHAVIOR: OPEN

Expected next-stage markers after the fixed receiver gate, if routing continues:
  [DSRRL][ASSET_DIAG_RX] ... local 0..22 DifSpcBmp PASS
  [DSRRL][ASSET_DIAG_SAFE] ... exact SRV lookup PASS
  then either safe tuple/pair REJECT or existing GATE / SIDECAR / BIND markers.
