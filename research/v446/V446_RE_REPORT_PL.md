# DSRRL V446 — FrpgModel first LightBank selector / message graph (offline RE)

**Target:** PTDE-visible armor lighting; DSR host. **Typ:** dowód statyczny, bez zmiany renderera. **Data:** 2026-11-07. **Oryginalny EXE SHA-256:** `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. Wszystkie adresy są RVA, VA = `0x140000000 + RVA`. V359-RC1 pozostaje aktywną, zamrożoną bazą; V439 ostatnim zaobserwowanym runtime, V443 wyłącznie zbudowanym diagnostykiem. **STATIC PASS / LIVENESS OPEN / PIXEL OPEN**.

## 1. Dokładny semantyczny producent selektora

MSVC RTTI identyfikuje vtable `0x12EC838` jako `NS_FRPG::FrpgModel`. Metoda vtable[4] wskazuje na `0x21C970` i jest także wywoływana przez `0x20B546` (w kodzie bazowym modelu). W `0x21C970`, po zaobserwowaniu dirty bits `0x80|0x84`, branch `0x21CA5B` bada grupę `0xE0`: zmiana któregokolwiek z trzech kanałów. Gałąź `0x21CAC7` emituje przez `0x222AC0` zdarzenie `0x804C` z duration + trzema docelowymi selector IDs.

Funkcja `0x21F600` koduje **pierwszy** selector z bajtów `FrpgModel+0x92` (signed family/flags) i `FrpgModel+0x93` (signed first-index). Dla poprawnych, nieujemnych wartości:

```
signed_family_group = int8(FrpgModel[0x92]) >> 3    // signed arithmetic shift
first_index        = int8(FrpgModel[0x93])
selector = (signed_family_group << 8) | uint8(first_index)
```

Jeśli grupa lub indeks są ujemne, funkcja zapisuje `-1`. Równoważne funkcje `0x21F5C0` i `0x21F640` kodują drugi i trzeci kanał z `+0x94` oraz `+0x95`.

Warunkowa algebra dla obserwowanych w V439 **1310/1311**:

- `1310 = 0x051E`: rodzina=5, indeks=30;
- `1311 = 0x051F`: rodzina=5, indeks=31.

To wyjaśnia reprezentację **jeśli ten właśnie FrpgModel jest faktycznym producentem wydanego RX34**. Nie certyfikuje to mapowania modelu V439 na sprzęt gracza, generacji aktora ani tego, że różnica indeksu jest błędem względem PTDE.

## 2. Nowy bounded census źródłowych zapisów indeksu

Zidentyfikowano sześć zweryfikowanych, instrukcyjnie wyrównanych ścieżek `read FrpgModel+0x93 → compare → if changed OR 0x20 into FrpgModel+0x84 → write FrpgModel+0x93`. Pozycje poniżej dotyczą rzeczywistych odbiorników `FrpgModel`, a nie tylko przypadkowego offsetu `0x93` w obcej klasie:

| Źródłowa funkcja | Odczyt starej wartości | Dirty `0x20` | Zapis `+0x93` | Udokumentowany wskaźnik |
|---|---|---|---|---|
| `0x2EF7A0` | `0x2EFA79` | `0x2EFA84` | `0x2EFA8B` | `producer+0x30` |
| `0x2F9E30` | `0x2FA36C` | `0x2FA378` | `0x2FA37F` | `producer+0xB0` |
| `0x326360` | `0x326368` | `0x326373` | `0x32637A` | `object+0x60` |
| `0x326BB9` | `0x326C17` | `0x326C22` | `0x326C29` | `object+0x60`; resolver źródła |
| `0x326F35` | `0x326F83` | `0x326F8E` | `0x326F95` | `object+0x60`; resolver źródła |
| `0x327081` | `0x3270D2` | `0x3270DD` | `0x3270E4` | `object+0x60`; resolver źródła |

**To nie jest globalny census wszystkich writerów**. Inlined writes, kopiowanie całych struktur, indirect calls, VM, obce obiekty z takim samym offsetem oraz aliasy pozostają poza udowodnionym zakresem. Dokładna klasa gameplay poszczególnych obiektów nie jest jeszcze certyfikowana poprzez RTTI.

## 3. FrpgModel → native event → FrpgDrawParam → builder

```
FrpgModel [+0x92, +0x93, +0x94, +0x95]
    │ (conditional writes set dirty +0x84 bits 0x20/0x40/0x80)
    ▼
0x21C970, vtable[4] of FrpgModel
    │ helpers 0x21F600/0x21F5C0/0x21F640
    ▼
