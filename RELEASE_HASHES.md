# Release hashes

## Material Response 1.45 — exact Nexus release

### Nexus ZIP

`DSRRL_Material_Response_1.45.addon64(20260923-005017).zip`

Size: `244,154 bytes`

SHA-256:

`2439d24644a0dc5bef6e29b1d980bab5b421a08c6bc71fca93d4c88859ad0893`

### Addon contained in the ZIP

`DSRRL_Material_Response_1.45.addon64`

Size: `1,803,264 bytes`

SHA-256:

`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

Version: `1.45.0.0`

PE checksum: `0x001BC666`

### Clean intermediate before final terminal-SAT delta

SHA-256:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

PE checksum:

`0x001BE0F4`

### Integrated basis used by the release builder

SHA-256:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

### Final SAT DXBC hashes

- 33: `ee2511b2c3c6a822c921ad0ca5ef9eaf78b05d1992ce88851b5ecae95601c872`
- 34: `3cb53c033f61ef7664be097373c87d1a5f0933c2d3342feb6c50a63b5e3b49dd`
- 35: `71b973e36cb2ebbabc05455c1f882653ad39532644051d7d1e2e045874d427d3`

### Integrated dedicated DXBC hashes retained in the release

- 72: `159e9bbcb36c110e0e6e223f740986844cc8f0882d472afaf5b65abee4301e98`
- 73: `726461e5308788f75e7dba4e0e7c85e9fcc801faec38c060635dff40fb9ae877`
- 74: `11a405536a0600037b11817579a29cf5663bc62e8b17955af255d279e03417c7`

### Release exclusions

No active PackedGI/EnvSpec resource replacement or EnvSpec loader is part of the final release path. Activation telemetry, periodic counter/status telemetry and startup shader-count telemetry are disabled/removed; safety/error/fail-open logging remains.

The previous `3dcb50...` value is retained above only as the deterministic clean intermediate. It is **not** the Nexus shipping addon.
