# V444 offline RE — reproducibility

This is **research-only**, descending from V443. It deliberately does not modify the integrated ReShade addon.

- Original untouched retail DSR EXE SHA-256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.
- Offline evidence bundle SHA-256: `0b1e8d011c389527f47b83903eb23c4df9e7b43f8a809bb0b0515bdac`.
- Bundle: `DSRRL_V444_ARMOR_OFFLINE_RE_EVIDENCE.zip` (report, Python verifier, results and SHA-256 manifest).
- Command, run against your *unmodified* copy of the original binary:

```sh
python research/v444/verify_v444_actor_lifecycle.py DarkSoulsRemastered.exe --mutations --json research/v444/V444_STATIC_RE.json
```

Static results: 34 byte preimages; 13 direct E8 candidate sites in six callee groups; four verified factory pointers and four MSVC RTTI classes; all 52 negative mutations correctly rejected. Exact actor generation, issued RX34 GPU state, runtime liveness and PTDE pixel behavior remain **OPEN**.

The full verifier and exact JSON are retained in the evidence bundle because V444 is an offline research artifact, not a deployable renderer version. Current source lineage: frozen V359-RC1 -> tested V439 -> CI-built but runtime-untested V443 -> V444 static RE. Preserve protected R43 `latest_hook_source`, Source1 and all material sidecars; no wrapper detours from V438.
