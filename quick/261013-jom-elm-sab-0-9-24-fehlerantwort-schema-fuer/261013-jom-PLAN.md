---
quick_id: 261013-jom
type: quick
phase: quick
plan: 261013-jom
wave: 1
depends_on: []
autonomous: false
files_modified:
  - ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp
  - ELM_SAB_Tapir/Sources/SetLayerCommand.cpp
  - ELM_SAB_Tapir/Sources/SetObjectParametersForceCommand.cpp
  - ELM_SAB_Tapir/Sources/SetPenOfElementsCommand.cpp
  - ELM_SAB_Tapir/Sources/StoryVisibilityCommands.cpp
  - ELM_SAB_Tapir/Sources/SetTextSizeCommand.cpp
  - ELM_SAB_Tapir/Sources/AddOnMain.cpp
  - ELM_SAB_Tapir/Sources/AddOnVersion.hpp
  - reference/mcp-extension.md
  - ELM_SAB_Tapir/README.md
must_haves:
  truths:
    - "Die sechs Befehle CreateWallOpenings, SetLayerOfElements, SetObjectParametersForce, SetPenOfElements, SetStoryVisibilityOfElements, SetTextSizeOfElements deklarieren ihr Antwortschema über ELMResponseSchemaWithError (oneOf Erfolg|Fehler); das Erfolgsschema (Raw-String) ist byte-gleich zu 92ab02a"
    - "Jedes der sechs Erfolgsschemas hat ein nicht-leeres required ohne 'error' und kein 'error' in properties — eine Fehlerantwort passt also auf genau einen oneOf-Zweig"
    - "Alle 9 Top-Level-Fehlerrückgaben in den sechs Execute-Funktionen laufen über CreateErrorResponseELM mit unveränderter Konstante und byte-gleichem Meldungstext; Fehler je Element in executionResults/results und alle Erfolgsantworten sind unverändert"
    - "GetStoryVisibilityOfElementsCommand ist unangetastet (Schema + Execute byte-gleich)"
    - "ELM_SAB_VERSION = 0.9.24 (bzw. die nächste freie Patch-Version, falls 0.9.24 auf origin/main schon vergeben ist — dann Nutzer informiert); ADDON_VERSION bleibt 1.5.4; Registrierungsversionen der sechs Befehle unverändert, Beschreibungen um ' 0.9.24: Aufruffehler kommen schemakonform beim Client an (statt 4009).' ergänzt"
    - "elm_schema_scan.py zeigt für alle sechs errorAllowed=True (ELMResponseSchemaWithError); es bleibt kein Kandidat (errorAllowed=False und topLevelErrorReturns>0); alle übrigen Scan-Zeilen sind unverändert"
    - "reference/mcp-extension.md hat den Abschnitt '## ELM_SAB 0.9.24 — Fehlerantwort schemakonform für sechs Befehle <!-- 2026-10-13 -->' mit Befehlstabelle, Muster, Scan vorher/nachher, Stand 'Gebaut (CI), noch NICHT live verifiziert' und Testplan (je Befehl ein provozierbarer Top-Level-Fehler); der offene Punkt im 0.9.23-CaptureView-Unterpunkt verweist auf 'erledigt in 0.9.24'"
    - "Gepusht wird erst nach Gate-Freigabe (Live-Gegenprobe CaptureView-Fehlermuster bestanden); danach nennt ci-status/status.md den gepushten HEAD-SHA mit allen 6 Bundles und ohne '**fehlgeschlagen**' (oder Abbruch nach 4 Fix-Runden sauber dokumentiert)"
  artifacts:
    - path: "ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp"
      contains: "ELMResponseSchemaWithError"
    - path: "ELM_SAB_Tapir/Sources/SetLayerCommand.cpp"
      contains: "ELMResponseSchemaWithError"
    - path: "ELM_SAB_Tapir/Sources/SetObjectParametersForceCommand.cpp"
      contains: "ELMResponseSchemaWithError"
    - path: "ELM_SAB_Tapir/Sources/SetPenOfElementsCommand.cpp"
      contains: "ELMResponseSchemaWithError"
    - path: "ELM_SAB_Tapir/Sources/StoryVisibilityCommands.cpp"
      contains: "ELMResponseSchemaWithError"
    - path: "ELM_SAB_Tapir/Sources/SetTextSizeCommand.cpp"
      contains: "ELMResponseSchemaWithError"
    - path: "ELM_SAB_Tapir/Sources/AddOnVersion.hpp"
      contains: "#define ELM_SAB_VERSION \"0.9.24\""
    - path: "ELM_SAB_Tapir/Sources/AddOnMain.cpp"
      contains: "0.9.24: Aufruffehler kommen schemakonform beim Client an (statt 4009)."
    - path: "reference/mcp-extension.md"
      contains: "## ELM_SAB 0.9.24"
    - path: "ELM_SAB_Tapir/README.md"
      contains: "0.9.24"
  key_links:
    - from: "sechs GetResponseSchema-Definitionen"
      to: "ELMCommandBase.hpp"
      via: "ELMResponseSchemaWithError (GS::UniString (R\"(<Erfolgsschema unverändert>)\"))"
      pattern: "return ELMResponseSchemaWithError \\(GS::UniString \\(R\""
    - from: "Top-Level-Fehlerpfade der sechs Execute-Funktionen"
      to: "ELMCommandBase.hpp"
      via: "CreateErrorResponseELM (GSErrCode, const GS::UniString&) statt Tapirs CreateErrorResponse"
      pattern: "return CreateErrorResponseELM \\("
    - from: "reference/mcp-extension.md (0.9.23, CaptureView-Unterpunkt, Offener Punkt)"
      to: "reference/mcp-extension.md (## ELM_SAB 0.9.24)"
      via: "Verweis 'erledigt in 0.9.24'"
      pattern: "erledigt in 0.9.24"
---

# Quick 261013-jom: ELM_SAB 0.9.24 — Fehlerantwort schemakonform für sechs weitere Befehle

**Problem (wie CaptureView vor 0.9.23):** Das Antwortschema dieser sechs Befehle hat ein nicht-leeres `required` und
keinen Fehlerzweig. Jede Top-Level-Fehlerantwort `{error:{code,message}}` verletzt das Schema. Der Client sieht dann nur
`4009 Invalid command response … '#/error'` statt der echten Meldung. Vorbild ist der Fix `217d2ad` (CaptureView).

**Feste Vorgaben (nicht neu verhandeln):**
- Kein lokaler Build. Bauen = Push → CI AC27/28/29 × Mac/Win.
- C++17 (AC27/28), String-Literale nur ASCII.
- Archicad nicht ansprechen, kein Bundle-Download und kein Bundle-Tausch.
- Nichts außerhalb von Repo und Session-Scratchpad schreiben. Hilfsskripte liegen im Quick-Ordner (Repo).
- Commits direkt auf `main`, mit Autor wie lokal gesetzt (Mudi). Jede Message endet mit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Keine `.planning`-Dateien committen, das macht der Orchestrator.

