---
quick_id: 261013-fel
status: complete
commit: 4e804b3
ci_sha: 4e804b3
ci_run: 38381229921
fix_rounds: 0
started: 2026-10-13T09:34:15Z
completed: 2026-10-13T10:12:25Z
duration_min: 38
key-files:
  created:
    - ELM_SAB_Tapir/Sources/FindElementsInternal.hpp
    - ELM_SAB_Tapir/Sources/FindElementsCommand.hpp
    - ELM_SAB_Tapir/Sources/FindElementsCommand.cpp
    - ELM_SAB_Tapir/Sources/FindElementsLeaves.cpp
  modified:
    - ELM_SAB_Tapir/Sources/ELMCommandBase.hpp
    - ELM_SAB_Tapir/Sources/IFCCommands.cpp
    - ELM_SAB_Tapir/Sources/IFCCommands.hpp
    - ELM_SAB_Tapir/Sources/AddOnMain.cpp
    - ELM_SAB_Tapir/Sources/AddOnVersion.hpp
    - ELM_SAB_Tapir/Sources/CaptureViewCommand.hpp
    - reference/mcp-extension.md
    - ELM_SAB_Tapir/README.md
---

# Quick 261013-fel: ELM_SAB 0.9.23 — FindElements (und/oder/nicht) + CaptureView-Fehlerantwort

ELM_SAB 0.9.23 hat zwei Teile.

1. **`FindElements` (neu, nur lesen):** ein `where`-Baum mit `and`/`or`/`not` in beliebiger Tiefe und allen 18 Blättern aus dem Handoff.
   - Ausgewertet wird immer die Grundriss-Datenbank über `ExecuteActionForEachDatabase`. Optional geht es über `databases`.
   - Weitere Funktionen: `scope`, `return`-Spalten, `countOnly`/`limit`, `criteriaStats` je Knoten und `select` mit Rücklese.
2. **`CaptureView`:** Fehler kommen jetzt schemakonform an (`oneOf` Erfolg|Fehler). Jede Meldung nennt den Fenstertyp und den echten Archicad-Code. Layout-Fenster werden versucht und sonst mit einem Hinweis abgelehnt.

CI: alle 6 Builds grün beim ersten Push, keine Fix-Runde nötig.

## Commits (alle auf origin/main)

| Task | SHA | Nachricht |
|---|---|---|
| 1 | `4eee7bf` | feat(ELM_SAB): 0.9.23 FindElements – Auswertung, Suchbereich, guenstige Kriterien (6 Dateien) |
| 2 | `a452082` | feat(ELM_SAB): 0.9.23 FindElements – Eigenschafts-, Wert- und Beziehungskriterien (3 Dateien) |
| 3 | `217d2ad` | fix(ELM_SAB): 0.9.23 CaptureView – Fehlerantwort schemakonform, Fenstertyp und Archicad-Code (2 Dateien) |
| 4 | `4e804b3` | docs(ELM_SAB): 0.9.23 FindElements + CaptureView-Fix – Referenz, Fallen, Testplan (2 Dateien) |

`git diff fad4abb..origin/main --stat` zeigt genau die 12 Dateien aus `files_modified`. Es sind keine `.planning`-Dateien dabei, CMakeLists ist unverändert.

## CI

- Grüner SHA **`4e804b3`**, Lauf 38381229921, ci-status-Commit `e2e7d1a` (13.10. 10:11 UTC).
- `status.md`:
  ```
  Gebaut aus `4e804b3` — Basis Tapir 1.5.4 plus die SAB-eigenen Befehle.
  | AC29 | `ELM_SAB_Tapir_AC29_Mac.zip` | `ELM_SAB_Tapir_AC29_Win.apx` |
  | AC28 | `ELM_SAB_Tapir_AC28_Mac.zip` | `ELM_SAB_Tapir_AC28_Win.apx` |
  | AC27 | `ELM_SAB_Tapir_AC27_Mac.zip` | `ELM_SAB_Tapir_AC27_Win.apx` |
  ```
  Keine einzige `**fehlgeschlagen**`-Markierung.
- Logs:
  - Mac AC27/28/29: `** BUILD SUCCEEDED **`.
  - Win AC27/28/29: `ELM_SAB_AC2x_Win.apx` erzeugt, 0 Fehler- und 0 Warnzeilen.
- Alle 6 Bundles liegen unter `bundles/` auf ci-status. Die Bundles sind **nicht** heruntergeladen oder installiert. Archicad wurde nicht angesprochen.
- Fix-Runden: 0.

## Schema-Scan (`python3 -I ~/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources`)

- `CaptureViewCommand`: errorAllowed=True (ELMResponseSchemaWithError), topLevelErrorReturns=5.
- `FindElementsCommand`: errorAllowed=True (ELMResponseSchemaWithError), topLevelErrorReturns=16.
- Weitere Kandidaten (errorAllowed=False und topLevelErrorReturns>0). Sie sind nicht gefixt, sondern als offener Punkt in der Doku vermerkt:

| Befehl | Fundstelle |
|---|---|
| CreateWallOpenings | CreateWallOpeningsCommand.cpp Z. 584/587 |
| SetLayerOfElements | SetLayerCommand.cpp Z. 102/105 |
| SetObjectParametersForce | Z. 326 |
| SetPenOfElements | Z. 244 |
| SetStoryVisibilityOfElements | StoryVisibilityCommands.cpp Z. 132/141 |
| SetTextSize | Z. 64 |

Das entspricht genau der Erwartung des Plans.

