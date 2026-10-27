# V404 — ShaderContext virtual dispatch static RE

Pinned unmodified DSR EXE SHA256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Static binary only. Source verification is included in reproducible V404 handoff.

- RTTI type `NS_FRPG::ShaderContext_FrpgFxNormalEntity`, vtable RVA `0x136C520`.
- Factory RVA `0x501330` requests size **0xB0** and alignment **0x10** at `0x50135A`, then invokes constructor `0x4FF5A0` via direct call `0x50137F`. Constructor installs vtable `0x136C520` at `0x4FF5D1`. Destructor `0x4FF6C0` installs same vtable at `0x4FF6DC`, called from `0x500C7F`.
- Method22 RVA `0x5000E0` calls virtual method21 (vtable slot +`0xA8`) at exact RVA `0x50019B`, passing **the same context in RCX** and **the same RDX destination**. Virtual22 and virtual21 are not independent competing sources.
- Method21 `0x4FFF10` loads context cache `[RCX+0x60]` at `0x4FFFAC`. Null branch writes zero vectors to destination offsets `+0x1240/+0x1250/+0x1260`. Non-null branch calls decoder `0x563460` (site `0x500006`) before storing outputs.
- Method22 also tests `[context+0x60]` at `0x5001A1`. Null branch writes constants vec4(0,0,1,0) and vec4(0,0,0,0) at RDX offsets `+0x1320` and `+0x1330`; nonnull branch invokes same decoder `0x563460` at `0x5001E2` and writes its outputs.
- Context object and RDX destination are distinct pointers; destination type, actual actor owner, receiver RX34, D3D11 CB consumer and pixel equivalence **remain OPEN**. The 0xB0 context size does not authorize treating RDX destination as the native actor from LightBank V401.
- Raw indirect call slot +0xA8/+0xB0 search cannot certify a virtual receiver without exact vtable identity. Keep the LightBank R43 `latest_hook_source` and its consumer call unchanged. Do not inject any RGB, row restrictions, heuristic gains or native EXE patch.

Recommended next RE cut: type-verify the virtual dispatcher that provides RDX, and only join the V401 model wrapper when provenance can be proven. Status: SOURCE RE PASS; RUNTIME and PIXEL OPEN.
