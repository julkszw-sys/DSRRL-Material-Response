# V456 — pełny natywny tor indeksu modelu do `FrpgDrawParam` (bez patcha)

## Zakres i pochodzenie

Oryginalny retail `DarkSoulsRemastered.exe`, SHA256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b` (PE64, image base `0x140000000`; wszystkie adresy poniżej to **RVA**, nie absolutne VA).

Nowy, samodzielny verifier `verify_v456_lightbank_command_roundtrip.py` + `V456_OPCODE_WITNESSES.json` sprawdza **101 niezależnie zaadresowanych preimage instrukcji, 14 bezpośrednich krawędzi CALL, 3 sloty vtable, rzeczywiste mapowanie jump-table dla opcodu `0x804C` i 106/106 testów mutacyjnych**. Uruchomienie na oryginalnym EXE: STATIC PASS. Wynik nie świadczy o uruchomieniu addona ani o widocznej równoważności PTDE.

## Łańcuch natywny: owner → model → transport → per-entity consumer

### A. `PlayerIns` ma własną ścieżkę indeksu

- `PlayerIns` vtable `0x13251F0` slot `+0xC0` → `0x357AE0`.
- W tej funkcji `0x357B65` ładuje `RBX=[PlayerIns+0x60]`.
- `0x357BFD` wywołuje z `RBX` jako `RCX` koder `0x21F600`, który czyta `FrpgModel+0x92/+0x93`.
- `0x357C02` ładuje osobny obiekt `RCX=[PlayerIns+0x850]`; `0x357C14` pobiera dolne 8 bitów zakodowanego indeksu, `0x357C17` porównuje z `controller+0xAB`; jeśli inny, `0x357C22` ustawia `controller+0x9C|=0x20`, a `0x357C29` wpisuje indeks do `controller+0xAB`. Analogiczny transfer `0x21F5C0` aktualizuje `controller+0xAC` (`0x357C34`, `0x357C5C`, `0x357C63`).
- **Znaczenie:** potwierdzono drugi, indywidualny dla `PlayerIns`, odbiorca pochodnej wartości `model+0x93`. Nadal nie zidentyfikowano nazwy/roli obiektu `+0x850` ani jego związku z RX34. Nie wolno nazywać go bez dowodu głównym rendererem zbroi.

### B. Per-`FrpgModel` dirty update i 16-bit encoding

- W V455 ścieżki deskryptora `ChrIns` wpisywały wybrane wartości do `model+0x93`, ustawiając `model+0x84 |= 0x20` tylko przy zmianie.
- `ChrModel` vtable `0x12EAF30` slot `+0x20 → 0x20B4E0`; `0x20B546` woła `FrpgModel::update` RVA `0x21C970`. Bazowa vtable `0x12EC838` slot `+0x20` wskazuje również `0x21C970`.
- `0x21C978/0x21C981` łączy `model+0x80` z `model+0x84` w maskę dirty. `0x21C98F` zeruje `model+0x84`. `0x21CA5B` sprawdza **co najmniej jeden** bit `0x20/0x40/0x80` w dolnym bajcie tej maski.
- `0x21CA6C`, `0x21CA7C`, `0x21CA89` kodują trzy pary indeksów przez odpowiednio `0x21F600`, `0x21F5C0`, `0x21F640`. Pierwsza używa `model+0x92/+0x93`, druga `+0x92/+0x94`, trzecia `+0x92/+0x95`.
- Dla poprawnych signed-byte wartości kodowanie ma postać `((signed(model+0x92) >> 3) & 0x7F) << 8 | uint8(model+0x93/94/95)`. Jeżeli grupa lub indeks jest ujemny, wynik = `0xFFFFFFFF`. To **kodowanie indeksu**, a nie forward shaderowego oświetlenia.
- `0x21CAA1` przekazuje **`FrpgModel+0x10` uchwyt rejestracji** w `EDX` do emitera `0x222AC0` wywoływanego pod `0x21CAC7`. Nie potwierdzono równości tego uchwytu z `Seed+0x10`: generator `0x157D11` daje osobny ID.

### C. Kod komendy `0x804C` ma dokładny, zweryfikowany odbiornik

- `0x222AD2` ustawia opcode `0x804C` podczas serializacji; `0x222AC0` umieszcza **float parametru przejścia plus trzy indeksy** w strumieniu.
- Dispatcher `0x223380` indeksuje komendy przez `opcode - 0x8046` dla 19 przypadków. Weryfikowany wpis jump-table pod RVA `0x2235E0` (`index=6`) zawiera `0x223466`.
- Branch `0x223466 → 0x22346C → CALL 0x222B60`. Decoder czyta ten sam format z trzema indeksami. `0x222BA7` przygotowuje **`RCX = DrawEntity+0x108`** i `0x222BBB` woła `0x1D7580`.
- `0x1D7580` rozdziela trzy indeksy na stanowe 16-bit lanes `FrpgDrawParam+0x8`, `+0x18` oraz `+0x28`; wszystkie przechodzą przez wspólną funkcję `0x1D7820` (`0x1D75AA`, `0x1D75C0`, `0x1D75DB`). Dolne 16 bitów `0xFFFFFFFF` stają się invalid sentinel `0xFFFF`.
- Jest to **potwierdzony typowany tor operatora** od modelu do per-entity DrawParam, nie tylko podobne opcodes. Nadal **brakuje połączenia danego `FrpgModel+0x10` z dokładnie jednym z sześciu runtime `DrawEntity` RX34**. Ścieżka nie udowadnia, że ich zasoby FLVER implikują tego samego właściciela.

### D. `0x1D7820` utrzymuje temporalne pary indeksów — nie zawsze natychmiast zastępuje jeden drugim

- `FrpgDrawParam` lane zawiera dwa 16-bitowe indeksy w `lane+0x0` i `lane+0x2`, oraz parametry float pod `+0x4`, `+0x8`, `+0xC`.
- `0x1D782F` odczytuje istniejący indeks końcowy, `0x1D7834` porównuje nowy indeks. Jeśli niezmieniony — brak ponownego retargetowania.
- Przy indeksie niepoprawnym (bit15 ustawiony) lub ujemnym czasie (`0x1D7820`) ścieżka przechodzi do bezpośredniego ustawienia obu końców (`0x1D7890`–`0x1D78AC`).
- Dla poprawnych różnych indeksów istnieje stan przejścia / retargetowania; `0x1D7848` testuje relację nowego indeksu do wcześniejszego początku, `0x1D784D` porównuje czas, a `0x1D7859`–`0x1D787A` może zachować odpowiednio skorygowany czas. Inna gałąź resetuje stan (`0x1D787B`–`0x1D788F`).
- **Nowa konkurencyjna hipoteza H_LERP:** row30 i row31 mogłyby być poprawnymi endpointami jednej instancji `FrpgDrawParam`, niekoniecznie wierszem innego NPC. Sama statyczna obecność interpolatora tego nie dowodzi. Nie potwierdzono, że obserwowane row30/31 pochodzą właśnie z tego opcode ani że pokrywają piksele gracza.

## Interpretacja oryginalnego migotania w pełnym słońcu

1. **H_NPC**: row31 to stan NPC w cieniu użyty omyłkowo na zbroi. Nadal OPEN. Zbieżny FLVER i 27 wspólnych CPU frame buckets nie są dowodem właściciela ani GPU-pixel coverage.
2. **H_SELF**: jeden `PlayerIns` zmienia `model+0x93` po uprawnionej/złej zmianie deskryptora. Native writer V455, PlayerIns direct copy V456 pozwalają to przetestować, ale runtime OPEN.
3. **H_LERP**: w ramach jednej `DrawEntity` istnieją dwie temporalne wartości indeksu (start/target). Potwierdzony operator per-entity, **niepotwierdzona** aktywacja dla rows30/31.
4. **H_RENDER**: różne drawy/pass albo niewłaściwy routing downstream. Nadal OPEN.

## Minimalna kolejna próba runtime (bez automatycznego masowego CSV)

Jedna ręczna sesja F9 ograniczona do 8 s i 1024 zdarzeń, tylko dla exact typed receiverów i komendy `0x804C`. W dwóch punktach obserwować osobno **`PlayerIns*`, jego `model+0x10`, `model+0x93`, `[PlayerIns+0x850]+0xAB`** oraz **`DrawEntity*`, jego `FrpgDrawParam` trzy `from/to` i parametry przejścia**, wraz z rzeczywistą generacją. Tożsamość łączyć po zweryfikowanym ABI/uchwycie rejestracji, **nie po model resource/FLVER**, nie po czasie ani podobnym opkodzie. Połączyć z dokładnym RX34 receiver/draw/actual shader i porównać GPU pixel/pas. Typowy owner `EnemyIns` może być zmierzony analogicznie; nie zakładać, że `+0x850` istnieje u NPC.

Gdy `player.model+0x93` stabilnie wskazuje 30, ale 31 pojawia się tylko w obcym `EnemyIns` — to argument za osobnymi aktorami, **nie jeszcze za błędem wpływu na piksele**. Jeżeli `player.model+0x93` przełącza 30↔31, H_NPC jako **wyłączna** przyczyna jest obalona. Jeśli te same odczytane `DrawEntity` ma `from=30` i `to=31`, H_LERP potwierdza się dla tego obiektu, ale jeszcze nie dowodzi poprawności PTDE-visible zachowania.

**Nie wykonano** patcha, nowego `.addon64`, zmiany Source1/Source2, R43, row30/31, ani ingerencji w EXE na dysku. V451 pozostaje ostatnim addonem diagnostycznym; V456 jest read-only RE.
