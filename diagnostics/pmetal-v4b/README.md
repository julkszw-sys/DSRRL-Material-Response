# P_Metal PTDE Domain Bridge V4B

Runtime candidate for DSRRL Material Response 1.45, based on the V3 exact EnvSpec island.

Scope:
- only P_Metal alternate DXBC 33/34/35,
- recover PTDE-linear FogRGB from DSR cb12.rgb=q^2.2,
- keep the P_Metal surface in the legacy-linear atmosphere domain,
- retain the existing homologous LightScattering kernel,
- remove the DSR-only post-LightScattering x^2.2 continuation,
- enable PTDE terminal RGB SAT.

Not changed: U/L, PointLight, EnvSpec resources/source/sample math, c87, c101, SpecRGB, COLOR0, material routing.

Known residual: PTDE c135.x/c135.y dynamic scene-encoding scale remains OPEN and is not hardcoded in V4B.

Runtime ZIP SHA-256:
68c6fb3ad449de37e3b1f9ac0fb94ffa0f9ff80b0a491b98e57eb49f8cda4ad3

Addon SHA-256:
68402c2bb784385b88157234216c48f3a1605ab916b5290c0c3174d3b768cf16