**ABLAUF-GATE:** Tasks 1–2 = nur lokale Commits, KEIN Push. Task 3 ist ein `checkpoint:human-action`. Der Executor
**stoppt davor** und gibt an den Orchestrator zurück. Push, CI und Scan-Nachweis kommen erst nach Freigabe. Grund: Eine andere
Session macht gerade die Live-Gegenprobe des CaptureView-Fehlermusters aus 0.9.23. Kommt dort trotz `oneOf` weiter 4009,
wirkt das Muster nicht → dann NICHT pushen.

## Vorab geprüft (Planer) — Fundstellen, Stand `92ab02a` (= HEAD = origin/main)

Pfade relativ zu `ELM_SAB_Tapir/Sources/`.

| Befehl (`commandName`) | Klasse / Datei | Erfolgsschema `GetResponseSchema` → `required` | Top-Level-Fehler im Execute (Zeile, Konstante, Text — Text bleibt BYTE-GLEICH) | Registrierung AddOnMain.cpp |
|---|---|---|---|---|
| `CreateWallOpenings` | `CreateWallOpeningsCommand` / CreateWallOpeningsCommand.cpp | Z. 96–108 (Raw-String Z. 98–107) → `["results","undoScope"]` | Z. 584 `APIERR_BADPARS` "openings ist leer." · Z. 587–591 (nur AC27/28, `#ifndef ServerMainVers_2900`) `APIERR_NOTSUPPORTED`, 4 aneinandergereihte Literale ab "CreateWallOpenings braucht die ACAPI::Element::Opening-API und damit Archicad 29. " | Z. 1053–1056, Version "0.9.21" |
| `SetLayerOfElements` | `SetLayerOfElementsCommand` / SetLayerCommand.cpp | Z. 65–75 (Raw Z. 67–74) → `["executionResults"]` | Z. 102 `APIERR_BADPARS` "layerId oder layerName erforderlich." · Z. 105 `APIERR_BADPARS` "Ebene nicht gefunden: " + `what` (GS::UniString) | Z. 1065–1068, "0.9.19" |
| `SetObjectParametersForce` | `SetObjectParametersForceCommand` / SetObjectParametersForceCommand.cpp | Z. 73–83 (Raw Z. 75–82) → `["executionResults","undoScope"]` | Z. 326 `APIERR_BADPARS` "elements ist leer." | Z. 1025–1028, "0.9.16" |
| `SetPenOfElements` | `SetPenOfElementsCommand` / SetPenOfElementsCommand.cpp | Z. 79–97 (Raw Z. 81–96) → `["executionResults"]` | Z. 244 `APIERR_BADPARS` "Mindestens ein Stift-Parameter oder clearForegroundRGB/clearBackgroundRGB muss gesetzt sein." | Z. 961–964, "0.9.0" |
| `SetStoryVisibilityOfElements` | `SetStoryVisibilityOfElementsCommand` / StoryVisibilityCommands.cpp | Z. 112–119 (Raw Z. 114–118) → `["executionResults"]` | Z. 132 `APIERR_BADPARS` "Unbekannter visibility-Wert: " + `preset` · Z. 141 `APIERR_BADPARS` "visibility oder custom erforderlich." | Z. 1017–1020, "0.9.15" |
| `SetTextSizeOfElements` (!) | `SetTextSizeCommand` / SetTextSizeCommand.cpp | Z. 40–52 (Raw Z. 42–51) → `["executionResults"]` | Z. 64 `APIERR_BADPARS` "Genau einer von sizeMm oder factor muss angegeben sein (> 0)." | Z. 985–988, "0.9.0" |

- **Falle beim Editieren:** Jede dieser Dateien hat VOR dem Antwortschema ein `GetParameterSchema` mit demselben Ende `})");`
  (Z. 93 / 62 / 70 / 76 / 109 / 37). NUR das `GetResponseSchema` anfassen.
- **Nicht anfassen:** `GetStoryVisibilityOfElementsCommand` in StoryVisibilityCommands.cpp (Schema Z. 207–212, Execute ab Z. 214). Es hat keine Top-Level-Fehler, nur `error` je Element (Z. 228).
- **Weitere Rückgaben im Execute (geprüft):** Außer den 9 oben gibt es nur `return response;` und `return NoError;` in Undo-Lambdas.
  `CreateFailedExecutionResult` kommt nur innerhalb von `executionResults`/`results` vor (je Element) → bleibt.
  Der Executor prüft das trotzdem selbst (Task 1, Schritt 3).
- **Erfolgsschemas (geprüft):** Alle sechs haben ein nicht-leeres `required` ohne `error` und kein `error` in `properties`.
  `additionalProperties` ist nicht gesetzt. Eine Fehlerantwort scheitert am Erfolgszweig nur über `required`, deshalb ist die Prüfung in Task 1 Pflicht.
  Alle Erfolgsantworten liefern die `required`-Felder auf jedem Pfad: CreateWallOpenings Z. ~719–766 `results` + `undoScope`, SetObjectParametersForce `executionResults` + `undoScope`, die übrigen `executionResults`.
- **Header:** Alle sechs .hpp binden `ELMCommandBase.hpp` ein. Beide Helfer sind dort `inline` (Z. ~101 und ~113) → keine Include-Änderung nötig.
- **Fehlercodes für den Testplan:** `APIErrorStart` = 0x81060000 (`API_ModuleId` 262). Daraus folgt
  `APIERR_BADPARS` = APIErrorStart+104 = **-2130313112** und `APIERR_NOTSUPPORTED` = +903 = **-2130312313** (APIdefs_ErrorCodes.h Z. 27/57/262).
- **Nicht-ASCII-Bestand:** Die .cpp-Dateien enthalten schon Nicht-ASCII-Bytes (Kommentare). Die Registrierungszeile von SetObjectParametersForce
  (AddOnMain Z. 1027) enthält ein „—". Die Prüfung lautet deshalb „Anzahl Nicht-ASCII-Bytes je Datei unverändert", nicht „Datei rein ASCII".
- **Scan-Ausgangslage** (`python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources`): Die sechs Befehle haben
  `errorAllowed=False`, `topLevelErrorReturns` = 2/2/1/1/2/1. Alle anderen `errorAllowed=False`-Befehle haben 0. CaptureView und FindElements haben schon True.
  Der Scan zählt per Regex `return\s+CreateErrorResponse`, das trifft auch `CreateErrorResponseELM`. Die Zahlen bleiben also gleich, nur `errorAllowed` kippt.
- **Lokale Syntaxprüfung (vom Planer am HEAD erprobt):** `clang++ -std=c++20 -fsyntax-only` gegen
  `/Users/ap/SchwarzPlatform/devkit/API Development Kit 29.3100/Support`. Includes: `-isystem Inc` plus `-isystem` je `Modules/*/`, `-I ELM_SAB_Tapir/Sources`.
  Dazu `-Dmacintosh=1 -DACExtension` und die Mac-Flags aus `ELM_SAB_Tapir/Tools/CMakeCommon.cmake` Z. 34–53.
  Ergebnis: 0 Fehler für die sechs .cpp und AddOnMain.cpp, je ~1 s. Ein absichtlicher Fehler wird erkannt.
  **C++17 ist lokal NICHT prüfbar:** Die DevKit-29-Header nutzen `concept` (HashCommon.hpp), schon am HEAD gibt es 20 Fehler.
  Die C++17-Sicherheit kommt deshalb über das Diff-Grep. Auch der AC27/28-Zweig `#ifndef ServerMainVers_2900` in CreateWallOpenings wird lokal nicht kompiliert (ACAPinc.h definiert das Makro). Dort nur die Form ändern.
