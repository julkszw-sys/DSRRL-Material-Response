# V426 native LightBank type5 static RE (2026-10-31)

Source: original PTDE DATA.exe and DarkSoulsRemastered.exe, SHA-locked in handoff. No runtime and no lighting patch.

DSR 563B80 handles raw record+40/+48 at draw time through 564510 (abs, powf 2.2), while V425 563590 loader also precomputes other type5 vectors. PTDE FF1600 reads same groups using signed linear response. Both emit four vectors duplicating the two color sources. DSR 1C69FE -> 563B80 writes FrpgDrawContext+1240..1270. GPU CB/PS association OPEN.

PTDE blend FF1850 uses 4BBF70 at FF1930/1953/1978/199D. DSR blend 563C30 uses 564510 at 563D4E/65/7D/95. Both produce separate A+40,A+48,B+40,B+48 endpoints with beta in W; no final CPU lerp in those functions. Shader composition OPEN.

PTDE FF1600 -> 424A40 -> F94260 stores color in CPU backend shadow+15F0, marks dirty logical 0x56 via 422D40. PTDE 424440 -> F94380 stores three direction colors in shadow+1650 / +1680 and logical dirty indices 5C..61. These are not certified D3D9 register IDs. DSR context fields 1240..12B0 cannot be equated to GPU CB by raw offset search.

Evidence: exact original binaries, local V426 verifier 48 instruction preimages and 21 CALL edges PASS; full report and verifier in handoff DSRRL_V426_TYPE5_STATIC_RE_HANDOFF_2026-10-31.zip.

Open: exact receiver/MTD, asset homology, PTDE D3D9 and DSR D3D11 upload, shader equation, pixel behavior. Do not invert gamma globally. R43 Source2, V359-RC1 and EXE unchanged.