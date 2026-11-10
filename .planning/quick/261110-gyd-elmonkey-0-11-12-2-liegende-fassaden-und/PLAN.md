---
slug: 261110-gyd
status: planned
---
# ELMonkey 0.11.12.2 — liegende Fassaden (Ebene/Segment) + Menübefehl „Bodenfliese aus Auswahl“

Quelle: zwei Kollegen-Patches, zusammen gestagt im Probe-Worktree
`P=/private/tmp/claude-501/-Users-ap/f3a4e181-68d7-4c0d-a14e-ba717e34ea19/scratchpad/elm-probe`
(`git -C $P diff --cached`: 9 Dateien, +704/−8). Inhalt: neue Befehle `ELM_SAB.GetCurtainWallPlaneELM`
und `ELM_SAB.SetCurtainWallSegmentELM`, `CreateCurtainWallFromAxes` mit `angle` 0 + optional
`planeMatrix`/`planeOffset`, Menübefehl „Bodenfliese aus Auswahl“, Version 0.11.12.2.
Laut Kollegen **Zwischenstand**, live nur AC29 Mac erprobt.

Ausgangslage: main = `af69eb9`, Repo öffentlich. Kein lokaler Build (nur CI AC27–30 × Mac/Win,
MSVC /W4 /WX, C++17). Nicht pushen, nicht taggen. Ungetrackte `core/uv.lock` und
`mcp/elmonkey-mcp/uv.lock` nicht anfassen.

Wichtige Fakten aus dem Bestand (für die Durchsicht):
- `MigrationHelper.hpp` hat `inline Int32 GetAttributeIndex (const API_AttributeIndex&)` (AC27+:
  `ToInt32_Deprecated`, sonst Rohwert) und `ACAPI_CreateAttributeIndex (Int32)`.
- `MigrationHelper.hpp` Z. 11–14: unter `#ifndef ServerMainVers_2700` werden
  `ACAPI_MenuItem_RegisterMenu`/`ACAPI_MenuItem_InstallMenuHandler` auf die Alt-Namen gemappt →
  Menüregistrierung in `AddOnMain.cpp` ist versionsfest, solange `MigrationHelper.hpp` eingebunden ist.
- `CMakeLists.txt`: `GenerateAddOnProject (… ${AddOnSourcesFolder} …)` mit `Sources` → neue .cpp wird
  automatisch erfasst.

## Tasks

### 1. Add-On-Code übernehmen, neutralisieren, prüfen
- **files:** `archicad-addon/ELM_SAB_Tapir/Sources/{AddOnMain.cpp, AddOnVersion.hpp, BodenflieseAusAuswahlELM.cpp, BodenflieseAusAuswahlELM.hpp, CreateCurtainWallCommand.cpp, CreateCurtainWallCommand.hpp, ResourceIds.hpp, RINT/AddOn.grc}`
- **action:**
  - Die 8 Quelldateien per `cp` aus `$P/archicad-addon/ELM_SAB_Tapir/Sources/…` übernehmen (inhaltlich,
    KEIN `git am`/`cherry-pick` – fremder Autor/Commit-Text). Vorher `git -C $P diff --cached --stat`
    gegen die Liste abgleichen; vorher prüfen, dass die Repo-Fassung der geänderten Dateien == `af69eb9`
    ist (sonst würden lokale Stände überschrieben).
  - Öffentliches Repo: „Futurelab“ in den Kommentaren von `CreateCurtainWallCommand.cpp` (Z. ~138, ~534)
    neutral umformulieren (z. B. „Live-Test AC29 2026-11-09“). Auch sonst keine Personen-/Projekt-/
    Büronamen in Code/Kommentaren (Kürzel „SAB“ als Add-On-Name bleibt).
  - Schema-Prüfung: Python-Skript nach `~/.scratch/elmonkey/tools/check_raw_json_schemas.py`
    (nicht Scratchpad) – extrahiert alle `R"(` … `)"`-Raw-Strings aus `CreateCurtainWallCommand.cpp`
    (und `BodenflieseAusAuswahlELM.cpp`, falls vorhanden) und ruft `json.loads` auf; jede Zeichenkette
    muss parsen (kaputtes Schema registriert den Befehl still nicht).
  - CI-Festigkeit per Durchsicht des Diffs (`git diff af69eb9 -- archicad-addon`): ungenutzte
    Variablen/Parameter (→ entfernen oder `UNUSED_PARAMETER`/unbenannt), signed/unsigned-Vergleiche
    (`UIndex`/`USize` vs. `Int32`, `size_t` vs. `int`), Shadowing, implizite Verengungen
    (`double`→`float`/`Int32`, `size_t`→`Int32`/`UInt32` → explizit casten), fehlende `default`
    im `switch` falls /W4 meckert. `def.header.layer.ToInt32_Deprecated ()` in
    `BodenflieseAusAuswahlELM.cpp` (Z. ~107) auf `GetAttributeIndex (def.header.layer)` umstellen und
    `MigrationHelper.hpp` einbinden. Weitere API-Aufrufe der neuen Datei (Element-/Library-/Selection-API,
    `API_Guid`-Konvertierung, `GS::UniString`) gegen bestehende Muster in anderen Sources und gegen
    `MigrationHelper.hpp` abgleichen (z. B. `ACAPI_Selection_Get`, `ACAPI_LibraryPart_Search`,
    `elemHead.type` vs. `typeID` → vorhandene Weichen nutzen). Menü: prüfen, dass `AddOnMain.cpp`
    `MigrationHelper.hpp` einbindet, `ID_ADDON_MENU_BODENFLIESE` in `ResourceIds.hpp` eindeutig ist und
    in `RINT/AddOn.grc` ein passender `'STR#'`-Eintrag existiert; `MenuCommandHandler`-Zweig ruft die neue
    Funktion auf. CMake-Glob bestätigen (keine CMake-Änderung).
  - `AddOnVersion.hpp` = 0.11.12.2; Registrierung in `elmSabCommands` mit Version "0.11.12.2".
