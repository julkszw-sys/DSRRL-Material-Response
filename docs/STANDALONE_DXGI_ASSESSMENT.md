# Standalone DXGI assessment

The current DSRRL package uses two different binary roles:

- `dxgi.dll` — ReShade 6.8.0.1 proxy/runtime
- `DSRRL_Material_Response_1.45.addon64` — DSRRL ReShade addon

These are not two halves of one DSRRL library. The addon imports and registers against the ReShade addon API, while the proxy implements the DXGI/D3D entry surface and loads addons.

The supplied Dark Souls II Lighting Engine `dxgi.dll` is architecturally different: static inspection shows a custom DXGI/D3D proxy surface (including `CreateDXGIFactory*`, `D3D11CreateDevice*` and `DarkSoulsHookVersion`) rather than a simple DSRRL-style addon.

Therefore a true one-file DSRRL release would require one of two explicit projects:

1. a maintained ReShade fork with Material Response linked/registered in-process, or
2. a standalone DSR DXGI/D3D11 proxy that reimplements the hooks/events currently supplied by ReShade.

Binary concatenation/embedding of the current addon into stock ReShade is not treated as release-safe. It would expand the compatibility surface and obscure ownership of hook/state restoration.

For the 1.45 clean release, the narrow safe architecture remains stock ReShade `dxgi.dll` plus one clean DSRRL `.addon64`. A standalone proxy can be developed separately without destabilizing the release branch.
