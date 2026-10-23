# PTDE packed armor archive inventory — 2026-10-23

This is a reproducible **source-side archive-path census**, not extracted TPF payloads or renderer pixel validation.

Baseline: v2.3.0 #235 (commit `232ac131fe55c670047bc633e6b13d3c8c95a306`). The addon is unchanged.

* Source path names: `Nordgaren/UXM-Selective-Unpack` `UXM/res/DarkSoulsDictionary.txt`, revision `9501be87e272b6dae55e60a13c3f4753ca6fb3bb`.
* Algorithm: Dark Souls 1 normalized lowercase path with leading slash, 32-bit overflow polynomial hash multiplier **37**.
* Generated dataset: `renderer-core/data/provenance/ptde_packed_armor_archive_candidates_20261023.csv`.
* Generator: `renderer-core/tools/generate_ptde_packed_armor_candidate_map.py --dictionary PATH --output PATH`.
* Static candidates: **1437** PTDE `parts/*.{partsbnd,flver}[.dcx]` armor assets, **958** `_L` / `_M` LOD/mid variants, all kept as distinct original assets.
* Piece families: am=327, bd=333, hd=363, lg=393, fc=21.
* These are **BHD5 path-hash candidates only**; presence in a given PTDE retail archive, actual FLVER material reference, MTD slot identity, PTDE TPF texture hash and installed DSR sidecar availability all require reading physical source assets.

The companion local `DSRRL_PTDE_PACKED_ARMOR_MAPPER_V2` kit reads untouched `DATA/dvdbnd0-3.bhd5/.bdt` files through pinned SoulsFormatsNEXT and this PTDE dictionary. It inventories **mesh-used** material texture slots (Specular, Diffuse, Normals and others), reports missing/colliding logical resources, compares DSR models and installed sidecars optionally, and does not export DDS unless explicitly requested. No manual full PTDE unpack is needed.

Full-game coverage is OPEN until PTDE physical BHD+BDT scan and an authorized EquipParamProtector model catalog join succeed. No source-name alias of PTDE `*_L_s` to DSR base `*_s` is permitted. C# direct-packed reader was not native-compiled in this source-only validation environment.

This branch is **not** an addon candidate and does not affect `DarkSoulsRemastered.exe`, hooks, shaders, or resource routing.
