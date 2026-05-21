# 2D-Schraffuren (Hatch-Elemente)

Dieses Recipe deckt Lesen, Modifizieren, Löschen und Klassifizieren eigenständiger
2D-Schraffur-Elemente (Hatch) ab — sowie das Auflisten der zugehörigen Fill-Pattern-Attribute.

**Hatch-Erstellung ist im Archicad-MCP v29 nicht verfügbar.** Kein `elements_create_hatches`-Tool
im Server. User zeichnet Schraffuren manuell in Archicad; wir arbeiten dann mit diesen Elementen
weiter. Vollständige Capability-Tabelle: `../reference/mcp-conventions.md` §
Live-verifizierte Element-Create-Capabilities.

<!-- 2026-05-21 live-verifiziert AC29 port 19723 (Ohne-Titel-Sandbox) — Tool-Namen via Discovery bestätigt; Hatch-Modifikation via MCP NICHT möglich (set_details typeSpecificDetails=WallSettings-only), siehe Abschnitt Hatch modifizieren -->

---

## Inhaltsverzeichnis

1. [Hatch vs. Fill — Begriffsklärung](#hatch-vs-fill--begriffsklärung)
2. [Umfang](#umfang)
3. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
4. [Discovery-Anker](#discovery-anker)
5. [Typische Parameter](#typische-parameter)
6. [Worked Example — Alle Hatches einer Story lesen](#worked-example--alle-hatches-einer-story-lesen)
7. [Worked Example — Fill-Pattern-Attribute auflisten](#worked-example--fill-pattern-attribute-auflisten)
8. [Worked Example — Hatch-Eigenschaften lesen](#worked-example--hatch-eigenschaften-lesen)
9. [Worked Example — Hatch modifizieren (Fill-Pattern + Pen ändern)](#worked-example--hatch-modifizieren-fill-pattern--pen-ändern)
10. [Worked Example — Hatch löschen](#worked-example--hatch-löschen)
11. [Worked Example — Hatch klassifizieren](#worked-example--hatch-klassifizieren)
12. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
13. [Gotchas](#gotchas)
14. [Verwandte Recipes](#verwandte-recipes)

---

## Hatch vs. Fill — Begriffsklärung

Diese Unterscheidung ist zentral — Verwechslung führt zu falschen Tool-Calls.

| Begriff | Was es ist | Archicad-Entsprechung |
|---------|------------|----------------------|
| **Hatch** | Ein konkretes 2D-Element im Grundriss mit eigener Geometrie (Polygon), eigenem Layer, eigenen Pen-Indices und einem Verweis auf ein Fill-Pattern | Tool-Familie `elements_*`, `elementType: "Hatch"` |
| **Fill** | Ein Attribut (Muster-Vorlage), das beschreibt, wie eine Schraffur aussieht — Linienabstand, Winkel, Vektorgeometrie | Tool-Familie `attributes_*`, `attributeType: "Fill"` |

**Analogie:** Fill verhält sich zu Hatch wie ein Linienstil zu einer einzelnen Linie — das Attribut
ist das Muster, das Element ist die konkrete Instanz mit Position und Pens.

Ein Hatch-Element **verweist** per `fillAttributeIndex` (Integer-Index) auf das Fill-Attribut. Wenn man das
Fill-Attribut ändert (oder löscht), ändert sich das Aussehen **aller** Hatch-Elemente, die es verwenden.
Wenn man ein Hatch-Element ändert, betrifft das nur dieses eine Element.

---

## Umfang

- Alle Hatches einer Story oder des gesamten Projekts auflisten.
- Fill-Pattern-Attribute (Muster-Vorlagen) auflisten — für Index-Lookup und Namens-Auflösung.
- Hatch-Eigenschaften lesen: Fill-Pattern, Vordergrund-Pen, Hintergrund-Pen, Layer, Polygon-Geometrie.
- Hatch modifizieren: Fill-Pattern wechseln, Pen-Indices ändern, Layer ändern.
- Hatch löschen.
- Hatch klassifizieren (im aktiven Klassifikations-System).

Nicht abgedeckt: Hatch erstellen (MCP-Lücke, kein Create-Tool in v29).

---

## Erforderlicher Warm-up-Kontext

Details in `../reference/workflow-context.md`.

| Feld | Warum es hier wichtig ist |
|------|---------------------------|
| **Port** | Jeder MCP-Call braucht `"port"`. |
| **Aktive Story** (floorIndex) | Filter `OnActualFloor` greift per Default; bei anderen Geschossen explizit angeben. |
| **Sichtbare Layer** | SAFE-03: Create ist frei, aber Hatches auf nicht sichtbaren Layern fehlen im Grundriss visuell. Layer-Zugehörigkeit immer prüfen. |
| **Pen-Set** | Pen-Indices in Hatch-Settings beziehen sich auf das aktive Pen-Set. Bei Pen-Wechsel kann sich das Aussehen ändern. |
| **Klassifikations-System** | Nur bei Klassifizierungs-Auftrag — System-GUID ist Pflicht für Classify-Calls. |

---

## Discovery-Anker

| Operation | Discovery-Query | Verifizierter Tool-Name |
|-----------|-----------------|------------------------|
| Hatches listen | `"get elements by type hatch"` | `elements_get_elements_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Fill-Attribute listen | `"get fill attributes by type"` | `attributes_get_attributes_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Element-Typ prüfen | `"get type of element"` | `elements_get_types_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` <!-- 2026-05-21 verifiziert AC29 --> |
| Property-Werte holen | `"get property values of elements"` | `properties_get_property_values_of_elements` <!-- 2026-05-21 verifiziert AC29 --> |
| Fill-Details lesen | `"get fill attributes"` | `attributes_get_fill_attributes` <!-- 2026-05-21 verifiziert AC29 --> |
| Bounding Box (2D) | `"get 2d bounding boxes of elements"` | `elements_get2_d_bounding_boxes` <!-- 2026-05-21 verifiziert AC29 — Tool-Name OHNE Unterstrich zwischen "get" und "2" --> |
| Hatch-typeSpecific modifizieren | (nicht möglich) | — <!-- 2026-05-21 verifiziert AC29: elements_set_details_of_elements.typeSpecificDetails ist hart auf WallSettings festgenagelt. Hatch-spezifische Felder NICHT änderbar. --> |
| Hatch löschen | `"delete elements"` | `elements_delete_elements` <!-- 2026-05-21 verifiziert AC29 --> |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` <!-- 2026-05-19 verifiziert AC29; bekannter Pydantic-Bug (siehe Memory) --> |
| Klassifikation setzen | `"set classifications of elements"` | `elements_set_classifications_of_elements` <!-- 2026-05-21 verifiziert AC29 --> |
| Klassifikations-Systeme | `"get all classification systems"` | `classifications_get_all_classification_systems` <!-- 2026-05-19 verifiziert AC29 --> |

---

## Typische Parameter

### Hatch-spezifische Parameter — NICHT via `elements_set_details_of_elements` änderbar

<!-- 2026-05-21 verifiziert AC29 via Discovery-Schema-Inspektion -->

**Befund:** `elements_set_details_of_elements.params.elementsWithDetails[].details.typeSpecificDetails` ist im MCP v29-Server hart auf `WallSettings` festgenagelt (nur Wand-Geometrie-Felder). Es gibt KEIN HatchSettings im Schema. Konsequenz:

- `fillAttributeIndex`, `foregroundPenIndex`, `backgroundPenIndex`, `contourPenIndex`, `angle`, `offsetX`, `offsetY`, `polygon` einer Hatch sind **NICHT via MCP änderbar**.
- Hatch-Modifikation = UI-Workflow (siehe Worked Example „Hatch modifizieren" unten).

**Was via `set_details` AUF Hatches GEHT** (Top-Level-Details-Felder):

| Feld | Typ | Beschreibung |
|------|-----|-------------|
| `floorIndex` | Number | Hatch zwischen Stories verschieben. |
| `layerIndex` | Number | Hatch auf anderen Layer verschieben. |
| `drawIndex` | Number | Zeichnungs-Reihenfolge (vorne/hinten). |

Lesend (für Confirm-Dialog-Anzeige) sind die Hatch-spezifischen Werte über das Property-System (`properties_get_property_values_of_elements`) zugänglich — siehe Worked Example „Hatch-Eigenschaften lesen".

### Fill-Attribut-Listing-Parameter

```
{
  "port": <port>,
  "params": {
    "attributeType": "Fill"
  }
}
```

Rückgabe: `{attributes: [{attributeId: {guid}, index, name}], next_page_token?}`.
Der `index` (Integer) entspricht dem `fillAttributeIndex` im HatchSettings.

---

## Worked Example — Alle Hatches einer Story lesen

Wir listen alle Hatch-Elemente der aktiven Story. <!-- 2026-05-21 verifiziert AC29 — elements_get_elements_by_type unterstützt "Hatch" als ElementType -->

```
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {
      "elementType": "Hatch",
      "filter": "OnActualFloor"
    }
  }
)
```

Response-Format:

```json
{
  "elements": [
    {"elementId": {"guid": "a1b2c3d4-..."}},
    {"elementId": {"guid": "e5f6a7b8-..."}}
  ]
}
```

**Paginierung beachten:** Wenn `next_page_token` in der Response vorkommt, erneut aufrufen mit
demselben Tool-Namen + `page_token`-Argument bis kein Token mehr. Erst dann vollständige
Ergebnismenge verwenden. Grundregel: niemals auf einem partiellen Seiten-Ergebnis operieren.

**Alle Geschosse:** Filter `"OnActualFloor"` weglassen oder `"All"` setzen, um
projektweite Hatches zu listen.

---

## Worked Example — Fill-Pattern-Attribute auflisten

Wir listen alle verfügbaren Fill-Pattern-Attribute des Projekts. Dieser Call ist nötig, wenn man den
`fillAttributeIndex` für ein bestimmtes Muster (z. B. „Beton", „Mauerwerk") herausfinden will.
<!-- 2026-05-20 schema-only — attributes_get_attributes_by_type mit Fill nicht live gegen Hatch getestet,
aber der Tool-Call für andere Attributtypen verifiziert 2026-05-19 AC29 -->

**Schritt 1 — Fill-Liste holen (erste Seite):**

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {
      "attributeType": "Fill"
    }
  }
)
```

Response:

```json
{
  "attributes": [
    {"attributeId": {"guid": "fill-guid-1"}, "index": 1, "name": "Vollton"},
    {"attributeId": {"guid": "fill-guid-2"}, "index": 2, "name": "Linie 45°"},
    {"attributeId": {"guid": "fill-guid-3"}, "index": 3, "name": "Beton"},
    {"attributeId": {"guid": "fill-guid-4"}, "index": 4, "name": "Mauerwerk"}
  ],
  "next_page_token": "eyJwYWdl..."
}
```

**Schritt 2 — Paginierung vollständig durchlaufen** <!-- 2026-05-21 verifiziert AC29 — page_token ist Top-Level-Argument, NICHT in params -->:

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "Fill"},
    "page_token": "eyJwYWdl..."
  }
)
```

Wiederholen bis kein `next_page_token` mehr zurückkommt. Dann aus der vollständigen Liste
den gewünschten `index` extrahieren. Der `index` aus der Listing-Response ist der `fillAttributeIndex`
für HatchSettings — kein separater Look-up nötig.

---

## Worked Example — Hatch-Eigenschaften lesen

Wir lesen die Eigenschaften eines bekannten Hatch-Elements. Wie bei Wänden empfehlen wir
das Property-System statt `elements_get_details_of_elements` — letzteres ist in AC29
durch den Pydantic-Bug unzuverlässig (siehe Memory `issue_archicad_mcp_get_details_bug`).
Property-System ist auch der einzige Weg, Hatch-Spezifika (Fill, Pen, Winkel) zu lesen,
da das Detail-Schema keinen HatchSettings-Variante hat.

**Schritt 1 — Property-IDs des Elements holen** <!-- 2026-05-21 verifiziert AC29 -->:

```
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}]
    }
  }
)
```

Response: `{"propertyIds": [{"propertyId": {"guid": "prop-guid-1"}}, ...]}`.
Typische Hatch-Properties: `FillAttribute`, `ForegroundPen`, `BackgroundPen`, `Layer`,
`DrawingOrder`.

**Schritt 2 — Werte für relevante Properties holen** <!-- 2026-05-21 verifiziert AC29 -->:

```
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}],
      "properties": [
        {"propertyId": {"guid": "prop-guid-1"}},
        {"propertyId": {"guid": "prop-guid-2"}}
      ]
    }
  }
)
```

**Alternative — Typ und räumliche Ausdehnung ohne Property-System:**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_types_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — liefert {"typesOfElements": [{"type": "Hatch"}]} -->

Räumliche Ausdehnung via Bounding-Box <!-- 2026-05-21 verifiziert AC29 — elements_get2_d_bounding_boxes (siehe Discovery-Anker zur Underscore-Konvention) -->:

```
mcp__archicad__archicad_call_tool(
  name="elements_get2_d_bounding_boxes",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}]
    }
  }
)
```

Gibt `xMin`, `yMin`, `xMax`, `yMax` in Metern zurück. Nützlich für Layer-Zuordnung
und Überlappungs-Checks.

---

## Hatch modifizieren — typeSpecific-Felder NICHT via MCP änderbar

<!-- 2026-05-21 verifiziert AC29 via Discovery-Schema-Inspektion -->

**Befund:** Fill-Pattern, Pen-Indices (Vorder-/Hinter-/Konturgrund), Winkel, Offset, Polygon einer Hatch sind im MCP v29 NICHT via `elements_set_details_of_elements` änderbar — typeSpecificDetails ist hart auf WallSettings festgenagelt.

### Was via MCP geht (Top-Level-Details)

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "a1b2c3d4-..."},
        "details": {
          "floorIndex": 1,
          "layerIndex": 7.0,
          "drawIndex": 3.0
        }
      }]
    }
  }
)
```

