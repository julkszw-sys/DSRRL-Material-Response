# A3 monolithic PE integration design

## Requirement

The final test/runtime artifact is one `.addon64` file on disk. A1, A2 and PTDE U/L are not shipped as separate companion addons.

## Immutable legacy basis

A3 starts from the exact shipping Material Response 1.45 image:

- SHA-256 `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- size `1803264`
- PE32+ x64
- image base `0x180000000`
- AddonInit export RVA `0x8C80`, currently a jump to legacy extension RVA `0x1B8E50`
- AddonUninit export RVA `0x9260`, currently a jump to legacy extension RVA `0x1B91F0`

The historical integrated source that originally produced all 1.45 functionality is not available. Therefore the shipping 1.45 file is treated as an immutable, hash-guarded legacy basis. Every A3 delta must be generated from source in this branch.

## Storage strategy

The final section `.srgbmt` is the existing 1.45 extension section:

- RVA `0x134000`
- virtual size `0x209B000`
- current file-backed raw size `0x8B000`
- executable/readable characteristics

The file ends at the current raw end of `.srgbmt`. A3 appends its generated extension payload to this same section and increases only its file-backed raw size. The existing virtual reservation is already sufficient, so no new PE section is required.

## Init/uninit chaining

Do not replace legacy logic.

A source-controlled injector changes the exported five-byte jumps to A3 wrapper functions appended to `.srgbmt`.

A3 init wrapper:

1. validates the exact shipping 1.45 basis before transformation;
2. calls the original legacy target `0x1B8E50`;
3. only if legacy init succeeds, initializes A3 additions;
4. preserves legacy return semantics.

A3 uninit wrapper:

1. unregisters/restores A3-owned state in defined reverse order;
2. calls the original legacy uninit target `0x1B91F0`;
3. leaves legacy cleanup authoritative for all 1.45-owned resources.

Exact order may be refined if callback wrapping requires pre-registration before legacy init; any such change must be reflected here and in BUILD_AUDIT.

## Control Flow Guard

Shipping 1.45 has CFG enabled.

Load-config state:

- GuardCFFunctionTable VA `0x18000E608`
- function count `74`
- GuardFlags `0x10017500`
- table entry size `5` bytes (4-byte RVA plus one metadata byte)

The A3 injector must not register appended callback addresses with ReShade until they are represented as valid CFG call targets. The injector therefore rebuilds a sorted Guard CF table in appended A3 data, preserving all original 74 entries and adding every A3 function address that can be reached by an indirect call. It updates the load-config table pointer/count and retains the existing relocation on the GuardCFFunctionTable pointer.

Direct internal calls/jumps do not require CFG entries.

## Existing 1.45 draw transactions

A3 must not create an independent draw replay that competes with shipping 1.45 SpecRGB/Normal/Diffuse/P_Metal transactions. In particular, a second replay could draw the same geometry twice.

The integration rule is:

- reuse/wrap the existing 1.45 draw transaction when 1.45 already owns the draw;
- use a standalone U/L replay only when the draw is not otherwise intercepted;
- preserve exact material gating for shared `FRPG_Phn_DifSpc*` hosts;
- restore all modified PS/CB state before returning.

The implementation must prove callback ordering or intercept the known 1.45 callback itself; it must not assume ordering without evidence.

## U/L payload policy

PTDE U/L consumer transport is:

- `b13[6].xyz = Upper_PTDE`
- `b13[7].xyz = Lower_PTDE`

Payloads:

- 48 generic verified HemEnv/HemEnvLerp consumers are regenerated from exact certified vanilla host identities.
- P_Metal receiver indices 894/913/932 use U/L variants generated from shipping 1.45 embedded alternatives 33/34/35. This preserves shipping material-response behavior while changing only U/L consumption.

EnvDiffuse resource injection remains disabled in the first A3 build.

## Source-first gate

An A3 binary must not be promoted unless the branch contains:

- extension source;
- monolithic injector source;
- U/L generator;
- A1/A2 source or reproducible generated tables;
- exact input hashes;
- CFG audit;
- PE-delta audit;
- deterministic build instructions;
- final output SHA-256.
