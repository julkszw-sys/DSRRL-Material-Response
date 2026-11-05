# V441 original V439 model SHA asset crosscheck — bounded negative

Source: *user-provided* `DSRRL_FINAL_EVIDENCE_PART04(2).zip`. No original game data added to GitHub.

Live V439 RX34 model candidate `0xE979970` had FLVER SHA-256 `ed0a79b086f1cff26005e0d2c184e269b532046d7cfb83d1eda3bcc3911ac6cc`. Native parser hashes FLVER header `u32[+0xC] + u32[+0x10]` bytes from FLVER pointer, not just a guessed resource name.

A read-only scanner decompressed DCX, verified original FLVER signatures, parsed exactly 151 BND3 archives (`.partsbnd.dcx` and `.chrbnd.dcx`) plus direct map FLVERs, and compared SHA256 of precisely that native parser-length byte interval for **851 map FLVERs + 158 extracted parts/character FLVERs = 1009 candidates**. Every FLVER passed format/length validation; zero SHA matches, zero errors. Repro tool `renderer-core/tools/verify_v441_v439_flver_asset_sha_census.py`.

**Conclusion restricted to this supplied evidence subset:** the exact candidate model is not present as raw FLVER with this digest among the 1009 checked assets. This is **NOT** proof it is not armor, not evidence of a renderer mismatch, and not proof that the game lacks the file; incomplete asset coverage, runtime parser representation or game state remain possibilities. A matching FLVER or exact live parser identity is needed before calling model `0xE979970` the player's armor. No canonical bridge authorized.

Other V441 static findings (actor classes, channels 2/3 and source identity census) are in `V441_TWO_ACTOR_THREE_CHANNEL_COMPLETE_STATIC_RE.md`.
