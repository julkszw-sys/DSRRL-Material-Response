DSRRL V350 GPU RESIDENT RC1 - BEZPIECZNE CZYSZCZENIE LOW-LOD SpecRGB

CEL:
Usuwa z aktywnego DSRRL/Specular TYLKO scisle 544 nazwy PTDE low LOD,
dla ktorych w zatwierdzonym V350 RC1 nie istnieje DSR direct g_Specular.
Nie usuwa pieciu poprawnie mapowanych *_l_s, pozostalych 749 DSR direct,
dwoch nierozstrzygnietych weaponow, ani Diffuse/Normals/EnvSpec.
Nie usuwa plikow, ktorych nazwy wystepuja w Twoich lokalnych manifestach
DSRRL/manifests lub mapowaniach na najwyzszym poziomie DSRRL.

WAZNE:
Pakiet narzedzia sklada sie z 2 plikow BAT, 1 PS1 i 1 TXT INVENTORY.
Ten TXT jest TYLKO lista nazw do kontroli skryptu: addon GPU Resident RC1
nadal NIE wymaga zadnego TXT do dzialania.
Jest sprawdzana SHA256 listy (1295 nazw) i SHA256 konkretnego RC1 addon64.
Skrypt odmawia przenosin przy nieznanym lub zmienionym addonie.

INSTRUKCJA:
1. Rozpakuj WSZYSTKIE pliki narzedzia obok DarkSoulsRemastered.exe
   (tam gdzie folder DSRRL z podfolderem Specular).
2. Uruchom AUDYT_SIDEcAR.bat - zapisze CSV, ale nie przeniesie niczego.
3. Uruchom OCZYSC_LOW_LOD.bat - przeniesie znalezione, nieuzywane DDS
   poza aktywny DSRRL/Specular, do _DSRRL_LOWLOD_QUARANTINE/<czas>/Specular.
   Niczego nie kasuje bezpowrotnie.
4. Raport: _DSRRL_LOWLOD_REPORTS/*.csv.
5. W razie nieprawidlowego wyswietlania po czyszczeniu: przywroc zawartosc
   kwarantanny do DSRRL/Specular lub uruchom:
   powershell -NoProfile -ExecutionPolicy Bypass -File .\DSRRL_SAFE_CLEAN_LOWLOD.ps1 -Restore "<sciezka_do_batch_kwarantanny>"

Jezeli addon64 jest w innym katalogu, uruchom:
powershell -NoProfile -ExecutionPolicy Bypass -File .\DSRRL_SAFE_CLEAN_LOWLOD.ps1 -Apply -AddonPath "C:\pelna\sciezka\addon64"

DOMYSLNIE: DRY-RUN. BRAK WYMUSZONEGO USUWANIA.
NARZEDZIE ZGODNE WYŁĄCZNIE Z V350 Scope Shell GPU Resident RC1
SHA256: c298ca9e4f4c34a427ff29d7a72fba02fab8f0f9019de2fd1098a1c73d9ba5fa

Ograniczenie: jesli masz niezarejestrowane, reczne mapowanie w innym addonie
lub modzie, skrypt nie moze go wykryc. Dodaj nazwy do DSRRL_LOWLOD_PROTECT.txt
obok EXE; podane tam nazwy zostana zachowane. Nie uzywaj skryptu
do przyszlych wersji addonu: SHA lock zabroni modyfikacji.
