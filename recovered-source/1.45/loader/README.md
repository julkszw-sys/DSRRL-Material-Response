# External PackedGI loader — lifted source recovery

Status: **BYTE-EXACT LIFTED SOURCE / NOT ORIGINAL HANDWRITTEN ASM**

The lost historical source `loader_sha.s` is registered in Supabase as size `8,209`, SHA-256
`01d6587924928f63601c4e58ae153ebe15183f3a9a734ddf3bcf16c2831d3fe4`.

The compiled historical payload is recovered exactly:

- `loader_sha.bin`
- size `1,504`
- SHA-256 `6dad44e0b8beef0420fe2d9c88d5004a52d81906ea176a4884a0bb8f1a9d6a60`
- injected at RVA `0x1BC100`
- first 1,501 bytes SHA-256 `819f9cda297b09a24cb3eb2a9f6699dc5bb7807ac47ba2a94dbbdf50419d9405`

`loader_sha_lifted.S` is a textual, label-annotated byte-exact lift.  
`build_loader_from_lifted_source.py` rematerializes the exact payload and rejects any SHA mismatch.

Recovered behavior:

- constructs `DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin`;
- checks exact size `33,619,968`;
- reads into the historical reserved pack range;
- dynamically resolves CryptoAPI hashing;
- verifies SHA-256 before setting READY;
- fails open with READY false;
- exposes the pack pointer only when READY is true.

This closes the binary/source-representation gap for the loader. It does **not** claim recovery of the original handwritten `loader_sha.s`.