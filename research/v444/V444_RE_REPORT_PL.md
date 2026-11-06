# DSRRL V444 — Armor flicker: owner lifecycle + native DrawParam writer boundary (offline RE)

**Data:** 2026-11-06. **Target:** PTDE-visible behavior; DSR host; minimal receiver-/operator-local bridge. **Zakres:** statyczna analiza oryginalnego, niemodyfikowanego `DarkSoulsRemastered.exe` SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b` + audyt V439/V443 handoff. Wszystkie adresy poniżej to **RVA** (VA = `0x140000000 + RVA`). **Nie ma nowego hooka ani patcha, runtime/pixel pozostają OPEN**. V359-RC1 frozen root, V439 to ostatni wykonany runtime, V443 Win64 CI/construction PASS i runtime NIEPOTWIERDZONY.

## 1. Wynik nowej analizy — pełny bezpośredni census konstruktorów i fabryk

`FrpgModelDrawEntity` nie gwarantuje tożsamości jednostki ekwipunku gracza. Dokładnie ten sam obiekt wewnętrzny może być samodzielną jednostką albo częścią emitera efektów FX; vtable pozostaje **identyczne**. V443 `status=9` z definicji nie może certyfikować fizycznej instancji zbroi bez odrębnej informacji o lifetime/parent.

| Typ oryginalny (RTTI) | Samodzielna fabryka / alokacja | Konstruktor — dokładne direct E8 | Destruktor — dokładne direct E8 | Znaczenie |
|---|---|---|---|---|
| `NS_FRPG::FrpgModelDrawEntity`, vtable `0x12ECA10` | factory function `0x223AC0` (`0x1C0` B) w `rdata[0x12CC310]` | `0x223B0F→0x21F900` | `0x2237EF→0x21FBD0` | samodzielny/heap object, jeszcze nie dowód gracza |
| ten sam `FrpgModelDrawEntity` | FX Cluster, parent vtable `0x136C0D0`, `inner = parent+0x50` | `0x4FD3DE→0x21F900` | `0x4FC09E→0x21FBD0` | wewnętrzny pod-obiekt efektu, **taki sam vtable** |
| ten sam `FrpgModelDrawEntity` | FX Particle, parent vtable `0x136C208`, `inner = parent+0x30` | `0x4FE7FE→0x21F900` | `0x4FDC7E→0x21FBD0` | wewnętrzny pod-obiekt efektu, **taki sam vtable** |
| `NS_FRPG::FrpgAsmModelDrawEntity`, vtable `0x12EBFE8` | factory `0x21AF40` (`0x250` B) w `rdata[0x12CC2F0]` | `0x21AF8F→0x215960` | `0x21AA3F→0x215D20` | osobna rodzina ASM, statyczna fabryka i lifecycle |

Weryfikator odczytuje MSVC `CompleteObjectLocator`/TypeDescriptor z vtable (nie zgaduje nazw po samych offsetach). Factory functions są obecne w tym samym rejestrowanym static table co odpowiadające getter routines: `0x12CC308→0x223AA0`, `0x12CC310→0x223AC0`, oraz `0x12CC2E8→0x21AF20`, `0x12CC2F0→0x21AF40`. W samym modelowym ctor `0x21F934..42` i ASM ctor `0x215992..A0` vtable i model field `entity+0x40` są ustawiane z descriptor `+0x20`; obie rodziny inicjalizują embedded `FrpgDrawParam` `entity+0x108` poprzez `0x1D7490`.

**Ograniczenie:** To dokładny census *bezpośrednich* `E8 rel32` do konkretnego konstruktora i destruktora w **obu wykonywalnych sekcjach `.text`**. Nie wyklucza thunków pośrednich/inlined constructors/callbacks, innych rodziców ani reuse wskaźnika. Znane dwie gałęzie FX były już opisywane w V441 — **nowe jest ich wspólne zestawienie z drugim standalone factory i kontrolą kompletności direct E8**. Żaden ten fakt nie dowodzi, że zarejestrowany V439 model `0xE979970` to zbroja.

## 2. Upstream writer census — ważna pułapka tożsamości

Dla wspólnego setter-a trzech kanałów `0x1D7580` kompletny surowy *direct E8* census obu `.text` znalazł dokładnie:

| wywołanie | nośnik i znaczenie |
|---|---|
| `0x219B2B` | `0x219B17 lea rcx,[r11+0x108]` — serializowany input pierwszego kanału obiektu modelowego |
| `0x222BBB` | `0x222BA7 lea rcx,[r11+0x108]` — drugi serializowany input obiektu modelowego |
| **`0x0C0240`** | **trzeci, wcześniej nieoddzielony bezpośredni caller**: odczyt strumienia serializacji `0xC0160..C0240`, `RCX=RBX+0x48`, z `R8/R9` ze strumienia i wspólnym duration w XMM1; **brak dowodu** powiązania `RBX+0x48` z `FrpgModelDrawEntity+0x108`. Nie wolno automatycznie dopisać do aktora. |

`0x1D7530` ma dwa widoczne `CALL E8` (`0x219D8F`, `0x222D5F`), obydwa po przekazaniu `entity+0x108` z inputu czasu. Istnieje też **tail-JMP** `0x0C0261→0x1D7530` z serializowanego obiektu `RBX+0x48`. Nie ma tu podstaw do przypisania gameplay writer-a sterowanego kamerą.

**Nowy powód ograniczenia RE:** wejście `0x1D7530` jest kodem obfuscation/control-flow virtualization. `E9` w `0x1D7530` kieruje do drugiego `.text` `0x21AA328`, stamtąd `stack/exchange/RET` wybiera `0x21B730B` i dalszy target `0x296DCA0`. Nie jest to konwencjonalny prolog/epilog z prostym callgraph; sam rel32 caller census nie identyfikuje wewnętrznego mutatora ani efektu kamery. Wszelkie próby uproszczenia tego do „tick 0x1D77E0 wywoływany bezpośrednio z 0x1D7530” pozostają HIPOTEZĄ, dopóki nie zostanie zweryfikowana ścieżka VM i live state.

## 3. Znaczenie dla V443 — czym jest, a czym NIE jest `status=9`

`v443::probe` czyta `dp-0x108` i dopuszcza vtable `0x12ECA10` oraz `0x12EBFE8`. **To poprawna klasyfikacja bazowego natywnego entity typu MODEL/ASM, ale nie player armor**. Natywny model może być zagnieżdżony w FX parent; pointer-only `(entity, model, FLVER, slot)` nie zapewnia również trwałości generacji (ABA). Nie wolno rozciągać `status=9` na `ACTOR_GENERATION_CERTIFIED` ani `PIXEL_PASS`.

Nie zalecam wprowadzania teraz nowego whitelist/gate dla `FrpgModelDrawEntity`, bo towarzysząca ochrona projektu wymaga dowodu konkretnego błędu i testu nonregression. **Jedynie read-only telemetry** może osobno zapisywać: zweryfikowaną parent/creator provenance, generation token z istniejącego lifecycle registry (gdy faktycznie dostępny), packet/builder invocation, source serial, model/material/PS, issued RT/CB/SRV. W V443 brak tej informacji jest powodem `OPEN`, a nie powodem `FAIL` renderingu.

Nie używać V438 wrapper detours (crash-suspect). Nie usunąć chronionego R43 `latest_hook_source()` ani aktywnego Source1. Nie pinować row30/31. Nie odwracać gammą czy exposure innego operatora.

## 4. Uczciwy model rozstrzygania przyczyn

1. **Native upstream:** jeśli ten sam *weryfikowany entity instance+generation+builder invocation* ma różne `dp.A/B/beta` podczas dwóch wydań RX34, zmiana musiała powstać przed/z trakcie native emitter (albo istnieje race), lecz czy odpowiada PTDE pozostaje osobno do sprawdzenia.
2. **Bridge association:** jeżeli dp i emiter są stabilne, a `native_producer_sequence` i wydany RX34 nie odpowiadają im dla tego samego model+material+pass, podejrzany jest Source1/selektor/cache/routing. Na tym etapie nie wyłączamy Source1 globalnie.
3. **Consumer:** jeżeli operator-specific transport odpowiada obu grom, ale obserwowany materiał reaguje inaczej, dopiero wtedy shader/CB/resource/material island, z exact receiver/asset sidecar i fail-open.

V439 dowodzi rapid row30/31 C86/probe switching dla wskazanego RX34 w zgłoszonej sesji, **ale nie zawiera** certyfikatu aktora czy GPU pass. V443 ma Win64 CI, lecz brak danych liveness/activation na komputerze użytkownika. Bez runtime ani PTDE frame nie można uczciwie wybrać gałęzi (1)–(3) ani tworzyć kanonicznej korekty.

## 5. Weryfikacja i następna bezpieczna granica

`python verify_v444_actor_lifecycle.py DarkSoulsRemastered.exe --mutations --json V444_STATIC_RE.json`

- Dwusekcyjne `.text` i dokładny retail SHA: PASS.
- **34** exact opcode witnesses, **13** raw E8 direct targets w **6** target groups, **4** fabryczne qword + **4** MSVC RTTI: PASS.
- Tail `jmp 0xC0261→0x1D7530` i pierwsza VM trampoline: PASS.
- **52/52** mutacyjnych negatywów: PASS (zmieniony opcode/call/factory ptr lub SHA musi zostać wykryty).
- Gameplay, native actor generation, exact issued RT/CB/SRV, PTDE visible pixel: **OPEN**.

Nie ma nowego `.addon64`; obowiązuje dotychczasowy pojedynczy zintegrowany **V443 diagnostic**, a frozen V359-RC1 pozostaje bazą. V444 jest RE report i verifierem, który można odtworzyć bez uruchamiania gry. Dopiero na rozstrzygającym certyfikacie będziemy projektować nowy bridge operator-local.