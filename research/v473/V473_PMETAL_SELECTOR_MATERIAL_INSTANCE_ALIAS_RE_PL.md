# V473 — rzeczywisty alias klucza selektora P_Metal w v2.3.1

**Cel:** zidentyfikować najwęższy poprawny semantycznie punkt naprawy LightBank dla row30/row31, bez naruszania świeżego wydania 2.3.1, bez badania wydajności i bez wymagania nowych zrzutów/logów od użytkownika.

## Podstawy źródłowe (konkretne pliki)

- v2.3.1, PR #552, commit `614a55cda57296f6e460c740f556e534c4114f8b`, material lineage oryginalnego B235.
- `renderer-core/include/dsrrl/operators/material_response/material_response_island.hpp` struktura `material_identity`: FLVER SHA-256, raw MTD SHA-256, material_slot, route, semantic and material family plus bool guards, **bez natywnego uchwytu instancji modelu/receivera**.
- `renderer-core/src/runtime/pmetal_producer_state.cpp`: `same_material` i `material_key` porównują tylko ten zestaw. `g_record` thread-local przechowuje ostatnią wartość dla material identity. Synchronizowane `g_sync_slots[256]` rozróżniają material key i `epoch`, ale nie aktora/FrpgModel/FrpgModelDrawEntity.
- `pmetal_producer_state_begin(material,epoch)`, `publish(material,source,epoch)`, `latest(material,epoch,out)` nie przyjmują instancji, full DWORD registration handle ani issued RX34 identity.
- `renderer-core/src/runtime/pmetal_env_source_runtime.cpp`: `pmetal_env_source_selector_event` publikuje do powyższego cache; `pmetal_env_source_runtime::latest` wybiera cached material-only source, a dopiero po jego braku chroniony `latest_hook_source` R43. Selector `owner` w miejscu hooka nie stanowi zweryfikowanego ChrIns / render entity, tylko FrpgDrawContext.
- `research/v472`: oryginalny retail EXE SHA `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`, komenda `0x804C` produkowana przez FrpgModel per-instance `+0x10`, konsumowana przez odpowiadający mu FrpgModelDrawEntity `+0x10` / DrawParam `+0x108`. Nie dowodzi to, że gameplay-root ChrModel i każdy receiver to ta sama instancja.
- `renderer.armor.rx34_native_entities_fixed_light_pair_not_own_lerp`: historyczne V438 runtime ma współistniejące różne FrpgModelDrawEntity, ze stabilnymi indywidualnymi parami row30/row31 i wspólnym FLVER. To sprawia, że problem identyfikacji instancji jest rzeczywistą potrzebą, nie hipotezą wymyśloną bez dowodów.
- `renderer.v448.csv_20261107_rx34_source_issued_exact_294`: 294/294 Source1 producer→issued RX34 matches istnieją na poziomie *native producer entity and DrawParam*, ale to wciąż nie jest gameplay actor proof.

## Demonstrator z produkcyjnego pliku (nie model symulacyjny)

`research/v473/test_v473_real_pmetal_material_alias.cpp` kompiluje i linkuje **rzeczywisty** `renderer-core/src/runtime/pmetal_producer_state.cpp` (bez stubbing). Dwa hipotetyczne uchwyty instancji A/B mają to samo źródłowo certyfikowalne MTD/FLVER/slot `material_identity`, ale publikują różne źródła row30/row31. Pierwsza publikacja umożliwia pobranie row30. Po drugiej, request `latest(material,epoch)` zwraca row31. API nie daje możliwości wyrażenia „zwróć mi dla A, nie dla B” mimo różnych uchwytów. Kontrola dwóch różnych `material_slot` potwierdza, że rozdział według **materiału** działa poprawnie; brak dotyczy rozdziału według **instancji**. Test chroni przed niespodziewaną zmianą ABI.

Samo przekazanie generation `g_record.generation` nie wystarczy, ponieważ jest generacją *source value* w TLS, nie natywnym lifecycle token dla konkretnego modelu. Tak samo `bank_signature`, `row_id`, wspólny FrpgDrawContext, FLVER i raw MTD nie identyfikują gameplay owner.

## Bieżący status i ograniczenie wnioskowania

**CONFIRMED (SOURCE / UNIT):** aktualny synchronizowany transport nie jest injektywny w przestrzeni aktorów/instancji. Dwa źródła z identyczną material_identity nie mogą być równocześnie adresowane osobno na podstawie aktualnego API. Alias występuje dla TLS i cross-thread fallback według kolejności wykonania.

**NOT CONFIRMED:** że w testowanej scenie row31 jest nieprawidłowy dla widocznej zbroi gracza; że wystąpiło rzeczywiste błędne odczytanie source w v2.3.1; że alias jest jedyną przyczyną migotania; że PTDE pixel poprawiłby się po zmianie. V438 6 renderables != 6 gameplay owners. Nie przenosić HYPOTHESIS do CONFIRMED pixel.

## Właściwy następny cut

1. Identyfikować **właściwego FrpgModel instance**, jego full 32-bit handle, exact native `0x804C` command recipient i lifecycle. Mapowanie `FrpgModel+0x10` do `FrpgModelDrawEntity+0x10` musi bazować na verified registration/dispatcher, nie na root PlayerIns `ChrModel+0x10`.
2. Powiązać issuer native Source1 z exact receiver `FrpgModelDrawEntity*`, handle i tym samym `DrawParam+0x108` / issued RX34, korzystając z już potwierdzonego stage `native_builder sequence + ticket`. Odróżnić rodzinę default i ASM oraz Source2.
3. Dopiero potem rozdzielić producer state według composite `(material_identity, validated native entity registration, lifetime/issue ticket)`, a `latest()` dopuścić tylko przy exact full join. Przy braku identity — **fail-open stock DSR dla nieautoryzowanego bridge**. Zachować osobną chronioną ścieżkę R43 `latest_hook_source`; nie usuwać jej.
4. Zmiana powinna dotyczyć **transportu/source authority**, nie wartości LightBank, gamma, t11, SpecRGB, material response, tonemapu, global row30↔31 ani informacji o NPC wywiedzionej z class2.

**Release guard:** PR #552 v2.3.1 source/PE unchanged, no integrated addon, no user runtime capture requirement. Nowy kod wyłącznie `research/v473` i osobny CI test source. Po implementacji exact-instance join potrzebny oddzielny operator-local construction/compatibility pass, a wynik pixel OPEN bez niezależnej walidacji.
