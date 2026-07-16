# 2D-Linien und Polylinien

Dieses Recipe deckt alle fünf 2D-Linien-Element-Typen ab: **Line**, **PolyLine**, **Arc**,
**Circle** und **Spline**. Davon ist nur **PolyLine** via MCP erstellbar — live verifiziert
in Phase 2 (2026-05-19). Die anderen vier Typen haben keinen bekannten Create-Endpoint in
MCP v29; der User zeichnet sie manuell in Archicad, und wir arbeiten danach mit den
resultierenden GUIDs weiter. Alle fünf Typen sind via Read, Update und Delete vollständig
zugänglich.

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Create-Capabilities im Überblick](#create-capabilities-im-überblick)
3. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
4. [Discovery-Anker](#discovery-anker)
5. [Typische Parameter](#typische-parameter)
6. [Worked Example — PolyLine erstellen](#worked-example--polyline-erstellen)
7. [Worked Example — PolyLine mit Bogen (Arc-Segment)](#worked-example--polyline-mit-bogen-arc-segment)
8. [Worked Example — Alle 5 Typen einer Story lesen](#worked-example--alle-5-typen-einer-story-lesen)
9. [Worked Example — PolyLine modifizieren (Koordinaten ändern)](#worked-example--polyline-modifizieren-koordinaten-ändern)
10. [Worked Example — Element löschen (alle 5 Typen)](#worked-example--element-löschen-alle-5-typen)
11. [Worked Example — Pen-Index und Line-Type zuweisen](#worked-example--pen-index-und-line-type-zuweisen)
12. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
13. [Gotchas](#gotchas)
14. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang

- **PolyLine erstellen** (Create, verifiziert): Koordinaten-Array + optionale Arc-Segmente.
- **Workaround Line via PolyLine**: Eine PolyLine mit genau 2 Punkten ist visuell eine gerade Linie.
- **Line, Arc, Circle, Spline**: Nur Read / Update / Delete — kein bekannter Create-Endpoint in MCP v29. User zeichnet manuell; wir holen danach die GUID und arbeiten damit weiter.
- **Alle 5 Typen auf einer Story lesen** via `elements_get_elements_by_type`.
- **Geometrie modifizieren**: Koordinaten, Linientyp, Pen-Index, Layer.
- **Linientyp-Attribut lesen** via `attributes_get_attributes_by_type` mit `attributeType: "Line"`.
- **Löschen** mit SAFE-01-Confirm (alle 5 Typen, kein Hosted-Element-Check nötig).

---

## Create-Capabilities im Überblick

| Element-Typ | Create via MCP | Hinweis |
|---|---|---|
| PolyLine | ✓ `mcp__archicad__elements_create_polylines` <!-- 2026-05-19 verifiziert AC29 --> | Einziger erstellbarer 2D-Linientyp |
| Line | ✗ kein Create-Endpoint in MCP v29 <!-- 2026-05-20 schema-only --> | Workaround: PolyLine mit 2 Punkten |
| Arc | ✗ kein Create-Endpoint in MCP v29 <!-- 2026-05-21 verifiziert AC29 (Discovery liefert nur elements_create_polylines) --> | User zeichnet manuell, oder PolyLine mit Arc-Segment (arcs-Array) |
| Circle | ✗ kein Create-Endpoint in MCP v29 <!-- 2026-05-21 verifiziert AC29 --> | User zeichnet manuell |
| Spline | ✗ kein Create-Endpoint in MCP v29 <!-- 2026-05-21 verifiziert AC29 --> | User zeichnet manuell |

Vollständige Capability-Tabelle aller Element-Typen:
[`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Live-verifizierte Element-Create-Capabilities.

---

## Erforderlicher Warm-up-Kontext

Vollständiges Warm-up-Protokoll: [`../reference/workflow-context.md`](../reference/workflow-context.md).

| Feld | Warum es hier wichtig ist |
|---|---|
| **Port** | Jeder MCP-Call braucht `"port"`. |
| **Aktive Story** (`floorIndex`) | Default-Story bei Create, wenn `floorInd` weggelassen wird. |
| **Längeneinheit** | Alle Koordinaten in **Meter** (MCP-Konvention verifiziert). |
| **Sichtbare Layer** | SAFE-03: Ziel-Layer nicht sichtbar → erst konfirmieren. |
| **Pen-Set** (Feld 6) | Nur bei 2D-Arbeit: aktive Pen-Tabelle bestimmt, welche Pen-Indizes welche Farbe/Gewicht haben. |

Pen-Set-Details (Tool-Namen, Query-Strings, Stifte-per-Index):
[`../reference/workflow-context.md`](../reference/workflow-context.md) § Feld 6.

---

## Discovery-Anker

<!-- 2026-05-20 schema-only -->

| Operation | Discovery-Query | Typischer Tool-Name |
|---|---|---|
| PolyLine erstellen | `"create polyline 2d element"` | `elements_create_polylines` <!-- 2026-05-19 verifiziert AC29 --> |
| Elemente nach Typ listen | `"get elements by type"` | `elements_get_elements_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Element modifizieren | `"set details of elements"` | `elements_set_details_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Element löschen | `"delete elements by id"` | `elements_delete_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Bounding Box (2D) | `"get 2d bounding boxes of elements"` | `elements_get2_d_bounding_boxes` <!-- 2026-05-21 verifiziert AC29 — ACHTUNG: Tool-Name OHNE Unterstrich zwischen "get" und "2", nur zwischen "2" und "d" --> |
| Bounding Box (3D) | `"get 3d bounding boxes of elements"` | `elements_get3_d_bounding_boxes` <!-- 2026-05-21 verifiziert AC29 — gleiche Underscore-Konvention --> |
| Linientypen listen | `"get attributes by type line"` | `attributes_get_attributes_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Pen-Tabellen listen | `"get attributes by type pen table"` | `attributes_get_attributes_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` <!-- 2026-05-19 verifiziert AC29; bekannter Pydantic-Bug, siehe Memory issue_archicad_mcp_get_details_bug --> |
| Klassifikation setzen | `"set classifications of elements"` | `elements_set_classifications_of_elements` <!-- 2026-05-21 verifiziert AC29 --> |

---

## Typische Parameter

### PolyLinesDatum (für `elements_create_polylines`)

<!-- 2026-05-19 verifiziert AC29 -->

| Feld | Typ | Pflicht | Beschreibung |
|---|---|---|---|
| `floorInd` | Integer | nein | Story-Index. Wenn weggelassen → aktive Story (Warm-up Feld 4). |
| `coordinates` | `[{x, y}, ...]` | ja | Koordinaten-Array, min. 2 Punkte. Meter-Einheit. Kein `z`-Feld (Coord2D). |
| `arcs` | `[{begIndex, endIndex, arcAngle}, ...]` | nein | Bogen-Segmente zwischen zwei Stützpunkten. Siehe Arc-Konvention in Gotcha 2. |

Response: `{"elements": [{"elementId": {"guid": "<polyline-guid>"}}]}`.

### PolyLine-Modifikation via `elements_set_details_of_elements` — eingeschränkt

<!-- 2026-05-21 verifiziert AC29 via Discovery-Schema-Inspektion -->

**Kritische Einschränkung:** `elements_set_details_of_elements.params.elementsWithDetails[].details.typeSpecificDetails` ist im MCP v29-Server **hart auf `WallSettings` festgenagelt** (Schema akzeptiert nur die Wand-spezifischen Felder begCoordinate/endCoordinate/height/bottomOffset/offset/begThickness/endThickness). Es gibt KEINE PolyLine/Line/Arc/Circle/Spline-spezifischen Felder in dem Endpoint.

**Konsequenz:** Geometrische Modifikation einer PolyLine (Koordinaten, Arc-Segmente) ist **NICHT** via `set_details` möglich. Workaround: PolyLine löschen + neu erstellen (siehe Worked Example unten).

**Was via `set_details` AUF PolyLines GEHT** (Top-Level-Details-Felder, kein typeSpecificDetails):

| Feld | Typ | Beschreibung |
|---|---|---|
| `floorIndex` | Number | Story-Zuweisung (Element zwischen Stories verschieben). |
| `layerIndex` | Number | Layer-Index aus dem Projekt-Register. |
| `drawIndex` | Number | Stift-Index (Pen-Nummer). |

`lineTypeIndex` ist **NICHT** Teil des `Details`-Schemas. Linientyp einer existierenden PolyLine zu ändern ist via MCP nicht direkt möglich — Workaround: User-UI oder delete+recreate mit gewünschtem Linientyp-Default-Stempel.

---

> **User sagt:** „Zeichne mir eine offene Polylinie mit 4 Stützpunkten im EG-Grundriss."

## Worked Example — PolyLine erstellen

<!-- 2026-05-19 verifiziert AC29 — elements_create_polylines -->

Wir erstellen eine offene PolyLine mit 4 Stützpunkten auf Story 0 (EG).

**Warm-up-Kontext:** Port 19723, Story 0, Ziel-Layer sichtbar (SAFE-03 erfüllt).

```python
mcp__archicad__archicad_call_tool(
  name="elements_create_polylines",
  arguments={
    "port": 19723,
    "params": {
      "polylinesData": [
        {
          "floorInd": 0,
          "coordinates": [
            {"x": 1.0, "y": 0.0},
            {"x": 4.0, "y": 0.0},
            {"x": 4.0, "y": 3.0},
            {"x": 7.0, "y": 3.0}
          ]
        }
      ]
    }
  }
)
```

**Erwartete Response:**

```json
{
  "elements": [
    {"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}
  ]
}
```

Die zurückgegebene GUID im Arbeitsgedächtnis behalten (SAFE-05) — sie wird für alle
Folge-Operationen in dieser Session direkt weiterverwendet.

**Workaround „Linie via PolyLine":** Wenn der User eine einfache gerade Linie braucht,
genügen genau 2 Koordinaten — das ergibt visuell eine Linie ohne BIM-Semantik:

```json
"coordinates": [
  {"x": 0.0, "y": 0.0},
  {"x": 5.0, "y": 0.0}
]
```

Ein `arcs`-Feld ist in diesem Fall nicht nötig. Kein Arc-Segment → gerade Strecke.

---

> **User sagt:** „Setz eine Polylinie mit einem Viertelkreis-Bogen im ersten Segment."

## Worked Example — PolyLine mit Bogen (Arc-Segment)

<!-- 2026-05-21 Schema verifiziert AC29 via Discovery (PolylinesDatum.arcs[].arcAngle, begIndex, endIndex bestätigt) -->

Eine PolyLine mit 3 Stützpunkten, wobei das Segment zwischen Punkt 0 und Punkt 1 als
Kreisbogen verläuft.

```python
mcp__archicad__archicad_call_tool(
  name="elements_create_polylines",
  arguments={
    "port": 19723,
    "params": {
      "polylinesData": [
        {
          "floorInd": 0,
          "coordinates": [
            {"x": 0.0, "y": 0.0},
            {"x": 4.0, "y": 0.0},
            {"x": 4.0, "y": 3.0}
          ],
          "arcs": [
            {
              "begIndex": 0,
              "endIndex": 1,
              "arcAngle": 1.5708
            }
          ]
        }
      ]
    }
  }
)
```

`arcAngle` ist in **Radiant** (nicht Grad). Positive Werte: Bogen nach links (gegen
Uhrzeigersinn); negative Werte: Bogen nach rechts (im Uhrzeigersinn). Vollkreis wäre
`2π ≈ 6.2832`. Für die üblichen Werte: 90° = `π/2 ≈ 1.5708`, 180° = `π ≈ 3.1416`.

`begIndex` / `endIndex` sind nullbasierte Indizes in das `coordinates`-Array — der Bogen
verbindet `coordinates[begIndex]` mit `coordinates[endIndex]`. Nicht jedes Segment muss ein
Bogen sein; Segmente ohne Eintrag in `arcs` bleiben gerade.

---

> **User sagt:** „Zeig mir alle 2D-Linien-Elemente auf der aktiven Story."

## Worked Example — Alle 5 Typen einer Story lesen

<!-- 2026-05-19 verifiziert AC29 — elements_get_elements_by_type -->

Wir listen alle 2D-Linien-Elemente der aktiven Story, jeweils ein Call pro Typ.
`elementType` muss exakt dem Enum-Wert entsprechen (Groß-/Kleinschreibung beachten).

**Line:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {
      "elementType": "Line",
      "filters": ["OnActualFloor", "IsVisibleByLayer"]
    }
  }
)
```

**PolyLine:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {
      "elementType": "PolyLine",
      "filters": ["OnActualFloor", "IsVisibleByLayer"]
    }
  }
)
```

**Arc, Circle, Spline:** identischer Aufruf mit `"elementType": "Arc"`, `"Circle"` bzw.
`"Spline"`.

Pagination mit `next_page_token` vollständig abarbeiten, bevor mit Auswertung begonnen wird.
Response-Format: `{"elements": [{"elementId": {"guid": "..."}}, ...], "next_page_token": "..."}`.

**Bounding Box für gefundene Elemente** <!-- 2026-05-21 verifiziert AC29 — Tool-Name elements_get2_d_bounding_boxes (siehe Discovery-Anker-Hinweis zur Underscore-Konvention) -->:

```python
mcp__archicad__archicad_call_tool(
  name="elements_get2_d_bounding_boxes",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "<guid-1>"}},
        {"elementId": {"guid": "<guid-2>"}}
      ]
    }
  }
)
```

Response: `{"boundingBoxes": [{"xMin": ..., "yMin": ..., "xMax": ..., "yMax": ...}, ...]}`.
Gibt Lage und Ausdehnung in Metern zurück — für alle 5 Typen einheitlich nutzbar.

---

> **User sagt:** „Verschieb den dritten Stützpunkt dieser Polylinie um 2 Meter nach rechts."

## Worked Example — PolyLine-Geometrie ändern: delete + recreate

<!-- 2026-05-21 verifiziert AC29 — set_details typeSpecificDetails ist WallSettings-only, daher Geometrie-Update via delete+recreate -->

**Warum nicht via `set_details`:** Wie in § Typische Parameter dokumentiert, akzeptiert `elements_set_details_of_elements.details.typeSpecificDetails` ausschließlich `WallSettings` (Discovery-Schema-Inspektion 2026-05-21). PolyLine-Koordinaten oder Arc-Segmente können **nicht** direkt über diesen Endpoint modifiziert werden.

**SAFE-02-Konflikt:** Wir implementieren Updates normalerweise NICHT als „delete + recreate", weil das die Confirm-Schleife für Modify mit der laxeren Create-Schleife mischt. Für Geometrie-Änderungen einer PolyLine ist das aber die einzige programmatische Option. Wir behandeln den kombinierten Schritt deshalb als **Update** mit voller Confirm-Schleife UND warnen ausdrücklich, dass die GUID sich ändert (SAFE-05 muss nachgezogen werden).

**Confirm-Dialog (SAFE-01 + SAFE-02-Hinweis) vor dem Aufruf:**

```
Ich werde folgendes ändern:
- PolyLine a1b2c3d4-e5f6-7890-abcd-ef1234567890  (Story 0, 4 Punkte)
  Stützpunkt 3: (4.0, 3.0) → (6.0, 3.0)

ACHTUNG: MCP v29 hat keinen In-Place-Update für PolyLine-Koordinaten.
Workaround: Element wird gelöscht und mit neuer Geometrie neu erstellt.
Folgen: Neue GUID, Element-Reihenfolge im Layer kann sich ändern,
Klassifikation/Property-Werte gehen verloren (manuell wiederherstellen).

Ausführen? (ja / nein / details / abbrechen)
```

**Aufruf nach ausdrücklichem `ja` (zwei Schritte):**

```python
# Schritt 1: alte PolyLine löschen
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {"elements": [{"elementId": {"guid": "a1b2c3d4-..."}}]}
  }
)

# Schritt 2: neue PolyLine mit geänderter Geometrie erstellen
mcp__archicad__archicad_call_tool(
  name="elements_create_polylines",
  arguments={
    "port": 19723,
    "params": {
      "polylinesData": [{
        "floorInd": 0,
        "coordinates": [
          {"x": 1.0, "y": 0.0},
          {"x": 4.0, "y": 0.0},
          {"x": 4.0, "y": 3.0},
          {"x": 6.0, "y": 3.0}
        ]
      }]
    }
  }
)
# Response liefert NEUE GUID — alte GUID a1b2c3d4-... ist nicht mehr gültig.
```

**Für Line/Arc/Circle/Spline (keine create-Endpoints):** Geometrie-Update via MCP ist NICHT möglich. User in Archicad-UI ändern lassen, danach die neue GUID via `elements_get_elements_by_type` neu holen.

**Was via `set_details` AUF PolyLine GEHT:** floorIndex, layerIndex, drawIndex (Pen-Nummer). Beispiel siehe nächster Worked Example.

---

> **User sagt:** „Lösch diese drei 2D-Linien auf dem Grundriss-Layer."

## Worked Example — Element löschen (alle 5 Typen)

<!-- 2026-05-19 verifiziert AC29 — elements_delete_elements -->

2D-Linien-Elemente haben keine gehosteten Kinder — SAFE-04-Pre-Check (Hosted-Element-Prüfung)
entfällt. Wir zeigen dennoch den SAFE-01-Confirm, bevor der Delete-Call abgesetzt wird.

**Beispiel: 3 Elemente verschiedener Typen löschen**

```
Ich werde folgendes löschen:
- Line    guid-1111-...  (Story 0, Layer A_2D-Grundriss)
- Arc     guid-2222-...  (Story 0, Layer A_2D-Grundriss)
- PolyLine guid-3333-... (Story 0, Layer A_2D-Grundriss)

Diese Elemente haben keine gehosteten Kinder.
Die Operation kann via Archicad-Undo rückgängig gemacht werden.

Ausführen? (ja / nein / details / abbrechen)
```

**Aufruf nach `ja`:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "guid-1111-..."}},
        {"elementId": {"guid": "guid-2222-..."}},
        {"elementId": {"guid": "guid-3333-..."}}
      ]
    }
  }
)
```

`elements_delete_elements` nimmt alle 5 Element-Typen im selben Call entgegen — kein
separater Call pro Typ nötig.

**Bulk-Delete (> 10 Elemente):** Summary-Confirm-Format verwenden:

```
Ich werde folgendes löschen:
- 47 PolyLine-Elemente auf Story 0
- 12 Line-Elemente auf Story 0

Ausführen für alle 59? (ja / details / abbrechen)
```

Confirm-Format-Details: [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Confirm-Format.

---

> **User sagt:** „Setz für diese Polylinie einen dünneren Stift, Pen 2."

## Worked Example — Pen-Index zuweisen (Linientyp NICHT via MCP änderbar)

<!-- 2026-05-21 verifiziert AC29 — Details-Schema enthält drawIndex (Pen-Index), aber KEIN lineTypeIndex. Linientyp-Wechsel einer existierenden PolyLine via MCP nicht möglich. -->

Wir setzen für eine PolyLine Pen-Index 2 (dünnere Linie). Der Linientyp-Wechsel ist via MCP v29 NICHT möglich (kein `lineTypeIndex` im `Details`-Schema) — Workaround: UI-Bearbeitung oder delete+recreate mit anderem Default-Linientyp.

**Schritt 1 — Verfügbare Linientypen listen (für Default-Wahl bei delete+recreate):**

```python
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "Line"}
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — attributes_get_attributes_by_type -->

Response: `{"attributes": [{"attributeId": {"guid": "..."}, "index": 1, "name": "Vollline"}, ...]}`.
Der `index`-Wert (Float, 1-basiert) wird als `lineTypeIndex` beim Update verwendet.
Pagination mit `next_page_token` durchziehen.

**Schritt 2 — Aktives Pen-Set prüfen (optional, für Pen-Farbe/Gewicht-Info):**

```python
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "PenTable"}
  }
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Response gibt alle Pen-Tabellen-Namen und ihre Indizes zurück. Detail-Abfrage eines
Pen-Sets (Farbe + Gewicht pro Pen-Nummer):

```python
mcp__archicad__archicad_call_tool(
  name="attributes_get_pen_table_attributes",
  arguments={
    "port": 19723,
    "params": {"attributeIds": [{"guid": "<pen-table-guid>"}]}
  }
)
```

**Schritt 3 — Confirm (SAFE-01):**

```
Ich werde folgendes ändern:
- PolyLine a1b2c3d4-...  (Story 0)
  Pen-Index: 1 → 2

(Linientyp-Wechsel via MCP nicht möglich — manuell in Archicad oder
delete+recreate mit neuem Default-Linientyp.)

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 4 — Aufruf nach `ja`:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"},
          "details": {
            "drawIndex": 2.0
          }
        }
      ]
    }
  }
)
```

`drawIndex` ist Number (Pen-Nummer, 1-basiert). Der Index stammt aus dem Pen-Set des Schritts 2 — nie raten.

---

## Bulk-Klassifizierung (Stub)

2D-Linien-Elemente werden im Kontext von Archicad-Projekten weniger häufig klassifiziert
als 3D-Bauteile, können aber dasselbe Klassifikations-System verwenden.

Das universelle Read → Filter → Group → Confirm → Apply-Pattern ist dokumentiert in
[`../reference/bulk-operations.md`](../reference/bulk-operations.md).

Kurzfassung: `elements_get_elements_by_type` für gewünschten Typ (paginiert) → Layer /
Bounding-Box-Filter → Gruppen bilden → Confirm (Summary für > 10 Elemente) →
`elements_set_classifications_of_elements`.

---

## Gotchas

### 1. Nur PolyLine ist via MCP v29 erstellbar — Line als PolyLine-Workaround

`elements_create_lines`, `elements_create_arcs`, `elements_create_circles` und
`elements_create_splines` existieren nicht in MCP v29. Wenn der User eine gerade Linie
benötigt: PolyLine mit genau 2 Punkten erstellen — visuell nicht von einer Line
unterscheidbar, aber technisch ein anderer Element-Typ. Wenn echter `Line`-Typ
Pflichtanforderung ist: User zeichnet manuell in Archicad. Capability-Tabelle:
[`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Live-verifizierte Element-Create-Capabilities.

### 2. `arcAngle` ist in Radiant, Vorzeichen bestimmt Krümmungsrichtung

`arcAngle` im `arcs`-Array ist **nicht in Grad** — `90` (Grad) wird als `1.5708` (Radiant)
übergeben. Positive Werte: Bogen gegen Uhrzeigersinn (links); negative Werte: Bogen im
Uhrzeigersinn (rechts). Ein Wert von `0` bedeutet gerade Strecke (kein Bogen).
Häufige Umrechnungen: 90° = `1.5708`, 180° = `3.1416`, 270° = `4.7124`.

### 3. `begIndex` / `endIndex` in `arcs` sind nullbasiert

Im `arcs`-Array verweisen `begIndex` und `endIndex` auf die **nullbasierten** Positionen
im `coordinates`-Array. Das erste Segment ist `begIndex: 0, endIndex: 1`. Nicht mit dem
1-basierten `lineTypeIndex` / `layerIndex` / `drawIndex` verwechseln — die sind 1-basiert.

### 4. Pen-Index (`drawIndex`) ist 1-basiert — `0` ist ungültig

`drawIndex: 0.0` wäre kein Stift — Archicad-Pen-Indizes starten bei 1. Pen-Index 1 ist
standardmäßig schwarz und dünn (projektabhängig). Den konkreten Pen-Wert immer aus dem
aktiven Pen-Set ableiten (`attributes_get_pen_table_attributes`) — nie direkt raten.
Das User-Setup hat 6 Pen-Tabellen (Memory `project_archicad_user_setup.md`).

### 5. Linientyp vs. Pen-Index — Konzept gilt, aber nur Pen-Index via MCP setzbar

Im Konzept sind das zwei unabhängige Attribute: **Linientyp** steuert das Muster (Vollline, Strichlinie, Strichpunkt etc., aus dem `Line`-Attribut-Register), **Pen-Index** steuert Farbe und Strichgewicht (aus der aktiven Pen-Tabelle).

**Via MCP v29 modifizierbar (auf existierender PolyLine):** nur Pen-Index (`drawIndex` im `Details`-Schema). Linientyp ist NICHT im `Details`-Schema — siehe Gotcha 9. Workaround: User-UI oder delete+recreate mit dem als Default gesetzten Linientyp.

### 6. `floorInd` in `polylinesData` vs. `floorIndex` in `elementsWithDetails`

Die Feldbezeichnungen unterscheiden sich je nach Operation:
- **Create** (`elements_create_polylines`): `floorInd` (ohne `ex`).
- **Update** (`elements_set_details_of_elements`): `floorIndex` (mit `ex`).
Falscher Name → Schema-Fehler. Beim Schreiben der Parameter-Namen immer auf den Kontext
achten (Create vs. Update).

### 7. `floorInd` weggelassen → aktive Story, nicht Story 0

Wenn `floorInd` beim Create fehlt, landet die PolyLine auf der **aktuell in Archicad aktiven
Story** — nicht notwendigerweise auf Story 0 (EG). Da der User während einer Session die
aktive Story gewechselt haben könnte (SAFE: Story-Feld ist volatil), immer den aktuellen
`floorIndex`-Wert aus dem Warm-up (Feld 4) explizit übergeben, wenn die Ziel-Story feststeht.

### 8. `elements_set_details_of_elements.typeSpecificDetails` ist WallSettings-only

<!-- 2026-05-21 verifiziert AC29 via Discovery-Schema-Inspektion -->

Im MCP v29-Server ist das Schema für `params.elementsWithDetails[].details.typeSpecificDetails` hart auf `WallSettings` festgenagelt (nur Wand-Felder: begCoordinate, endCoordinate, height, bottomOffset, offset, begThickness, endThickness). Es gibt KEINE PolyLine/Line/Arc/Circle/Spline/Slab/Hatch-spezifischen Felder in dem Endpoint.

**Konsequenzen:**
- PolyLine-Geometrie (coordinates, arcs) NICHT via set_details änderbar → delete+recreate (siehe Worked Example).
- Hatch-Innen-Schraffur-Parameter (fillIndex, orientation) NICHT änderbar.
- Slab-Polygon NICHT änderbar.
- Linientyp einer PolyLine NICHT änderbar (kein `lineTypeIndex` im Schema).

**Was geht:** Top-Level-Details-Felder `floorIndex`, `layerIndex`, `drawIndex` für ALLE Element-Typen.

### 9. Modal-Dialog blockiert MCP-Calls — vor Bulk-Operationen prüfen

Wenn Archicad einen modalen Dialog offen hat (Linientyp-Editor, Layer-Manager,
Element-Einstellungen etc.), frieren MCP-Calls ein oder timeouten.
Symptom: Tool-Call hängt > 10 Sekunden oder Server gibt `Invalid program status
(there is an open modal dialog: <Dialog-Name>)` (Code 4001) zurück.
Vor größeren 2D-Batch-Operationen User bitten, alle offenen Dialoge zu schließen.
Details + Reaktionsprotokoll: [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md)
§ Modal-Dialoge in Archicad blockieren MCP.

---

## Verwandte Recipes

- [`fills-hatches.md`](fills-hatches.md) — 2D-Schraffuren (Hatch-Elemente), die oft zusammen
  mit 2D-Linien auf denselben Layern liegen.
- [`wall-operations.md`](wall-operations.md) — Wände als Linie-ähnliche BIM-Elemente; kein
  Create via MCP v29 (Stil-Referenz für No-Create-Recipes).
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Capability-Tabelle,
  Confirm-Format, Modal-Dialog-Protokoll, Fehlerklassen.
- [`../reference/workflow-context.md`](../reference/workflow-context.md) — Warm-up-Felder 4
  (aktive Story) und 6 (Pen-Set) im Detail.
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Universelles
  Bulk-Klassifizierungs-Pattern.

## Hand-gestrichelte Ketten → eine Polylinie mit Linientyp (live 2026-07-16, THN)

Bestandspläne zeichnen Strich-/Punktlinien oft als HUNDERTE Einzel-Elemente
(THN: 1.320 Stück à 10 cm im 42-cm-Raster auf FW_Längen). Ersatz durch echte
Polylinien mit Linientyp:

- **Linientyp-Index ist per API nicht ermittelbar** (Tapir kennt kein
  LineType-Attribut; API.GetLineAttributes liefert nur GUIDs, CreatePolylines
  will einen Index) → Trick: **User stellt das Polylinien-Werkzeug ein**,
  `CreatePolylines` OHNE lineTypeIndex erbt den Werkzeug-Default.
- Ketten-Erkennung: kurze Elemente (< 1,5 m) je Ebene clustern (Enden < 2 m),
  dann **Greedy entlang der Laufrichtung** (nächster Mittelpunkt < 1,2 m,
  Richtungs-Kosinus > 0,3, sonst Split) — Graph-Ansätze über Endpunkt-Grade
  scheitern, weil Punkt-Raster jedem Ende 3+ „Nachbarn" gibt.
- Kollineare Zwischenpunkte ausdünnen (Kreuzprodukt-Toleranz) → 134 Striche
  werden 1 Zug mit ~10 Ecken.
- **Zweistufig ausführen**: erst alle neuen Züge erzeugen (alte bleiben!),
  User vergleicht im Blatt, DANN alte löschen. Pfeile/Symbole auf derselben
  Ebene vorher per User-Abwahl rauswerfen.
- THN: 25 Züge (1,6–52,5 m) ersetzen 1.248 Strichlein, 0 Fehler.