- **CI:** Der Trigger schließt `ELM_SAB_Tapir/**/*.md` aus (README allein baut nicht). status.md nennt `${GITHUB_SHA:0:7}` = HEAD des Pushs. Ein Fehlschlag
  steht als `**fehlgeschlagen**` in der Tabellenzelle. Die feste Zeile „Zu jeder fehlgeschlagenen Version …" steht IMMER drin → nie auf das bloße Wort prüfen.
  Logs liegen unter `origin/ci-status:logs/build-{mac,win}-AC{27,28,29}.log`. `concurrency cancel-in-progress` gilt: Ein zweiter Push bricht den laufenden Lauf ab.

## Entwurfsentscheidungen des Planers (Claude's Discretion)

1. **Meldungstexte byte-gleich, KEIN Befehlspräfix** (anders als bei CaptureView „CaptureView: …"). Der Auftrag verlangt unveränderte Texte, der Aufrufer
   kennt den Befehl ohnehin, und Byte-Gleichheit ist maschinell prüfbar. Literale werden explizit in `GS::UniString (…)` gefasst. Verkettungen bleiben als
   `GS::UniString ("Ebene nicht gefunden: ") + what` bzw. `GS::UniString ("Unbekannter visibility-Wert: ") + preset` erhalten.
2. **Beschreibungs-Suffix in AddOnMain** (Muster aus 217d2ad): an alle sechs Beschreibungen exakt ` 0.9.24: Aufruffehler kommen schemakonform beim Client an (statt 4009).` anhängen (ASCII, führendes Leerzeichen, vor dem schließenden `"`).
3. **Ein Kommentar je Datei** direkt über dem neuen `return` im GetResponseSchema, ASCII:
   `// ELM_SAB 0.9.24: Erfolgszweig unveraendert; ELMResponseSchemaWithError erlaubt zusaetzlich {error:{code,message}} (sonst 4009 statt Meldung).`
4. **Prüfskripte im Quick-Ordner** statt im Scratchpad. Sie überleben einen Neustart, und der Fortsetzungs-Executor nach dem Gate braucht sie wieder. Der Orchestrator committet sie mit den Planungsdateien.

<interfaces>
Aus ELM_SAB_Tapir/Sources/ELMCommandBase.hpp (seit 0.9.23, unverändert benutzen):

```cpp
inline GS::UniString ELMResponseSchemaWithError (const GS::UniString& successSchema);
//  -> "{\"oneOf\": [" + successSchema + ", {\"type\":\"object\",\"properties\":{\"error\":{\"type\":\"object\",\"properties\":
//     {\"code\":{\"type\":\"integer\"},\"message\":{\"type\":\"string\"}},\"additionalProperties\":false,\"required\":[\"code\",\"message\"]}},
//     \"additionalProperties\":false,\"required\":[\"error\"]}]}"
inline GS::ObjectState CreateErrorResponseELM (GSErrCode errorCode, const GS::UniString& message);
//  -> {"error": {"code": errorCode, "message": message}}   (Meldung als UniString, Umlaut-sicher)
```

Vorbild 217d2ad (CaptureViewCommand.hpp), Form vorher → nachher:

```cpp
// vorher
return R"({ ...Erfolgsschema... })";
return CreateErrorResponse (APIERR_BADPARS, "outputPath fehlt oder ist leer");
// nachher
return ELMResponseSchemaWithError (GS::UniString (R"({ ...Erfolgsschema UNVERAENDERT... })"));
return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("CaptureView: outputPath fehlt oder ist leer"));
```

In den sechs Dateien steht das Erfolgsschema schon als `return GS::UniString (R"({ … })");`. Umbau = nur
`return GS::UniString (R"({` → `return ELMResponseSchemaWithError (GS::UniString (R"({` und das zugehörige `})");` → `})"));`.
Der Raw-String-Inhalt (inkl. Einrückung) bleibt unberührt.
</interfaces>

<context>
@.planning/STATE.md
@.planning/quick/261013-fel-elm-sab-0-9-23-findelements-mit-und-oder/261013-fel-SUMMARY.md
@ELM_SAB_Tapir/Sources/ELMCommandBase.hpp
@ELM_SAB_Tapir/Sources/AddOnVersion.hpp
</context>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Sechs Befehle schemakonform (Schema-Wrapper + Fehlerpfade), Version 0.9.24, Prüfskripte, Syntax + Scan lokal</name>
  <files>ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp, ELM_SAB_Tapir/Sources/SetLayerCommand.cpp, ELM_SAB_Tapir/Sources/SetObjectParametersForceCommand.cpp, ELM_SAB_Tapir/Sources/SetPenOfElementsCommand.cpp, ELM_SAB_Tapir/Sources/StoryVisibilityCommands.cpp, ELM_SAB_Tapir/Sources/SetTextSizeCommand.cpp, ELM_SAB_Tapir/Sources/AddOnMain.cpp, ELM_SAB_Tapir/Sources/AddOnVersion.hpp — Hilfsdateien (NICHT committen): .planning/quick/261013-jom-elm-sab-0-9-24-fehlerantwort-schema-fuer/{schema_check_0924.py, syntax_check_0924.sh, scan_vorher.txt, scan_nachher.txt}</files>
  <behavior>
    - schema_check_0924.py am unveränderten HEAD: Exit ≠ 0 (RED, kein ELMResponseSchemaWithError in den sechs Schemas).
    - Nach dem Umbau: Exit 0. Der Raw-String jedes der sechs GetResponseSchema ist byte-gleich zur Basis-Revision und steckt in ELMResponseSchemaWithError (GS::UniString (R"(…)")).
    - Jedes Erfolgsschema: json.loads ok, required nicht leer, 'error' weder in required noch in properties. Das zusammengesetzte oneOf-Schema ist gültiges JSON mit genau 2 Zweigen.
    - Fehlerbeispiel {"error":{"code":-2130313112,"message":"x"}} erfüllt das required des Erfolgszweigs NICHT, Erfolgsbeispiel (alle required-Schlüssel) erfüllt required ["error"] NICHT.
    - Im Execute jedes der sechs: 0 × 'CreateErrorResponse (' und genau 2/2/1/1/2/1 × 'return CreateErrorResponseELM ('. Alle 9 Original-Konstanten und Meldungs-Literale sind unverändert vorhanden.
    - GetStoryVisibilityOfElementsCommand::GetResponseSchema und ::Execute byte-gleich zur Basis.
    - AddOnMain.cpp: genau die 6 Beschreibungszeilen geändert. Neu = alt + Suffix, Versionsargumente unverändert.
    - AddOnVersion.hpp: ELM_SAB_VERSION = Sollversion, ADDON_VERSION = "1.5.4".
    - Nicht-ASCII-Byte-Anzahl je geänderter Datei unverändert (keine neuen Nicht-ASCII-Zeichen).
  </behavior>
  <action>
Vorbereitung (Arbeitsverzeichnis /Users/ap/.claude/repos/archicad-skill, Quick-Ordner Q = .planning/quick/261013-jom-elm-sab-0-9-24-fehlerantwort-schema-fuer):

0. Ausgangslage sichern. `git status --short -- ELM_SAB_Tapir reference` muss leer sein, und `git diff --stat 92ab02a HEAD -- ELM_SAB_Tapir reference` muss leer sein. HEAD ist also 92ab02a oder hat darüber nur `.planning`-Commits des Orchestrators. Sonst stoppen und berichten.
   Danach den Scan laufen lassen und die Ausgabe nach Q/scan_vorher.txt schreiben: `python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources`.

1. Prüfskript Q/schema_check_0924.py mit dem Write-Tool anlegen.
   - Aufruf: `python3 -I schema_check_0924.py <repo> <basis-rev> <sollversion>`. Nur Standardbibliothek (json, re, subprocess, sys). Die Basis-Dateien liest das Skript per `git -C <repo> show <basis-rev>:<pfad>`.
   - Es prüft genau die Punkte aus `<behavior>`:
     - Klassen→Datei-Zuordnung und Soll-Anzahlen aus der Tabelle oben.
     - Das Schema findet es per Regex `<Klasse>::GetResponseSchema \(\) const\s*\{(.*?)\n\}` (re.S), den Raw-String darin per `R"\((.*?)\)"` (re.S).
     - Das Execute findet es per `<Klasse>::Execute \(.*?\n\}` (re.S).
     - Der Fehlerzweig wird für den JSON-Test identisch zum Text aus ELMResponseSchemaWithError zusammengesetzt.
     - AddOnMain-Prüfung: Zeilen von Basis und aktuell vergleichen (difflib). Erwartet sind genau 6 geänderte Zeilen, und jede neue Zeile ist gleich der alten Zeile, wobei vor dem letzten `"` der Suffix ` <sollversion>: Aufruffehler kommen schemakonform beim Client an (statt 4009).` eingefügt ist.
     - Nicht-ASCII-Prüfung: Bytes > 127 je Datei (6 .cpp + AddOnMain.cpp + AddOnVersion.hpp) zählen, Basis gegen aktuell.
   - Ausgabe: je Prüfpunkt eine Zeile `OK …`/`FEHLER …`. Exit 1 bei mindestens einem Fehler.
   - Danach RED-Lauf: `python3 -I Q/schema_check_0924.py . 92ab02a 0.9.24` → muss mit Exit ≠ 0 scheitern.

2. Q/syntax_check_0924.sh mit dem Write-Tool anlegen (bash, `set -u`). Inhalt:
   - DK="/Users/ap/SchwarzPlatform/devkit/API Development Kit 29.3100/Support". INC-Array: `-isystem "$DK/Inc"` plus `-isystem` je `"$DK"/Modules/*/`.
   - FLAGS: `-std=c++20 -fsyntax-only -Dmacintosh=1 -DACExtension -Wall -Wextra -Werror` plus alle 19 `-Wno-…` aus CMakeCommon.cmake Z. 35–53 (multichar, ctor-dtor-privacy, invalid-offsetof, ignored-qualifiers, reorder, overloaded-virtual, unused-parameter, unused-value, unused-private-field, deprecated, unknown-pragmas, missing-braces, missing-field-initializers, non-c-typedef-for-linkage, uninitialized-const-reference, shorten-64-to-32, sign-compare, switch).
   - Schleife über die sechs .cpp und AddOnMain.cpp mit `-I ELM_SAB_Tapir/Sources`. Je Datei `OK <datei>` bzw. die ersten 20 `error:`-Zeilen ausgeben. Exit 1, wenn eine Datei scheitert.
   - Kopfkommentar: nur der AC29-Zweig ist prüfbar, C++17 nicht (DevKit-29-Header brauchen `concept`), es wird nichts geschrieben.

3. Umbau der sechs Dateien (Fundstellen siehe Tabelle „Vorab geprüft"). Vorher jede Execute-Funktion komplett lesen und JEDE `return`-Anweisung auf oberster Ebene prüfen. Findet sich eine weitere Top-Level-Fehlerrückgabe, die in der Tabelle fehlt, wird sie gleich behandelt und als Abweichung notiert.
   a) GetResponseSchema: genau wie im `<interfaces>`-Block. `return GS::UniString (R"({` wird zu `return ELMResponseSchemaWithError (GS::UniString (R"({`, das zugehörige `})");` wird zu `})"));`. Darüber kommt der Kommentar aus Entwurfsentscheidung 3. Raw-String-Inhalt nicht anfassen. NICHT das GetParameterSchema derselben Datei ändern.
   b) Top-Level-Fehler: `return CreateErrorResponse (K, <text>);` wird zu `return CreateErrorResponseELM (K, GS::UniString (<text>));`. Konstante K und Literal bleiben zeichengleich.
      Verkettungen werden zu `GS::UniString ("Ebene nicht gefunden: ") + what` bzw. `GS::UniString ("Unbekannter visibility-Wert: ") + preset`.
      Für CreateWallOpenings Z. 587–591 (AC27/28-Zweig) bleiben die vier aneinandergereihten Literale innerhalb von `GS::UniString ( … )` unverändert. Das `#ifndef`/`#else`/`#endif`-Gerüst bleibt, wie es ist.
   c) Unverändert lassen: Fehler je Element (`CreateFailedExecutionResult`, `entry.Add ("error", …)`), Erfolgsantworten, Undo-/Teamwork-Logik und GetStoryVisibilityOfElements.
   d) Keine C++20-Konstrukte: kein `!=`/`<=>` auf UniString, kein `std::format`, keine designated initializers.

