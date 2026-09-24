# Renderer Core v1 — SpecRGB Resource Loader Contract

Status: **RE/design closed; Renderer Core implementation pending**  
Canonical: `renderer.core.islands.specrgb_resource_loader_architecture_v1` (rev 8895)

## Scope

This contract belongs only to `material_response.spec_rgb` on stable no-PointLight Phn Spc HemEnv receivers **24..47**. It does not authorize EnvSpec, Subsurf, Normal, Diffuse, PointLight, HemEnvLerp or pixel-equivalence claims.

## Proven source -> consumer chain

1. **Source identity** — capture the exact DSR logical texture name at the certified UTF-16 lookup boundary (`0x140583AB7`, scoped to `0x140583E81`). Do not infer a PTDE name from filenames, material family, receiver id or hash aliases.
2. **Producer** — resolve the exact-name PTDE DDS companion from the canonical `DSRRL/Specular` sidecar namespace. Existing confirmed lineage preserves supported BC1/BC7 mip payloads and creates addon-owned D3D11 Texture2D/SRV companions.
3. **Association/transport** — associate the stock DSR SRV identity with its exact PTDE companion. Stock DSR `t1` remains bound and authoritative for alpha/roughness. PTDE SpecRGB is an independent resource transported on `t10`.
4. **Consumer gate** — reuse the existing FULL24 route contract: receiver 24..35 = DifSpcBmp; 36..47 = DifSpc; actual material verified; real specular consumer verified; exact-name PTDE companion verified; companion/SRV ready; native t10 transport ready; stock t1 preserved.
5. **Composition** — the SpecRGB island supplies PTDE RGB only. Material-response c101/c100, EnvSpec, local PointLight, terminal SAT and downstream composition remain independently owned islands.
6. **Fail-open** — missing logical identity, ambiguous association, missing/unsupported DDS, failed GPU resource creation, unsupported receiver/material, absent specular consumer, unavailable t10 transport or any t1-preservation failure => preserve stock DSR and do not partially activate SpecRGB.

## Evidence boundary

Historical construction established the exact-name producer and addon-owned companion-resource architecture; historical runtime telemetry `T10_PTDE_BIND` established that a non-stock PTDE companion reached the live t10 consumer path. This closes the **loader architecture / carrier selection** question. It does **not** prove that a new Renderer Core implementation is live, that every FULL24 route activates, or that PTDE-visible pixels are equivalent.

## Narrowest implementation carrier

`exact logical-name resolver -> addon-owned companion SRV cache -> guarded t10 bind/restore`

No second addon, no shader-name inference, no replacement of stock t1, no blanket resource substitution, and no pixel-status promotion.

## Remaining gates

- port the proven producer/cache path into the source-complete Renderer Core addon;
- bind/restore t10 only under the existing FULL24 route gate;
- deterministic tests for identity ambiguity, unsupported DDS/resource creation failure and t1 preservation;
- Windows/ReShade CI construction/compatibility;
- runtime per-route activation remains separate;
- PTDE-visible pixel equivalence remains OPEN.
