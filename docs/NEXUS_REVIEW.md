# Nexus Mods review notes

Mod: **Dark Souls Remastered – Restored Lighting**  
Nexus mod ID: **1423**  
Component: **Material Response 1.45**

## File under review

`DSRRL_Material_Response_1.45.addon64`

SHA-256:

`41690c6212157eb772ae0c75c055d0bb7a842709f65f02c3689b87714151e1f7`

Size:

`1,803,264 bytes`

Type:

64-bit ReShade addon / PE DLL.

## Purpose

The addon changes selected rendering behaviour inside Dark Souls Remastered. It handles material response and a small number of verified texture/resource paths, including PTDE-derived SpecRGB, diffuse/normal assets and EnvSpec cubemaps.

It is not a launcher or installer.

## Why heuristic scanners may flag it

The addon runs in-process with the game and performs renderer hooks, D3D11 resource tracking, draw-scoped resource substitution/restoration and controlled memory-protection changes. Those operations are normal for this mod but can overlap with generic malware heuristics.

Observed labels include:

- `Gen:Variant.Barys.441135`
- `Trojan.Barys.D6BB2F`
- `Dll.unknown.barys`
- `Trojan:Win32/Wacatac.B!ml`

I also tested a build with the external EnvSpec loader disabled; the general detection pattern remained.

## Source/build material

Relevant files in this repository:

- [BUILD.md](../BUILD.md)
- [SECURITY.md](../SECURITY.md)
- [src/envspec_loader.c](../src/envspec_loader.c)
- [tools/build_release.py](../tools/build_release.py)
- [tools/verify_release.py](../tools/verify_release.py)
- [reference/loader_bytes.hex](../reference/loader_bytes.hex)
- [RELEASE_HASHES.md](../RELEASE_HASHES.md)

The release build step is reproducible from the exact development basis and exact PackedGI resource listed in `BUILD.md`. The resulting file is checked against the public release SHA-256.

The development basis itself is an intermediate addon binary from the project's incremental development process and is not stored in this repository. I can provide that exact basis to Nexus staff if required.

## External asset access

Material Response 1.45 reads one fixed EnvSpec resource from the DSRRL directory:

`DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin`

The file must match both the expected size and SHA-256 before the EnvSpec path is enabled.

## Review request

I am requesting a manual review of the quarantined file. I can provide the exact development basis, VirusTotal report or any additional build material if needed.