4. AddOnVersion.hpp: `#define ELM_SAB_VERSION "0.9.23"` wird zu `"0.9.24"`. ADDON_VERSION bleibt "1.5.4".
   Versionsprüfung: `git ls-remote origin main` darf noch 92ab02a… zeigen. Wenn nicht, mit `git show origin/main:ELM_SAB_Tapir/Sources/AddOnVersion.hpp` nachsehen (vorher `git fetch -q origin main`). Ist dort 0.9.24 schon vergeben, NICHT selbst umnummerieren. Weiter mit 0.9.24 lokal und den Befund in der Gate-Rückgabe melden. Die Umnummerierung macht Task 3.

5. AddOnMain.cpp: an die Beschreibungs-Strings der sechs Registrierungen (Z. 963, 987, 1019, 1027, 1055, 1067) den Suffix aus Entwurfsentscheidung 2 anhängen. Versionsargumente ("0.9.0", "0.9.0", "0.9.15", "0.9.16", "0.9.21", "0.9.19") NICHT ändern.

6. Prüfen (GREEN):
   - `python3 -I Q/schema_check_0924.py . 92ab02a 0.9.24` → Exit 0.
   - `bash Q/syntax_check_0924.sh` → Exit 0.
   - Scan erneut laufen lassen, Ausgabe nach Q/scan_nachher.txt.
   - Diff-Grep auf C++20-Konstrukte in den hinzugefügten Zeilen (siehe verify).
   - Bei Fehlern korrigieren und erneut prüfen, bis alles grün ist.

