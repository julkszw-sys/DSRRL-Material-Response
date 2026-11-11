# V463 — typed WorldChrManImp actor model handle → native RX34 source (manual bounded)

Retail EXE: SHA256 a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b, unmodified.

## New native static RE

WorldChrManImp is an authenticated RTTI type, COL RVA 0x164B700, TD RVA 0x1B00C08, vtable RVA 0x1328210. Global singleton slot RVA 0x1C77E50 is initialized at 0x27BF6D by ctor 0x36D8D0 (0x1F30 bytes allocation). WorldChrManImp+0x68 is cleared during ctor 0x36D938 and loaded by game at 0x24EEDF→0x24EEE6, whose returned pointer receives a ChrIns-family method call 0x321B70. Another caller 0x130A71..0x130A89 calls virtual slot+0x5B0 on the same candidate object. PlayerIns vtable0x13251F0 slot+0x5B0 is 0x360D80; EnemyIns slot is 0x331210 returning zero. This is a plausible player-pointer candidate, NOT a static proof that field +0x68 always stores PlayerIns. V463 uses exact dynamic RTTI to test.

PlayerIns exact MSVC vtable RVA 0x13251F0 COL0x164AD48; ChrModel vtable0x12EAF30 COL0x163CAA8. ChrIns+0x60 model field known (PlayerIns writer status still open, EnemyIns ctor writes it). ChrModel is FrpgModel derived and FrpgModel+0x10 carries the registration handle. V462 proves the full 32-bit model handle travels through native opcode1 creation to matching FrpgModelDrawEntity+0x10. This includes high-byte generation; low24 alone is NOT identity.

Original retail static verifier: 52 instruction witnesses, 8 exact E8 and RTTI, 56/56 negative mutations PASS. Native rendering paths and original EXE unchanged.

## Single integrated addon: consequential owner/handle test

V463 layers on source-reproducible V439 V443 V447 V448 V450 V451 V459. Replaces previously redundant V459 Light-pair reads in existing V451 kind21 issued RX34 event; STILL manual F9 up to 8s, capped at 1024 events and ONLY RX34 route345 row30/31. No new EXE hooks, draw replay, shader/CB change or row modification. Same one addon.

V463 kind21 payload v00..v16 unchanged; v17..v23 semantics are NEW (must NOT parse as V459):
- v17 status: bit1=issued entity full handle readable; bit2=exact typed WorldChrManImp; bit4=world candidate actor pointer/vtable readable; bit8=exact PlayerIns vtable; bit16=exact ChrModel vtable from PlayerIns+0x60; bit32=player model full handle readable; bit64=complete 32-bit handle equal between player ChrModel and issued RX34 entity.
- v18 issued FrpgModelDrawEntity+0x10 full handle, v19 candidate actor pointer, v20 actor vtable, v21 PlayerIns+0x60 model pointer, v22 player model handle+0x10, v23 typed WorldChrManImp pointer.

Offline analyzer: renderer-core/tools/analyze_v463_playerins_rx34_owner.py. Full 32-bit comparison, including generation. The synthetic test distinguishes 0x8100007a player handle from 0x8200007a nonmatch even though low24 same; nonplayer != NPC proof. Unknown or wrong owner vtable => INCONCLUSIVE, not a game eligibility gate.

## Meaningful runtime falsifier

One bounded capture in original sun-player / shadow-NPC case, press F9 once for 8s, optionally F10 when flicker; supply latest %TEMP%/DSRRL_V438/V438_*.csv and ReShade.log. If status127 with row30, player model exactly emitted row30; if separate typed full-handle comparison has status63 and row31, row31 is NOT emitted by same PlayerIns ChrModel. This STILL does not prove row31 is enemy/talkable NPC or contaminates same GPU pixels. Other status combinations indicate missing identity or unexpected routing; never force a row.

Status: STATIC_RE PASS, source/CI compile separately verified, V463 runtime/owner attribution still OPEN before actual game test, GPU/pixel PTDE behavior OPEN. V359-RC1 frozen active development base, Source1/2, R43 and original executable remain unchanged.
