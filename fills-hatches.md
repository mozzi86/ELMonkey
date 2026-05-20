# 2D-Schraffuren (Hatch-Elemente)

Dieses Recipe deckt Lesen, Modifizieren, Löschen und Klassifizieren eigenständiger
2D-Schraffur-Elemente (Hatch) ab — sowie das Auflisten der zugehörigen Fill-Pattern-Attribute.

**Hatch-Erstellung ist im Archicad-MCP v29 nicht verfügbar.** Kein `elements_create_hatches`-Tool
im Server. User zeichnet Schraffuren manuell in Archicad; wir arbeiten dann mit diesen Elementen
weiter. Vollständige Capability-Tabelle: `../reference/mcp-conventions.md` §
Live-verifizierte Element-Create-Capabilities.

<!-- 2026-05-20 schema-only — kein Live-Test möglich (Archicad-Instanz in dieser Session nicht erreichbar) -->

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
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` <!-- VERIFY --> |
| Property-Werte holen | `"get property values of elements"` | `properties_get_property_values_of_elements` <!-- VERIFY --> |
| Bounding Box | `"get 2d bounding boxes of elements"` | `elements_get_2_d_bounding_boxes` <!-- VERIFY --> |
| Hatch modifizieren | `"set details of elements"` | `elements_set_details_of_elements` <!-- VERIFY --> |
| Hatch löschen | `"delete elements"` | `elements_delete_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Klassifikation setzen | `"set classification of elements"` | `elements_set_classifications_of_elements` <!-- VERIFY --> |
| Klassifikations-Systeme | `"get all classification systems"` | `classifications_get_all_classification_systems` <!-- 2026-05-19 verifiziert AC29 --> |

---

## Typische Parameter

### HatchSettings (für `typeSpecificDetails` in `elements_set_details_of_elements`)

Alle Felder optional — nur die zu ändernden angeben. <!-- VERIFY — Schema-only, nicht live getestet -->

| Feld | Typ | Beschreibung |
|------|-----|-------------|
| `fillAttributeIndex` | `integer` | 1-basierter Index des Fill-Attributs (aus `attributes_get_attributes_by_type` mit `Fill`). |
| `foregroundPenIndex` | `integer` | Pen-Index für die Schraffurlinien (Vordergrund). Aus dem aktiven Pen-Set. |
| `backgroundPenIndex` | `integer` | Pen-Index für den Schraffurhintergrund. `0` = transparent/kein Hintergrund. |
| `contourPenIndex` | `integer` | Pen-Index für die Konturlinie des Hatch-Polygons. |
| `layerIndex` | `number` (float) | 1-basierter Layer-Index (Float, analog zu WallSettings). |
| `angle` | `number` (Rad) | Rotationswinkel der Schraffur in Radiant. |
| `offsetX` | `number` (m) | X-Offset des Fill-Musters (Ursprungs-Verschiebung). |
| `offsetY` | `number` (m) | Y-Offset des Fill-Musters. |
| `polygon` | Polygon-Objekt | Umriss-Polygon des Hatch-Elements (Coord2D-Array). |

**Details-Rahmen:** `{"floorIndex": 0, "layerIndex": 3.0, "drawIndex": 1.0, "typeSpecificDetails": {...}}`.
`layerIndex` ist ein Float (1-basiert, nicht 0-basiert) — wie bei WallSettings.

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

Wir listen alle Hatch-Elemente der aktiven Story. <!-- VERIFY -->

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

**Schritt 2 — Paginierung vollständig durchlaufen:**

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {
      "attributeType": "Fill",
      "page_token": "eyJwYWdl..."
    }
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
durch den Pydantic-Bug unzuverlässig. <!-- VERIFY — HatchDetails-Schema nicht live getestet -->

**Schritt 1 — Property-IDs des Elements holen:** <!-- VERIFY -->

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

**Schritt 2 — Werte für relevante Properties holen:** <!-- VERIFY -->

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

