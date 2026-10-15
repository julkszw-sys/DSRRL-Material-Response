# V277 owner DXBC: exact R5 Stable-only, Lerp missing

Owner private capture ZIP SHA256 `fcb182f4569229b8985743d1068e89b198723d3b2d97b4200437ad6298605981` has 12 shaders: RX33/RX34/RX35 times Stable/Lerp times Stock/Bridge. All 12 have ReShade capture `write_ok=1`. Original assets remain private and are not committed.

**Confirmed bytecode difference, 3/3 RX:** all Stable bridge shaders contain the exact 43-DWORD R5 PTDE cross-product bitangent reconstruction and neutralized frontface MOVC operand `0x00000001`. All Lerp bridge shaders lack the R5 cross construction, retain native DSR 19-DWORD interpolated-bitangent normalization, and retain frontface negation operand `0x00000041`.

**All three Lerp bridge shaders PASS every exact native Stable R5 precondition:** one frontface MOVC, one LightProbeParam normal MAD, one minus-one, exact tangent and interpolated bitangent normalization windows, unique register identities. Source-only in-memory dry-run replaces 19 with 43 DWORD and removes sign flip, passes R5 postconditions, and produces a bit-for-bit matching R5 sequence already present in the Stable bridge for the same RX. SHEX grows +24 DWORD; instruction count RX33 467 to 470, RX34 459 to 462, RX35 421 to 424. No game shaders altered.

**Runtime correlation:** 280 Lerp RX33 DRAW_ISSUED events including 56 P_Leather, 84 P_DullLeather, 56 DullLeather_Edge, 84 P_Metal; all planned a replacement PS plus PTDE t10 and t12. Stable RX33 43 draws, RX34 42. Lerp RX34/35 were built at create time but have no DRAW_ISSUED in this session. Cloth missing. First actual GPU N/T/B, reflection vector, preFog and PTDE pixel still OPEN.

Original PTDE 24 matched noPointLight Stable/Lerp shader pairs conserve all 132 NRM and 108 RSQ exact opcode+operands. Owner DXBC therefore proves a source-operator Lerp-only R5 restoration omission, not yet the visual cause of the laminate. The next isolated A/B should apply the existing Stable R5 to exact Lerp shader family after full receiver/material identity check, never through arbitrary specular gain or global rollback. Preserve stock Fog and LS.

**Status:** source shader R5 Lerp difference CONFIRMED; runtime Lerp RX33 issued PASS; pixel cause OPEN.