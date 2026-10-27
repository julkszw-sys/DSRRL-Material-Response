# V410 — Native EnvSpec producer to RX34 indexed draw: evidence limits and diagnostics

Source pinned: owner-supplied `DSRRL_V398_FLICKER_FULL_HANDOFF_2026-10-26(1).zip`, unmodified original `DarkSoulsRemastered.exe` SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. V409 branch is the addon construction baseline. **V410 does not infer PTDE target RGB from DSR samples.**

## New definitive falsification: V398 serial is not native packer sequence

- V383 native packer **raw log line 202**, time `15:05:45.811`, `serial=3`, `decoded_rowA=20`, thread 21012.
- V388 RX34 GPU issued **line 607**, time `15:05:48.187`, `actor_owner=0x219A4D80`, route345 slot1, `Source1 row25 serial=3`, C86=(0,0,0), COMBINED, bank 6da2e3578ff67baf, probe244.
- There are 19 Source1 GPU samples with selector `serial=3` across rows 20,21,25,30,31. This is a selector *epoch*, not a distinct native event ID. A numerical `serial=3` join would incorrectly connect the row25 GPU draw to a row20 native packer event.
- This justifies preserving **native_producer_sequence** separately from selector serial (V400). It does **not** certify actor ownership.

## Bounded native row25 evidence

Natively decoded row25 occurred in only the **sampled V383** events (line/time/native serial): 373/15:05:45.828/25, 420/45.835/38, 468/45.863/63, 483/45.865/73, 520/45.876/98. All five carry `model_join=OPEN`, and their shared `srcA` bank pointer cannot name an actor. The first of 8 logged row25 Source1 GPU draws for owner `0x219A4D80` is at 15:05:48.186; observed sample gaps range **2310–2358 ms**, **not a bound on actual runtime event gap**. Among 59 sampled REMO events for that owner, zero have selected descriptor 1305 (= row25); this is a negative *sample* result only.

V398 45 GPU issued samples contain 33 exact (owner,RX,route,slot,semantic,shader,FLVER SHA,MTD SHA) keys and ten changes. GPU evidence confirms alternating source data in logs, not which producer belongs to which draw or whether the pixel improved.

## V410 source construction changes

1. `pmetal_env_source_runtime.cpp` diagnostic `same_producer` now checks nonzero exact native sequence **plus A and B bank signatures, A/B row IDs, bitwise beta, A/B RGB and EnvDiffuse validity/endpoints**. This read-only comparison runs in the already-authenticated V408 bounded readback logging path. It never gates Source1/Source2, never writes RGB, and does not modify protected R43 `latest_hook_source()`.
2. `tools/analyze_v410_native_producer_draw_join.py` reconstructs historical V398 serial collisions and classifies V383/V408/V409 joins. Even a cache+native sequence match is explicitly **not** native actor ownership proof. Unsampled rows, Source2, zero sequences and competing owners stay OPEN.
3. `tools/test_v410_native_draw_join.py` includes independent synthetic fixtures for incorrect epoch joins, owner mismatch, Source2 and incomplete source evidence.
4. `tools/test_v410_source_consistency.py` validates the readback fields and the additive-only nature of the diagnostic.
5. CMake now registers the inherited V409 8-thread transition regression as a native target, run on Windows as well as GCC/Linux. Workflow `v410-producer-draw-join.yml` runs strict compiler and Python evidence tests, builds one integrated x64 addon and publishes a single `.addon64`.

## Guardrails and next bound

No Source1/Source2 reclassification, no ban on zero/row25, no material gain, no blanket Remaster override, no EXE on-disk patch. A future native producer → owner → draw certification requires full owner-qualified native wrapper event and context lifetime generation, plus command list timing and actual RX34 consumer/CB evidence. `V408 same_producer` attests cached Source1 payload agreement **only**.

CONSTRUCTION: SOURCE PATCHED / LOCAL CLASSIFIER PASS; COMPATIBILITY: WINDOWS CI OPEN; RUNTIME LIVENESS: OPEN; BRIDGE ACTIVATION: OPEN; PTDE PIXEL BEHAVIOR: OPEN. No selector flicker fix claimed.