7. Commit (nur die 8 Quelldateien, `git add` mit expliziten Pfaden, NICHT pushen). Betreff: `fix(ELM_SAB): 0.9.24 Fehlerantwort schemakonform fuer sechs Befehle`.
   Body-Stichpunkte:
   - Antwortschema oneOf Erfolg|Fehler (ELMResponseSchemaWithError), Erfolgsschema byte-gleich
   - 9 Top-Level-Fehler ueber CreateErrorResponseELM, Code und Text unveraendert
   - Befehle: CreateWallOpenings, SetLayerOfElements, SetObjectParametersForce, SetPenOfElements, SetStoryVisibilityOfElements, SetTextSizeOfElements
   - Registrierungsversionen bleiben, Beschreibung um 0.9.24 ergaenzt; ELM_SAB_VERSION 0.9.24
   Abschlusszeile: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && Q=.planning/quick/261013-jom-elm-sab-0-9-24-fehlerantwort-schema-fuer && python3 -I $Q/schema_check_0924.py . 92ab02a 0.9.24 && bash $Q/syntax_check_0924.sh && python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources > $Q/scan_nachher.txt && test "$(grep -E '^(CreateWallOpenings|SetLayerOfElements|SetObjectParametersForce|SetPenOfElements|SetStoryVisibilityOfElements|SetTextSize)Command ' $Q/scan_nachher.txt | grep -c 'errorAllowed=True (ELMResponseSchemaWithError)')" = 6 && test "$(grep 'errorAllowed=False' $Q/scan_nachher.txt | grep -vc 'topLevelErrorReturns=0$')" = 0 && diff <(grep -vE '^(CreateWallOpenings|SetLayerOfElements|SetObjectParametersForce|SetPenOfElements|SetStoryVisibilityOfElements|SetTextSize)Command ' $Q/scan_vorher.txt) <(grep -vE '^(CreateWallOpenings|SetLayerOfElements|SetObjectParametersForce|SetPenOfElements|SetStoryVisibilityOfElements|SetTextSize)Command ' $Q/scan_nachher.txt) && ! git diff 92ab02a HEAD -- ELM_SAB_Tapir/Sources | grep '^+' | grep -v '^+++' | grep -nE '!=|<=>|std::format|\{ *\.[A-Za-z_]+ *=|co_await|concept ' && test $(git show --name-only --format= HEAD | wc -l) -eq 8 && git log -1 --format=%B | grep -q 'Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>' && test "$(git rev-list --count origin/main..HEAD -- ELM_SAB_Tapir reference)" = 1 && test -z "$(git status --short -- ELM_SAB_Tapir reference)"</automated>
  </verify>
  <done>Alle sechs Antwortschemas sind oneOf Erfolg|Fehler, die Erfolgsschemas byte-gleich. Alle 9 Top-Level-Fehler laufen über CreateErrorResponseELM mit unveränderter Konstante und unverändertem Text. GetStoryVisibilityOfElements ist unberührt, ELM_SAB_VERSION = 0.9.24, und die sechs Beschreibungen sind ergänzt.
  Lokal grün: schema_check (vorher RED belegt), clang-Syntaxprüfung AC29, Scan (6 × errorAllowed=True, 0 Kandidaten, Rest unverändert). Es gibt genau 1 lokalen, nicht gepushten Commit mit 8 Dateien.</done>
</task>

<task type="auto">
  <name>Task 2: Doku — Abschnitt 0.9.24 (Tabelle, Muster, Scan vorher/nachher, Stand, Testplan), Verweis im 0.9.23-Unterpunkt, README-Hinweis</name>
  <files>reference/mcp-extension.md, ELM_SAB_Tapir/README.md</files>
  <action>
