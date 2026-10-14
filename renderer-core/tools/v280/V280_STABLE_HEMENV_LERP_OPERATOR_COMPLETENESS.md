# V280 — Stable HemEnv + HemEnvLerp, jedna ciągłość operatorów PTDE

## Punkt odniesienia i ograniczenia

**Stable (referencja):** kanoniczny finding Supabase
`renderer.pmetal.stable_hemenv_ptde_materialworkflow_bridge_v1`
(rewizja 11133) potwierdza na oryginalnej linii R17,
dla `P_Metal` i właściciela RX33/34/35, runtime activation
material/specular/diffuse/normal/EnvSpec oraz pozytywną ocenę obrazu
u właściciela. NIE potwierdza to wszystkich 24 stabilnych rodzin,
materiałów `P_Leather` ani równoważności przejścia do Lerp.

**Lerp:** odbudowywany R19 z V276/V277; finalne DXBC RX33/34/35
są odtworzone i poddane lokalnemu RE, lecz PTDE replacement PS pozostaje
niepodłączony. Potwierdzono odbudowane EnvSpec/EnvDiffuse i
MaterialWorkflow, a nie zgodność obrazu.

Nie utożsamiać takiego samego semantic receiver ID z tym samym
fizycznym shader SHA. Stable i Lerp mają odrębne oryginalne DXBC.
Wybór materiału, MTD, tekstur, normalnych i resource ownera musi
odpowiadać temu samemu slotowi wyposażenia.

## Macierz kompletności

| Cięcie operatora PTDE | Stable HemEnv | HemEnvLerp | Ciągłość |
|---|---|---|---|
| Diffuse material base | `materialize_ptde_diffuse_response_v1` | ten sam base, family=Lerp | wspólny carrier; wynik końcowy OPEN |
| EnvSpec | `apply_build131` | `k_ptde_rgba_envspec_chain` | różne kodowania; zgodność endpointów OPEN |
| EnvDiffuse | exact t11 RGBA decode i źródło | exact t11/t13 A/B, wspólny blend | A/B w pozycjach krańcowych vs Stable OPEN |
| Material c100/c101 | Stable R17 bez inner DSR cb9→cb10 blend | R19 R15/R16/R17 | pełna material locality OPEN |
| SpecRGB, t10 i stock t1 alpha | Stable Build131 późny material consumer | Lerp `materialize_spec_rgb_consumer` | fizyczny slot i sumowanie materiału OPEN |
| Normals (R5) | jawny `apply_exact_ptde_normal_basis_r5` | brak tego samego jawnego wywołania | równoważny Lerp spatial basis NIEUSTALONY |
| Csd/Sdw Visibility (R7) | jawny `apply_exact_ptde_shadow_visibility_r7` | brak analogicznego jawnego wywołania | branch-specific PTDE visibility OPEN |
| Postmerge (R3/R11) | jawne wyizolowane korekty | odrębna kompozycja R19 | wspólne wyjście preFog OPEN |
| PHN terminal c135 | exact consumer przy R6; producer niepewny | analogiczny exact consumer | producent, skala i piksele OPEN |
| Fog/LS/Glow | niezależne systemy DSR | zachowane instrukcje stock DSR (V271) | zachowanie runtime per shader wymagane |
| Materiał, CB, SRV i sidecar | Stable P_Metal R17 material gate | Lerp wymaga własnego exact draw gate | fizyczny owner na obu stanach OPEN |

Dla zwykłej skóry **nie przepinać** automatycznie udanego Stable
`P_Metal` na `P_Leather`/`P_DullLeather`. Ważna jest też
falsyfikacja dla `Ps_Body`: aktywny tylko w Stable Subsurface
produkował mokre/oleiste ciało i widoczny przełącznik na Lerp.
Projektowy PROTECT wymaga jednoczesnego, zgodnego operatora obu stanów.

## Obowiązkowe inwarianty funkcjonalne

1. Dla tego samego FLVER+MTD SHA+slotu+logical texture i
   kompatybilnych source endpoints sprawdzić
   `HemEnvLerp(t=0, A, B)` kontra `HemEnv(A)` w
   **pre-Fog linear RGB**, następnie w obrazie końcowym.
2. Analogicznie `HemEnvLerp(t=1, A, B)` kontra `HemEnv(B)`.
   Równość wolno uznać dopiero przy identycznych wejściach fizycznych.
3. Dla `0<t<1` zachować rzeczywiste odpowiedzi materiału,
   c101, EnvDiffuse/EnvSpec, normalnych, Csd/Sdw i c110/k135
   bez fałszywego przełączenia operatora.
4. Zmierzyć fizyczny draw gate, aktywne PS, dokładne CB/SRV,
   gałęzie materiałowe i pixel target; sam `prepared` lub
   pozytywny CI nie wystarcza.
5. Materiały nieobjęte autoryzacją i inne rodziny muszą
   przechodzić **fail-open do stock DSR**.
6. Nie wiązać V278/V279 jako gotowej poprawki: realny R19
   posiada już ścieżkę COLOR0 do SpecRGB i dodatkowy MUL
   może błędnie podwoić tint.

`renderer-core/tools/v280/verify_stable_lerp_continuity_contract.py`
weryfikuje wyłącznie obecność źródłowych wywołań operatorów,
ich rozdzielne rodziny i census Stable RX33/34/35. To
**regresyjny Source/Construction PASS, nie pixel PASS**.
Każdy nowy patch Lerp powinien zachować tę macierz lub
udokumentować i dowieść jej zmianę, zanim stanie się
aktywowanym material-local replacement w jednym addonie.
