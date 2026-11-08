# V455 — RE: Seed ID nie jest generacją aktora; lokalny routing deskryptorów i selektora

**Oryginalny DSR EXE**: SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. **Status**: STATIC CONFIRMED; runtime actor identity / owner→RX34 / PTDE pixels **OPEN**. Bez modyfikacji EXE, bez nowego addona, bez zmiany Source1/R43 lub row30/31.

## 1. Korekta błędnej interpretacji V451–V454

Wcześniejsze raporty nazywały `0x1D2180` generatorem nowego Seed ID. Dokładny kod jest:

```asm
0x1D2180 mov rax, qword ptr [rip+0x1A9A6E1] ; global pointer at RVA 0x1C6C868
0x1D2187 mov eax, dword ptr [rax+0x10]
0x1D218A ret
```

Funkcja jest **odczytem globalnego pola**, nie wykonuje alokacji ani incrementu. W `0x157CF0` jej zwrócona wartość zostaje zachowana w `R15D` przy `0x157D1A`, przekazana jako `EDX` do `FrpgModelDrawEntitySeed` przy `0x157DDD → 0x157DF6` i zapisana w `Seed+0x10`.

To **nie dowodzi** unikalności Seed ID, stabilności generacji ani własności PlayerIns/EnemyIns. Globalna wartość może zmienić się między wywołaniami; znamy jej producenta jako globalny odczyt, nie semantykę życia aktora. Surowe E8 xref: 22 kandydatów wywołania gettera w sekcjach wykonywalnych (niekoniecznie 22 różne logiczne ścieżki).

Osobno `0x157D24` wywołuje `0x11BA160`, a jego wynik w `EDI` jest zwracany z rejestracji przy `0x157E00` i zapisywany jako **`FrpgModel+0x10`** przy `0x21C559`. **Seed+0x10 i FrpgModel+0x10 pochodzą z dwóch niezależnych wywołań.** Ich równość ani unikalność nie jest potwierdzona.

`0x11B3D50` używa pola `Seed+0x10` jako indeksu do tablicy zasobów: dla ujemnych liczb bierze dolne 24 bity, a dla nieujemnych pozostawia `-1`. To **lookup zasobu**, nie dowód actor-generation.

**Korekta canonical**: uchylić jedynie wcześniejsze określenie „fresh/generated seed ID”; statyczny łańcuch `FrpgModel → Seed → DrawEntity` pozostaje ważny, lecz wskazany identyfikator NIE jest certyfikatem postaci.

## 2. Rzeczywiste zależności aktualizacji LightBank

`0x326B50` działa na konkretnym `ChrIns* owner` w `RCX`, `descriptor*` w `RDX`.
- `0x326B74` odczytuje `owner+0x370`; `0x326B7B` porównuje go z nowym deskryptorem.
- **Jeśli wskaźnik jest ten sam**, `0x326B80` czyta `owner+0x4B1`. Wartość zero powoduje wyjście, ale **niezerowa pozwala przeliczyć ponownie ten sam descriptor**.
- `0x326B8D`: `owner+0x384` blokuje aktualizację; `0x326B9F` sprawdza `descriptor+0x5A` przed przyjęciem niepustego deskryptora.
- `0x326BA9` zapisuje przyjęty descriptor do `owner+0x370`.
- `0x326C0A` wywołuje resolver kategorii 5 (`0x3E2120`). Wartości ujemne są ignorowane. Jeśli `model+0x93` (gdzie `model=[owner+0x60]`) różni się, kod przy `0x326C22` ustawia `model+0x84 |=0x20` i zapisuje nowy bajt `0x326C29`.
- `0x326F76` używa innej ścieżki kategorii 1 (`0x2F89B0`); `0x326F8E` i `0x326F95` aktualizują ten sam kanał `model+0x93` i dirty bit.
- `0x326D29` ustawia `owner+0x4B1 = 0x1E`; `0x326D3B-3D` dekrementuje i zapisuje ten licznik. To mechanizm ponownego przeliczenia; **nie dowodzi** samoczynnego migotania w runtime.

Źródła deskryptora w obrębie jednego owner:
- `owner+0x370`: ostatni zaakceptowany descriptor w `0x326B50`.
- `owner+0x378`: odrębny deskryptor ładowany przy `0x326E52` i kierowany do `0x326B50` przy `0x326E5C`.
- `owner+0x4E0`: fallback w `0x32D50E → 0x32D518`.
- `0x32D410` ma też aktywny caller `0x3205D0` oraz warunkowy tail-jump `0x320ED0`.

Są to **dwie różne klasy operatora po stronie ChrIns**: przełączenie źródła deskryptora oraz ponowne przeliczenie tej samej referencji. Obydwie mogą precedować Source1, ale ich wystąpienie w badanej scenie pozostaje OPEN.

## 3. Census ograniczeń
Całe sekcje wykonywalne oryginalnego PE:
- `0x1D2180`: 22 kandydatów bezpośredniego E8,
- `0x326B50`: 16 kandydatów E8 (nie wszystkie muszą być osiągalne instrukcjami: możliwy obfuskowany payload),
- `0x326320`: 3 E8,
- `0x326EF0`: 1 E8,
- `0x326360`: 0 E8, **to nie oznacza, że jest nieużywany**,
- `0x32D410`: 1 E8 i 1 E9,
- `0x157CF0`: 2 E8,
- `0x15A8E0`: 3 E8.

Weryfikator `research/v455/verify_v455_seed_global_and_descriptor.py` zawiera **79** dokładnych preimage instrukcji i kontrolę mutacyjną SHA; certyfikuje konkretny oryginalny EXE. Wariant lokalny dodatkowo sprawdza 80 preimage i 81 kontroli negatywnych. Wyniki lokalne zostały dołączone do handoffu.

## 4. Hipotezy migotania — bez nieuprawnionego wniosku
**H_NPC** (przeniesienie ciemnego row31 NPC na gracza), **H_SELF** (inny descriptor/recalc w ramach PlayerIns) i **H_RENDER** (odrębne drawy/pass/coverage) pozostają **OPEN**. 294/294 poprawnych native ticket→RX34 nie dowodzi ani owner-to-player, ani pixel coverage. Indeks Seed jest niewłaściwym kluczem tożsamości.

Następny diagnostyczny cut powinien mierzyć tylko rzeczywisty `model+0x93` before→after dla **typed** `PlayerIns` lub `EnemyIns` wraz z `owner pointer`, `model=[owner+0x60]`, `owner+0x370/378/4E0`, kategorią resolvera i generacją alokacji. Potem sprawdzić powiązanie ze `Seed`/RX34 oraz pikselami zbroi. Jeśli nie można potwierdzić jednego właściciela i jego cyklu życia, zostawić stock DSR. Żadnego przymusowego `row31 → row30`.

## Odtworzenie
```bash
python research/v455/verify_v455_seed_global_and_descriptor.py DarkSoulsRemastered.exe --negative
```

W GitHub źródła są kompletne. Nie ma nowego builda, patcha ani obietnicy FPS czy PTDE pixel improvement.
