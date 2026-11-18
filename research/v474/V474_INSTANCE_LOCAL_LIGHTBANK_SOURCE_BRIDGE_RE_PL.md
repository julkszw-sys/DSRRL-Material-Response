# V474 — selector LightBank Source1: exact native instance → issued RX34, bez wymuszania wiersza

**Data:** 2026-11-18. **Punkt odniesienia:** wydanie 2.3.1 z PR552, source commit `614a55cda57296f6e460c740f556e534c4114f8b`. **Bazowe RE:** V473 (PR553) oraz V437/V448/V456/V458/V469/V472. Oryginalny DSR `DarkSoulsRemastered.exe` SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.

## 1. Wniosek z istniejącego runtime, którego nie wolno pominąć

Historyczny V448 Source1 RX34 ma **294/294** zweryfikowanych powiązań `issued draw ↔ native builder ticket`: identyczny `FrpgModelDrawEntity*`, jego `FrpgDrawParam*=entity+0x108`, model pointer, packet, sekwencja, Light A/B i beta; **66** issued row30 oraz **228** row31; pozostałych **12** Source2 nie przypisywać. Odrębny V438/V443 census: natywne A=B dla każdego z sześciu renderables, 2 obiekty row30 i 4 row31; są to dwie częściowo nakładające się czasowo trójki. W tym zakotwiczonym przebiegu **nie potwierdzono** nadpisania Source1 inną instancją.

Wspólna tożsamość `FLVER+MTD+slot` nie identyfikuje instancji: V473 wykazał realnym produkcyjnym kodem, że `pmetal_producer_state_latest(material,epoch)` nie umie zadać pytania „instancja A czy B”. To **potencjalna luka semantyczna**, a nie dowiedziony przyczynowy błąd pikseli z V448. Nie klasyfikować row31 jako NPC ani row30 jako gracz.

## 2. Wynik dokładnego statycznego RE EXE

Odtworzony łańcuch źródła/transportu i consumera:
```
FrpgModel+0x10 FULL registration handle
  -> FrpgModel RVA 0x21C970, dirty E0
  -> native opcode 0x804C serializer RVA 0x222AC0
  -> FrpgModelDrawEntity dispatch RVA 0x223380 (case 0x804C)
  -> RVA 0x222B60 + RVA 0x1D7580
  -> TYPED FrpgDrawParam (same FrpgModelDrawEntity+0x108)
      Light lane: entity+0x110 current WORD, +0x112 target WORD
                  entity+0x114 beta float, +0x118 duration, +0x11C elapsed
  -> default same-entity native builder RVA 0x2200D0
  -> issuer RVA 0x220632/0x220639 -> type5/type6
  -> original native renderer draw with SAME packet/model.
```
`research/v474/verify_v474_native_light_instance.py` kontroluje 30 oryginalnych sekwencji instrukcji, 8 krawędzi CALL i 30 negatywnych mutacji, z fail-closed przy innym EXE/SHA. Dodatkowo 30 witnessów ponownie porównano lokalnie do oryginalnego EXE: 30/30. **Nowość V474 nie polega na odkryciu samych RVA** (V437/V458 już to potwierdzały), lecz na przełożeniu ich na konstrukcyjny kontrakt separujący **źródło na dokładnej instancji** od materiałowego cache.

## 3. Nowy, testowalny kontrakt Source1

`research/v474/instance_source_journal.hpp` to odrębna prototypowa implementacja **wyłącznie source/test**, niepoprawiająca działającego addona. Może przyjąć tylko oryginalny, uwierzytelniony emitowany sygnał Source1 i dokładny bilet `native builder / issued RX34`; nie generuje arbitralnych donorów, współczynników, wierszy ani przypisania aktora.

