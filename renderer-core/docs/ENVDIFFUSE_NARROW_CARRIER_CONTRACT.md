# Renderer Core v1 — EnvDiffuse Narrow Carrier Contract

Status: **HIGH CONFIDENCE carrier boundary; assignment-gated**  
Canonical: `renderer.core.islands.envdiffuse_narrow_carrier_v1` (rev 8913)

## Scope

This contract belongs only to `surface.envdiffuse`. It does not authorize EnvSpec, Upper/Lower, PointLight, material-domain, postprocess, runtime-activation, or pixel-equivalence claims.

## Source -> producer -> transport -> consumer -> composition -> downstream

1. **Source / producer** — on the supported DSR executable, the operator-clean EnvDiffuse source cut is the output of the single/blended profile packers (`0x140563B80` / `0x140563C30`) before the later draw multiplier. Bridge only EnvDiffuse A/B XYZ through the confirmed inverse `q^(1/2.2)`; preserve alpha/beta and independent EnvSpec lanes.
2. **Resource producer** — PTDE EnvDiffuse additionally requires the exact assigned GI EnvDiffuse probe resource. Probe A and B are independent from the CPU endpoint bridge.
3. **Transport** — endpoint state remains a CPU/source-domain semantic carrier; probe A is transported on `t11/s11`, probe B on `t13/s13` for the applicable HemEnv/HemEnvLerp path.
4. **Consumer** — only certified EnvDiffuse receiver bodies may consume the bridge. Representative PTDE Phn HemEnv RE samples the assigned probe with the final shaded normal and decodes `RGB/A`.
5. **Composition** — the local EnvDiffuse term is environment visibility × decoded probe × bridged profile endpoint before the common legacy diffuse-material product. Independent diffuse lighting, specular, Upper/Lower and PointLight remain separately owned islands.
6. **Downstream** — preserve the existing receiver continuation, atmosphere/fog/terminal output and postprocess unless independently certified by their own islands.

## Narrowest carrier

`pre-draw-multiplier EnvDiffuse endpoint bridge + exact assigned PTDE GI probe SRV/sampler sidecar (t11/t13)`

Do not root already draw-multiplied GPU endpoint vectors, replace whole shaders merely to transport EnvDiffuse state, infer a scalar probe transfer, or globally substitute GI resources.

## Fail-open gates

Preserve stock DSR if any of these are unresolved or ambiguous:

- exact legacy probe assignment / resource ownership;
- exact PTDE companion resource identity;
- exact D3D11 sampler descriptor for `s11` / `s13`;
- receiver-family applicability / HemEnv vs HemEnvLerp route;
- endpoint/profile identity or beta preservation;
- resource lifetime or draw-local restore.

## Remaining work

- close legacy probe assignment and exact resource ownership;
- recover/certify the `s11`/`s13` D3D11 sampler descriptors;
- certify receiver portability beyond the representative body;
- implement source-complete resource ownership and draw-local bind/restore;
- runtime activation proof remains OPEN;
- PTDE-visible pixel equivalence remains OPEN.
