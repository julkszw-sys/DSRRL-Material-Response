# PTDE archive preflight improvement — V2.1, 2026-10-23

Owner's Windows input `C:\\DarkSouls` lacked `dvdbnd2.bhd5` in the supplied root. This is **source incompleteness or wrong DATA root**, not a ReShade error. The original UXM `DarkSoulsGameInfo.xml` declares four PTDE retail `dvdbnd0..3` BHD5+BDT pairs.

The separate read-only `DSRRL_PTDE_PACKED_ARMOR_MAPPER_V2_1_PREFLIGHT_FIX.zip` adds nested-directory discovery and a complete source report `PTDE_ARCHIVE_PREFLIGHT.json`. Its scanner selects **exactly one** directory with all eight nonempty files. It does not combine BDT/BHD5 files from different installations or substitute missing paths. Source-integrity tests simulate a nested complete `DATA`, missing `dvdbnd2.bhd5`, ambiguous duplicate full installations, and empty BDT.

Reproduction of preflight check: `python renderer-core/tools/ptde_packed_archive_preflight.py --ptde C:\\DarkSouls --json-out PTDE_ARCHIVE_PREFLIGHT.json`.

The existing 1437 archive path-hash candidates are a separate, dictionary-based source census, **not a count of physically extracted textures**. No owner PTDE physical BHD5/BDT scan or C# compiler validation has yet occurred. The additive preflight tool does not alter the existing single DSRRL addon.
