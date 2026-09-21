DSRRL Material Response 1.45 — P_Metal Fresh SpecRGB V5

Drop-in runtime candidate based directly on V3 exact EnvSpec.

Root cause fixed:
V3 sampled the PTDE SpecRGB sidecar correctly, but that sample register was overwritten before the final material multiply. The final EnvSpec material tail therefore consumed a stale DSR material-workflow temporary (r11/r10/r9) instead of live PTDE SpecRGB. This is the same stale-register failure class previously seen in the V2.21-V2.24 branch and matches the global cyan/rainbow phenotype.

V5 changes only embedded P_Metal DXBC 33/34/35:
- reacquires t10 SpecRGB immediately at the final material cut,
- computes fresh SpecRGB * PTDE c101 * COLOR0,
- enables the confirmed PTDE terminal RGB SAT,
- leaves atmosphere-domain, U/L, PointLight, EnvDiffuse, resource routing, c87, sidecar sampling and all other shaders at the V3 basis.

Existing sidecar remains required:
DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin
size 33,619,968
SHA256 c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3