- **verify:**
  - `git -C /Users/ap/.claude/repos/ELMonkey grep -i futurelab` → leer
  - `python3 -I ~/.scratch/elmonkey/tools/check_raw_json_schemas.py archicad-addon/ELM_SAB_Tapir/Sources/CreateCurtainWallCommand.cpp` → alle OK, Exit 0
  - `git grep -n "ToInt32_Deprecated" archicad-addon/ELM_SAB_Tapir/Sources/BodenflieseAusAuswahlELM.cpp` → leer
  - `git grep -n "GetCurtainWallPlaneELM\|SetCurtainWallSegmentELM" archicad-addon/ELM_SAB_Tapir/Sources/AddOnMain.cpp` → beide registriert
  - `git diff --stat af69eb9 -- archicad-addon` → nur die 8 Dateien
- **done:** Code im Repo, neutral, Schemas parsen, Durchsicht-Befunde behoben und in SUMMARY notiert.

### 2. Skill-Doku „Liegende Fassaden“
- **files:** `skills/archicad/recipes/curtain-walls.md`
- **action:** Kollegen-Änderung (1 Zeile) per `cp` aus `$P` übernehmen, dann Abschnitt „Liegende Fassaden“
  ergänzen, ehrlich und mit Kennzeichnung „Zwischenstand, live nur AC29 Mac erprobt (2026-11-10)“:
  `angle` 0 / `planeMatrix` / `planeOffset` beim `CreateCurtainWallFromAxes` wirken in Archicad derzeit
  NICHT; funktionierender Weg = liegendes Muster reservieren, per `TapirCommand.MoveElements` mit
  `copy: true` duplizieren, dann `ELM_SAB.SetCurtainWallSegmentELM`; `columnWidths`/`rowHeights` wirken,
  `height` wirkt nicht; Kontur/`baseLine` in Ebenen-Lokalkoordinaten, `GetCurtainWallPlaneELM` liefert
  Weltkoordinaten; nach Kontur-Änderung greift das Raster nicht; nach Änderung neue Segment-GUID
  (zurücklesen!); Menübefehl „Bodenfliese aus Auswahl“ braucht das geladene Bibliothekselement
  „SAB Bodenfliese liegend“ (liegt nicht im Repo). Keine Personen-/Projektnamen.
- **verify:** `grep -n "Liegende Fassaden\|Zwischenstand\|MoveElements\|SAB Bodenfliese liegend\|GetCurtainWallPlaneELM" skills/archicad/recipes/curtain-walls.md` → alle Treffer; `git grep -i futurelab` leer.
- **done:** Abschnitt vorhanden, alle Punkte aus Vorgabe 5 enthalten.