1. reference/mcp-extension.md, im 0.9.23-Unterpunkt „### CaptureView: Fehlerantwort schemakonform, Layout-Verhalten" (Z. ~1550–1556), Bullet „**Offener Punkt (nicht gefixt):**":
   - Das Label wird zu „**Offener Punkt → erledigt in 0.9.24** (Abschnitt „ELM_SAB 0.9.24 — Fehlerantwort schemakonform für sechs Befehle" unten):".
   - Die bestehende Befehlsliste bleibt als Historie stehen. Nur „`SetTextSize`" wird dort zu „`SetTextSizeOfElements`" korrigiert (echter commandName).
   - Sonst im 0.9.23-Abschnitt nichts ändern. Die andere Session pflegt dort evtl. Live-Ergebnisse → Konflikte vermeiden.

2. Am Dateiende (nach „### Testplan Einbau + Gegenprobe" von 0.9.23) anhängen. Deutsch, knapp, im Stil der Nachbarabschnitte, Umlaute in Markdown erlaubt:
   „## ELM_SAB 0.9.24 — Fehlerantwort schemakonform für sechs Befehle <!-- 2026-10-13 -->" mit folgendem Inhalt:
   - **Anlass:** offener Punkt aus 0.9.23 (CaptureView-Befund WiSchu 13.10.). Antwortschema mit nicht-leerem `required` ohne Fehlerzweig → jede Top-Level-Fehlerantwort `{error:{code,message}}` kam als `4009 Invalid command response … '#/error'` an statt der Meldung.
   - **Muster (wie CaptureView 0.9.23):** `GetResponseSchema` → `ELMResponseSchemaWithError (<Erfolgsschema>)`, also `oneOf` Erfolg | Fehler mit byte-gleichem Erfolgsschema. Top-Level-Fehler → `CreateErrorResponseELM`; Code und Text sind unverändert und kommen jetzt Umlaut-sicher an (Ebenennamen!).
     Unverändert bleiben: Erfolgsantworten, Fehler je Element in `executionResults`/`results`, Registrierungsversionen (Beschreibung um „0.9.24: …" ergänzt). `GetStoryVisibilityOfElements` ist unangetastet, weil es keine Top-Level-Fehler hat.
     Bedingung für `oneOf`: Das Erfolgsschema muss ein nicht-leeres `required` ohne `error` haben, sonst passt eine Fehlerantwort auf beide Zweige. Geprüft für alle sechs.
   - **Tabelle** „Befehl (`commandName`) | Datei | Top-Level-Fehler (Code) | Meldung | Probe-Aufruf (`addOnCommandParameters`)". Die Werte stehen in der Tabelle „Vorab geprüft" dieses Plans. Codes als Konstante + Zahl (APIERR_BADPARS -2130313112, APIERR_NOTSUPPORTED -2130312313). Proben:
     - CreateWallOpenings `{"openings":[]}` → „openings ist leer." Unter AC27/28 antwortet jeder Aufruf mit APIERR_NOTSUPPORTED und dem AC29-Hinweis.
     - SetLayerOfElements `{"elements":[]}` → „layerId oder layerName erforderlich.", und `{"elements":[],"layerName":"ZZ_Prüfebene_gibt_es_nicht"}` → „Ebene nicht gefunden: ZZ_Prüfebene_gibt_es_nicht". Das „ü" muss unverstümmelt ankommen.
     - SetObjectParametersForce `{"elements":[]}` → „elements ist leer."
     - SetPenOfElements `{"elements":[]}` → „Mindestens ein Stift-Parameter …"
     - SetStoryVisibilityOfElements `{"elements":[],"visibility":"Quatsch"}` → „Unbekannter visibility-Wert: Quatsch", und `{"elements":[]}` → „visibility oder custom erforderlich."
     - SetTextSizeOfElements `{"elements":[],"sizeMm":2.5,"factor":1.2}` → „Genau einer von sizeMm oder factor muss angegeben sein (> 0)."
   - **Scan vorher/nachher** (`python3 -I ~/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources`): die sechs Zeilen aus Q/scan_vorher.txt und Q/scan_nachher.txt wörtlich (Codeblock).
     Hinweis: `topLevelErrorReturns` bleibt gleich, weil der Regex auch `CreateErrorResponseELM` zählt. Nachher gibt es 0 Kandidaten, alle übrigen Zeilen sind unverändert.
   - **Stand:** „Gebaut (CI), noch NICHT live verifiziert — CI-SHA im Quick-Task-SUMMARY 261013-jom." Gepusht wurde erst, nachdem die Live-Gegenprobe des CaptureView-Fehlermusters (0.9.23, Testplan 9/11) bestanden war.
   - **### Testplan Live-Gegenprobe:**
     1. Einbau nach den Bundle-Regeln von 0.9.23: vorheriges Bundle sichern, Archicad komplett beenden (nie kill), nur EINE Instanz. `ELM_SAB.GetAddOnVersion` meldet 0.9.24 + Build-Zeitstempel. Bei Problemen erst Rückbau.
     2. Je Befehl die Probe(n) aus der Tabelle über `API.ExecuteAddOnCommand` (`commandNamespace` "ELM_SAB"). Erwartet: die Antwort enthält `error.code` und `error.message` mit genau dem Tabellentext, KEIN 4009.
        Begründete Ausnahme von der Regel „Aktionsbefehle nie mit leeren Parametern aufrufen": Die Elementliste ist leer, und der Fehlerpfad kehrt laut Code VOR jeder Reservierung, Undo-Klammer und Änderung zurück (Zeilen in der Tabelle). Es wird nichts ausgeführt.
     3. Erfolgsweg unverändert: mindestens ein Schreibbefehl mit gültigen Parametern an einem EIGENEN Testelement, z. B. SetLayerOfElements hin und zurück wie in 0.9.20 Schritt 2. Erwartet: `executionResults` wie bisher, kein 4009. Nie Danis Elemente, Teamwork danach senden. Nur mit Freigabe des Nutzers (Update-Regel).
     4. Danach ist nichts reserviert, und keine Auswahl oder Hervorhebung bleibt zurück.

3. ELM_SAB_Tapir/README.md: direkt nach der Befehlstabelle (nach der `FindElements`-Zeile, Z. 94, vor „## Selbst bauen") einen Absatz einfügen:
   „Ab 0.9.24 kommen Aufruffehler (fehlender oder ungültiger Parameter) bei `SetPenOfElements`, `SetTextSizeOfElements`, `SetStoryVisibilityOfElements`, `SetLayerOfElements`, `CreateWallOpenings` und `SetObjectParametersForce` als lesbare Fehlermeldung beim Client an statt als `4009 Invalid command response` (Details: [reference/mcp-extension.md](../reference/mcp-extension.md), Abschnitt 0.9.24)."
   Sonst nichts ändern.

4. Commit (nur diese 2 Dateien, NICHT pushen). Betreff: `docs(ELM_SAB): 0.9.24 Fehlerantwort schemakonform – Referenz, Scan, Testplan`. Abschlusszeile `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && grep -c '^## ELM_SAB 0.9.24 — Fehlerantwort schemakonform für sechs Befehle <!-- 2026-10-13 -->' reference/mcp-extension.md && S=$(awk '/^## ELM_SAB 0.9.24/{f=1} f' reference/mcp-extension.md) && for t in CreateWallOpenings SetLayerOfElements SetObjectParametersForce SetPenOfElements SetStoryVisibilityOfElements SetTextSizeOfElements ELMResponseSchemaWithError CreateErrorResponseELM '-2130313112' '-2130312313' 'noch NICHT live verifiziert' 'errorAllowed=True (ELMResponseSchemaWithError)' 'errorAllowed=False' 'Testplan' 'openings ist leer.' 'Unbekannter visibility-Wert' 'GetStoryVisibilityOfElements' '261013-jom'; do printf '%s' "$S" | grep -qF -- "$t" || { echo "Doku fehlt: $t"; exit 1; }; done && grep -q 'erledigt in 0.9.24' reference/mcp-extension.md && test "$(grep -c '^## ELM_SAB 0.9.24' reference/mcp-extension.md)" = 1 && grep -q '0.9.24' ELM_SAB_Tapir/README.md && test $(git show --name-only --format= HEAD | wc -l) -eq 2 && git log -1 --format=%B | grep -q 'Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>' && test "$(git rev-list --count origin/main..HEAD -- ELM_SAB_Tapir reference)" = 2</automated>
  </verify>
  <done>Der Abschnitt 0.9.24 ist vorhanden: Anlass, Muster mit oneOf-Bedingung, Tabelle mit Probe-Aufrufen und Codes, Scan vorher/nachher wörtlich, Stand „noch NICHT live verifiziert", Testplan in 4 Schritten. Der 0.9.23-Unterpunkt verweist auf „erledigt in 0.9.24", der README-Absatz ist ergänzt. Lokal liegen 2 Commits, nicht gepusht.</done>
</task>

<task type="checkpoint:human-action" gate="blocking">
  <name>Task 3 (GATE): Freigabe abwarten → Versionscheck, Push, CI-Schleife (max. 4 Fix-Runden), Scan-Nachweis</name>
  <files>ggf. bei Fix-Runden bzw. Versionssprung: die 8 Quelldateien aus Task 1, reference/mcp-extension.md, ELM_SAB_Tapir/README.md</files>
  <action>FREIGABE durch Orchestrator/Nutzer: Die andere Session hat die Live-Gegenprobe des CaptureView-Fehlermusters (0.9.23, Testplan Schritt 9 und/oder 11) abgeschlossen. Das Ergebnis wird mit dem Resume-Signal mitgeteilt. Vorher wird NICHT gepusht.</action>
  <instructions>
VOR DIESEM TASK STOPPEN. Der Executor gibt an den Orchestrator zurück:
- Zwei lokale Commit-SHAs mit Betreff.
- Ausgabe von schema_check_0924.py und syntax_check_0924.sh (jeweils Kurzfassung).
- Die sechs Scan-Zeilen vorher/nachher.
- `git rev-list --count origin/main..HEAD -- ELM_SAB_Tapir reference` = 2.
- Befund aus Task 1 Schritt 4 (origin/main noch 92ab02a? 0.9.24 frei?).
- Abweichungen.
Zusätzlich schreibt er Q/261013-jom-SUMMARY.md mit `status: gated` (Commits, Prüfergebnisse, offene Schritte = Task 3). Nicht committen.

Nach dem Resume-Signal führt der Fortsetzungs-Executor Folgendes aus (Q wie in Task 1):

A) Signal auswerten.
   - „stopp: …" (z. B. CaptureView zeigt trotz oneOf weiter 4009 oder liefert eine andere Fehlerform): NICHTS pushen. Die Commits bleiben lokal. SUMMARY auf `status: blocked` mit Grund setzen und berichten. Ein Umbau des Musters (z. B. anyOf statt oneOf) ist dann ein neuer Auftrag.
   - „push frei": weiter mit B.

B) Versions- und Basischeck. `git fetch -q origin main ci-status`.
   - Gilt `git rev-parse origin/main` = 92ab02a…: direkt zu C.
   - Sonst:
     - `git log --oneline 92ab02a..origin/main` und `git diff --stat 92ab02a origin/main` notieren.
     - `git rebase origin/main` ausführen (nicht interaktiv).
     - Konflikte nur so lösen:
       - AddOnVersion.hpp: die höhere Version übernehmen, dann wie unten umnummerieren.
       - reference/mcp-extension.md: beide Seiten behalten. Deren Änderungen stehen zuerst, unser 0.9.24-Abschnitt bleibt am Dateiende, die „erledigt"-Markierung wird in deren Fassung der Zeile übernommen.
       - AddOnMain.cpp: beide Zeilenänderungen behalten.
     - Konflikt in einer der sechs Befehlsdateien oder in ELMCommandBase.hpp → `git rebase --abort`, nicht pushen, berichten.
   - Ist auf origin/main `ELM_SAB_VERSION` ≥ "0.9.24", gilt die nächste freie Patch-Version V (z. B. 0.9.25). Dann umnummerieren:
     - AddOnVersion.hpp, die sechs AddOnMain-Suffixe („ 0.9.24:" → „ V:"), die Überschrift und Stand-Zeile des Doku-Abschnitts und den README-Absatz auf V setzen.
     - EIN Commit: `fix(ELM_SAB): V – Versionsnummer (0.9.24 auf origin/main bereits vergeben)` mit Co-Authored-By-Zeile.
     - Den Nutzer in der Abschlussmeldung ausdrücklich informieren.
   - Lokale Gates erneut laufen lassen:
     - `python3 -I Q/schema_check_0924.py . origin/main <V oder 0.9.24>` (prüft unsere Änderungen gegen die neue Basis).
     - `bash Q/syntax_check_0924.sh`.
     - Scan.

C) Push: `git push origin main`. Danach `S=$(git rev-parse --short=7 HEAD)`.

D) CI abwarten, NUR im Hintergrund (Bash mit run_in_background, kein Vordergrund-sleep):
   - Schleife: `for i in $(seq 1 120); do git fetch -q origin ci-status && git show origin/ci-status:status.md | grep -q "$S" && break; sleep 60; done; git show origin/ci-status:status.md`.
   - Grün heißt:
     - status.md nennt `$S`.
     - `git show origin/ci-status:status.md | grep -oE 'ELM_SAB_Tapir_AC2[789]_(Mac\.zip|Win\.apx)' | sort -u | wc -l` = 6.
     - `git show origin/ci-status:status.md | grep -cF '**fehlgeschlagen**'` = 0. Nie auf das bloße Wort prüfen.
   - Rot:
     - `git show origin/ci-status:logs/build-<mac|win>-AC<NN>.log | grep -nE 'error|Error' | head -60` für jede fehlgeschlagene Kombi.
     - Gezielt fixen. Typisch: MSVC C4100/C4244/C4456/C4702, `-Werror`, ein Ausdruck im AC27/28-Zweig von CreateWallOpenings.
     - Commit `fix(ELM_SAB): 0.9.24 Fehlerantwort – <Ursache>` mit Co-Authored-By-Zeile, pushen, erneut warten.
   - Max. 4 Fix-Runden. Danach stoppen. In reference/mcp-extension.md die Stand-Zeile des 0.9.24-Abschnitts auf „CI rot nach 4 Fix-Runden — NICHT einsetzen (siehe SUMMARY 261013-jom)" setzen, als `docs(ELM_SAB): …` committen und pushen. Das ist nur Doku und löst keine CI aus. Ursache und Log-Auszüge kommen in die SUMMARY.
   - Läuft die Schleife ohne Treffer ab (120 min): `gh run list --workflow build-elm-sab-tapir.yml -L 3` (falls angemeldet) ansehen, berichten, nicht blind neu pushen.

E) Scan-Nachweis nach grünem CI:
   - `python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources`: die sechs Zeilen zeigen `errorAllowed=True (ELMResponseSchemaWithError)`, 0 Kandidaten, die übrigen Zeilen gleich Q/scan_vorher.txt.
   - Ergebnis, grünen CI-SHA, Lauf-Nummer (aus dem ci-status-Commit-Betreff „Build-Status <sha> (Lauf N)") und Fix-Runden in die SUMMARY schreiben und `status: complete` setzen. Kein weiterer Doku-Commit nur für den SHA.

KEIN Bundle-Download und kein Bundle-Tausch, Archicad nicht ansprechen. SUMMARY nicht committen.
  </instructions>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && git fetch -q origin main ci-status && git merge-base --is-ancestor HEAD origin/main && S=$(git rev-parse --short=7 HEAD) && git show origin/ci-status:status.md | grep -q "$S" && test "$(git show origin/ci-status:status.md | grep -oE 'ELM_SAB_Tapir_AC2[789]_(Mac\.zip|Win\.apx)' | sort -u | wc -l | tr -d ' ')" = 6 && test "$(git show origin/ci-status:status.md | grep -cF '**fehlgeschlagen**')" = 0 && test "$(python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources | grep -E '^(CreateWallOpenings|SetLayerOfElements|SetObjectParametersForce|SetPenOfElements|SetStoryVisibilityOfElements|SetTextSize)Command ' | grep -c 'errorAllowed=True (ELMResponseSchemaWithError)')" = 6 && test "$(git log origin/main --format=%B -n 3 | grep -c 'Co-Authored-By: Claude Opus 5.5')" -ge 2</automated>
  </verify>
  <done>Push erst nach Freigabe „push frei". Die Version ist gegen origin/main geprüft; bei Vergabe gilt die nächste freie Version, und der Nutzer ist informiert. ci-status nennt den gepushten HEAD-SHA mit allen 6 Bundles und ohne `**fehlgeschlagen**`, oder der Abbruch nach 4 Fix-Runden ist mit Doku-Warnung dokumentiert. Der Scan nach grünem CI ist belegt, die SUMMARY steht auf complete (bzw. blocked mit Grund). Kein Bundle installiert.</done>
  <resume-signal>„push frei" (CaptureView-Fehler kam live lesbar an, kein 4009) oder „stopp: &lt;Grund&gt;"</resume-signal>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| MCP/Claude → ELM_SAB JSON-Befehl | lokale, vom Nutzer gestartete Anfrage. Parameter (Ebenenname, visibility-Preset) können in Fehlermeldungen zurückgespiegelt werden |
| Archicad-Antwortvalidierung → Client | Archicad prüft jede Antwort gegen GetResponseSchema. Eine nicht passende Antwort wird durch 4009 ersetzt |
| lokaler Stand → origin/main → CI-Bundles | Ein Push erzeugt Bundles, die der Nutzer später einsetzt |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-261013-jom-01 | Repudiation | Top-Level-Fehlerpfade der sechs Befehle | mitigate | oneOf-Fehlerzweig per ELMResponseSchemaWithError, CreateErrorResponseELM mit echtem Code und Text. Fehlschläge werden nicht mehr als anonymes 4009 verschluckt (schema_check_0924.py + Scan) |
| T-261013-jom-02 | Tampering | Modell (Schreibbefehle) | mitigate | Nur Rückgabeform und Schema ändern sich. Undo-, Teamwork- und Änderungslogik bleibt. Prüfung: schema_check (Execute-Zählung, Erfolgsschema byte-gleich, AddOnMain nur 6 Zeilen) + Diff-Review. Die Proben im Testplan nutzen leere Elementlisten auf Fehlerpfaden vor jeder Änderung |
| T-261013-jom-03 | Tampering | Bundle-Kette (Push ohne bestätigtes Muster) | mitigate | blocking Gate (checkpoint:human-action) vor dem Push, das bei auto_advance NICHT automatisch freigegeben wird. Bei „stopp" bleiben die Commits lokal. CI rot nach 4 Runden → Doku-Warnung „NICHT einsetzen" |
| T-261013-jom-04 | Tampering | Versionskollision mit paralleler Session | mitigate | Vor dem Push fetch + Versionscheck auf origin/main, Rebase mit eng begrenzten Konfliktregeln, Abbruch bei Konflikt in Befehlsdateien, nächste freie Version + Nutzerhinweis |
| T-261013-jom-05 | Information Disclosure | Fehlermeldungen spiegeln Eingaben (Ebenenname, Preset) | accept | nur an den lokalen Aufrufer, der die Werte selbst gesendet hat. Unverändert gegenüber bisher (nur jetzt lesbar statt 4009) |
</threat_model>

<verification>
- Task-Verifies 1–3 sind grün.
- `git diff 92ab02a..origin/main --stat` (nach Task 3) zeigt genau die 10 Dateien aus files_modified. Bei Rebase zählen zusätzlich die Fremd-Commits der anderen Session, separat ausweisen. Keine `.planning`-Dateien, CMakeLists unverändert.
- Keine neuen Schreib-APIs: `git diff 92ab02a HEAD -- ELM_SAB_Tapir/Sources | grep '^+' | grep -E 'ACAPI_Element_(Change|Create|Delete)|ACAPI_CallUndoableCommand|ACAPI_Teamwork_'` liefert nichts.
</verification>

<success_criteria>
- Die sechs Befehle melden Top-Level-Fehler schemakonform. Der Client bekommt `error.code` und `error.message` statt 4009, mit unverändertem Text und Code.
- Erfolgsschemas, Erfolgsantworten, Fehler je Element und GetStoryVisibilityOfElements sind unverändert.
- ELM_SAB_VERSION 0.9.24 (oder die nächste freie Version, Nutzer informiert), Registrierungsversionen unverändert, Beschreibungen ergänzt.
- Doku-Abschnitt 0.9.24 mit Testplan, Verweis im 0.9.23-Unterpunkt, README-Absatz.
- Push erst nach Gate-Freigabe. CI 6/6 grün für den gepushten HEAD (oder dokumentierter Abbruch). Scan nach CI: 6 × errorAllowed=True, 0 Kandidaten.
</success_criteria>

## Quellen-Abdeckung (Audit)

| Quelle | Punkt | Task |
|---|---|---|
| Auftrag 1 | sechs GetResponseSchema → ELMResponseSchemaWithError (Erfolgsschema unverändert) | 1 (Schritt 3a, schema_check) |
| Auftrag 2 | alle Top-Level-Fehler → CreateErrorResponseELM, jede Fundstelle im Execute prüfen, Codes/Texte unverändert, Fehler je Element bleiben | 1 (Schritt 3b/3c, Tabelle mit 9 Fundstellen) |
| Auftrag 3 | Erfolgsantworten/-schemas byte-gleich; required nicht leer und ohne error als Prüfschritt | 1 (schema_check: Byte-Vergleich + JSON-Prüfung) |
| Auftrag 4 | ELM_SAB_VERSION 0.9.24, vor Push gegen origin/main prüfen, sonst nächste freie + Nutzer informieren; Registrierungsversionen bleiben; Beschreibung „0.9.24: …" | 1 (Schritt 4/5), 3 (B) |
| Auftrag 5 | Doku-Abschnitt 0.9.24 (Liste, Muster, Scan vorher/nachher, Stand, Testplan mit provozierbarem Fehler je Befehl); 0.9.23-Verweis „erledigt in 0.9.24"; README | 2 |
| Randbedingung | C++17, ASCII-Literale, kein lokaler Build, clang -fsyntax-only gegen DevKit 29.3100 mit Mac-Warnflags | 1 (syntax_check, Diff-Grep, Nicht-ASCII-Zählung) |
| Randbedingung | Archicad nicht ansprechen, kein Bundle-Tausch, nichts außerhalb Repo/Scratchpad | 1–3 |
| Randbedingung | Commit-Präfixe fix/docs(ELM_SAB): 0.9.24 …, Co-Authored-By | 1, 2, 3 |
| Randbedingung | CI über ci-status, gebauter SHA im Kopf, 6 Kombis, Marker `**fehlgeschlagen**` (fett), max. 4 Fix-Runden | 3 (D) |
| Randbedingung | Nach grünem CI Scan: sechs errorAllowed=True, übrige unverändert | 1 (lokal vorab), 3 (E) |
| Gate (Orchestrator) | Push erst nach Live-Gegenprobe CaptureView; eigener letzter Task als Gate, Executor stoppt davor | 3 (checkpoint:human-action, blockiert auch bei auto_advance) |
| Vorgänger-Lehre | Falle „fehlgeschlagen" in status.md, UniString `!=` nur C++20 | 3 (D), 1 (Diff-Grep) |

<output>
- Am Gate (vor Task 3): `.planning/quick/261013-jom-elm-sab-0-9-24-fehlerantwort-schema-fuer/261013-jom-SUMMARY.md` mit `status: gated` anlegen (Commits, Prüfergebnisse, offene Schritte).
- Nach Task 3: die SUMMARY auf `complete` bzw. `blocked` fortschreiben (CI-Lauf/SHA, Fix-Runden, Versionsentscheidung, Scan-Nachweis, Abweichungen).
- NICHT committen, das macht der Orchestrator zusammen mit Plan, Prüfskripten und Scan-Dateien.
</output>
