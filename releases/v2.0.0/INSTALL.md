# Install DSRRL v2.0.0

1. Close DSR. Keep exactly one DSRRL .addon64 beside DarkSoulsRemastered.exe.
2. Use ReShade 6.8.0.1 x64. Do not patch the game's executable.
3. Place PTDE PackedGI assets under DSRRL/EnvSpec/PackedGI. Use the hashes in RELEASE_MANIFEST.json.
4. Place equipment DDS in DSRRL/Specular, DSRRL/Diffuse and DSRRL/Normals (plural). Existing BHD5 extractor outputs Normal (singular); copy those DDS into Normals for the runtime loader.
5. Missing or unauthenticated resources fail open to stock DSR.
6. Keep PointLight, Upper/Lower, HemDir3 and Subsurface physically cut as in the owner-tested release baseline.
7. Binary internal 2.0.0-dev identity is preserved to keep exact SHA256 of the tested build.

This public release requires users to obtain PTDE assets from their own installation.
