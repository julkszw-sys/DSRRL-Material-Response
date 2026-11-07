# DSRRL V445 — native FrpgDrawParam writer-family / owner RTTI census (offline RE)

**2026-11-07.** Target: PTDE-visible armor lighting; stock DSR host. Nie wprowadzono patcha ani nowego `.addon64`. Dowód: niezmodyfikowany `DarkSoulsRemastered.exe`, SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Wszystkie adresy to **RVA**, image base `0x140000000`. Status **STATIC CONFIRMED** dla podanych bezpośrednich `E8`, **OPEN** dla pośredniego CFG, live aktora, RX34/RTV, PTDE pixel.

## Nowy wynik 1 — brakujący wariant trzykanałowego setter-a

V444 badał `0x1D7580`, ale istnieje również `0x1D7600`. Weryfikacja prologu oraz trzech callsite `0x1D762D/0x1D7646/0x1D7664` do `0x1D7820` dowodzi, że otrzymuje **trzy wskaźniki do 16-bitowych selector targetów** oraz wspólny duration w XMM1, aktualizując kanały `dp+0x08/+0x18/+0x28`.

Pełny **surowy bezpośredni E8 candidate census** dla `0x1D7600` w obu wykonywalnych `.text` to 7 adresów:

| Caller RVA | Przekazane `this` | Ustalony, oryginalny typ obiektu |
|---|---|---|
| `0x574D48`, `0x5751B4`, `0x575407`, `0x5759CA`, `0x575C1D` | `r13+0xE8` | **`NS_FRPG::RemoPartsIns`**, funkcja `0x5749D0` |
| `0x576B9F` | `r15+0xE8` | **`NS_FRPG::RemoPartsIns`**, funkcja `0x5767C0` |
| `0x705113` | `rsi+0x110` | **`NS_FRPG::EzMenuAsmMdlIns`**, konstruktor `0x704ED0` |

Dowód owner class nie opiera się na zgadywaniu po offsetach. `RemoPartsIns` ctor `0x5741C0` ustawia vtable RVA `0x137EA10` i inicjalizuje DP pod `this+0xE8` poprzez `0x574270→0x1D7490`. `EzMenuAsmMdlIns` ctor `0x704ED0` ustawia vtable RVA `0x13CBA80` i inicjalizuje DP pod `this+0x110` poprzez `0x704F4A→0x1D7490`. Oba vtable mają poprawny MSVC RTTI `CompleteObjectLocator`, który zwraca dokładne powyższe nazwy; nie jest to `FrpgModelDrawEntity` DP pod `this+0x108`.

**Granica:** To dowód dla bezpośrednich callerów i tych owner layouts. `RemoPartsIns` jest znaną osobną ścieżką REMO/cutscene, `EzMenuAsmMdlIns` ścieżką modelu menu; nie ma dowodu, że wykonują się dla obserwowanej w V439 zbroi gameplay. Nie wolno na tej podstawie utożsamiać `0x1D7600` z przyczyną flicker ani odwrotnie wykluczać downstream transferu do jakiegoś modelu.

## Nowy wynik 2 — rozdzielenie operacji COPY od RETARGET/TICK

Oryginalny `0x1D7680` przyjmuje `RCX`=docelowy `FrpgDrawParam`, `RDX/R8/R9`=trzy źródłowe pakiety 0x10 B. Kod **bez `0x1D7820`** kolejno zapisuje do `dp+0x08..0x17`, `+0x18..0x27`, `+0x28..0x37` każde `A WORD`, `B WORD`, `beta FLOAT`, `duration FLOAT`, `elapsed FLOAT`. To **bezpośrednia kopia raw transition state**, nie rekonstrukcja na podstawie nowego celu ani pojedynczy intensity gain. Bezpośrednie E8 w obu `.text`: `0x4FC015`, `0x4FD71A`, `0x4FDBC9`, `0x4FEB6A`.