Minimalna pozytywna tożsamość:
- dokładny typ `FrpgModelDrawEntity`, `receiver_id=34`, `route_index=345` oraz pełna źródłowa `material_identity`;
- dokładny wskaźnik `entity`, `draw_param==entity+0x108`, pełny 32-bitowy `entity+0x10` registration handle;
- **ten sam** niezerowy, lokalny `native_builder_sequence` i `selector_epoch` w producer i issued draw; brak zgadywania po czasie;
- natywny Source1 packet/row/c86 uzyskany wyłącznie z odpowiedniego wywołania emitera; **nie** z globalnego latest-material.

Fail-open: rozbieżny `full_handle`, odmienny receiver, niezgodny DP, przestarzały/późniejszy ticket, epoch, materiał, brak obiektu lub alias rejestracyjny = **brak autoryzacji do nowej ścieżki**. Wdrożony przyszły operator island musi w tych przypadkach zachować stock DSR; istniejącego R43 `latest_hook_source` nie usuwamy ani nie zmieniamy semantycznie.

`research/v474/test_v474_instance_source_join.cpp` kompiluje się na **prawdziwych produkcyjnych typach** `material_identity` i `pmetal_envspec_source`. Testuje obiekty o tym samym materiale i różnych row30/31, opóźnione/stare eventy, ABA handle, konflikt payload, inną generację, inny material slot, zmianę receivera, zły DP, zerwanie lifetime i cross-thread bez dzielenia źródła. GitHub Actions V474 Linux C++17 -Werror oraz ASan+UBSan: PASS.

## 4. Pozostałe, rzeczywiście krytyczne ogniwo integracji

Prototyp **nie odbiera** jeszcze rzeczywistych native `0x804C` i V448 issued ticket w produkcyjnym 2.3.1. API produkcyjne `pmetal_env_source_selector_event(owner,ret,r14,r15,stack,material)` nie zawiera dokładnego `FrpgModelDrawEntity+0x10`. W historycznym V448 istniał observational same-instance join, ale nie został przeniesiony do 2.3.1 i nie certyfikuje gameplay owner, GPU pixel ani PTDE expectation.

Następna **jedyna autoryzowalna** zmiana w active addon to zabezpieczona odbiornikowo, native-instance ingress w **tym samym default native builder/emitter** (z V448 ticket), a następnie issued RX34 callsite. Dopiero po połączeniu per-instance ticket w aktywnym runtime można zastąpić material-only Source1 tam, gdzie jest rzeczywisty match. **Nie przestawiać Source2**, nie ustawiać row30/row31 globalnie, nie dokonywać auto-promocji obrazu bez downstream.

Do poprawnej PTDE-visible emisji potrzebny jest dodatkowo operator-local target: rzeczywisty wybrany PTDE LightBank/EnvSpec/EnvDiffuse semantic route i właściwy material/shader/CB/RT downstream. Same zgodne A/B i nr row nie gwarantują pixel equivalence.

## 5. Status i release guard

| Gate | V474 |
|---|---|
| Retail EXE command→typed DP→default builder | **CONFIRMED STATIC** (V437/V458 + 30/30 local replay) |
| V448 same-entity Source1 issued join | **CONFIRMED historical runtime, 294/294** |
| v2.3.1 material-only producer key non-injective | **CONFIRMED SOURCE, V473** |
| V474 per-instance fail-open contract C++/ASan | **PASS construction/test** |
| Native exact-instance ingress do produkcyjnego 2.3.1 | **OPEN, not wired** |
| Rzeczywista zmiana selektora w wydanym addonie | **NO** |
| Właściciel PlayerIns/EnemyIns i PTDE pixel | **OPEN** |

Nie wymagamy nowych zrzutów/logów: wszystkie następne RE są prowadzone na istniejących źródłach EXE, materiałach V448/V469 i publicznie odtwarzalnym kodzie. **Wersja 2.3.1 zamrożona, bez modyfikacji**. Nie powstał nowy `.addon64` ani nowy runtime hook; ten branch jest badawczym krokiem do najwęższego bezpiecznego bridge'a.
