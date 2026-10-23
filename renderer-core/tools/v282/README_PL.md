# V282 — atomowy HemEnvLerp non-P_Metal: plan rozpoznawania przy ładowaniu

**Status: SOURCE-ONLY / testowy planner, bez nowego .addon64, bez zmiany renderera, PIXEL=OPEN.**
Nie jest to gotowy bridge. Produkcyjny shader wymaga jeszcze operator-local PTDE forward
i dokładnego pierwszego consumera. P_Metal, PointLight i DoF pozostają nietknięte.

## Fakt: trzy różne rodzaje "hash"

1. **Nazwa logiczna / FNV64** — tabela `spec_routes_v12.tsv`, itp. Ten hash to
   identyfikator napisu, nie fingerprint bajtów DDS. Sam nie certyfikuje zawartości.
2. **SHA256 DDS sidecar** — zapisany w historycznym inwentarzu właściciela z 2026-10-10.
   Do runtime authority trzeba jeszcze faktycznie zweryfikować bajty DDS i SRV.
3. **SHA256 surowego FLVER + SHA256 MTD** — ustalają tożsamość ownera i donor
   parametrów. Te SHA powstają podczas parsowania modelu i MTD, nie per draw.

## Fakty w repo

- 769 dokładnych logicznych `SpecRGB` (V12), 528 par `Spec→Diffuse`,
  557 trójek `Diffuse+Spec→Normal`.
- Jeden SpecRGB `bd_m_body_m_s` (hash 5e4fbb58eb8dc682) ma **trzy**
  warianty Diffuse i **trzy** trójki. Nie wolno wybierać pojedynczego
  zestawu po samym `t1`/hashu Specular.
- Dane 2026-10-10 to **inny, szerszy inwentarz plików**:
  1295 Specular, 555 Diffuse, 1098 Normal, z czego 542 duplikaty Normal
  ` (2).dds` zgłoszono jako usunięte; oczekiwane Normal=556.
  Same rekordy z SHA256 nie są obecne w tym branchu, więc planner nie
  może deklarować kompletnego inventory join bez podania rzeczywistego pliku.
- `generated_material_constants_v1.hpp` + `material_response_routes_v1.json`
  dostarczają zweryfikowane per-route `c100`, `c101`, PTDE power, LOD,
  family i receiver triplet. **Nie są tabelą tekstura→MTD**.
- `dsr_flver_owner_tuple_corpus_v1.bin.z`: 3944 exact FLVER i
  19985 slotów. Gate: `raw FLVER SHA + slot + MTD basename hash`.
- `texture_identity_transport` publikuje nazwę w loader scope;
  `material_resource_draw_runtime::on_init_resource_view` już dziś
  może ładować DDS sidecar przy utworzeniu SRV, a TLS i global cache
  indeksują stock SRV. `flver_identity_observe_parse` liczy FLVER SHA
  tylko przy parser event. `observe_exact_runtime_mtd` liczy raw MTD SHA
  tylko przy MTD event. Selector i draw posiadają już niezależne cache.
- Samo `on_init_resource_view` nie gwarantuje prawidłowej asynchronicznej
  identyfikacji loader name→SRV dla wszystkich kanałów i wątków;
  brak join musi fail-open.

## Docelowa ścieżka

```text
LOAD:  exact loader logical_name / stock native SRV identity
       -> name_hash -> manifest texture candidates
       -> verify sidecar DDS SHA only at file load
       -> immutable per-device companion SRV cache (generation/lifetime)
       -> [ready candidates; NOT final material]
MODEL: FLVER parse -> full SHA256 ONCE -> model generation
MTD:   MTD parse -> raw SHA256 ONCE -> material generation
       -> exact FLVER+slot+raw MTD -> material recipe candidate
PS:    shader create -> full DXBC SHA/receiver/CB ABI -> shader bank
DRAW:  cached (model,slot,material,shader,stock t0/t1/t2,resource generation)
       -> join exact tuple and consumer (no texture-only inference)
       -> atomic PS + CB + SRV + sampler transaction or STOCK DSR
```

Params must be keyed by **actual material and receiver**, never SpecRGB name alone.
The per-draw key should include engine object+generation, actual model+generation,
slot, verified material/MTD, receiver shader identity, binding SRV generation and
backend device epoch. Hashes live in immutable/static metadata; do not run
SHA256 per draw, and never reuse a pointer across destroy/recreate without epoch.

For unknown/mismatched MTD/FLVER, unsatisfied sampler/CB ABI, alias/conflict,
different shader family, material with nonhomologous assets, wrong sidecar SHA,
stale cache generation, missing PTDE source/consumer or postprocess mismatch:
**stock DSR / fail-open**.

## Koszt i pułapki wydajności

- Avoid creating/decoding every one of 2406 potential DDS GPU resources at
  process startup. Precompute metadata, then realize first-needed sidecar with
  a **bounded** budget; reuse one SRV per logical resource/device where valid.
- The current DDS loader performs synchronous file I/O + CreateTexture2D and
  CreateShaderResourceView in the resource-view callback. Moving it blindly
  into an arbitrary worker may violate D3D11 device/context threading and
  loader lifetime; if staging is required, do file I/O/decode separately,
  create GPU resources only on a verified safe device event, and never block draw.
- Misses should be cached with a generation and retried only on appropriate
  resource/version/lifetime changes. Never accept a stale negative cache after
  parser publication. Avoid invalidating all unrelated material cache entries
  when an unrelated FLVER is unloaded; project R44 addresses that class
  separately, do not revive old performance bisects.
- Event-driven early lookup is cheaper than repeated per-draw physical-texture
  hashing. A texture hash by the renderer is **not necessarily the original
  logical name or a stable D3D11 SRV identity**. Proven linkage is mandatory.

## Guarded implementation sequence

1. Run `compile_prebind_candidates.py` on the three retained V12 TSV files and
   verified MTD donors. It emits **candidates, not authorization**.
2. Supply normalized exact 2026-10-10 inventory JSON only if the original
   manifest records are available. Detect conflicts and report coverage;
   don't invent SHA values or material joins.
3. Build producer-time prebind record for **one** exact Leather FLVER+slot+MTD
   and its exact t0/t1/t2 tuple; verify source resource identity and lifespan.
4. Reconstruct complete PTDE **non-P_Metal** MaterialWorkflow/SpecRGB/EnvSpec
   per RX33/34/35; stable endpoints are controls. No copy/paste P_Metal equation.
5. Add atomically gated native draw transaction into the existing **single
   .addon64** only once consumer+materials+resources are certified.
6. Benchmark selector events/s, CPU time, DDS load hitch, GPU cost and
   material-specific PTDE pixel residual separately. Protect P_Metal.
7. Never modify stock EXE on disk; preserve PointLight/DoF workstream ownership.

`compile_prebind_candidates.py` reads all actual repo TSV/MTD constants,
lists ambiguous candidates and clearly marks `activation_authorized=false`.
Nothing from this source-only PR is implicitly a shipped or runtime-active build.
