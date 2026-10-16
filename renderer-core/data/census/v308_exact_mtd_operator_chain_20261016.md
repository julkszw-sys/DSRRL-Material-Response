# V308 — exact six additional PTDE-authored MTDs, 17 total

Scope: original PTDE/DSR EnvSpec router 325-row registry, exact SPX source.
V307 parent 6 authored slot0 + 5 Leather slot1. V308 adds 5 slot0:
P_RoughCloth[DSB].mtd, P_RoughCloth[DSB]_Edge.mtd,
C_RoughCloth[DSB].mtd, C_RoughCloth[DSB]_Edge.mtd
(original PTDE EnvSpec slot0 c100=0.5, c101=1, power=2)
and Ps_Body[DSB]_Alp.mtd (slot0 c100=.5, c101=1, power=8);
also one slot1 P_Leather[DSB]_Alp.mtd (c100=.5,c101=1.5,power=4).
These have source-original PTDE/DSR identical
FRPG_Phn_ColDifSpcBmp.spx, HOMOLOGOUS_SPC, EXACT_SPX and exact DSR
raw MTD SHA256. V308 materializer verifies bitwise-authored PTDE
c100/c101/c102, exact EnvSpec slot and full DSR SHA.
The prior V293 independent RoughCloth SPX receiver gate is retained;
no guessed material family or unverified MTD aliases.

Two existing physical GPU operator islands use V297/v299 authored slot0
and V300 Leather slot1. Counts now slot0=11 and slot1=6; parser already
derives exact cohort lengths from compile-time arrays and has disjoint
offsets. Both need actual original FLVER SHA + material slot + MTD,
parser MTD, exact native t1, PTDE sidecar, shader RX33..35 and
complete atomic GPU state / receiver or fail-open.
This adds **source-eligible variants**, not confirmed in-game activation.
R43 latest_hook_source TLS+global fallback, V304 DSR-only three
LightBank rows, V305 source 749/544/2, PointLight and DoF unchanged.
