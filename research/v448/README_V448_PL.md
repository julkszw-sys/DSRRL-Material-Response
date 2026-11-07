# V448 — Live builder frame → actor source → RX34 (SHADOW ONLY)

Data: 2026-11-07. Cel: poprawne przypisanie LightBank do konkretnej instancji aktora bez globalnego wymuszania row30 ani odrzucania row31. Host DSR; target PTDE. **Nie jest to patch jasności ani jeszcze produkcyjna korekta.**

## Stwierdzony błąd zakresu identyfikacji

W istniejącym V447 Source1 `material.model_owner_instance` jest natywnym wskaźnikiem `FrpgDrawContext`, nie bezwarunkowo niepowtarzalnym ID aktora. Source1 `model_key()` wykorzystuje ten wskaźnik, a normalny `latest()` odczytuje ostatnio zapisany payload dla klucza. Dotychczasowy pomiar V439 wykazuje row30/31 przy tym samym kontekście, ale nie udowadnia bezpośrednio, czy jest to konflikt dwóch aktorów, dwóch passów, czy natywne przełączenie jednego aktora. **Nie wolno zastąpić tego domysłem „row31 jest błędny”.**

## Nowy sprawdzalny cut ABI

Oryginalny SHA-locked DSR EXE `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`:

1. FrpgModelDrawEntity (vtable RVA `0x12ECA10`) ma embedded `FrpgDrawParam` `entity+0x108`, którego vtable RVA `0x12E5C08`.
2. W default builderze `0x2200D0`, zachowane `RDI` jest natywnym `entity`, a `RBP-0x30` jest pakietem drawa (`FrpgDrawContext` pod packet+0, A/B/beta pod +0x4C/+0x4E/+0x50).
3. Pod RVA `0x220CED` builder wywołuje `[vtable+0x18]` na silnikowym modelu `[entity+0x40]`. Zwrot do buildera to **RVA `0x220CF0`** — stos powinien dalej zawierać tę aktywną ramkę podczas synchronizowanego renderowania.
4. Oba vtable FlverModel `0x12EB760` i FgFlverModel `0x12EB130` pod `+0x18` wskazują RVA `0x20DDA0`. Ta metoda wywołuje już hookowany native material selector `0x22BA20` w trzech zweryfikowanych sites `0x20E014/0x20EB7A/0x20FB99`.
5. `RtlVirtualUnwind` umożliwia wyszukanie **caller return `0x220CF0`** z istniejącego native selector hooka, podczas gdy V443 na wcześniejszym etapie packera szukał powrotów z emitera. To ważna różnica semantyczna — packer już mógł zakończyć się, kiedy zaczyna się RX34.

## Rozbudowa pojedynczego addona

Baza: V359-RC1 (zamrożona) → ostatni rzeczywisty runtime V439 → V443 → V447 → V448. Żaden nowy detour ani modyfikacja EXE na dysku.

- `v448_live_builder_core.hpp`: deterministyczny, thread-local, jednozdarzeniowy transport `(producer sequence, entity, DrawParam, packet, context, engine model, native A/B/beta)` i walidator zgodności. Brak semantyki na podstawie samego ctx/row/FLVER SHA. Nie zakłada certyfikatu lifetime.
- `v448_live_builder_probe.hpp`: ograniczony read-only Win64 `RtlVirtualUnwind`, typed vtable validation, packet-vs-embedded-DP equality. Obsługuje tylko oryginalny MODEL default builder, inne rodziny `fail-open`.
- `v448_actor_live_overlay.py`: instrumentacja istniejącego packera, istniejącego hooka selektora FLVER i RX34 issued. Wykorzystuje jednorazowy `material_owner_selection_consume` do transferu tego samego wyboru. Wpisy CSV kind18 selector native frame, kind19 issued native frame. Blok aktywacji/patchowania jest bezwarunkowo OFF.
- `analyze_v448_capture.py`: zwraca liczby przypadków, konflikty, dwie tożsamości przy jednym kontekście i status missing receiver; nie podnosi żadnego do pixel PASS.
- `verify_v448_native_builder.py`: weryfikuje oryginalne 15 preimage, 2 vtable, 4 `.pdata` i 15 mutacji negatywnych.

## Kryterium dla późniejszej poprawki (nie wdrożonej tutaj)

Potrzebne jest **runtime potwierdzenie**, że podczas wydanego RX34 native stack nadal zawiera tę samą ramkę `0x220CF0` co podczas selektora, i że `(native producer seq, entity, DrawParam, packet, context, model, A/B/beta)` się zgadzają. Wtedy można rozważyć operator-local Source1 override dla dokładnie tego drawa, bez globalnego row lock; inne ścieżki pozostają stock DSR/R43 według dotychczasowych PROTECT. Dodatkowo potrzebna jest walidacja actual receiver/materiały/RT i PTDE pixel behavior; samo `verdict11` nie jest pixel PASS.

## Testy i statusy

```
c++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic -pthread -Irenderer-core/include/dsrrl/runtime renderer-core/tests/test_v448_live_builder_core.cpp -o test_v448 && ./test_v448
python renderer-core/tools/verify_v448_native_builder.py DarkSoulsRemastered.exe --mutations --json v448_static.json
python renderer-core/tests/test_v448_overlay.py
```

Status: SOURCE/STATIC PASS, native Windows CI po opublikowaniu source w repo; runtime NOT TESTED, bridge OFF, PTDE pixel OPEN. V438 wrapper detours pozostają disabled, bez F9, R43 latest_hook_source pozostaje chroniony.
