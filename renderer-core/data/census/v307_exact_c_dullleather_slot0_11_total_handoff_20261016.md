# V307 — exact C_DullLeather slot0 original PTDE material source, 11 total

R43 -> V296 -> V297/V299/V300 -> V304 typed DSR-only LightBank
-> V305 749 direct DSR host Spec names -> V306 exact C_Leather 3
-> V307 exact C_DullLeather 3. One integrated ReShade addon.

Original PTDE/DSR V1 router 325 records certifies these 3 new
original retail DSR raw MTD SHA256 records, each source
PRESENT, HOMOLOGOUS_SPC, EXACT_SPX, same original
FRPG_Phn_ColDifSpcBmp.spx in PTDE and DSR, EnvSpec slot0,
original PTDE c100=.5, c101=1.0, c102 power=2.0:
- C_DullLeather[DSB].mtd 613a3296e7906b2dea5c0cd48bda37c8628b850bad9988688a40595c4c49eaad
- C_DullLeather[DSB]_Alp.mtd c1b1baca69cc33b449f7ff06bb08a959a1b74c90862d67acd003194062306dff
- C_DullLeather[DSB]_Edge.mtd e2d819b90e8201a93e67e84e1179e9c3bf3f57988d5f86b44c5e024d05dee1a1

V307 adds three to the V297 original 3 slot0 MTDs (now 6).
V306 remains V300 5 slot1 MTDs. Total 11 exact source
MTD cases, with disjoint retail MTD parser offset mapping to
both subsets. No collisions with P_Metal route345 or
unrelated renderer operators. Materializer verifies exact
MTD SHA, source-authored float bits, SPX, slot and provenance
before applying. Fails on ambiguous source.

Existing V299 source producer, V297 exact owner/MTD/slot
authoring, P_Metal-style atomic consumer & render transaction:
authenticated owner full FLVER SHA+slot+MTD, native receiver
RX33..35, exact stock logical t1 and PTDE SpecRGB t10,
PS+CB+SRV+sampler+probe complete or fail-open stock DSR.

11 exact *MTD source candidates* do not mean 11 actual
activated materials, 749 active names or PTDE pixels. No global
PBL changes; PointLight/DoF and R43 latest_hook_source
untouched. Construction tests and native single addon only.
