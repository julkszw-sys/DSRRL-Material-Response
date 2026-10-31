# V425 — PTDE type5 linear decoding vs DSR preload/prebake | 2026-10-31

**RE-only, no addon mutation.** Original PTDE/DATA.exe PE32 SHA256 `88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d`; DSR PE32+ SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Original EXEs unchanged. No claim of shader/pixel equivalence.

## Confirmed original type5 operator split

PTDE active draw route `F957F4 -> FF1A20`, format WORD [block+8]=4, row offsets at [block+34+12*row]; three directional records at offsets 0,C,18 are decoded by `4BBC60` at direct callsites `FF1A6D/FF1A8B/FF1AAA`; RGB components at +4,+8,+A for each group scaled by signed WORD gain +6 using 0.01 and /255. PTDE uses float/double conversions, *no abs/powf in this local decoder*. PTDE consumer F95730 also has further transforms; final GPU input is OPEN.

DSR **precompute** `565142 -> 563590` builds expanded rows of **stride 0x110**, separating directional angle conversion (`564870`, calls at `5635BA/5635CD/5635E1`) from color decoding (`564510`, calls at `5635F8/56360F/... `). DSR live type5 `1C6900` selects area/type5 via `1C6986 -> 569B40` then reads precomputed records using `1C69C3 -> 563460` and `1C69FE -> 563B80`. The renderer writes selected data into `FrpgDrawContext+1240/1260/1280/12A0` families, but shader CB/RT and pixel remain OPEN.

**Verified equation-level difference:** DSR `564510` reads signed color WORD and gain WORD, multiplies by `float32(0.01)` (VA 1412DC48C), divides by `float32(255)` (VA 141615C80), masks sign using `andps` at `564568`, and invokes **CRT powf(x, 2.200000047683716)** at `564573`. `564573 -> 1410E59C6` thunk jumps through IAT VA `142018144`, which resolves to `api-ms-win-crt-math-l1-1-0.dll!powf`. Exponent lives at `VA 1412AA960`. PTDE FF1A20 signed RGB is scaled linearly in double with selected float32 rounding at `FF1AC2..FF1B56`, and analogously for subsequent groups.

Symbolic input response:
- PTDE: `xP ≈ signed_color * (signed_gain*0.01) / 255`.
- DSR: `xD ≈ powf(abs(signed_color*(signed_gain*0.01)/255),2.2)`.
Actual CPU instructions differ in float rounding; not bit exact. DSR also changes sign semantics for negative channel values. These distinct **CPU intermediate operators** are CONFIRMED even though the two games may include other differences later.

Additionally `563460` copies prebaked rows and multiplies specific vector fields `+60/+70` by `1.5` (VA 141A2A130). Do not port that scale globally: its PTDE downstream counterpart and scope remain OPEN. DSR `563B80` separately assembles directional-like vectors from prebaked record `+40/+48`; it is NOT a synonym for PTDE 4BBC60.

## Verification and limitations

The V425 handoff contains a standalone Python stdlib verifier that checks both exact executable SHAs, **40 instruction preimages**, **16 direct call edges**, table setup, imports and float constants, plus deterministic synthetic operator examples. It succeeds on original owner-provided EXEs. The synthetic comparisons are NOT measured game pixels or a solver.

Next before runtime: exact DSR type5 receiver/shader family + real CB/SRV upload from context+1240..12B0, asset equivalence classification HOMOLOGOUS/REBAKED, PTDE downstream `424440/424640/424840`, material/MTD and pass census. No global gain, ToneMap, shader rewrite or row30 pinning.

Statuses: static CPU producer/decoder/routing CONFIRMED; GPU input, material and asset identity, pixel OPEN. V359-RC1 and R43 Source2 PROTECT remain untouched.