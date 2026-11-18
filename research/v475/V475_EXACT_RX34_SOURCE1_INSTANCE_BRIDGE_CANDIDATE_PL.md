# V475 — pierwszy aktywny candidate bridge Source1, native-instance / same-builder RX34

**Status:** source-integrated experimental only; v2.3.1 release remains unchanged. **Nie jest to pixel PASS ani dowód naprawionego wizualnego flickeru.**

## Przyczynowy punkt cięcia

v2.3.1 `pmetal_producer_state_latest(material,epoch)` posiada tylko FLVER+MTD+slot+route+semantic; nie posiada pełnego `FrpgModelDrawEntity+0x10` ani ramki buildera. V473 wykazał to na prawdziwym kodzie source, ale V448 wykazał **294/294** historycznych Source1 draw poprawnie związanych z natywnym biletem. Zatem nie można twierdzić, że każdy Source1 jest błędny ani że row31 to automatycznie NPC.

Z oryginalnego DSR (SHA `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`) znamy:
`FrpgModel+0x10 → command 0x804C → FrpgModelDrawEntity+0x108 FrpgDrawParam → native builder RVA2200D0 → exact issued RX34`.
Portujemy *tylko* audytowany V448 live native default-builder frame, nie natywny hook opcode i nie EXE patch.

## Co naprawdę dodano do wyizolowanej gałęzi

W `renderer-core/src/runtime/pmetal_env_source_runtime.cpp`, pod ścisłym `DSRRL_V475_EXACT_LOCAL_SOURCE1`:
- w oryginalnym `pmetal_env_source_selector_event()` po **oryginalnym** `pmetal_producer_state_publish(material,next,epoch)` przyjmujemy autentyczny `next` tylko jeśli bieżący natywny builder zostanie potwierdzony przez V448 Win64 unwind `RVA 0x220CF0`, typy vtable `FrpgModelDrawEntity (0x12ECA10)`, `FrpgDrawParam (0x12E5C08)`, typ native engine model i packet-to-DP A/B/beta;
- dodatkowy, krytyczny warunek nowości V475: **native selector descriptor owner == builder packet FrpgDrawContext** oraz **bit-identical `raw_a/raw_b/beta`** z deskryptora wobec natywnego pakietu (nie tylko ten sam low-byte row);
- odczytujemy cały 32-bitowy, generacyjny handle `entity+0x10`, nigdy low16/low8; łączymy dokładne entity+DP+packet+context+engine_model+frame+handle+selector_epoch+full material SHA;
- przechowujemy jeden `thread_local`, ściśle jednorazowy Source1. Inny selector unieważnia go; zmiana sygnału przy tym samym bilecie zatruwa wpis. Porównanie sygnału obejmuje także PHN k135 i EnvDiffuse carrier.
- `pmetal_env_source_runtime::latest(material,out)` **przed starym material-only lookup** sprawdza dokładnie tę samą natywną ramkę, pełny handle, oryginalny A/B/beta, epoch i materiał. Dopiero exact verdict używa per-instance Source1; pozostałe przypadki przechodzą bez zmian starą produkcyjną ścieżką `material+epoch` i chronionym R43 `latest_hook_source`.

**Scope:** tylko potwierdzony **rzeczywisty** `decision.receiver_id=34` przekazany z `pmetal_envspec_draw_runtime` (negatywne testy RX33/35/36), P_Metal route345, native RX34 default builder, steady `0x51E`/row30 i `0x51F`/row31, beta0, pełna material identity. Inne materiały, inne rows, Lerp, inne receivery, alternatywne buildery i cross-thread bez identycznej ramki nie otrzymują nowego źródła. Fail-open do dotychczasowego baseline, bez zmiany stock renderer ani całej polityki Source2.

**Brak** row30→31, globalnego row lock, gain, zmiany shader/MTD/CB/LightBank PBL/SSS, nowego detoura/draw replay czy modyfikacji DarkSoulsRemastered.exe na dysku. Bazowe V13 DDS/MTD i release-clean v2.3.1 niezmienione poza lokalnym warunkowym transportem w **eksperymentalnej** gałęzi. Bez telemetry burstów.

## Dlaczego to nadal nie jest „celny selektor” w sensie pełnego PASS

1. Win64 caller-frame `RtlVirtualUnwind` był rzeczywiście widoczny dla historycznego V448 (294/294), ale **ten nowy build nie ma jeszcze potwierdzonego runtime event rate i nikt nie dowiódł, że exact candidate staje się active**.
2. Nie wykazano, że material-only cache w aktualnym scene rzeczywiście oddaje zły sygnał zamiast prawidłowego; samo istnienie aliasu jest niewystarczające.
3. Nie mamy per-actor `PlayerIns/EnemyIns`→equipment child mapping. Nie nazywamy row30 „player” ani row31 „NPC”.
4. Exact native DSR selected row nie jest automatycznie PTDE target; bez niezależnego PTDE material/operator/RT/cutscene semantic validation pixel equivalence OPEN.
5. Ścieżka działa **same thread / same live builder only**. Sam materiał, globalny Source2, niepewna ramka lub nieznana generacja nie uzasadniają nowego Source1. Nie ma cross-thread actor repair.

## Certyfikacja source

`renderer-core/tests/test_v475_exact_source1_ticket.cpp` używa prawdziwych production `material_identity` i `pmetal_envspec_source` i falsyfikuje błędne joiny: różne obiekty o identycznym FLVER/MTD/slot ale row30/31, pełny handle, zmieniony packet/context/frame/model, descriptor owner/A/B/beta, epoch, material slot/MTD SHA, stary bilet/one-shot, Source PHN k135, cross-thread TLS i unsupported rows/Lerp. CI Linux C++17 Werror + ASan/UBSan i Windows native x64 one-addon po przejściu workflow.

**Release guard:** PR research pozostaje draft, nie integrować do publicznego v2.3.1 ani nie promować pixel statusu. Bez użytkownika nie wymagać nowych screenshotów/logów; używać istniejących V448, V469, EXE, PTDE sidecars, repo i analizy offline. Każda przyszła aktywacja bez dowodu = status OPEN.
