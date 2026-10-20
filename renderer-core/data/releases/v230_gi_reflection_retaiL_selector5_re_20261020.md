# Exact GI_Reflection retail producer branch — 2026-10-20

Status: original executable static **CONFIRMED**; new native observation **UNTESTED** pending owner runtime. No resource or pixel equivalence claim.

## Exact exe and source path

Owner DarkSoulsRemastered.exe SHA-256: `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Original RVA `0x2017EE` is `cmp r14d,5; jne 0x20180A`. When selector **5**, RVA `0x2017F4` executes `mov [rsp+20h],r15d; lea rdx,[GI_Reflection name]; call 0x1400CF290`. Exact stolen 17 bytes: `44897c2420488d1540850e01e88bdaecff`. The wide name constant `GI_Reflection_%s_%02d_%04d` has RVA `0x12E9D40` and is referenced exactly from this call. Another branch at `0x20181C` formats `GI_EnvSpc_%s_%02d_%04d` (selector 1–4), while distinct RVA `0x201D76` formats GI_EnvCapture. Distinct DSR loader identities exist for GI_Reflection map BHD and binder names.

This is a concrete engine producer family classification, not a name string alone. In the confirmed GI_Reflection branch, `r9d` holds the quotient `packed_id / 10000`, and `r15d` the remaining probe local value. The exact native BC6H t12 resource SHA `8dcb619433174de2ea4b9efbb9686fc25166f26ac2401cbee61e3013a40c94a5` is **not yet linked to this branch**.

## Next bridge experiment

One opt-in native witness uses existing exact exe SHA, validates the 17 bytes before arming, observes only GI_Reflection branch, and reconstructs stolen `mov/lea/call` then resumes original instruction. First twelve observations and sparse further hits log producer selector/subarea/local/packed. No input is assigned to a PTDE cubemap, no shader/CB/asset or resource state changes. Unknown remains fail-open; existing t12/t14 exact atlas of 342 GI_EnvSpc is preserved. Empty branch log means not observed during session (not proof loader never exists). ReShade addon runtime and pixel **OPEN** until owner game test.

## Existing evidence and negative scope

DSR supplied ZIPs contain m10–m18 `GI_EnvM` binder families but not independently named `GI_Reflection` or `GI_EnvCapture` asset source files. Supabase:
- canonical rev13704: 15 unknown BC6H hash not in 342 GI_EnvSpc source atlas
- rev13710: DSR distinct GI_Reflection loader source families static confirmed
- rev13711: assigning those hashes to GI_Reflection remained a hypothesis
- rev13789: owner v230 actual t12 SHA and exact no-probe certification
- 342 common GI probe identities and 5 PTDE-only EnvDiffuse resources remain separately certified, do not conflate.

Do not infer that GI_Reflection equals PTDE EnvSpc by visual appearance or material family. Source family -> resource creation -> actual native t12 must be separately attested prior to any bridge patch.
