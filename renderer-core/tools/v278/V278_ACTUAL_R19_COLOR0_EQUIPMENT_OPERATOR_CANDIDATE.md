# V278 — material-local PTDE SpecRGB × COLOR0 operator candidate

## Source and why

The V264 handoff verifies PTDE local material output:
`Spec_local = EnvSpec_P × SpecTex_P.rgb × c101.rgb × COLOR0.rgb`.
V276 certified TRUE final R19 RX33/RX34/RX35; V277 limited
the ordinary second-gamma ELSE operator to workflow != 0.
The ordinary workflow=0 path is the relevant candidate for plain
P_Leather and P_DullLeather (owner-reported overgloss).
A DSR shader's shared RX name alone is NOT an equipment-material identity.

The real R19 C++ final path first duplicates stock `t1` as `t10` with
`xyz` write mask (stock alpha left alone) then multiplies local
`t10.rgb` by exact PTDE raw `b12[0].rgb=c101`. It does not insert a
dedicated local `COLOR0` multiply adjacent to this sample. The historical
V13 diagnostic had an earlier `color_mul` implementation, but it was
disabled in the real V272/R19 path and did not constitute a full Lerp port.
Existing stock DSR COLOR0 is read upstream; its complete influence on
all later DSR specular branches is not settled merely by this observation.

## New operator candidate (offline, NOT activated)

`renderer-core/tools/v278/build_owner_r19_equipment_spec_rgb_color0_candidate.cpp`
accepts only the three genuine final-R19 shader full SHA256 preimages.
Each gets exactly one tokenized DXBC instruction:

```
MUL rSpec.xyz, rSpec.xyz, vCOLOR0.xyz
```

as the first instruction **inside the THEN block** following `IF_Z cb0[85].x`, preserving other original code and leaving nonzero-workflow ELSE behavior untouched. The tool does NOT
write the original file, register a pixel shader, or enable a replacement
on a world/body/material draw. Actual draw binding requires exact physical
equipment model+FLVER, SHA-pinned MTD, material slot, verified currently
bound stock t1, exact original PTDE companion sidecar/SRV, relevant
`cb0[85]` and verified PTDE consumer branch. **P_Metal is protected as
a working control; this candidate must not change its draw.**

| RX | Final R19 SHA prefix | Candidate SHA256 | Bytes |
|---|---|---|---:|
|33|1d9fed8a|aba3a135806a8b5396ae959e2eda94affed5b78e8439447002835301b4ef6733|22540|
|34|8f86b1ae|0f98a40c352fb562d98ec9b2e9c9b7e06c44b04158bc61bdbe6afb5f7bdfcb93|22240|
|35|b5f94cc|fb840c245c31d4094d112c8a77fee44eb5be273ed83bd3d98edd4afeb0f5c738|20612|

V278 derives owner-local outputs (not distributed). Offline tests:
3/3 input full SHA authenticated; 3/3 insertion exact and resulting
container size original + 28; all other opcode words *bitwise identical*,
except SHEX size metadata; stock DSR 212-word Fog/LS and both Glow writes
not changed; all non-SHEX chunks (RDEF, input/output signatures) identical;
official project DXBC checksum regenerated; wrong shader SHA rejected.
Asset-free Ubuntu CI compiles this tool and rejects four negative cases.

## Important boundary

This proves **construction of a mathematically meaningful, minimal
PTDE-factor candidate**, not that live PTDE/DSR specular pixel mismatch
has been diagnosed or fixed. In particular:

- Branch 0 exact material+resource ownership and actual GPU `cb0[85]`
  remain unverified for the proposed replacement; enabling on a common RX
  would affect unrelated materials and is forbidden.
- Native D3D11 `CreatePixelShader` / WARP validation of *these 3 owner
  outputs* remains OPEN (the tool is tested on Linux offline).
- No scene-level Pixel PTDE comparison has been performed; another
  DSR postmerge could already incorporate vertex tint, hence a
  double-multiply cannot be ruled out from immediate t10 staging alone.
  Do NOT promote this as a pixel CAUSE_CONFIRMED patch.
- Stock DSR Fog/LightScattering/Glow and `latest_hook_source` unchanged.
- Do not repurpose raw `b12[0].w` (PHN k135) as specular power.

**Next step to FINISH the operator:** complete the zero-workflow
per-component `t10*raw_c101` vs stock COLOR0 def-use up to pre-Fog,
prove the exact PTDE/DSR material equation and physical sidecar owner,
then integrate the accepted variant as one material-selective draw
transaction within the single .addon64. Validate pixels against PTDE.
