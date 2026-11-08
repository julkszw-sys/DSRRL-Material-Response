# DSRRL V452 — właściciel postaci a row30/row31 (RE statyczne)

**Cel:** zweryfikować hipotezę, że ciemny LightBank row31 widoczny na zbroi gracza w słońcu pochodzi od NPC stojącego w cieniu. **Werdykt:** **HYPOTHESIS / OPEN** — nie wykazano przekazania LightBank NPC do pikseli gracza. Nie zmieniono wyboru wierszy ani kodu renderującego.

## Źródła i dowód

Oryginalny, nieruszany `DarkSoulsRemastered.exe` (PE64 image base `0x140000000`) SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Samodzielny RE verifier `verify_v452_chr_owner_graph.py` (+ odtwarzalny helper `verify_v444_actor_lifecycle.py`) sprawdza **78 dokładnych preimage instrukcji, 14 bezpośrednich celów CALL, 7 RTTI, 4 sloty vtable i 91/91 mutacji negatywnych**. `V452_RESULTS.json` jest wynikiem uruchomienia na powyższym EXE. Badanie jest statyczne; nie udaje testu runtime ani PTDE pixel.

Dane poprzedniego właścicielskiego V448 CSV (`V438_20261107_231505.csv`, SHA-256 `744d3af90a4f99eff8e7d22d8e78c2c6ced2e09141bda3ea0a23906d3e7d4329`): 294/294 obserwowanych RX34 Source1 zachowuje **tę samą natywną FrpgModelDrawEntity, packet, sequence i row** po stronie producenta i issued draw (66 row30, 228 row31). Pozostałe 12 row30 to Source2 i nie mają certyfikatu native issued. Sześć instancji model-draw (2 row30, 4 row31) współdzieli FLVER i `FrpgDrawContext`. W 27 z 67 bucketów CPU obecne są obydwa wiersze w tych samych slotach materiału. Równość natywnego model/packet nie jest tożsamością postaci gameplay ani dowodem nakładania na te same piksele.

## Potwierdzona typowana ścieżka do modelu NPC

| Funkcja/operacja (RVA) | Ustalenie |
|---|---|
| `EnemyIns` ctor `0x349620`, vtable `0x1322E68` | Dokładny typ RTTI `EnemyIns` (podklasa `ChrIns`) |
| vtable slot +`0x40` → `0x349FD0` | Wirtualna ścieżka inicjalizacji modelu EnemyIns |
| `0x34A173` → `ChrModel` ctor `0x20B0B0` | Tworzy model postaci |
| `0x34A17F` | Zapisuje wynik jako `EnemyIns+0x60 = ChrModel*` |
| `ChrModel` vtable `0x12EAF30`, `0x20B152` → `0x21C3B0` | Dziedziczy konstruktor `FrpgModel` |
| `0x21C554` → `0x157CF0` → `0x15A8E0` | Rejestruje typowany `FrpgModelDrawEntitySeed` |
| Seed factory vtable `0x12CC310` → `0x223AC0` | Buduje `FrpgModelDrawEntity`, z własnym embedded `FrpgDrawParam` +`0x108` |

**Oddzielny `PlayerIns`** RTTI vtable `0x13251F0` slot+`0x40` → `0x355890`. Jej metoda jest obfuskowana i nie udowodniono analogicznego offsetu modelu. Nie wolno bez dowodu uznać `PlayerIns+0x60` za to samo co `EnemyIns+0x60`.

## Natywne zapisy selektora dla modelu wskazywanego przez właściciela

