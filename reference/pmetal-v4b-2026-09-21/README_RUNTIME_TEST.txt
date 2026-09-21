DSRRL Material Response 1.45 — P_Metal PTDE Domain Bridge V4B

Drop-in runtime candidate based on the corrected 1.45 / V3 exact EnvSpec island.

V4B changes only the three exact P_Metal alternate pixel shaders (33/34/35):
- recovers PTDE-linear FogRGB from DSR cb12.rgb (which stores q^2.2),
- keeps the P_Metal surface linear into Fog,
- retains the existing homologous LightScattering kernel,
- removes the DSR-only post-LightScattering x^2.2 continuation,
- enables the confirmed PTDE terminal RGB SAT.

NOT changed: U/L, PointLight, EnvSpec resources/source/sample math, c87, c101, SpecRGB, COLOR0, routing.

Important: this build does not hardcode c135.x/c135.y. The typical PTDE 1/2 scene-encoding ratio is not assumed as a universal constant here. Runtime/pixel status remains open until tested.

Keep existing sidecar:
DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin
size 33,619,968
SHA256 c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3
