# DSRRL Renderer Edition — PTDE Material Response V2.11

## Cel
V2.11 jest kolejnym **material-response diagnostic**, nie poprawką EnvSpec.

V2.9.1 diffuse/c100 zostaje bez zmian. V2.10 dodał analityczny PTDE `c101` przez wejście DSR pre-F0:

`q = c101_PTDE^(1/2.2)`

przy stockowym:

`F0_V210 = SAT(SpecTex * q)^2.2`

Offline audit rzeczywistych `_s` textures z retained evidence wykazał, że stockowy SAT jest realnym ograniczeniem dla materiałów `Leather` i `Metal`, a nie tylko teoretyczną granicą.

Dla `Metal c101=2.5` granica wynosi:

`SpecTex <= 2.5^(-1/2.2) = 0.659353...`

Dla `Leather c101=1.5`:

`SpecTex <= 1.5^(-1/2.2) = 0.831684...`

W bezpośrednio przypisanym retained subset 4/5 unikalnych metalowych `_s` i 3/3 leather `_s` przekracza tę granicę; dla większości z nich dotyczy to co najmniej jednego kanału na >90% pikseli.

## Jedyna nowa zmiana V2.11
Na 24 stabilnych no-PointLight `Phn Spc HemEnv` hostach:

`MUL_SAT -> MUL`

wyłącznie w **jednym kanonicznym F0 material constructorze**.

Czyli:

`F0_V211 = (SpecTex * c101_PTDE^(1/2.2))^2.2`

a dla nieujemnego sygnału:

`F0_V211 = SpecTex^2.2 * c101_PTDE`

bez projekcji do `F0<=1`.

W każdym z 24 shaderów zmieniany jest tylko bit SAT w jednym instruction tokenie:
- V2.10: `0x08002038`
- V2.11: `0x08000038`

DXBC checksum jest przeliczony.

## Co nadal pozostaje stock DSR
- SPEC material exponent `2.2`,
- SpecTex RGB i alpha,
- roughness,
- dynamic EnvSpec LOD,
- native DSR cubemap,
- angular/horizon,
- BRDF LUT / split-sum,
- LightBank EnvSpec,
- PointLight receiver,
- HemEnvLerp,
- postprocess.

To nie jest próba przebudowy EnvSpec.

## Donor / routing
Identyczne jak V2.10:
- c100 registry = 368,
- c101-enabled = 306,
- Tier2 = c100-only,
- 24 stable HemEnv hosts,
- HemEnvLerp = stock DSR.

## Interpretacja
V2.11 testuje bardzo wąską rzecz:

**czy PTDE-like liniowy material-specular gain powinien pozostać nieograniczony zanim wejdzie do stockowego DSR PBL.**

Jeżeli wynik będzie lepszy niż V2.10, stock F0 bound był rzeczywistym residualem material-response.
Jeżeli będzie przepalony/gorszy, oznacza to, że PTDE `c101` nie powinien być reprezentowany jako nieograniczone F0 i trzeba przenieść liniowy gain na późniejszy, ale nadal materialowy cut wokół stockowego PBL response.

Nie zmieniamy EnvSpec source/cubemap/LOD ani HemEnvLerp.

## Build Windows
Uruchom:

`BUILD_PTDE_MATERIAL_RESPONSE_V2_11.cmd`

Wynik:

`build-msvc\bin\DSRRL_PTDE_MATERIAL_RESPONSE_V2_11.addon64`

## Audyty
- `audit/DSRRL_C101_NATIVE_SAT_REACHABILITY_AUDIT_V1.json`
- `audit/V211_C101_UNBOUNDED_AUDIT.json`
- `scripts/audit_v211.py`
- inherited V2.10/V2.9.1 audits for donor/routing provenance

Runtime/final visual fidelity: OPEN.
