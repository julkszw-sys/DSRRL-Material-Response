# V422 — Native LightBank producer-to-consumer static reverse engineering
Date 2026-10-30. Original retail DSR SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b; PTDE/DATA.exe original SHA256 88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d. No runtime claims, no new addon, no shader/PARAM intervention, frozen V359-RC1 and R43 intact.

## Exact retail native DSR producer

0x2200D0 creates a stack draw packet [RBP-30h]. 0x220256 calls 0xC04A0, a pure WORD getter of state +0x110; at 0x220265 the returned WORD is stored to packet +0x4C (selector A). 0x22026E calls 0xC0550, a pure WORD getter of state+0x112; 0x2202A5 writes packet+0x4E (selector B). 0x220273 reads float state+0x114, 0x22028B stores packet+0x50 beta. 0x220CE6 passes the stack packet via RDX; 0x220CED calls model vtable slot +0x18, which equals 0x14020DDA0 for attested FgFlverModel and FlverModel vtables. Draw method moves RDX into R15 at 0x20DDE4 and reads FrpgDrawContext at [R15] 0x20DDFF. Packed selector words are read directly at 0x20DF70/+0x4E and 0x20DF86/+0x4C. Material selector 0x22BA20 is only called later at 0x20E014. This is a verified producer->packet->native draw input chain for those classes, NOT a general rule for other draw families.

0x1D7180 packs area/row positive values as ((area&0x7F)<<8)|(row&0xFF), negative returns 0xFFFF. Thus 0x051E = area5 row30 and 0x051F = area5 row31.

## Upstream temporal writer candidate (NOT connected by instance proof)

RVA 0x56F4D0 invokes 0x56E960 to get RBX target state, computes normalized temporal phase from a time interval/input, encodes six raw WORD IDs using 0x1D7180 (or signed-ID fallback helpers), stores pairs at RBX+0xF0/+0xF2, +0x100/+0x102, +0x110/+0x112 and beta at +0xF4/+0x104/+0x114. The target RBX returned by 0x56E960 has NOT been proven identical to RDI state read by 0x2200D0. No direct call rel32 to 0x56F4D0 was found; do not call it active scene producer without pointer/callgraph evidence.

## Two distinct operator consumers (important)

Within native shared FlverModel::draw, XMM6 is set zero at 0x20DE73; 0x20E2FE reads float from *packet*+0xE0, 0x20E30C reads byte packet+0xF0. If E0 < 0 (or unordered) and F0==0, 0x20E342 invokes 0x1EADE0. Otherwise 0x20E38B invokes 0x1EB320.

For full builder 0x2200D0, 0x22036A writes packet+0xE0 from state+0x168 or literal -1.0 (DSR constant RVA 0x1615C88). 0x220384 sets packet+0xF0 to zero. Thus producer controls the consumer branch. Historical V413 log has no E0/F0 and cannot assign row30 or row31 to either branch.

0x1EADE0 decodes packed area/row, looks up LightBank through 0x569B40 with area11/type5 fallback and has separately structured A==B and A!=B processing. 0x1EB320 caches four native source fields in FrpgDrawContext +0x2348/+0x234A/+0x234C/+0x2350. At 0x1EB479..0x1EB4A3 it tests packed A and B plus extra arguments; on difference 0x1EB539 updates the key before re-resolving source. This native cache is distinct from addon R43 and different packed row30/31 cannot be considered the same 16-bit cache key in this branch. Neither function is automatically PTDE-equivalent or global.

The 0x217370 builder is an alternative native family routing to 0x20E9D0, NOT the same 0x20DDA0 call. Another 0x220835 path calls 0x20FEE0 conditionally before the virtual draw: pass semantics OPEN.

## PTDE origin and verification boundary

PTDE original PE32 DATA.exe was extracted and SHA verified from the project's existing reference archives. Candidates 0x636D37 and 0x92E5DA containing 16-bit <<8 have been inspected: bit-flag mutation and compact numeric-field packing respectively; neither is proven homologous LightBank. No PTDE producer->packet->pixel operator chain is established, so pixel behavior and exact PTDE carrier mapping OPEN. Negative byte-pattern result is not proof of semantic absence.

A separately generated Python verifier pins 45 exact DSR opcode preimages, six call targets, both native vtable slots, single verified direct 0x2200D0 callsite and the literal -1.0, plus both original EXE SHA values. It is in the conversation handoff, not checked in here.

## Verification hierarchy and next *static* tasks

1. Enumerate writers of RDI+0x110/0x112/0x114 and prove the actual object identity for each native receiver. Reconstruct virtual dispatch into 0x56F4D0 if any.
2. Inspect 0x220835 prepass and the alternate 0x217370/0x20E9D0 draw family separately; no shared-context assumption.
3. Trace downstream of 0x1EADE0 and 0x1EB320 to exact shader, constant-buffer and SRV consumers, with material/receiver census.
4. Find the actual 32-bit PTDE semantic homolog and compare equations/encoding/fallback/temporal behavior to DSR, avoiding arbitrary LightBank patches.
5. Only once static receiver/callgraph information is exhausted, use minimal runtime to certify per-instance packet and PS/CB/RTV/pixel attribution.

Status: DSR native packet copy/encoding and dual consumer branch CONFIRMED static; temporal writer-to-live instance OPEN; PTDE homolog OPEN; runtime E0/F0 OPEN; PTDE pixel OPEN.