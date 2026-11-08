# DSRRL V453 — wspólna referencja modelu PlayerIns/EnemyIns; separacja właściciela LightBank

**Zakres:** RE statyczne oryginalnego `DarkSoulsRemastered.exe`, tylko DSR host, bez zmian binarnych i bez zmian addona. **Cel:** rozstrzygnąć, czy row31 przy zbroi gracza w słońcu może pochodzić od sąsiedniego NPC stojącego w cieniu. **Werdykt:** modelowa struktura właściciela została doprecyzowana; konkretny `row31 -> NPC` nadal OPEN.

## Źródło i sprawdzalność

- Oryginalny PE64, image base `0x140000000`, SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`.
- `verify_v453_shared_chrins_model.py` bez zewnętrznych zależności: **35** zgodnych instrukcji, **7** celów E8, **10** komórek vtable, **6** RTTI, **52/52** testów negatywnych, `STATIC_PASS`.
- Adresy poniżej są RVA, nie absolutnymi VA. Kod oryginalnego EXE jest tylko czytany.

## 1. Wspólny transport `ChrIns +0x60` dla obu klas

| Class/vtable RVA | Slot `+0x120` | Slot `+0xA8` | Slot `+0x40` |
|---|---|---|---|
| `ChrIns / 0x131E008` | `0x321CE0` | `0x320980` | `0x31EF90` |
| `EnemyIns / 0x1322E68` | **`0x321CE0`** | `0x320980` | `0x349FD0` |
| `PlayerIns / 0x13251F0` | **`0x321CE0`** | **`0x357A20`** | **`0x355890`** |

Trzy wskazania slotu `+0x120` do tej samej funkcji są sprawdzone bezpośrednio z tablic vtable. W `0x321CE0` instrukcja `0x321D05` wykonuje `mov rcx, [rcx+0x60]`; następnie bada null, odczytuje vtable modelu (`0x321D1D`) i wywołuje jej slot `+0x28` (`0x321D28`). `ChrModel` ma na tym slocie metodę `0x20BA20` o właściwym ABI przyjmującym wskaźnik wyjściowy `RDX`. Tym samym **oba typy używają pola `+0x60` jako obiektu o zgodnym interfejsie modelowym**. W ścieżce `EnemyIns` typ `ChrModel*` jest potwierdzony przez konstruktor.

Nie udowodniono jednak *dynamicznego typu* ani miejsca zapisu `PlayerIns+0x60` podczas rzeczywistego wykonania. Implementacja `PlayerIns` dla slotu `+0x40` jest obfuskowana; nie dziedziczy jej 1:1 po `EnemyIns`. Podobnie slot `+0xA8` u gracza jest **inny** (`0x357A20`), więc nie wolno globalizować reguł model-update z NPC na gracza.

## 2. Natywny zapis modelu NPC — rozdzielenie owner vs resource

- `EnemyIns` metoda `0x349FD0`: przy `0x349FEC` przenosi `EnemyIns this` do `R13` i utrzymuje tę referencję aż do końca tworzenia modelu.
- `0x34A101` ustawia `RCX=R13`; `0x34A104` wywołuje `0x31F1C0`, następnie `0x34A10C` wywołuje resolver `0x347AE0`, którego wynik zachowuje się w `RBX` (`0x34A111`).
- `0x34A16C` ładuje do `RDX` *referencję model-resource* `qword[RBX+0x28]`; po alokacji `ChrModel` konstruktor `0x20B0B0` jest wywoływany przy `0x34A173`.
- `0x34A17F` zapisuje **wynikowy `ChrModel*` do `[R13+0x60]`**, czyli `EnemyIns+0x60`.
- Konstruktor bazowy `FrpgModel` zapisuje argument `RDX` do `FrpgModel+0x18` przy `0x21C4F3`, a potem uruchamia rejestrację Seed przy `0x21C554`.

**Konsekwencja:** w tej ścieżce `owner=this` i `model-resource=RDX` są odrębnymi sygnałami. Nie ma dowodu, że sam `FrpgModel+0x18`, `Seed+0x20`, `DrawEntity+0x40`, model FLVER albo `FrpgDrawContext` pozwalają jednoznacznie odzyskać `EnemyIns*`. Własność trzeba zachować w rzeczywistej relacji **`EnemyIns/PlayerIns → ChrModel`**, a nie odtwarzać z renderowanego zasobu.

## 3. Gdzie nadal istnieje możliwość źle przypisanego LightBanku

`0x326360` jest owner-aware writer: czyta `[owner+0x60]`, porównuje i zapisuje `FrpgModel+0x93`, ustawiając dirty mask `model+0x84 |= 0x20`. Są również co najmniej trzy inne writer paths `0x326C29`, `0x326F95`, `0x3270E4` (V452). V448 294/294 zgodnych Source1→issued RX34 dowodzi spójności pakietów, **nie poprawności wejściowego owner->row w wcześniejszym writerze ani przypisania pikseli do gracza**.

Hipoteza NPC wymaga dwóch dodatkowych dowodów: (1) konkretny writer row31 ma `owner` z RTTI `EnemyIns` pobliskiego NPC; (2) wydany na tej podstawie draw/material przyczynia się do powierzchni/pikseli gracza. W tym momencie oba są OPEN.

## 4. Najwęższy dalszy krok — bez przeciążającego CSV

Proponowany read-only diagnostic, **niezaimplementowany i niewłączony w V453**:

1. Przy jednym z potwierdzonych writerów indeksu (tylko w krótkiej sesji na żądanie) zapamiętać `owner*`, jego faktyczną vtable RTTI, `qword[owner+0x60] == ChrModel*`, row old/new, callsite, model handle/generation. Wymaga kompletnego writer census — cztery znane ścieżki nie dowodzą globalnej zupełności.
2. Przy natywnej rejestracji modelu zachować mapping `ChrModel* -> Seed*` i generację przez **potwierdzoną relację tworzenia**, nigdy przez równość `FrpgModel+0x10 == Seed+0x10` (różna proweniencja).
3. Przy issued RX34 wykonać dopasowanie exact `Seed/FrpgModelDrawEntity/packet/row/sequence`, osobno dla Source1 i chronionego Source2. Utrzymać receiver family, FLVER/MTD, shader, probe, slot i GPU target.
4. Pomiary w pamięci i rzadkie liczniki; opcjonalnie ręczne F9 maksimum kilka sekund. Bez automatycznego CSV i bez unwind na każdym selektorze. Fail-open na nieznanym receiver, ponownym użyciu wskaźnika, rozbieżnym pokoleniu, zbyt starej generacji.

**Falsifiers:**
- `row31 -> PlayerIns`, a draw faktycznie trafia na zbroję gracza: hipoteza *źródło row31 = NPC* niepotwierdzona; szukać właściwego authoringu indeksu/passa.
- `row31 -> EnemyIns`, ale draw rysuje wyłącznie NPC: nie ma przekazania do powierzchni gracza w tym drawie.
- `row31 -> EnemyIns`, a niezależny receiver/pixel witness wykazuje ten sygnał na geometrii gracza: wspiera hipotezę cross-actor i uzasadnia wąski bridge po potwierdzeniu aktora/generacji.
- Brak modelu lub ambiguity: pozostawić stock DSR (fail-open); nie wymuszać `row30`.

## Status

| Etap | Wynik |
|---|---|
| V453 statyczne RTTI/vtable oraz owner→model | `CONFIRMED STATIC` |
| PlayerIns `+0x60` jako obiekt o interfejsie modelowym | `CONFIRMED STATIC CONSUMER` |
| PlayerIns `+0x60` jako zweryfikowany `ChrModel*` konkretnej postaci | `OPEN RUNTIME` |
| Row31 należy do pobliskiego NPC | `OPEN` |
| Row31 trafia na piksele gracza | `OPEN` |
| Bridge/korekta LightBank | `OFF` |
| DSR/PTDE pixel equivalence | `OPEN` |

Nie powstał nowy `.addon64`, V451 pozostaje ostatnim odtwarzalnym diagnostycznym single-addon. Nie zmieniono Source1, chronionego `R43 latest_hook_source`, żadnego renderer operatora ani EXE na dysku.