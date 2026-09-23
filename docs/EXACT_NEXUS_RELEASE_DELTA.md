# Exact Nexus 1.45 release delta

This note records the byte-level relationship between the earlier clean intermediate and the actual Material Response 1.45 file uploaded to Nexus Mods.

## Identities

Clean intermediate:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

Actual Nexus addon:

`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

Both files are exactly `1,803,264 bytes`.

Actual Nexus ZIP:

`2439d24644a0dc5bef6e29b1d980bab5b421a08c6bc71fca93d4c88859ad0893`

## Binary comparison

The final addon differs from the clean intermediate in exactly 52 bytes.

Those bytes are confined to:

- the PE checksum
- DXBC 33 checksum + one terminal instruction modifier
- DXBC 34 checksum + one terminal instruction modifier
- DXBC 35 checksum + one terminal instruction modifier

No other embedded DXBC body changes between the clean intermediate and the Nexus shipping file.

## Shader semantic delta

For DXBC 33/34/35, the terminal RGB write changes from an ordinary MOV token:

`0x05000036`

to the same instruction with the DXBC saturate modifier:

`0x05002036`

The affected terminal instruction-word positions inside SHEX/SHDR are:

- DXBC 33: word 2822
- DXBC 34: word 2741
- DXBC 35: word 2394

This implements:

`RGB_out = saturate(RGB_preout)`

at that final separate RGB write.

The patch does not alter alpha.

## Exact final shader identities

- DXBC 33: `ee2511b2c3c6a822c921ad0ca5ef9eaf78b05d1992ce88851b5ecae95601c872`
- DXBC 34: `3cb53c033f61ef7664be097373c87d1a5f0933c2d3342feb6c50a63b5e3b49dd`
- DXBC 35: `71b973e36cb2ebbabc05455c1f882653ad39532644051d7d1e2e045874d427d3`

The integrated dedicated payloads 72/73/74 retained by the shipping PE are:

- DXBC 72: `159e9bbcb36c110e0e6e223f740986844cc8f0882d472afaf5b65abee4301e98`
- DXBC 73: `726461e5308788f75e7dba4e0e7c85e9fcc801faec38c060635dff40fb9ae877`
- DXBC 74: `11a405536a0600037b11817579a29cf5663bc62e8b17955af255d279e03417c7`

Their presence is part of the integrated binary lineage. The shipping release still disables the PTDE EnvSpec resource-replacement path, so this document does not claim active EnvSpec substitution.

## Reproduction source

The exact final delta is implemented by:

`tools/patch_pmetal_terminal_sat.py`

Given the exact clean intermediate, that script:

1. verifies input SHA-256;
2. locates the 78 embedded DXBC containers;
3. checks the expected terminal MOV token in DXBC 33/34/35;
4. applies the SAT modifier only to those three terminal RGB writes;
5. recomputes the three DXBC checksums;
6. recomputes the PE checksum;
7. verifies exact output size and SHA-256.

The resulting file is byte-for-byte identical to the addon contained in the Nexus ZIP.

## Scope statement

This document records the binary/consumer mutation exactly. It intentionally does not infer that DXBC identity alone proves exclusive P_Metal runtime selection; runtime/material routing is a separate concern from exact release reproduction.