Damit kann die Hatch auf eine andere Story (`floorIndex`), einen anderen Layer (`layerIndex`) oder eine andere Zeichnungs-Reihenfolge (`drawIndex`, 1-basiert) verschoben werden. **NICHT** möglich: Fill-Pattern wechseln, Pen ändern, Polygon neu zeichnen, Winkel drehen.

### Workaround — User-UI

```
Bitte in Archicad öffnen:
  1. Hatch a1b2c3d4-... selektieren (oder per Bearbeiten → Suchen+Auswählen → nach Layer)
  2. Strg/Cmd+T → Schraffur-Einstellungen
  3. Fill-Pattern: "Linie 45°" → "Beton"
     Vordergrund-Pen: 2 → 3
  4. OK

(MCP v29 hat keinen programmatischen Endpoint für Hatch-typeSpecific-Modify.)
```

### Workaround — Fill-Definition-Wechsel (Kaskaden-Effekt)

Wenn ALLE Hatches, die ein bestimmtes Fill-Attribut (z.B. „Linie 45°", Index 2) verwenden, ein neues Muster bekommen sollen: das Fill-Attribut selbst umdefinieren — kaskadiert auf alle Hatches automatisch. Achtung: das Tool dafür war in Discovery nicht eindeutig auffindbar (`attributes_create_fills` ist NICHT im Schema). Fill-Attribut-Verwaltung ist via MCP v29 LESEND (`attributes_get_fill_attributes`), aber nicht SCHREIBEND verfügbar. Workaround = UI.

### Bulk-Hatch-Modifikation — nicht via MCP

Wegen der oben genannten Limit ist Bulk-Modify mehrerer Hatches via MCP nicht möglich. Workflow ist:
1. MCP: Hatch-Liste (`elements_get_elements_by_type` mit `"Hatch"`) + Filter via Property-Werte
2. MCP: User die GUID-Liste + aktuelle Werte zur Selektion zeigen
3. UI: User selektiert in Archicad + Strg/Cmd+T → Bulk-Edit im Settings-Dialog
4. MCP: Verifikation via `properties_get_property_values_of_elements`

---

## Worked Example — Hatch löschen

Hatches haben keine Hosted-Elemente (kein Fenster, keine Tür hängt an einer Schraffur) —
daher kein SAFE-04-Pre-Check nötig. SAFE-01 greift trotzdem.
<!-- 2026-05-21 verifiziert AC29 — elements_delete_elements unterstützt alle Element-Typen einheitlich, kein Hatch-spezifischer Vorbehalt -->

**Confirm (SAFE-01):**

```
Ich werde folgendes löschen:
- Hatch a1b2c3d4-e5f6-7890-abcd-ef1234567890  (Story 0, Layer: Z_Grundriss-Schraffur, Fill: Beton)

Ausführen? (ja / nein / details / abbrechen)
```

**Delete nach ausdrücklichem `ja`:**

```
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}]
    }
  }
)
```

<!-- 2026-05-21 verifiziert AC29 — elements_delete_elements ist element-typ-agnostisch -->

**Bulk-Delete:** Mehrere Element-IDs in `elements[]` — einziger Call, kein Loop. Confirm zeigt
pro Element eine Zeile (1–10) oder Summary (> 10). Keine Obergrenze für Batch-Größe.

---

## Worked Example — Hatch klassifizieren

Wir setzen für ein Hatch-Element eine Klassifikation im Projekt-Klassifikations-System.
Das Pattern ist identisch zu Wänden — Hatches sind klassifizierbare Elemente.
<!-- 2026-05-21 verifiziert AC29 — elements_set_classifications_of_elements ist element-typ-agnostisch; Trust-but-verify via reverse-lookup wird empfohlen (siehe bulk-operations.md § success ist nicht success) -->

**WICHTIG: Trust-but-verify.** Wie in Phase 5 dokumentiert (siehe `bulk-operations.md`), gibt `elements_set_classifications_of_elements` `success:true` zurück, auch wenn die Klassifikation nicht tatsächlich greift (silent rejection bei manchen Element-Typen). Nach jedem Set: via `elements_get_elements_by_classification` mit der gesetzten Klasse die tatsächliche Adoption zählen.

**Schritt 1 — Klassifikations-System-GUID ermitteln (falls nicht bekannt):**

```
mcp__archicad__archicad_call_tool(
  name="classifications_get_all_classification_systems",
  arguments={"port": 19723, "params": {}}
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Response enthält `{"classificationSystemId": {"guid": "<system-guid>"}, "name": "SAB_Klassifizierung_29", ...}`.
System-GUIDs sind projekt-spezifisch — nie hartkodieren.

**Schritt 2 — Aktuelle Klassifikation des Hatch-Elements lesen:**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}],
      "classificationSystemIds": [{"classificationSystemId": {"guid": "<system-guid>"}}]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Response zeigt aktuelle Klasse oder `null` — Ist-Zustand für den Confirm.

**Schritt 3 — Zielklassen-GUID holen (falls nicht bekannt)** <!-- 2026-05-21 verifiziert AC29 -->:

```
mcp__archicad__archicad_call_tool(
  name="classifications_get_all_classifications_in_system",
  arguments={
    "port": 19723,
    "params": {"classificationSystemId": {"guid": "<system-guid>"}}
  }
)
```

Gibt den vollständigen Klassen-Baum mit GUIDs zurück. Daraus Klartext → GUID mapping ableiten.

**Schritt 4 — Confirm (SAFE-01):**

```
Ich werde folgendes klassifizieren:
- Hatch a1b2c3d4-e5f6-7890-abcd-ef1234567890
  Klassifikation (SAB_Klassifizierung_29): <leer> → "2D-Schraffur"

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 5 — Klassifikation setzen nach Bestätigung** <!-- 2026-05-21 verifiziert AC29 — Schema bestätigt: params.elementClassifications[].classificationId.classificationSystemId + .classificationItemId -->:

```
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementClassifications": [{
        "elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"},
        "classificationId": {
          "classificationSystemId": {"guid": "<system-guid>"},
          "classificationItemId": {"guid": "<klassen-guid>"}
        }
      }]
    }
  }
)
```

`<klassen-guid>` = GUID der konkreten Zielklasse (nicht die System-GUID). Beide GUIDs stammen aus den vorherigen Schritten — nie raten.

**Schema-Hinweis** <!-- 2026-05-21 verifiziert AC29 -->: Param-Wrapper heißt `elementClassifications` (NICHT `elementsWithClassifications`), und der Klassifikations-Block heißt `classificationId` als Objekt mit zwei verschachtelten Feldern (NICHT ein Array `classifications`).

---

## Bulk-Klassifizierung (Stub)

Bulk-Klassifizierung aller Hatch-Elemente nach 2D-Kategorie ist ein seltener, aber
möglicher Workflow. Das universelle Pattern steht in `../reference/bulk-operations.md`.

Kurzfassung für Hatches: `elements_get_elements_by_type` mit `elementType: "Hatch"` (paginiert)
→ Fill-Pattern und Layer-Index pro Element via Property-System lesen → nach Muster/Layer-Klasse
gruppieren → Confirm (Summary-Format für > 10 Elemente) →
`elements_set_classifications_of_elements` im Block.

**Hinweis:** Hatches tauchen selten in Bulk-Klassifizierungs-Workflows auf (anders als Wände oder
Zonen). Wenn der User „alle Schraffuren klassifizieren" sagt, zuerst klären, ob er Hatch-Elemente
oder Fill-Attribute meint — häufige Verwechslung.

---

## Gotchas

1. **Hatch ≠ Fill.** `elementType: "Hatch"` und `attributeType: "Fill"` sind völlig verschiedene
   Konzepte und Tool-Familien. Wer `elements_get_elements_by_type` mit `"Fill"` aufruft,
   bekommt keinen Treffer (kein solcher ElementType). Wer `attributes_get_attributes_by_type`
   mit `"Hatch"` aufruft, erhält einen Fehler (kein solcher AttributeType). Immer die richtige
   Seite des Paares wählen. Faustregel: Element-Operationen = `elements_*`, Muster-Listing = `attributes_*`.

2. **Hatch-Erstellung nicht via MCP v29.** Kein `elements_create_hatches`-Tool im Server.
   User zeichnet die Schraffur manuell in Archicad; wir holen dann die GUID per
   `elements_get_elements_by_type` (neueste Elemente zuerst oder nach Layer-Filter) und
   arbeiten damit weiter. Vollständige Capability-Tabelle in `../reference/mcp-conventions.md`.

3. **`backgroundPenIndex: 0` bedeutet transparent.** Pen-Index `0` ist kein gültiger Stift,
   sondern der Spezialwert für „kein Hintergrund" (Hintergrund transparent = durchsichtig).
   `backgroundPenIndex: 1` würde den Hintergrund mit Pen 1 (typisch: Weiß oder Schwarz) füllen.
   Pen-Semantik hängt am aktiven Pen-Set — immer mit `attributes_get_pen_table_attributes` prüfen,
   welche Farbe und Gewicht ein Index hat, bevor man ihn setzt.

4. **Vordergrund-Pen vs. Hintergrund-Pen vs. Kontur-Pen.** Hatch-Elemente haben typischerweise
   drei Pen-Indices: `foregroundPenIndex` (die Schraffurlinien selbst), `backgroundPenIndex`
   (die Füllfarbe hinter den Linien), `contourPenIndex` (die Randlinie des Hatch-Polygons).
   Nur einen ändern und die anderen vergessen → unerwartete visuelle Ergebnisse. Im Confirm-Dialog
   alle drei aktuellen Werte zeigen, nicht nur den geänderten.

5. **Hatches sind layer-gebunden — Sichtbarkeit.** Ein Hatch-Element auf einem nicht sichtbaren
   Layer ist im Grundriss nicht zu sehen, auch wenn es korrekt erstellt und klassifiziert ist.
   Vor einem Modify-Aufruf den Layer prüfen (Warm-up Feld 5). SAFE-03 greift: Wenn der
   Ziel-Layer der Modifikation nicht sichtbar ist, erst konfirmieren. Typisches Z_/A_-Layer-Namensschema
   im User-Setup: `Z_Grundriss-Schraffur`, `A_Schraffur-Bestand` etc. — Layer-Namen aus dem Projekt
   holen, nie raten.

6. **Fill-Attribut löschen vs. Hatch-Element löschen.** Wenn ein Fill-Attribut (Muster-Vorlage)
   gelöscht wird, verlieren alle Hatch-Elemente, die es verwenden, ihr Pattern — Archicad
   substituiert automatisch mit dem Default-Fill. Das betrifft potenziell viele Elemente ohne
   expliziten Confirm. Fills sind Attribute, keine Elemente — ihr Löschen ist über `attributes_*`
   nicht `elements_*`. Dieses Recipe deckt nur das Löschen von **Hatch-Elementen** ab.
   Fill-Attribut-Verwaltung gehört zu `recipes/surfaces-materials.md` (Attribute-Sektion).

7. **Modal-Dialog-Warnung bei längeren Bulk-Operationen.** Vor Bulk-Modify oder Bulk-Delete
   bei Hatch-Elementen: User erinnern, offene Dialoge in Archicad zu schließen. Ein offener
   Element-Settings-Dialog für einen Hatch freezt den MCP-Server (Code 4001). Symptom:
   Tool-Call hängt > 10 Sekunden. Reaktion: nicht retrying, User informieren. Details in
   `../reference/mcp-conventions.md` § Modal-Dialoge.

8. **Polygon-Self-Intersection.** Wenn ein Hatch-Element via `elements_set_details_of_elements`
   mit einem neuen Polygon ausgestattet wird, und das Polygon sich selbst schneidet (Butterfly-Form,
   überkreuzende Kanten), kann Archicad das Element zurückweisen oder ein visuell verzerrtes
   Ergebnis erzeugen. Polygon-Koordinaten vor dem Update-Call auf Kreuzungsfreiheit prüfen.
   Einfachste Prüfung: alle Kanten-Paare auf Kreuzung testen, bevor der MCP-Call abgesetzt wird.

---

## Verwandte Recipes

- [`lines-polylines.md`](lines-polylines.md) — 2D-Linien-Elemente; ähnliches Layer/Pen-Modell.
- [`surfaces-materials.md`](surfaces-materials.md) — Building Materials, Composites, Fill-Attribut-Verwaltung (Muster-Vorlagen erstellen/löschen).
- [`wall-operations.md`](wall-operations.md) — Stil-Referenz für No-Create-Recipes; identisches Confirm- und Update-Pattern.
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Confirm-Format, Capability-Tabelle, Fehlerklassen, Modal-Dialog-Warnung.
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Bulk-Klassifizierungs-Pattern (Read → Filter → Group → Confirm → Apply).
- [`../reference/workflow-context.md`](../reference/workflow-context.md) — Warm-up-Felder im Detail, insb. Feld 5 (Layer) + Feld 6 (Pen-Set).
