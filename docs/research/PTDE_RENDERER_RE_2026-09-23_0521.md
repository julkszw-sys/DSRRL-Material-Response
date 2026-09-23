# PTDE Renderer Full Reconstruction — RE continuation 2026-09-23 05:21 CEST

Supabase bootstrap/control-plane read succeeded at revision 8664. Knowledge write via `dsrrl.promote_finding(...)` was blocked by the connector safety layer, so this note is the audit/failover record pending backfill.

## CONFIRMED — PTDE `ShaderConstant_DofParamEntity` is the central image-process constant carrier

Direct DATA.exe RE already persisted as raw Supabase evidence (`knowledge_evidence` evidence id `0e30823e-1c24-476b-bfcc-8e96934f1f44`) closes update vfunc `0x004252F0` and helpers `0x00F93040`, `0x00F93940`, `0x00F939B0`, `0x00F93870`, `0x00F92DA0`.

The entity populates the complete **PS c7..c71** interval, plus **VS c12** and **VS c68**. Important direct mappings:

- `+0x60/+0x70/+0x80/+0x90 -> PS c7/c8/c9/c10`
- `+0xC0 -> c11`
- `+0xE0 -> PS c12` and `VS c12`
- `+0xF0..+0x160 -> PS c13..c20`
- `+0x170 -> c21`
- `+0x180..+0x270 -> c22..c37`
- `+0x280..+0x370 -> c38..c53`
- `+0x380/+0x390/+0x3A0/+0x3B0 -> c54/c55/c56/c57`
- transformed matrix `+0x3C0 -> PS c58..c61`
- helper-derived matrix from floats `+0x400..+0x41C -> PS c62..c65`
- `+0xA0/+0xB0 -> PS c66/c67`
- `+0x420 -> VS c68`
- `+0x430 -> PS c68`
- `+0x440/+0x450 -> PS c69/c70`
- `+0xD0 -> PS c71`

Independent earlier RE anchors semantics inside this same interval: `c54` adaptation, `c56` legacy HDR/combine controls, `c62..c65` ToneCorrect ColorAdjust, `c68` overlay-related state, `c71` BrightPass/lightshaft endpoint. Therefore the old producer-map description `DrawEnv/DOF state -> DOF shader constants -> ImageProcessDof` is too narrow and must be superseded. `DofParamEntity` is an aggregate legacy **ImageProcess/postprocess state carrier**; high-level meaning of every lane is not yet closed.

Proposed canonical finding already exists as `renderer.ptde.dofparam_imageprocess_register_transport_v1` (`ce03030b-6ae5-4c68-a026-1605377de336`, status CONFIRMED) but was not canonical/promoted when checked. Backfill action: promote it and supersede current `renderer_producer_map.mapping_id=4` with transport `entity aggregate -> PS c7..c71 + VS c12/c68`, consumer family `PTDE ImageProcess/postprocess consumers`, route status `PARTIAL`, semantic status `PARTIAL`.

## CONFIRMED — DSR-native SFX has an explicit inverse-tonemap consumer island

Raw Supabase evidence from direct DXBC SHEX decode shows exactly 20 dedicated `FRPG_SfxPBL` shaders execute an identical conditional inverse-tonemap branch: LineType0, PointSpriteType0, SimpleSpriteType0..8, SimpleSprite_DepthType0..8. They read `gFC_InverseToneMapEnable=cb0[31].x`; when enabled they sample `gSMP_LumTex=t4/s4` at `(0.5,0.5)` and compute

`C_out = C_in * Adapt.y * (clamp(Ladapt, Adapt.z, Adapt.w) + 1e-4) / Adapt.x`,

where `Adapt=cb0[30]=gFC_AdaptParam`.

BlurType0..3, DistortionType0..5 and TracerType0..3 do not execute this branch. This strengthens the DSR-native SFX exclusion boundary: preserving DSR SFX is not just preserving its material shaders; these receivers are coupled to adaptation/luminance state. A PTDE postprocess bridge must therefore preserve or explicitly feed the DSR SFX inverse-tonemap contract rather than globally replacing adaptation state.

Runtime producer provenance for the dedicated SfxPBL `Adapt`/`LumTex` values remains OPEN.

## Next unresolved step

Highest-value continuation: trace **PTDE DofParam c54/c56 producer provenance and exact consumers**, then compare to DSR adaptation/HDR state. This now sits directly on the boundary between the PTDE world postprocess reconstruction and the stock-DSR SFX inverse-tonemap island. Do not assume register-name/producer equivalence implies final-pixel equivalence.