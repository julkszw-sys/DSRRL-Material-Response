# DSRRL PointLight R31 — DirectPointLightEntity source attestation

Date: 2026-10-05
Parent runtime-tested source: `f2162d3dd1bad075f7a8f8979540e3d4ed187bf0`
Active lineage: `r19-lerp-exact-envdiffuse-ab-beta`

## Runtime trigger

R30 evidence `75a46173-1567-40f7-aa05-60e0f25d6377` proved native PointLight application, then exposed one blocking target fail-open:

- source_id `2164263363` (`0x81000003`)
- category `1`
- DSR source vfunc `base+0x55C570`
- `source_class_unattested`
- downstream `clustered_sidecar_not_ready` with material/operator gate authorized

## Static cross-version attestation

Canonical binaries:
- DSR artifact 20, SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`
- PTDE artifact 109, SHA-256 `88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d`

DSR RVA `0x55C570` and PTDE VA `0x00D34D50` emit the same semantic carrier:

`{ position.xyz, 1/(End-Begin), RGB/source signal, End }`

PTDE object members are at +0x40/+0x44/+0x48, +0x4C/+0x5C, +0x50/+0x54/+0x58.
DSR members are shifted by +0x10 in the 64-bit object layout. The reciprocal near-zero fallback is also homologous.

Canonical finding:
`renderer.pointlight.direct_source_cpu_carrier_homology_v1`
(`b18bae1b-f7ca-4862-a2c7-840d3edb06b7`).

## R31 rule

Only DSR `base+0x55C570` is added to source attestation.
Existing BankPointLightEntity `+0x55BC00` and LerpBankPointLightEntity `+0x55D0B0` remain unchanged.
Every other source identity continues to fail open.

DirectPointLightEntity skips the PointLightBank donor lookup because it is not a bank source and its host packer is itself the PTDE-homologous carrier.

No change is made to selector membership, attenuation math, material authority, transaction semantics, U/L, HemDir3, Subsurface, EnvSpec, Material Response, or pixel status.
