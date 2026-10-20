# V230 owner BC1 asset source evidence (2026-10-20)

Owner file: ReShade(20261020-193958).log, 27862 bytes, SHA256 854ac4d89ac34683db9d72a58b5dae1678f92fda496f6d2ec31f1d477d96d572.

Runtime: RX33-35 Stable and Lerp shader creation 3+3 PASS. At 21:39:35: pmetal_prepared_ok=127530, draw_issued=127530; 1 SpecRGB rejection, 0 observed probe rejections. No pixel equivalence claim.

First SpecRGB reject: RX34 stable, route=1073741868 (0x4000002c), material slot2. owner_tuple_exact=1 and actual_mtd_exact=1. Native t1 BC1 DXGI71 texture 256x256 mip9, serial287, exact creation_mip0=1, logical_known=0, companion_ready=0. BC1 mip0 compressed 32768 byte SHA256: 68136bd6146339192c790f3b058b4fd621b20740b4f1f3a5227b7d0a35f1511d.

Archive crossmatch: exact BC1 top-mip hash checker on attached DSRRL_FINAL_EVIDENCE_PART01 through PART04 ZIPs: 2122 source containers, 731 DDS headers, 105 eligible 256x256 BC1 mip9 candidates, ZERO exact matches. Positive control f50fee432693e649ffccb3749fb3a6f973b48df73030531cac5a1ed3aaafd634 resolves AM_F_9450_L in two partsbnd containers, demonstrating source name resolution succeeds when supplied. The evidence archives are NOT a complete stock DSR asset installation. Split TPFBDT/BHD indexed formats are not fully decoded by this scanner.

Reproduce: python renderer-core/tools/test_v230_bc1_mip0_matcher.py; then python renderer-core/tools/match_v230_bc1_mip0_dsr_archive.py PATH_TO_EXTRACTED_TPF_DDS --sha 68136bd6146339192c790f3b058b4fd621b20740b4f1f3a5227b7d0a35f1511d.

Result only confirms missing exact asset match in supplied subset. No hash guess, probe alias, SpecRGB/MTD allowlist widening, shader patch, DrawParam correction, sidecar creation or addon binary change. PTDE pixel OPEN.