### 3. MCP 0.2.3 – Synonyme + Test
- **files:** `mcp/elmonkey-mcp/src/elmonkey_mcp/addons/archicad/catalog.py`, `mcp/elmonkey-mcp/tests/test_catalog.py`, `mcp/elmonkey-mcp/pyproject.toml`, `mcp/elmonkey-mcp/src/elmonkey_mcp/__init__.py`
- **action:** `SYNONYMS` ergänzen: „liegende fassade“/„fassadenebene“/„neigung“ → Plane/Segment
  (`curtain wall plane`), „fassadenraster“/„spaltenbreiten“/„zeilenhöhen“/„zeilenhoehen“ → Segment
  (`curtain wall segment`), „bodenfliese“ → Curtain-Wall-Befehle (Werte so wählen, dass die Suche
  `GetCurtainWallPlaneELM`/`SetCurtainWallSegmentELM` findet – Mehrwort-Werte sind erlaubt, siehe
  `split()` in Z. ~90; Mehrwort-Schlüssel prüfen, ggf. Einzelwörter als Schlüssel). Test
  `test_search_liegende_fassade` nach Muster `test_search_oberflaechen_export`. Version 0.2.2 → 0.2.3 in
  `pyproject.toml`; `__init__.py` (`__version__ = "0.1.0"`, veraltet) auf 0.2.3 angleichen. uv.lock nicht
  stagen.
- **verify:** `cd mcp/elmonkey-mcp && uv run pytest -q` → alle grün (bisher 8 + neuer Test).
- **done:** Tests grün, Version 0.2.3.

### 4. READMEs, Commits, SUMMARY
- **files:** `README.md`, `archicad-addon/README.md`, `archicad-addon/ELM_SAB_Tapir/README.md`, `mcp/elmonkey-mcp/README.md`, `core/README.md`, `docs/UMBAU.md`, `.planning/STATE.md`, `.planning/quick/261110-gyd-…/SUMMARY.md`
- **action:**
  - Versionsstellen: Add-On 0.11.12 → 0.11.12.2, MCP 0.2.2 → 0.2.3; „Abgleich zuletzt: 10.11.2026
    (Add-On 0.11.12.2 · Kern 0.3.0 · MCP 0.2.3 · Plattform-Monkey = Kern 0.3.0)“. Kern bleibt 0.3.0.
    In `ELM_SAB_Tapir/README.md` Tabellenzeilen für `GetCurtainWallPlaneELM`, `SetCurtainWallSegmentELM`
    und Menü „Bodenfliese aus Auswahl“ („Ab 0.11.12.2, Zwischenstand“). Alle Fundstellen mit
    `git grep -n "0\.11\.12\b\|0\.2\.2" -- '*.md'` (ohne `.planning`) abarbeiten.
  - `git config user.email mozzi666@gmail.com` (nur lokal im Repo). Atomare Commits, deutsch im
    `git log`-Stil, je mit Abschluss `Co-Authored-By: Claude <noreply@anthropic.com>`:
    1. `feat(elm_sab): liegende Fassaden – GetCurtainWallPlaneELM, SetCurtainWallSegmentELM, Menü „Bodenfliese aus Auswahl“ (0.11.12.2)`
    2. `docs(skill): liegende Fassaden – Zwischenstand AC29`
    3. `chore: elmonkey-mcp 0.2.3 (Synonyme liegende Fassade/Raster/Bodenfliese)`
    4. `docs: Versionsstände 0.11.12.2 / MCP 0.2.3 in READMEs`
    5. `docs(planning): Quick-Task 261110-gyd`
  - SUMMARY.md: Stand laut Kollegen Zwischenstand; ungeprüft = CI-Build AC27/28/30 + Windows (kein lokaler
    Build), Live nur AC29 Mac; `.gsm`/HSF („SAB Bodenfliese liegend“) bewusst nicht im Repo; Liste der
    Durchsicht-Befunde aus Task 1. STATE.md: Zeile in „Quick Tasks Completed“.
  - NICHT pushen, NICHT taggen.
- **verify:** `git log --oneline af69eb9..HEAD` → 5 Commits; `git status --short` → nur `?? core/uv.lock`, `?? mcp/elmonkey-mcp/uv.lock`; `git grep -i futurelab` leer; `git grep -n "Abgleich zuletzt: 10.11.2026" README.md` Treffer; `git log -5 --format=%ae` → mozzi666@gmail.com; `git status -sb` zeigt `ahead 5`, kein Tag neu.
- **done:** 5 lokale Commits, SUMMARY + STATE aktualisiert, nichts gepusht.
