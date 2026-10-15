# Owner V278 runtime negative A/B, 2026-10-15

Original ReShade log SHA256 `eb201176129ff8c8c22ee1d2b0ca65f28913bd8c1c4c01ad91a6f3608c822cf6`, 1,534,086 B / 2,648 lines.

Native addon V278 build `29b3c56b8a525421c55c2927646299c41d3b99ee` loaded. Lerp R5 applied on create-time RX33 (22,608 B), RX34 (22,308 B), RX35 (20,680 B), no fallback. The V277 DXBC capture executes BEFORE the V278 R5 insertion, so that logged bridge capture is not the final R5 shader.

Diagnostic V275/V276 shows 216 Lerp batches and 216 draw-issued transactions (RX33=168, RX34=48), zero Lerp prepare failures, twelve matched exact MTD+FLVER+slot tuples between Stable and Lerp. All Lerp batches planned replacement PS, PTDE t10 and PTDE t12. V251 also witnesses before-draw PS/CB/SRV/sampler presence. RX35 is not drawn in this session. No Cloth.

Owner reports NO visible reduction of laminate with V278. R5 therefore failed as a sufficient visual fix in this owner test despite create-time application and draw route witnesses. This does NOT negate the source-confirmed Stable/Lerp normal difference. GPU normal and preFog and PTDE pixel remain OPEN. Next test must contrast visually GOOD/BAD material first Lerp-only divergent consumer.

No production change or new canonical R5 patch authorized.