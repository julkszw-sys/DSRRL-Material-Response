V289 Stable HemEnv PTDE LightScattering post-domain, DSR Fog stock.

Scope: exact P_Metal[DSB].mtd and Stable RX33/RX34/RX35 only, using R43 existing verified material/probe routing. Lerp continues separate V287 exact-only registration with fail-open. All unrelated shader families are untouched.

Owner original DSR stock SHA pins:
RX33 35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24
RX34 d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837
RX35 7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701
204-DWORD Fog + LightScattering exact shared block SHA256:
28c23273a5fb0b984b55fde72ee59546af4aa8646c55e1cb1bf8db426e9a4363
21-DWORD DSR post-LightScattering gamma sequence SHA256:
d4fdd1ec09075f73609d978616733cc5bc97893606cb51751d97b61d28735512
21 source instructions are replaced with 21 NOPs; pre-Fog, Fog, the entire 204-DWORD Fog+LS source and downstream independently controlled operators are byte-identical. The older R10E composite atmosphere operator is NOT used because it also rewrote Fog. This is a PTDE post-scattering domain candidate, not proven pixel-equivalent PTDE LightScattering.

Build option DSRRL_PMETAL_STABLE_LS_PTDE_POSTDOMAIN_STOCK_FOG defaults OFF and is enabled only in PR366 diagnostic V287 single-addon workflow, not in active R43 RC1. PTDE input/consumer and output domain calibration and pixel comparison remain OPEN. Never extend to another shader family based on P_Metal success.
