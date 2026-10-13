# V262 — ordinary HemEnvLerp diffuse material operator

Source candidate only. The original DSR RX33/RX34/RX35 ordinary ELSE first diffuse lane multiplies its already-sampled diffuse and vertex carrier by cb0[9], then applies abs/log *2.2/exp. PTDE DifSpcBmp HemEnvLerp uses a linear local product with c100.

This tool rewrites only that first lane to use R19 b12[1] (PTDE c100), removes its gamma chain by same-size DXBC NOPs and moves the original linear product to its same live-out register. The independent second/specular lane remains stock. The 212-DWORD original Fog/LightScattering window remains identical. A genuine DXBC checksum is recomputed.

This is NOT an owner-ready addon. The shared RX33-35 shader is not P_Metal or nonmetal exclusive. Before activation a draw-specific shader + b12 transaction must check exact equipment model, MTD, material slot, sidecar SHA, receiver and CB0[85] state, with fail-open and full restore. The remaining nonmetal local specular/postmerge operator still requires independent reconstruction. No runtime or pixel pass is claimed.

The checked-in synthetic test uses no retail game shader bytes and rejects mutated gamma, unknown Fog window and missing b12. Run `python renderer-core/tools/v262/test_ptde_diffuse_lane.py`. Actual original donor verification can be performed locally with `--stock-dir` but retail shaders are not redistributed.
