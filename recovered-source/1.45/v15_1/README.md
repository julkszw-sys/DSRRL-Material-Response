# V15.1 EnvSpec extension — exact binary reconstruction

Status: **EXACT BINARY RECONSTRUCTION / SEMANTIC SOURCE PARTIAL**.

This directory reconstructs the historical V15.1 EnvSpec active-bind diagnostic exactly from the byte-exact V12 addon plus three independently identified inputs:

- `v15_1_ext_text.hex` — exact 0x6000-byte extension text image, SHA-256 `f8b474277787f9ae81cffc0c64f5586532bb804366accc3cbc8d877ea44852ac`;
- `v15_1_ext_rdata.hex` — exact 0x2000-byte extension rdata image, SHA-256 `5c7ceaee061d03218923b630ba984b3f1c23350dd861dc4072cab6871e468e7e`;
- external `PTDE_GI_ENVSPEC_PACK_RGBA.bin` — 33,619,968 bytes, SHA-256 `c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3`.

Input V12 addon SHA-256:

`3db7ad4a07c293d3b5a6579193c086bf96a7062f036d9ffb85efdb0400b00e79`

Exact reconstructed V15.1 SHA-256:

`7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc`

## What the builder changes

The historical V15.1 image differs from V12 in only 28 bytes inside the original V12 file extent, then extends `.srgbmt`:

- PE `SizeOfImage` -> `0x021CF000`;
- `.srgbmt` VirtualSize / SizeOfRawData -> `0x0209B000`;
- host init stub jumps to `envcube_init_wrapper` RVA `0x1B8E50`;
- host uninit stub jumps to `envcube_uninit_wrapper` RVA `0x1B91F0`;
- V12 PRE/POST/PREPARE draw calls are redirected to EnvSpec wrappers;
- zero padding aligns the extension to RVA `0x1B7000`;
- extension text occupies `0x1B7000..0x1BCFFF`;
- extension rdata occupies `0x1BD000..0x1BEFFF`;
- exact PTDE PackedGI occupies `0x1BF000..0x21CEFFF`.

The builder verifies all input hashes and refuses to patch unexpected bytes.

## Important source-completeness statement

The `.hex` files are recovered machine-image provenance, not the missing original handwritten `envcube_ext_v15_active_bind.c`. They close exact binary reproducibility of V15.1, but **do not by themselves satisfy the project source-complete release gate**.

`v15_1_ext_text.disasm.txt` and `SEMANTIC_MAP.md` expose the reconstructed control flow and verified semantic contract. A maintainable C/C++/ASM rewrite must remain labelled `RECONSTRUCTED` until independently checked against this exact image.