Räumliche Ausdehnung via Bounding-Box: <!-- VERIFY -->

```
mcp__archicad__archicad_call_tool(
  name="elements_get_2_d_bounding_boxes",
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

## Worked Example — Hatch modifizieren (Fill-Pattern + Pen ändern)

Wir wechseln das Fill-Pattern von „Linie 45°" (index 2) auf „Beton" (index 3) und setzen
den Vordergrund-Pen auf 3. Gemäß SAFE-01 zeigen wir zuerst den Confirm-Dialog.
<!-- VERIFY — HatchSettings-Schema nicht live getestet -->

**Confirm (SAFE-01) — vor dem Aufruf:**

```
Ich werde folgendes ändern:
- Hatch a1b2c3d4-e5f6-7890-abcd-ef1234567890  (Story 0, Layer: Z_Grundriss-Schraffur)
  Fill-Pattern: "Linie 45°" (index 2) → "Beton" (index 3)
  Vordergrund-Pen: 2 → 3

Ausführen? (ja / nein / details / abbrechen)
```

**Aufruf nach ausdrücklichem `ja`:**

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"},
        "details": {
          "typeSpecificDetails": {
            "fillAttributeIndex": 3,
            "foregroundPenIndex": 3
          }
        }
      }]
    }
  }
)
```

Response: `{"editedElements": [{"elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"}}]}`.

Die GUID bleibt nach dem Update stabil (SAFE-05) — für Folge-Operationen dieselbe ID weiterverwenden.

**Bulk-Muster (mehrere Hatches gleichzeitig):**

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "a1b2c3d4-..."},
          "details": {"typeSpecificDetails": {"fillAttributeIndex": 3, "foregroundPenIndex": 3}}
        },
        {
          "elementId": {"guid": "e5f6a7b8-..."},
          "details": {"typeSpecificDetails": {"fillAttributeIndex": 3, "foregroundPenIndex": 3}}
        }
      ]
    }
  }
)
```

Bei > 10 Elementen: Summary-Confirm-Format aus `../reference/mcp-conventions.md`
§ Confirm-Format für > 10 Elemente verwenden.

---

## Worked Example — Hatch löschen

Hatches haben keine Hosted-Elemente (kein Fenster, keine Tür hängt an einer Schraffur) —
daher kein SAFE-04-Pre-Check nötig. SAFE-01 greift trotzdem.
<!-- VERIFY — elements_delete_elements für Hatch-Elemente nicht live getestet -->

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

<!-- 2026-05-19 verifiziert AC29 — elements_delete_elements (auf Wänden getestet; für Hatches VERIFY) -->

**Bulk-Delete:** Mehrere Element-IDs in `elements[]` — einziger Call, kein Loop. Confirm zeigt
pro Element eine Zeile (1–10) oder Summary (> 10). Keine Obergrenze für Batch-Größe.

---

## Worked Example — Hatch klassifizieren

Wir setzen für ein Hatch-Element eine Klassifikation im Projekt-Klassifikations-System.
Das Pattern ist identisch zu Wänden — Hatches sind klassifizierbare Elemente.
<!-- VERIFY — elements_set_classifications_of_elements für Hatch nicht live getestet -->

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

**Schritt 3 — Zielklassen-GUID holen (falls nicht bekannt):** <!-- VERIFY -->

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

**Schritt 5 — Klassifikation setzen nach Bestätigung:** <!-- VERIFY -->

```
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithClassifications": [{
        "elementId": {"guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"},
        "classifications": [{
          "classificationSystemId": {"guid": "<system-guid>"},
          "classificationItemId": {"guid": "<klassen-guid>"}
        }]
      }]
    }
  }
)
```

`<klassen-guid>` = GUID der konkreten Zielklasse (nicht die System-GUID).
Beide GUIDs stammen aus den vorherigen Schritten — nie raten.

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
