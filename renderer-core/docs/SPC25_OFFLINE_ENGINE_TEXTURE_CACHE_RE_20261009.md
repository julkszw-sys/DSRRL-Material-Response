# SPC25 — offline RE: DSR UTF-16 texture cache entry is not a native SRV

Scope: **construction/static RE only**, host SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. PTDE=target, stock DSR=host. Do not patch the executable on disk.

Run against an exact user-owned EXE:

```sh
python renderer-core/tools/verify_spc25_engine_texture_cache_offline.py "DarkSoulsRemastered.exe" --out offline_spc25_name_cache.json
```

The script checks PE32+ AMD64, image base `0x140000000`, exact EXE SHA, 18 opcode anchors and the actual E8 REL32 target of the name lookup. It is read-only; no GPU or running game required.

## Confirmed producer and intermediate consumer

- Retail texture-name site **RVA `0x583AA6`** computes the UTF16 name pointer in R14. This site is already hooked in DSRRL.
- At **RVA `0x583AB7`**, the game passes the UTF16 name (`RDX=R14`) and the engine cache map (`RCX=R13+0x10`) to `0x140518A10`, which hashes lowercase ASCII/UTF16 with multiplier `0x89`, walks buckets, and compares wide strings.
- At **RVA `0x583AC3`**, `RAX` is placed in `RBX` as an **engine cache entry pointer**, not an attested D3D11 texture or SRV.
- The lookup compares node name at **`+0x08`** and bucket-next at **`+0x10`**. At `0x140518CB9`, `+0x18` is incremented. On miss, caller constructs an entry with `0x140582930` and inserts via `0x140518CB0`.
- The constructor clears **`+0x28`, `+0x30`, `+0x38`, `+0x40`**. Method `0x1405829C0` writes an integer at **`+0x28`**, derives a pointer from the name and forwards it to an opaque function. A separate method loads **`+0x30`**, passes it to a cleanup routine, and clears the member. Neither `+0x28` nor `+0x30` has been proven to be a native D3D11 handle. Do not treat either as `ID3D11Resource*` or `ID3D11ShaderResourceView*`.

## What the disk bytes do not prove

At `0x140582A50`, `0x140582B00`, `0x140582CA0`, `0x140584750` the **on-disk** bytes have high entropy and show invalid/implausible x64 instruction sequences during linear disassembly. The offline script reports quantitative 512-byte window entropies. It **does not** infer the protection type, a decryption key, or valid runtime behavior. Thus the CPU cache-entry → native D3D11 resource → bound `PS t1` connection is **OPEN**.

Last owner gameplay source `6cb0f4cb` established that `C_RoughCloth` route14 has exact owner and a real bound stock `t1` but no ANSI or UTF16 native debug label, `hash=0`, no SpecRGB companion; other observed V13 routes remained `request_ready`. No PTDE pixel equivalence was confirmed.

## Narrowest future bridge: precise proof obligations

1. Identify the **exact native producer assignment** connecting the engine cache entry to the renderer texture object and native `ID3D11Resource`, not merely the filename lookup/callback timing.
2. Trace the exact native SRV made from that resource to the actual stock `PS t1` for authenticated `FLVER SHA + material slot + raw MTD SHA + receiver`. Logically related but nonidentical resources must remain separate. Do not infer from MTD, BC1, dimensions, last-observed-name, or a global LightBank gain.
3. With both edges verified, add only an exact, lifetime-safe resource identity bridge in the **existing single `.addon64`**, including resource/view/device destruction and conflict quarantine. Fail open to stock DSR on any missing proof.
4. Validate construction, Windows CI, owner runtime liveness, bridge activation and PTDE pixels **separately**.

Until steps 1–2, no canonical patch for `stock_t1_logical_lookup_miss` is justified. The proven V13 fallback remains unchanged.

**Status:** CPU cache name lookup/operator **CONFIRMED**; downstream native resource/view identity **OPEN**; runtime SpecRGB **OPEN**; PTDE pixels **OPEN**.
