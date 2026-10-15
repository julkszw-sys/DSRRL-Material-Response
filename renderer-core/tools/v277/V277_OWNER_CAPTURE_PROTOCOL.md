# V277 exact Stable/Lerp shader capture

Build: one V272-descendant ReShade addon with unchanged renderer/shader math and V276 material trace. Create-time DXBC capture only for exact shader receiver RX33, RX34, RX35 when the authorized Stable or Lerp materializer returns `applied`.

With V277 installed as the sole DSRRL addon, trigger Stable and Lerp. Find the new `DSRRL_V277_CAPTURE` folder next to DarkSoulsRemastered.exe. Files are `RX33_STABLE_STOCK_<hash>.dxbc`, `RX33_STABLE_BRIDGE_<hash>.dxbc` and `RX33_LERP_STOCK_<hash>.dxbc`, `RX33_LERP_BRIDGE_<hash>.dxbc`, likewise RX34/RX35. If files exist, send them with the full ReShade.log. The logger marker is `[DSRRL V277 EXACT DXBC CAPTURE]`; it reports write_ok and source phase. Missing markers mean no authenticated materializer or failed capture, not material success. New files are diagnostics only and do not modify executable or replace installed shaders.

Offline: `python renderer-core/tools/v277/analyze_v277_dxbc_capture.py PATH_TO_CAPTURE --out v277.json`. Parse SHDR/SHEX instruction counts and SHA256 with fail-closed decoding. Never equate source construction with physical GPU shader execution or PTDE pixel match. Compare normal basis and reflection liveout with exact matched material MTD, FLVER, slot and real pixel history before patching R5. Preserve stock independent DSR operators.

Original PTDE 24 exact matched no-PointLight Stable and Lerp PS3.0 shaders: all 132 NRM and 108 RSQ instructions have identical full tokens across phases. V272 Stable materializer has explicit R5 normal basis restoration; Lerp has no same-named R5 call. This is source-local asymmetry, not root-cause pixel proof.