- `0x326320`: jeśli wejściowa rodzina jest nieujemna i różna, czyta `owner+0x60`, przepisuje rodzinę `FrpgModel+0x92` (trzy dolne bity zachowane), ustawia dirty mask `0xE0` w `model+0x84`.
- `0x326360`: analogicznie przy indeksie pierwszego LightBanku, `owner+0x60 → model+0x93`; przy zmianie `model+0x84 |= 0x20`.
- W ścieżce `0x326BB9…0x326C29` indeks z deskryptora przy `+0x10` jest rozwiązywany przez `0x3E2120`, a następnie zapisuje `model+0x93` (`0x326C29`), jeśli nieujemny i zmieniony.
- W ścieżce `0x326F35…0x326F95` analogicznie `0x2F89B0` → zapis `model+0x93` (`0x326F95`).
- Alternatywna gałąź `0x327081…0x3270E4` odczytuje indeks z listy rekordów i ustawia `model+0x93` (`0x3270E4`).
- `0x3E2120` prosi resolver `0x51F7C0` o kategorię **5** (`EDX=5`), `0x2F89B0` o kategorię **1** (`EDX=1`); oba przekazują `R8d = signextend int16[descriptor+0x08]`, wynik pobierają jako podpisany bajt przy rekordzie+offset+4. **Nazwy semantyczne kategorii 1 i 5 oraz powiązanie z NPC/graczem pozostają OPEN**.
- Funkcje z prefiksem `0x326…` mogą być wspólnymi metodami właściciela klasy `ChrIns`/pochodnych; **bez wiarygodnego runtime this/RTTI call stack nie przypisujemy każdego ich wywołania konkretnie EnemyIns/PlayerIns**.

## Istotne korekty do starszego RE

- `0x11B3D50` zaczyna z `EAX=-1`; dla ID **ujemnych** przechodzi do `id & 0x00FFFFFF`, a dla nieujemnych pomija maskowanie. To NIE jest dodatni uniwersalny identyfikator aktora.
- `FrpgModel+0x10` dostaje **wynik rejestracji EDI**; `Seed+0x10` dostaje osobno wygenerowany `R15d`. Bezpośrednia równość tych liczb nie jest potwierdzona.
- `ChrModel+0x18`, `Seed+0x20` i `DrawEntity+0x40` transportują referencję do modelu/renderera, nie unikalny gameplay actor ID. Wspólny wskaźnik nie może identyfikować aktora.

## Hipotezy i test falsyfikacyjny

**H_NPC**: row31 jest poprawnym lokalnym stanem NPC stojącego w cieniu, a draw/kompozycja (lub routowanie wcześniejszego ownera) przenosi jego wygląd na widoczną zbroję gracza.

**H_A**: row30 i row31 należą do różnych, poprawnych instancji/postaci i widoczne migotanie powstaje z niewłaściwej kompozycji, cullingu lub passu, a nie z pomieszania natywnego Source1.

**H_B**: jedna postać posiada kilka zgodnych natywnie encji/drawów z różnym stanem z powodu animacji/warstw/equipment, więc stan wiersza nie jest z definicji invariantem postaci.

**H_C**: błędny input do `FrpgModel+0x93` w owner-specific writer (source record/subtype), jeszcze przed natywnym packerem. Poprawność Source1→issued nie wyklucza błędu wejścia do modelu.

Test runtime bez masowego CSV: (1) nieruchomy gracz i kamera w tej samej pozycji, NPC obecny; (2) NPC poza zakresem/occlusion lub usunięty/oddalony przy niezmienionym oświetleniu, gracz i kamera w tej samej pozycji; (3) porównać obecność migotania, native owner/identity, `source kind`, `row` i material slot. Zniknięcie zjawiska po usunięciu NPC byłoby falsyfikacją hipotezy niezależności, ale samo w sobie **nie dowodzi przeniesienia jego LightBanku**. Ostateczny dowód wymaga: `EnemyIns/PlayerIns* + generation → ChrModel* → Seed/DrawEntity* → issued RX34 → target pass/pixel` wraz z routingiem i rzeczywistym konsumentem.

## Implementacja i status

V452 jest **wyłącznie analizą RE**, bez nowego `.addon64`. Reprodukowany V451 pozostaje pojedynczym add-onem diagnostycznym (manual F9 8s, max1024, bez automatycznego CSV). **Nie zmieniać** LightBank wierszami na sztywno, protected R43 ani Source1 bez native owner/pixel dowodu. `Game runtime`, `corrective bridge activation`, `PTDE pixel equivalence` nadal OPEN.