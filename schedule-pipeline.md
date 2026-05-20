# Schedule-Export-Pipeline

Pipeline-Pattern für Bulk-Updates, wenn der MCP-Server die Quell-Daten **nicht direkt** liefern kann — z. B. GDL-Stempel-Werte, komplexe Composite-Bezeichnungen, Schedule-spezifische Aggregat-Werte. [SKILL.md](../SKILL.md) verweist hierher bei großen Datenpflege-Aufgaben.

## Inhaltsverzeichnis

1. [Wann diese Pipeline verwenden](#wann-diese-pipeline-verwenden)
2. [Die 6 Schritte](#die-6-schritte)
3. [Schritt 1 — Schedule-Export](#schritt-1--schedule-export)
4. [Schritt 2 — Parse](#schritt-2--parse)
5. [Schritt 3 — Identifier-Mapping (GUID-Match)](#schritt-3--identifier-mapping-guid-match)
6. [Schritt 4 — Daten-Qualitäts-Check](#schritt-4--daten-qualitäts-check)
7. [Schritt 5 — Confirm + Apply](#schritt-5--confirm--apply)
8. [Schritt 6 — Verifikation + Report](#schritt-6--verifikation--report)
9. [Gotchas](#gotchas)

## Wann diese Pipeline verwenden

- Bulk-Property-Update für > 20 Elemente, wo die Ziel-Werte **nicht** direkt via MCP lesbar sind.
- Beispiele: GDL-Parameter-Werte ins Property-System synchronisieren; externe Daten-Quellen (Capmo, Bautagebuch, Excel-Listen) ins Modell einspielen.
- Wenn der reine [`bulk-operations.md`](bulk-operations.md) Read → Filter → Group → Confirm → Apply nicht reicht, weil READ den entscheidenden Wert nicht findet.

**Alternative prüfen zuerst:** Wenn der GDL-Wert via [Property-Expression-Linking](property-expression-linking.md) automatisch ans Property gebunden werden kann, ist das eleganter als diese Pipeline. Erst dann diese Pipeline, wenn Linking nicht passt oder nicht gewünscht.

## Die 6 Schritte

```
1. EXPORT   — User exportiert Schedule aus Archicad als XLSX/CSV
2. PARSE    — Wir parsen die Datei (openpyxl / csv) in Python-Dicts
3. MAP      — Wir mappen Schedule-Identifier (Raumnummer, Element-ID) auf MCP-GUIDs
4. CHECK    — Daten-Qualität: Tippfehler, Enum-Mismatch, fehlende Werte
5. CONFIRM  — Diff-Summary an User; Bulk-Update nur nach `ja`
6. VERIFY   — Stichproben-Read nach Update, Erfolgs/Fehler-Report
```

## Schritt 1 — Schedule-Export

**Wir können den Export nicht selbst auslösen** (keine MCP-API für „Schedule exportieren"). User-Schritt:

1. In Archicad: Navigator → Schedules → die relevante Schedule wählen.
2. File → Save as → XLSX oder CSV. Empfohlen: XLSX wenn Encoding kritisch (deutsche Umlaute), sonst CSV (einfacher zu parsen).
3. Speicherort: möglichst projekt-nah, damit der Pfad kurz und im Bash-Befehl leicht referenzierbar ist.

**Anforderungen an die Schedule:**
- Eine Spalte muss eine **eindeutige Identifier** enthalten — Raumnummer, Element-ID, oder ein anderer von Archicad gepflegter Schlüssel.
- Die Source-Spalten (z. B. GDL-Stempel-Wert „Bodenbelag") und die Target-Property-Werte müssen drin sein.

## Schritt 2 — Parse

Pro Format:

**CSV:**
```bash
# Quick-Look
head -5 "<pfad>.csv" | column -t -s ','
```

In Python (für Claude):
```python
import csv
with open("<pfad>.csv", newline='', encoding='utf-8') as f:
    rows = list(csv.DictReader(f, delimiter=';'))  # oder ',' je nach Locale
```

**XLSX:**
```python
from openpyxl import load_workbook
wb = load_workbook("<pfad>.xlsx", data_only=True)  # data_only=True → Formel-Ergebnisse statt Formeln
ws = wb.active
rows = [{cell.value for cell in row} for row in ws.iter_rows(values_only=True)]
# Oder strukturierter mit Header-Zeile als Schlüssel
```

**Resultat:** Liste von Dicts mit den Schedule-Spalten als Keys.

## Schritt 3 — Identifier-Mapping (GUID-Match)

Die Schedule hat Raumnummern (z. B. „1.05") — der MCP arbeitet mit GUIDs. Wir mappen.

### Methode A — über Property-Listing

Wenn die Raumnummer in einer Standard-Property gespeichert ist:

1. Alle Zonen via `mcp__archicad__elements_get_elements_by_type` mit `elementType: "Zone"` listen.
2. Für jede Zone via `mcp__archicad__properties_get_all_property_ids_of_elements` + `properties_get_property_values_of_elements` die Raumnummer-Property auslesen.
3. Dict bauen: `{raumnummer: zone_guid}`.

### Methode B — über Element-Type-Detail

Bei Zonen ist `numberStr` Teil der Zone-Datum-Struktur. Aber wegen `get_details_of_elements`-Bug nicht direkt lesbar (siehe [mcp-conventions.md § Fehlerklassen](mcp-conventions.md#fehlerklassen-und-reaktionen)) — Workaround: Property-System.

### Methode C — bei eindeutigem Schedule-Index

Falls die Schedule eine Element-ID-Spalte ohne GUID-Format führt, brauchen wir noch einen Discovery-Schritt: `mcp__archicad__properties_get_property_values_of_elements` mit der Property „Element-ID" (Built-in) und vergleichen.

**Konsistenz-Check:** Jede Schedule-Zeile sollte genau einen GUID-Match haben. Wenn mehrere oder keine → User informieren, **nicht raten**.

## Schritt 4 — Daten-Qualitäts-Check

Vor dem Bulk-Update prüfen wir die Daten. Sehr wichtig bei Enum-Properties.

### Property-Enum-Normalisierung

Wenn die Ziel-Property ein Single-Enum ist, müssen die Werte aus der Schedule **exakt** in der Enum-Definition vorkommen. Typische Probleme:

- **Tippfehler:** „Linolium" statt „Linoleum", „Fliese" Singular statt „Fliesen" Plural.
- **Trailing Spaces:** „Linoleum " (mit Leerzeichen am Ende) vs. „Linoleum" — zwei verschiedene Enum-IDs.
- **Trennzeichen-Inkonsistenz:** „Fliesen (R11+R12)" vs. „Fliesen (R11/R12)" — Plus vs. Slash.
- **Klammer-Inkonsistenz:** „Fliesen (R10)" vs. „Fliese R10" — mit/ohne Klammern.
- **Fehlende Enum-Werte:** Schedule enthält Werte, die noch nicht in der Property-Definition als Enum-Option angelegt sind.

**Pre-Flight-Workflow:**

1. Property-Definition lesen: alle existierenden Enum-Werte via `mcp__archicad__properties_get_property_values_of_elements` an einem Beispiel-Element (oder via dediziertem Property-Definition-Call wenn vorhanden).
2. Schedule-Werte gegen Enum-Definition diffen: `not_in_enum = [v for v in schedule_values if v not in enum_values]`.
3. Wenn `not_in_enum` nicht leer ist: **STOPPEN.** User-Report mit:
   - Welche Werte fehlen in der Enum-Definition.
   - Welche existierenden Enum-Werte könnten gemeint sein (Levenshtein-Distance oder Substring-Match).
   - Vorschlag: User ergänzt die Property-Definition in Archicad (Property-Manager), dann Re-Run.

**WICHTIG:** Niemals einen Bulk-Update mit unbekannten Enum-Werten starten — Archicad würde sie entweder ablehnen (Fehler) oder still ignorieren (Daten-Inkonsistenz).

## Schritt 5 — Confirm + Apply

Diff-Summary an User. Format folgt [mcp-conventions.md § Confirm-Format](mcp-conventions.md#confirm-format-für-schreibende-aufrufe):

```
Bulk-Update Bodenbelag aus Schedule (66 Räume):
- 31 Räume bekommen neuen Wert (Diff zur aktuellen Property).
- 28 Räume bereits korrekt (kein Update nötig).
-  5 Räume mit unklarem GDL-Wert (separate Liste).
-  2 Räume in Schedule, aber keine GUID gefunden (warnen).

Ausführen für die 31 Diff-Räume? (ja / details / abbrechen)
```

`details` zeigt die vollständige Liste mit Zone-GUID, Raumnummer, alter Wert → neuer Wert.

Bei `ja`: pro Element `mcp__archicad__properties_set_property_values_of_elements` mit der Property-ID + neuem Enum-Value-ID.

**Mid-Batch-Fehler:** Wie in [bulk-operations.md § Mid-Batch-Fehlerverhalten](bulk-operations.md#mid-batch-fehlerverhalten) — bei systemischem Fehler stoppen, bei einzelnen weitermachen, am Ende Report.

## Schritt 6 — Verifikation + Report

Nach dem Update:

1. Stichproben-Read (3-5 zufällige aus den geupdate'ten Elementen): aktueller Property-Wert == erwarteter Wert?
2. Wenn ja: Erfolgs-Report mit Zähler.
3. Wenn nicht: stoppen, ungeklärten Mismatch dem User berichten — nicht still „erneut versuchen".

**Report-Format:**
```
Bulk-Update Bodenbelag fertig:
- 31 von 31 Elementen erfolgreich geupdate'd.
- Stichprobe (Zone 1.05): erwartet „Fliesen (R10)", real „Fliesen (R10)" ✓
- Stichprobe (Zone 2.03): erwartet „Linoleum 2,5 (R9)", real „Linoleum 2,5 (R9)" ✓
[...]
```

## Gotchas

- **Modal-Dialog blockiert MCP** — Property-Manager im Bearbeitungs-Modus blockiert alle MCP-Calls. User vor Pipeline-Start erinnern. Siehe [mcp-conventions.md § Modal-Dialoge](mcp-conventions.md#modal-dialoge-in-archicad-blockieren-mcp).
- **Schedule-Daten sind ein Snapshot.** Wenn der User zwischen Export und Apply weiter im Modell arbeitet, kann die Schedule veraltet sein. Empfehlung: Pipeline in einer Sitzung durchziehen, oder explizit re-exportieren.
- **Enum-IDs vs. Display-Werte.** Properties haben Enum-IDs (GUIDs) und Display-Werte (Strings). Beim Set immer die GUID übergeben, nicht den String. Mapping via `properties_get_all_property_names` oder Property-Definition-Read.
- **`get_details_of_elements`-Bug in AC29** macht direkten Schedule-ähnlichen Read im MCP unzuverlässig — deshalb diese Pipeline überhaupt.
- **Encoding-Fallen** bei CSV: deutsche Umlaute werden je nach Archicad-Export-Variante als UTF-8, Windows-1252 oder Latin-1 gespeichert. `chardet`-Detect vor Parse hilft.
- **Schedule-Spalten-Reihenfolge** kann sich zwischen Archicad-Versionen ändern. Spalten **immer per Name addressieren** (DictReader), nicht per Index.