0x21CAC7 → 0x222AC0 (emit event 0x804C)
    │
    ├── MODEL dispatcher 0x223380, ID 0x804C → 0x223466
    │                                  → 0x222B60 → 0x1D7580
    │                                  (this+0x108 FrpgDrawParam)
    └── ASM dispatcher   0x21A520, ID 0x804C → 0x21A642
                                       → 0x219AD0 → 0x1D7580
                                       (this+0x108 FrpgDrawParam)
    ▼
  FrpgDrawParam first-channel +0x08: A WORD, +0x0A: B WORD, +0x0C: beta float
  entity offsets: +0x110(A), +0x112(B), +0x114(beta)
    ▼
MODEL builder 0x2200D0 reads first channel at 0x22024C/0x22025B/0x220273
ASM builder   0x2162D0 reads first channel at 0x216414/0x216423/0x21643B
```

`0xC04A0` i `0xC0550` są prostymi 16-bitowymi getterami A/B, nie setterami. Oba buildery czytają beta wprost. Weryfikacja obejmuje opcode witnesses, target direct-E8 i oryginalne jump tables (`MODEL 0x2235C8`; ASM compressed `0x21A85C → 0x21A800`). **Nie** potwierdza, że komunikat faktycznie dotarł w sesji V439 do konkretnego widocznego drawa.

## 4. Istotna korekta V445 — raw state copy ma także message routing

`0x804D` w MODEL dispatchuje do `0x222CA0`, gdzie **tail-JMP `0x222CCC → 0x1D7680`** kopiuje wszystkie pola trzech kanałów (A/B/β/duration/elapsed). Analogicznie ASM: `0x21A656 → 0x219C10`, tail-JMP `0x219C3C → 0x1D7680`. Wcześniejszy V445 census obejmował tylko `CALL E8` do `0x1D7680`, i przez to pomijał te dwie natywne ścieżki `JMP`.

`0x8052` przechodzi w MODEL do `0x222D30`, ASM do `0x219D60`, przekazuje float do wciąż nieodtworzonego w pełni `0x1D7530` i aktualizuje osobny kanał czasu. Nie należy na podstawie tego przyjmować kompletnej semantyki wewnętrznego VM.

**Ograniczenie ROUTING:** bezpośredni caller emitera MODEL raw state event (`0x222BD0`) jest jeden: `0x576C46` ze ścieżki `RemoPartsIns`. Nie wykazano, że event `0x804D` działa w sesji gameplay zbroi; model ma taką zdolność, ale nie wolno przypisywać jej V439 bez odbiorcy/generacji.

## 5. Czego to dowodzi, a czego NIE dowodzi

- **CONFIRMED STATIC:** istnieje typed source `FrpgModel+0x93` i sześć zweryfikowanych operatorów zapisujących indeks z bitową sygnalizacją zmiany; native message `0x804C` prowadzi do `FrpgDrawParam` w obu model families; message `0x804D` może dostarczyć pełny stan bez setter retarget; przy emitowaniu drawa A/B/beta są pobierane wprost z DP.
- **HYPOTHESIS, not promoted:** źródłem obserwowanych V439 1310 ↔ 1311 jest jedna z tych ścieżek na **tym samym konkretnym aktorze**; dynamiczny ruch kamery może zmieniać assignment albo pass. Bez same-instance native FrpgModel→model/entity→issued RX34 certyfikatu jest to OPEN.
- **NOT PROVEN:** błąd Source1, oryginalna semantyka PTDE dla tej zbroi, konkretne GPU pass/RT/SRV, poprawny finalny pixel bridge.

**Następny wartościowy offline krok:** identyfikacja oryginalnej klasy/właściciela senderów `0x2EF7A0/0x2F9E30/0x326360/0x326BB9/0x326F35/0x327081` i mapowania do EnemyIns/ChrModel; następnie source record→`FrpgModel` identity i exact receiver bez nowego heurystycznego gate. Do tego czasu nie wydawać fix-a row30/31, nie zmieniać R43/Source1, nie używać V438 detourów.

## 6. Odtwarzalność

```
python verify_v446_native_selector_graph.py DarkSoulsRemastered.exe --mutations --json V446_RE_RESULTS.json
```

Wymaga tylko standardowej biblioteki Python oraz dołączonego niemodyfikującego parsera PE z V444. Zaliczone: **54** instruction preimages, **12** direct-call edges, **2** tail JMP, trzy exact dispatcher mappings w każdej family, RTTI+source vtable i **70/70** testów negatywnych (w tym destrukcyjne mutacje tylko w pamięci kopii). EXE nie jest modyfikowany na dysku. Statusy: CONSTRUCTION=NOT_APPLICABLE / OFFLINE_STATIC=PASS / RUNTIME_LIVENESS=NOT_TESTED / BRIDGE_ACTIVATION=NOT_APPLICABLE / PIXEL_BEHAVIOR=OPEN.