Współwystępują z wcześniejszymi potwierdzonymi w V444 klasami `FrpgFxClusterAppearance_Model` i `FrpgFxParticleAppearance_Model`. Przykłady exact dest: `0x4FC002` przygotowuje `RCX=RBX+0x158` (= FX cluster inner+0x50, DP inner+0x108), `0x4FDBB6` przygotowuje `RCX=RBX+0x138` (= FX particle inner+0x30, DP inner+0x108). **V443 model vtable = właściwy pod-obiekt FX, ale nie tożsamość gameplay armor.** Nie wolno zakładać, że wszystkie wartości A/B/beta przeszły przez setter `0x1D7580/0x1D7600`, jeśli istnieje copy/deserialize.

## Nowy wynik 3 — odróżnienie 5-channel updatera od MODEL DP

Funkcja `0x2D1C90` zawiera pięć jawnych wywołań tick `0x1D77E0`: `0x2D1F8B`, `0x2D1F9A`, `0x2D1FA9`, `0x2D1FB8`, `0x2D1FC7` z jednym delta `XMM6` i `RCX=RSI+0x1B0/+0x1C0/+0x1D0/+0x1E0/+0x1F0`. Cztery? Nie: **pięć** osobnych 0x10 kanałów. Ostatni offset przekracza standalone alokację `FrpgModelDrawEntity` `0x1C0` B, więc nie jest to tick pierwszego DP (`entity+0x108`) w tym samym samodzielnym obiekcie. Nie ustalono RTTI większej klasy ani związków pośrednich z modelem ASM.

Odrębny `0x2D3986` wywołuje `0x1D7820` pięciokrotnie dla analogicznych kanałów. Te ścieżki nie są dowodem per-frame native armor selector writer.

## Korekta interpretacji V444

`0x219D8F` oraz `0x222D5F` wywołują `0x1D7530` po pobraniu float ze **strumienia deserializacji**, a następnie tail-call tick `0x1D77E0` do innych pól `entity+0x1F0` / `entity+0x164`. To nie dowodzi, że dana funkcja aktualizuje LightBank zbroi co klatkę. Ponadto `0x1D7530` jest wejściem do obfuskowanej ścieżki `0x21AA328→0x21B730B→0x296DCA0`; nadal nie ma pełnego transitive CFG.

`0x1D7580` ma tylko trzy bezpośrednie E8 (`0x0C0240`, `0x219B2B`, `0x222BBB`); dotychczasowe dwa modelowe to serializowane wejścia `entity+0x108`. Nowy wariant `0x1D7600` nie dopisuje żadnego **bezpośredniego** E8 do `FrpgModelDrawEntity+0x108`. W DSR mogą jednak istnieć inne writers: inlined, memcopy, indirect, VM/virtual dispatch, stany odziedziczone z parenta. Nie ogłaszamy globalnej kompletności CFG.

## Co dalej

1. W przyszłym offline RE szukać rzeczywistego **source writer for `FrpgModelDrawEntity+0x110/+0x112/+0x114`**, z rozróżnieniem `EnemyIns/ChrModel` vs inne owner, oraz pass invocation. Bez dowodu nie przypisywać mutacji kamerze.
2. V443 pozostaje istniejącym diagnostykiem. Runtime dopiero po odzyskaniu `actor generation` i spójnym `native producer → builder invocation → RX34 issued pass/RT/CB/SRV` rozróżni native upstream vs association vs consumer.
3. Nie zmieniać kodu renderera, `latest_hook_source`, Source1 ani row30/31; żadnego blanket gainu, bez nowej gate/whitelist. Nie ruszać V438 wrapper detours. V359-RC1 frozen root.

## Weryfikacja odtwarzalna

`python verify_v445_drawparam_writer_census.py DarkSoulsRemastered.exe --mutations --json V445_RE_RESULTS.json`

Zależność: dołączony niezmodyfikowany parser PE z V444. Weryfikuje 38 opcode preimages, **7 grup bezpośrednich raw E8**, dokładne owner RTTI x4, all owner DP offsets, konstruktor DP i 86/86 negatywnych mutacji. Logi i weryfikator nie modyfikują EXE. Wymagane 5 poziomów: STATIC/CONSTRUCTION=PASS; Win64 compatibility=NOT_APPLICABLE (no build); runtime liveness=NOT_TESTED; bridge activation=NOT_APPLICABLE; PTDE pixel=OPEN.