# Bulk-Operations — Archicad-Skill

Diese Datei dokumentiert, wie wir Massen-Updates und Klassifizierungen durchführen. [SKILL.md](../SKILL.md) verweist hierher bei Operationen, die mehr als ein paar Elemente betreffen — also bei den meisten echten Architekten-Aufträgen.

## Inhaltsverzeichnis

1. [Das universelle Read → Filter → Group → Confirm → Apply-Pattern](#das-universelle-read--filter--group--confirm--apply-pattern)
2. [Klassifikations-Spezifika](#klassifikations-spezifika)
3. [Pro-Element-Typ-Ableitungslogik](#pro-element-typ-ableitungslogik)
4. [Mehrdeutigkeits-Handling](#mehrdeutigkeits-handling)
5. [Mid-Batch-Fehlerverhalten](#mid-batch-fehlerverhalten)
6. [Property-Set live verifiziert (CAP-01)](#property-set-live-verifiziert-cap-01)
7. [Property-Definitions sind Create-Only (CAP-02 Negativ-Befund)](#property-definitions-sind-create-only-cap-02-negativ-befund)
8. [Schema-Bug: `defaultValue` ist required (nicht optional)](#schema-bug-defaultvalue-ist-required-nicht-optional)
9. [Pre-Flight: Property-Enum-Normalisierung](#pre-flight-property-enum-normalisierung)
10. [Identifier-Mapping bei externen Daten-Quellen](#identifier-mapping-bei-externen-daten-quellen)
11. [TODO — Phase 5 Live-Verifikation](#todo--phase-5-live-verifikation)

## Das universelle Read → Filter → Group → Confirm → Apply-Pattern

Bei jeder Operation, die mehr als eine Handvoll Elemente anfasst, folgen wir diesem Ablauf. Jeder Schritt hat seinen Zweck — und das Auslassen eines Schrittes ist meist ein Fehler.

### 1. READ — Alle Elemente vom Zieltyp laden

Wir holen die vollständige Liste der relevanten Elemente. Wenn der MCP-Server paginiert, ziehen wir alle Seiten — siehe [mcp-conventions.md § Paginierung](mcp-conventions.md#paginierung). Niemals auf einer partiellen Seite weitermachen, sonst wird die Bulk-Operation still inkorrekt.

### 2. FILTER — Auf relevante Teilmenge eingrenzen

Aus der Gesamtmenge nehmen wir nur die, die der Auftrag wirklich meint. Filter-Kriterien sind typischerweise:

- **Story** — „nur Elemente im 1. OG".
- **Layer** — „nur Layer ‚Wände-Bestand'".
- **Geometrie** — „nur Wände länger als 3 m" oder „nur Stützen mit quadratischem Querschnitt".
- **Property / Klassifikation** — „nur Wände ohne zugewiesene Klassifikation".

Wir filtern lokal nach dem READ; nicht über mehrere Discovery-Calls verteilt, weil das das Problem in Partial-Page-Fallen verlagert.

### 3. GROUP — Pro Element die Ziel-Klasse oder Property ableiten

Das ist der kreative Schritt. Pro Element entscheiden wir, was die richtige Klassifikation oder das richtige Property-Set ist. Quellen-Kombinationen sind oben in Sektion 3 (Pro-Element-Typ-Ableitungslogik) aufgeführt.

Das Resultat ist ein In-Memory-Mapping: `{elementId → zielKlasse}` oder `{elementId → zielProperty}`.

### 4. CONFIRM — Asymmetrische Sicherheit greift

Updates und Property-Changes sind nicht frei (siehe SAFE-01 in SKILL.md). Wir zeigen dem User vor APPLY eine Summary:

- Anzahl pro Ziel-Klasse / Ziel-Property.
- Mehrdeutige Fälle separat aufgeführt — der User entscheidet pro Fall (siehe Mehrdeutigkeits-Handling).
- `details`-Option für die vollständige ID-Liste, falls der User sie sehen will.

Erst nach `ja` geht es weiter. Format-Beispiele siehe [mcp-conventions.md § Confirm-Format](mcp-conventions.md#confirm-format-für-schreibende-aufrufe).

### 5. APPLY — Pro Element via MCP setzen

Wir iterieren durch das Mapping und setzen pro Element die Klasse / das Property. Bei Einzel-Fehler weitermachen (kein Auto-Rollback), Report am Ende. Bei systemischem Fehler stoppen.

## Klassifikations-Spezifika

Klassifizierung ist ein häufiger Bulk-Auftrag und hat eigene Eigenheiten, die wir explizit kennen müssen.

### Klassifikations-System ist Projekt-spezifisch

Ein Archicad-Projekt kann mehrere Klassifikations-Systeme parallel führen: das ARCHICAD-eigene, Uniclass 2015, OmniClass, Custom. **Aktiv** ist immer nur eines. Wenn wir eine GUID aus dem inaktiven Uniclass-System holen und sie an einer Wand setzen, die das ARCHICAD-System nutzen soll, ist die BIM-Datenstruktur korrupt und IFC-Export geht schief — ohne dass der User es merkt.

**Regel: System-scoped GUID-Lookup.** Vor jedem Klartext → GUID-Mapping holen wir das aktive System (Warm-up Feld 7) und scope'n unsere Discovery-Query darauf. Niemals eine globale „suche eine Klasse namens ‚Innenwand'"-Query absetzen.

### Klassifikations-IDs sind GUIDs, kein Klartext

Wenn der MCP-Server uns sagt, eine Wand ist klassifiziert als Klasse mit GUID `xxxxxxxx-yyyy-...`, ist das die Wahrheit. Wir mappen erst beim Anzeigen für den User auf den sprechenden Namen.

Beim Setzen einer neuen Klassifizierung machen wir das umgekehrt: User nennt „Außenwand", wir holen die GUID aus dem aktiven System, dann setzen.

### Hierarchische Klassen

Manche Systeme haben Hierarchien („Wand" > „Außenwand" > „Sichtmauerwerk"). Beim Mapping nach „Außenwand" zielen wir auf das passende Level, nicht zwingend auf den Root. Wenn der User „klassifiziere als Außenwand" sagt und das aktive System hat „Außenwand" als Sub-Klasse von „Wand", nehmen wir die Sub-Klasse.

## Pro-Element-Typ-Ableitungslogik

Wie man pro Element entscheidet, welche Klasse er bekommt. Diese Algorithmen werden in Phase 5 live verifiziert und pro betroffenem Recipe als Worked Example konkretisiert.

### Wand — Innen vs. Außen

Quellen, kombinierbar:

- **Layer-Name.** „Wände-Außen" / „Wände-AW" → Außenwand. „Wände-Innen" / „Wände-IW" → Innenwand. Wenn ein Layer-Schema vorhanden ist und konsistent, ist das die schnellste und sicherste Quelle.
- **Geometrische Lage.** Wand-Position relativ zu Raum-Polygonen. Wenn beide Seiten an Innenräumen anliegen → Innenwand. Wenn eine Seite an einem Innenraum und die andere Seite frei oder an einem Außenraum / Außenkontur → Außenwand.
- **Verbundene Räume.** Wenn der MCP-Server Zone-Membership pro Wand-Seite zurückgibt: Wand zwischen Innen-Zone und Außen-Zone → Außenwand.

**Reihenfolge der Quellen:** Layer-Name zuerst (schnellste Heuristik), Geometrie/Zone fallback bei unklarem oder nicht-vorhandenem Layer-Schema. TODO Phase 5: exakte Algorithmik festlegen.

### Öffnung — Tür vs. Fenster vs. Wandöffnung

Quellen:

- **Subtype** des Library-Objects, das die Öffnung repräsentiert (Fenster-Bibliothekstyp → Fenster-Klasse, Tür-Bibliothekstyp → Tür-Klasse).
- **Höhenposition.** Tür meist mit Sill-Höhe nahe 0, Fenster mit Sill > 0. Heuristik, kein Beweis.

TODO Phase 5: ob Subtype reliable genug ist als alleinige Quelle.

### Stütze — tragend vs. nicht-tragend

Quellen:

- **Property „tragend" / „structural function"** falls vorhanden im Projekt.
- **Composite-Material.** Stützen aus Stahlbeton mit Bewehrungs-Composite → tragend; rein dekorative Holz-Stützen → nicht-tragend.

### Library-Object — Möbel / Sanitär / Beleuchtung / sonstiges

Quellen:

- **Subtype-Pfad** im Library-System (z. B. „Möbel/Sitzmöbel/Stuhl") gibt direkt die Kategorie.
- **Library-Kategorie** falls separater Property.

## Mehrdeutigkeits-Handling

Manche Elemente passen in keine eindeutige Klasse. Beispiele:

- Eine Wand grenzt sowohl an einen Innen- als auch an einen Außenraum (Garage zwischen Haus und Carport).
- Eine Stütze sitzt auf der Geschoss-Grenze — gehört sie zum EG oder OG?
- Ein Object hat keinen klaren Subtype-Pfad.

**Regel: NICHT raten.** Wir schließen solche Elemente vom automatischen Mapping aus und listen sie SEPARAT im Confirm-Dialog auf, mit Markierung „unklar". Der User entscheidet pro Fall, oder schließt sie für diesen Auftrag vom Bulk aus.

Beispiel-Confirm-Format:

```
Klassifizierung Wände — Vorschlag:
- 84 Wände → Innenwand
- 43 Wände → Außenwand
- 5 Wände → UNKLAR (manuelle Entscheidung nötig)

Soll ich für die 127 eindeutigen ausführen?
Die 5 unklaren möchtest du dann separat behandeln? (ja / details-eindeutig / details-unklar / abbrechen)
```

`details-unklar` zeigt die 5 Wände mit ID, Lage, und warum sie unklar sind („grenzt an Innenraum links und Außenraum rechts").

## Mid-Batch-Fehlerverhalten

Während APPLY scheitert ein Einzel-Element. Was tun?

- **Kein Auto-Rollback.** Archicads Undo bleibt das Werkzeug des Users — wir rollen nicht „heimlich" zurück.
- **Systemisch oder einzeln?** Wenn der Fehler systemisch wirkt (gleicher Fehler bei *allen* Elementen, etwa „invalid GUID format"): sofort stoppen, vollständig berichten.
- **Weiterlaufen bei Einzel-Fehlern.** Wenn der Fehler nur ein Element betrifft (etwa „element not found" — der User hat es zwischenzeitlich gelöscht): weitermachen mit dem Rest.
- **Report am Ende.** Format: „127 verarbeitet, 124 erfolgreich, 3 Fehler bei IDs A, B, C — Gründe: X, Y, Z." Damit kann der User entscheiden, ob er die 3 manuell oder mit einem erneuten Lauf nachreicht.

## Property-Set live verifiziert (CAP-01)

Live verifiziert am 2026-05-21 gegen AC29: das verwendete Schema für Property-Updates ist viel einfacher als zunächst befürchtet.

**Set-Schema:**
```python
mcp__archicad__archicad_call_tool(
  name="properties_set_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementPropertyValues": [{
        "elementId": {"guid": "<element-guid>"},
        "propertyId": {"guid": "<property-guid>"},
        "propertyValue": {"value": "<display-string>"}  # einfach String!
      }]
    }
  }
)
```

`propertyValue.value` ist immer ein String — auch für Enum-Properties wird der Display-Wert als String übergeben (z. B. `"Linoleum 2,5 (R9)"`). Nicht das komplexe `{type, status, value: {type: "displayValue", displayValue: ...}}`-Konstrukt.

**Read-Schema** symmetrisch via `properties_get_property_values_of_elements` — Response liefert `propertyValue.value` als String zurück.

**Voraussetzung (kritisch!):** Das Element muss **klassifiziert** sein im Klassifikations-Item, das in der Property-`availability`-Liste steht. Sonst Fehler: `Not available or not evaluated property` (Code `-2130312908`).

**Workaround bei nicht klassifizierten Elementen:** Vor dem Set einmalig `mcp__archicad__elements_set_classifications_of_elements` aufrufen mit der passenden Klassifikation. Live verifiziert: nach Klassifizierung funktioniert Property-Set sofort.

## Property-Definitions sind Create-Only (CAP-02 Negativ-Befund)

Live verifiziert am 2026-05-21: **es gibt keinen Modify-Endpoint für Property-Definitions.** `properties_create_property_definitions` mit gleichem Namen wie eine existierende Property schlägt fehl (Code `-2130312988`).

**Auswirkung für Enum-Erweiterung:**
- Neue Enum-Werte zu einer existierenden Property hinzufügen → **nicht via MCP möglich**
- User muss in Archicad-UI Property-Manager → „Optionen-Setup" verwenden
- Alternative (zerstörerisch): Property löschen + neu erstellen mit erweiterter Enum-Liste — verliert alle zugewiesenen Werte

**Workflow-Konsequenz für Bulk-Klassifizierung:**
1. Vor jedem Bulk-Property-Set die Property-Definition lesen via `properties_get_details_of_properties`
2. Source-Werte gegen `possibleEnumValues` matchen
3. Bei Mismatch: STOPPEN und User auffordern, in Archicad-UI die fehlenden Werte zu ergänzen
4. NIEMALS „einfach" delete + recreate — Daten-Verlust

## Schema-Bug: `defaultValue` ist required (nicht optional)

Live verifiziert am 2026-05-21: das `properties_create_property_definitions`-Schema markiert `defaultValue` als optional, **aber tatsächlich ist es Pflicht**. Aufrufe ohne `defaultValue` schlagen fehl (Code `-2130313104`).

**Korrektes Schema** für Enum-Property-Create:
```python
{
  "name": "<name>",
  "description": "<desc>",
  "type": "singleEnum",
  "isEditable": true,
  "defaultValue": {  # <-- Pflicht trotz Schema-Optional
    "basicDefaultValue": {
      "type": "singleEnum",
      "status": "normal",
      "value": {"type": "displayValue", "displayValue": "<default-enum-value>"}
    }
  },
  "possibleEnumValues": [{"enumValue": {"displayValue": "..."}}, ...],
  "availability": [{"classificationItemId": {"guid": "<class-item-guid>"}}],  # min 1
  "group": {"propertyGroupId": {"guid": "<group-guid>"}}
}
```

Für Property-Groups: erst `properties_create_property_groups` aufrufen, dann die Group-GUID in `propertyDefinition.group` verwenden. Built-in-Group-GUIDs scheinen nicht für UserDefined-Properties verwendbar zu sein.

## Pre-Flight: Property-Enum-Normalisierung

Bei Bulk-Updates von Single-Enum- oder Multi-Enum-Properties prüfen wir **vor APPLY**, ob die Ziel-Werte exakt in der Property-Enum-Definition vorkommen. Sonst:

- Archicad lehnt den Set-Call ab (Fehler), oder
- Archicad ignoriert den Wert still (Daten-Inkonsistenz, schwer zu debuggen).

**Typische Inkonsistenzen** zwischen Source-Daten und Enum-Definition:

| Inkonsistenz-Typ | Beispiel | Auswirkung |
|---|---|---|
| Trailing Spaces | „Linoleum " vs. „Linoleum" | zwei verschiedene Enum-IDs intern |
| Tippfehler | „Linolium" statt „Linoleum" | kein Match, Update schlägt fehl |
| Trennzeichen | „R11+R12" vs. „R11/R12" | kein Match |
| Singular/Plural | „Fliese R10" vs. „Fliesen (R10)" | kein Match |
| Klammer-Stil | „Fliesen R10" vs. „Fliesen (R10)" | kein Match |
| Fehlende Enum-Werte | Source hat 21 Werte, Enum-Definition hat nur 8 | 13 Werte landen nirgendwo |

**Pre-Flight-Workflow:**

1. **Property-Enum-Definition lesen** — entweder via `mcp__archicad__properties_get_property_values_of_elements` an einem Beispiel-Element (zeigt aktuelle möglichen Werte), oder via dediziertem Property-Definition-Read falls verfügbar.
2. **Source-Werte gegen Enum diffen:**
   ```
   not_in_enum = [v for v in source_values if v not in enum_values]
   close_matches = {v: difflib.get_close_matches(v, enum_values) for v in not_in_enum}
   ```
3. **Wenn `not_in_enum` nicht leer:** STOPPEN. User-Report mit:
   - Werte, die fehlen.
   - Ähnliche existierende Werte (Substring-Match oder Levenshtein).
   - Vorschlag: User ergänzt die Property-Definition in Archicad (Property-Manager → Property-Edit → Enum-Werte hinzufügen), dann Re-Run.

**NIEMALS** einen Bulk-Update mit unbekannten Enum-Werten starten. Lieber Pre-Flight-Pause als Daten-Korruption.

## Identifier-Mapping bei externen Daten-Quellen

Bei Bulk-Updates aus externen Datenquellen (Schedule-Export, Capmo, Excel) ist der Identifier meistens **nicht** die MCP-GUID, sondern eine fachliche ID — Raumnummer, Element-Bezeichnung, Schedule-Index.

**Mapping-Workflow:**

1. Alle relevanten Elemente via `mcp__archicad__elements_get_elements_by_type` listen.
2. Pro Element die Identifier-Property auslesen via `mcp__archicad__properties_get_all_property_ids_of_elements` + `properties_get_property_values_of_elements`. Typische Identifier-Properties:
   - „Raumnummer" / „RoomNumber" — bei Zonen
   - „Element-ID" — Built-in
   - „Bezeichnung" / „Name" — bei Library-Objects
3. Dict bauen: `{identifier_string: element_guid}`.
4. **Eindeutigkeits-Check:** Jede Source-Zeile sollte genau einen GUID-Match haben. Bei mehreren oder keinem → User informieren, **nicht raten**.

Falls die externen Daten als XLSX/CSV vorliegen: siehe [`schedule-pipeline.md`](schedule-pipeline.md) für die End-to-End-Pipeline.

## TODO — Phase 5 Live-Verifikation

Diese Punkte werden in Phase 5 am laufenden Archicad geklärt:

- **Exakte Discovery-Query für System-scoped GUID-Lookup.** Was ist der Tool-Name? Welche Parameter? Wie unterscheidet sich die Antwort zwischen aktiven und inaktiven Systemen?
- **Geometrische „Innen vs. Außen"-Ableitung.** Welche MCP-Endpoints liefern Zone-Membership oder Polygon-Containment? Falls keine: müssen wir Geometrie selbst berechnen?
- **Innere-Raum-Erkennung.** Via Zone-Listing oder via Geometrie? Was ist effizienter und zuverlässiger im realen Projekt?
- **Subtype-Reliability bei Öffnungen.** Reicht der Subtype-Pfad alleine, oder brauchen wir Höhen-Fallback?
- **Pagination-Felder im Klassifikations-Response.** Liste der Klassen oft groß — wie ist die Pagination strukturiert?

Wenn Phase 5 abgeschlossen ist, ersetzen wir die TODO-Marker hier durch live-verifizierte Werte mit Datums-Marker (siehe [`self-improvement.md`](self-improvement.md)).
