# V447 — przypisanie LightBank do natywnego aktora (shadow routing)

Data 2026-11-07. Wariant diagnostyczny od ostatniego runtime V439 i V443: kod w gałęzi `agent/v447-actor-source-native-ticket-shadow-20261107`, bazowy commit V443 `5c4ee0e97d682e82e8eb7e36f872ecdfd9341a88`; zamrożony release-root V359-RC1 `6e852cdc9085dd175159e564e223e0cc8589c535` nietknięty.

## Co jest faktycznym defektem obecnego klucza

`renderer-core/src/runtime/flver_engine_hooks.cpp` w `observe_model_lightbank_source()` tworzy `model_owner_instance = reinterpret_cast<std::uintptr_t>(owner)`; `owner` jest kontekstem `FrpgDrawContext`, a nie udowodnionym aktorem. `pmetal_producer_state.cpp::model_key()` utożsamia ten adres z natywnym aktorem; per-instance cache indeksuje wyłącznie tym wskaźnikiem (przy włączonym V398). Rozdział producentów korzystających z tego samego kontekstu NIE jest więc zagwarantowany. To statycznie potwierdzona słabość klucza, ale z dotychczasowych logów nie wynika dowód rzeczywistej zamiany źródła konkretnej zbroi.

## Implementacja

`v447_actor_source_router.hpp`: niezależny `source_ticket` związany z unikatowym zdarzeniem natywnego packera, a nie z kontekstem. Pola: `native_producer_sequence`, `FrpgModelDrawEntity`, `FrpgDrawParam`, natywny pakiet buildera, kontekst, model, A/B/row/beta, token generacji (0 gdy niezweryfikowany). `registry` jest thread-safe, ograniczony do 8192 rekordów, kontroluje semantyczne dopasowanie, odrzuca niejednoznaczności i stale/ABA.

`v447_actor_shadow_overlay.py`: po istniejącym obserwatorze V443 zapisuje źródło wyłącznie dla sprawdzonego natywnego modelu/pakietu (`V443 status9`, bez wrapper detours), w trybie obserwacyjnym dla A=B, beta0. Przy wydanym RX34 route345, row30/31 próbuje **niezależnie** odzyskać natywny aktor + packet ze stosu wywołania przez funkcję V443. Brak tej ramki nie dopisuje aktora z cache. Łączy zdarzenia po `native_producer_sequence`, kontekście i row, a przy obecnym braku certyfikatu lifetime NIE autoryzuje podmiany. Rejestr służy do pozyskania brakującego dowodu, nie do sterowania materiałem.

Zdarzenia CSV (`v438::emit`):
- kind16 `V447_NATIVE_TICKET`: v0=seq, v1=entity, v2=dp, v3=packet, v4=context, v5=model, v6/7=native A/B, v8/9=row A/B, v10=beta_bits, v11=verdict, v12=generation.
- kind17 `V447_RX34_BIND`: v0=seq, v1=context, v2=row, v3=verdict, v4..12=źródłowe entity,dp,packet,context,model,A,B,rowA,rowB, v13=kind, v14=receiver, v15=route, v16=slot, v17=shader, v18=receiver stack verdict, v19=receiver entity (gdy exact verified), v20=receiver packet, v21=receiver model.

`analyze_v447_actor_sources.py` rekonstruuje źródłowy rozkład row według różnych natywnych entity, również przy współdzielonym kontekście, ale nie podnosi tego do pixel PASS.

## Gwarancje i ograniczenia

* Nie zmienia `pmetal_producer_state`, Source1 ani chronionego `latest_hook_source`/R43.
* Nie ustawia row30 globalnie i nie blokuje row31. Nie zmienia shaderów, zasobów, CB ani EXE.
* Nie włącza detourów V438: pozostają wyłączone w V439/V443.
* Gdy receiver nie ma własnej tożsamości i generacji, `missing_receiver_identity` / `missing_generation`; żadnej aktywacji.
* Materiał i pakiet buildera muszą odpowiadać tej samej instancji. Ten warunek ma niezależny test C++.
* Zmienność tylko podczas obracania kamery jest wg właściciela niepożądana dla tej zbroi; przypisanie jaśniejszego row30 do niej i ciemniejszego row31 do innego aktora pozostaje HYPOTHESIS.
* Status: STATIC UNIT PASS, SOURCE OVERLAY TEST PASS; Windows native CI i runtime wymagają odrębnej kontroli; aktywny bridge OFF; pixel OPEN.

## Reprodukcja

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic -pthread -Irenderer-core/include renderer-core/tests/test_v447_actor_router.cpp -o test_v447 && ./test_v447
python -m py_compile renderer-core/tools/v447_actor_shadow_overlay.py renderer-core/tools/v447_prepare_build.py renderer-core/tools/v447_name_binary.py
```

W Windows CI: V439 source overlays, V443 prepare/name, V447 prepare/name, następnie oryginalny `build_v438_one_shot.ps1` z pinowanym ReShade 6.8.0.1. Jeden `.addon64`, nie RC. Wraz z obrazem DSR pozostaje `PIXEL OPEN`.

## Pozostały rzeczywisty blokujący dowód

Niezależna weryfikacja tożsamości receiver entity/packet/generation dla wydanego RX34. Dopiero wtedy można przenieść actor source ticket z rejestru diagnostycznego do operatorowo izolowanego routingu LightBank. Jeżeli runtime pokaże oddzielne entity źródłowe row30 i row31 przy tej samej zbroi, należy wskazać dokąd przypisano źródło ciemnego aktora, a nie usuwać poprawny wiersz innemu aktorowi.