## Abweichungen vom Plan

1. **[Rule 3] `ACAPI_Grouping_GetConnectedElements` nimmt EINEN Typ (`const API_ElemType&`), kein OR von Typ-IDs.** `hosts` ruft die Funktion deshalb je Typ einzeln auf (Door, Window, Skylight, Opening).
2. **[Rule 1] `ifc`: nur `APIERR_MODULNOTINSTALLED` bricht ab (fatalError).**
   - Plan: „Accessor-Fehler beim ersten Element → fatal". Das hätte 2D-Elemente ohne IFC-Daten fälschlich zum Abbruch gemacht, wenn sie zuerst kommen.
   - Jetzt: andere Accessor-Fehler zählen als `notAvailable`, dazu ein Eintrag in `warnings` mit Anzahl und erstem Fehlercode.
3. **`hosts` wertet nur Wirt-Typen aus** (Wall, Slab, Roof, Shell, Beam, Column). Andere Typen zählen als `notAvailable`.
4. **`databases`:** die Elemente jeder Datenbank werden so ausgewertet, wie `GetElemList` sie liefert, ohne Umsetzung SectElem→Modellelement. Das steht so in der Doku.
5. **Vertrag:** `FindElementsInternal.hpp` hat zusätzlich gemeinsame Lese-Helfer (`IntToUni`, `NumToUni`, `TypeNameOf`, `ReadNumber`, `ReadNumberList`, `ReadStringOrList`) und in `EvalContext` die Ausgabe-Caches `storyNames`/`layerNames`. Alle Namen und Signaturen des Vertrags sind unverändert.
6. **Design-Optionen:** Namespace-Alias `ELMDesignOptions = ACAPI::DesignOptions` statt `using namespace`. Damit kollidieren die DesignOption-Namen nicht mit den FindElements-Namen.
7. **Portabilität:**
   - Vergleiche von UniStrings stehen als `!(a == b)` statt `a != b`. In DevKit 29 gibt es `!=` nur synthetisch über C++20.
   - Zahl↔Text-Umwandlung im Wert-Vergleich ist locale-unabhängig (`std::locale::classic`, Dezimalkomma wird akzeptiert).
   - Alle String-Literale sind ASCII. Die Unicode-Operatoren ≠ ≤ ≥ stehen als UTF-8-Escapes im Code.
8. **Lokale Syntax-Vorprüfung (kein Build):** vor dem Push lief `clang++ -fsyntax-only` gegen das lokale DevKit 29.3100 mit den CI-Mac-Warnflags plus `-Wshadow-all -Wconversion` als Näherung für MSVC /W4. Das prüft nur den AC29-Zweig. Es wurde nichts außerhalb von Repo und Scratchpad geschrieben.
9. **Falsch-negativer Verify in Task 4:** der Plan-Check `! grep -q 'fehlgeschlagen' status.md` greift immer, weil die feste Zeile „Zu jeder fehlgeschlagenen Version liegt …" jeder status.md diesen Text enthält. Geprüft wurde deshalb auf den echten Marker `**fehlgeschlagen**`: 0 Treffer. Alle übrigen Task-4-Prüfungen sind grün (SHA in status.md, Co-Authored-By in den 4 Commits, HEAD ist Vorfahr von origin/main).

## Offene Annahmen für die Live-Gegenprobe (Testplan steht in reference/mcp-extension.md)

1. **Scope-Zahlen:** liefert `GetElemList(API_ZombieElemID, flags)` wie erwartet 19.873 / 19.503 / 10.210 / 6.547? Und bleibt mit Layout vorn die Zahl für `projekt` gleich und `startDatabaseType` steht auf „Layout"?
2. **„Umbau-Status":** kommt er per `GetPropertyValuesByGuid` als Einfach-Aufzählung mit `keyVariant.guidValue`, sodass `"ToBeDemolished"` und `"Abbruch"` dieselbe Menge liefern? Sonst greift der Fallback über `p.definition.possibleEnumValues`.
3. **`select` mit Layout vorn:** die Auswahl wird in der Grundriss-Datenbank gesetzt und zurückgelesen. Offen ist, ob fremde Geschosse ausgewählt werden; `notSelected` und `extraSelectedCount` ansehen. Danach die Auswahl aufheben.
4. **Laufzeit (`elapsedMs`):** wie lange dauert `property` ohne vorgeschaltetes `type`, also bei rund 20k Elementen? Das gibt Hinweise für die Doku.
5. **Nicht live geprüfte Blätter** (hier nur die Plausibilität ansehen):
   - `hostedBy` für Öffnungen (die Rückwärtskarte wird beim ersten Bedarf aufgebaut)
   - `inZone` (Raumbeziehungen)
   - `ifc`-Attribute in AC27/28
   - Property-Winkel (in API-Einheit)
6. **CaptureView mit Layout:** kommt jetzt eine lesbare Fehlermeldung mit Fenstertyp und Code statt 4009? Falls Archicad das Layout-PNG doch schreibt, als „Layout geht" dokumentieren.

## Self-Check: PASSED

- 4 neue Dateien sind vorhanden; die Commits 4eee7bf, a452082, 217d2ad und 4e804b3 liegen auf origin/main.
- ci-status nennt 4e804b3, alle 6 Bundles sind vorhanden, 0 Markierungen `**fehlgeschlagen**`.
- Nur Lese-APIs: kein `ACAPI_Element_(Change|Create|Delete)` und kein `ACAPI_CallUndoableCommand` in FindElements und CaptureView.
