# Öffnungen — Fenster, Türen, Wandöffnungen

Dieses Recipe deckt das Lesen, Modifizieren, Löschen und Klassifizieren von Öffnungen ab. **Öffnungs-Erstellung ist im Archicad-MCP v29 nicht verfügbar** — Fenster und Türen werden manuell in eine existierende Wand gezeichnet. Wandöffnungen (Opening) ebenso. Siehe [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Live-verifizierte Element-Create-Capabilities.

Öffnungen sind immer **gehostete Elemente**: sie existieren nur innerhalb einer Host-Wand. Diese Abhängigkeit zieht sich durch alle Operationen — Discovery, Read, Update, Delete — und ist der zentrale Unterschied zu freistehenden Elementen wie Stützen oder Zonen.

<!-- 2026-05-19 verifiziert AC29 -->

---

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [Discovery-Anker](#discovery-anker)
4. [Typische Parameter](#typische-parameter)
5. [Worked Example — Öffnung lesen (mit Host-Wand-Identifikation)](#worked-example--öffnung-lesen-mit-host-wand-identifikation)
6. [Worked Example — Sill-Höhe ändern (GDL-Parameter-Update)](#worked-example--sill-höhe-ändern-gdl-parameter-update)
7. [Worked Example — Öffnung löschen](#worked-example--öffnung-löschen)
8. [Worked Example — Öffnung klassifizieren (Tür / Fenster)](#worked-example--öffnung-klassifizieren-tür--fenster)
9. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
10. [Gotchas](#gotchas)
11. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang

**Abgedeckt:**

- Alle Öffnungen einer bekannten Host-Wand lesen (Windows, Doors, Openings).
- Öffnung über den Element-Typ identifizieren und ihre GDL-Parameter auslesen.
- Eigenschaften modifizieren: Sill-Höhe, Breite, Höhe, Türschwenk-Richtung (alle via GDL-Parameter-Update).
- Öffnung löschen (mit SAFE-01-Confirm, ohne Hosted-Element-Pre-Check — Öffnungen hosten nichts).
- Öffnung klassifizieren: Tür / Fenster / Wandöffnung im aktiven Klassifikations-System.

**Nicht abgedeckt:**

- Öffnung erstellen (kein Create-Tool im MCP v29).
- Host-Wand einer Öffnung wechseln (Delete + manuell neu zeichnen).
- Sub-Library-Items innerhalb komplexer Tür-Objekte modifizieren.

---

## Erforderlicher Warm-up-Kontext

Vor dem ersten Öffnungs-Auftrag folgende Felder holen (Details in [`../reference/workflow-context.md`](../reference/workflow-context.md)):

| Feld | Warum bei Öffnungen wichtig |
|---|---|
| **Port** | Jeder MCP-Call braucht ihn. |
| **Aktive Story / floorIndex** | `elements_get_elements_by_type` filtert per `OnActualFloor`. |
| **Sichtbare Layer** | SAFE-03: Layer prüfen, bevor wir die Öffnung modifizieren. |
| **Klassifikations-System-GUID** | Für Classify-Operationen. Aktives System: `SAB_Klassifizierung_29`. |
| **Längeneinheit** | Sill-Höhe wird in Metern übergeben — bei mm-Projekten Einheit beachten. |

---

## Discovery-Anker

Diese Tool-Namen wurden live gegen Archicad 29 / Port 19723 verifiziert. Falls ein Name in einer anderen AC-Version nicht gefunden wird, Discovery mit Synonym-Query erneut ausführen (Anleitung in [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Discovery-Pattern im Detail).

| Operation | Discovery-Query | Verifizierter Tool-Name |
|---|---|---|
| Öffnungen einer Wand listen | `"get connected elements of wall"` | `mcp__archicad__archicad_call_tool` → `elements_get_connected_elements` <!-- 2026-05-19 verifiziert --> |
| Alle Fenster des Projekts / Stories | `"get elements by type window"` | `mcp__archicad__archicad_call_tool` → `elements_get_elements_by_type` <!-- 2026-05-19 verifiziert --> |
| Element-Typ bestätigen | `"get type of element"` | `mcp__archicad__archicad_call_tool` → `elements_get_types_of_elements` <!-- 2026-05-19 verifiziert --> |
| GDL-Parameter lesen | `"get gdl parameters of elements"` | `mcp__archicad__archicad_call_tool` → `elements_get_gdl_parameters_of_elements` <!-- 2026-05-19 verifiziert --> |
| GDL-Parameter schreiben | `"set gdl parameters of elements"` | `mcp__archicad__archicad_call_tool` → `elements_set_gdl_parameters_of_elements` <!-- VERIFY --> |
| Properties lesen | `"get property values of elements"` | `mcp__archicad__archicad_call_tool` → `properties_get_property_values_of_elements` <!-- 2026-05-19 verifiziert --> |
| Öffnung löschen | `"delete elements"` | `mcp__archicad__archicad_call_tool` → `elements_delete_elements` <!-- 2026-05-19 verifiziert --> |
| Klassifikation setzen | `"set classification of elements"` | `mcp__archicad__archicad_call_tool` → `elements_set_classifications_of_elements` <!-- VERIFY --> |
| Klassifikation lesen | `"get classifications of elements"` | `mcp__archicad__archicad_call_tool` → `elements_get_classifications_of_elements` <!-- 2026-05-19 verifiziert --> |

---

## Typische Parameter

### Gemeinsame Felder (Window / Door / Opening)

| Parameter | Typ | Bedeutung |
|---|---|---|
| `elementId.guid` | String (UUID) | GUID des Öffnungs-Elements selbst |
| Host-Wand-GUID | String (UUID) | GUID der Wand, in der die Öffnung sitzt — nicht im Element direkt, aber via `elements_get_connected_elements` aus Wand-Perspektive erreichbar |
| `floorIndex` | Integer | Geschoss-Index (0 = EG) |
| Layer | via Property-System | Layer-Zugehörigkeit (modifizierbar über Properties) |

### GDL-Parameter-Schlüssel (Window — Standard-Library-Objekt)

Diese Parameter-Namen gelten für das Standard-Archicad-Fenster-Library-Objekt. Bei Dritthersteller-Bibliotheken können die Namen abweichen — daher immer zuerst `elements_get_gdl_parameters_of_elements` aufrufen und die tatsächlichen Parameter-Namen und Indices aus der Response lesen.

| GDL-Parametername (typisch) | Bedeutung | Einheit |
|---|---|---|
| `SILL` / `sill` | Brüstungshöhe (Sill-Höhe) relativ zur Wand-Unterkante | m |
| `WIDTH` / `wido_width` | Fenster-Breite | m |
| `HEIGHT` / `wido_height` | Fenster-Höhe | m |
| `FRAME_THICKNESS` | Rahmendicke | m |

### GDL-Parameter-Schlüssel (Door — Standard-Library-Objekt)

| GDL-Parametername (typisch) | Bedeutung |
|---|---|
| `WIDTH` / `wido_width` | Türbreite |
| `HEIGHT` / `wido_height` | Türhöhe |
| `DOOR_SWING` / `RevealDepth` | Türschwenk-Richtung / Anschlag |
| `SILL` | Türschwelle (oft 0.0 bei Innentüren) |

> **Wichtig:** Türschwenk-Richtung und Sill-Höhe sind GDL-Parameter des Library-Objekts, nicht `WallSettings`-ähnliche Details. Sie werden ausschließlich über `elements_set_gdl_parameters_of_elements` geändert, nicht über `elements_set_details_of_elements`.

---

## Worked Example — Öffnung lesen (mit Host-Wand-Identifikation)

Dieses Beispiel demonstriert **SAFE-05 Element-ID-Threading**: die Wand-GUID `f1101930-e0bd-7044-a1f2-fdb20e520e21` aus dem Test-Set wird als Einstiegspunkt verwendet, ihre gehosteten Fenster werden abgefragt, und die zurückgegebene Fenster-GUID `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` wird direkt in den Folge-Calls weitergeführt — ohne erneute Suche.

### Schritt 1 — Alle Fenster der Host-Wand abfragen

Wir fragen die Wand nach ihren gehosteten Window-Elementen. Der `connectedElementType`-Parameter kann auf `"Window"`, `"Door"` oder `"Opening"` gesetzt werden — jeweils ein separater Call für jeden Typ.

```
mcp__archicad__archicad_call_tool(
  name="elements_get_connected_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}
      ],
      "connectedElementType": "Window"
    }
  }
)
```

Erwartete Response (Beispiel):

```json
{
  "connectedElements": [
    {
      "elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"}
    }
  ]
}
```

Die zurückgegebene GUID `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` ist unser Test-Fenster. Diese GUID wird in Schritt 2 und 3 direkt weitergeführt (SAFE-05 — kein erneutes Suchen).

Für Türen in derselben Wand denselben Call mit `"connectedElementType": "Door"` wiederholen.

### Schritt 2 — Element-Typ bestätigen

Als schneller Plausibilitäts-Check (und da `elements_get_details_of_elements` in AC29 einen bekannten Bug hat — siehe Gotchas):

```
mcp__archicad__archicad_call_tool(
  name="elements_get_types_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"}}
      ]
    }
  }
)
```

Erwartete Response:

```json
{
  "typesOfElements": [
    {"type": "Window"}
  ]
}
```

### Schritt 3 — GDL-Parameter des Fensters lesen

Sill-Höhe und andere Library-Parameter liegen im GDL-Parameterraum, nicht im Wand-Detail-Schema:

```
mcp__archicad__archicad_call_tool(
  name="elements_get_gdl_parameters_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"}}
      ]
    }
  }
)
```

Erwartete Response (Auszug — tatsächliche Parameter-Namen und Indices hängen vom Library-Objekt ab):

```json
{
  "gdlParametersOfElements": [
    {
      "elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"},
      "gdlParameters": [
        {"index": "1", "name": "SILL", "type": "Length", "value": 0.9},
        {"index": "2", "name": "WIDTH", "type": "Length", "value": 1.2},
        {"index": "3", "name": "HEIGHT", "type": "Length", "value": 1.4}
      ]
    }
  ]
}
```

**Wichtig:** Den Parameter-Index (`"1"`, `"2"`, …) notieren — er wird beim Update-Call benötigt. Den Namen allein reicht nicht; der MCP-Server identifiziert den Parameter intern über den Index.

### Schritt 4 — Properties lesen (ergänzend, z. B. Layer)

Für nicht-GDL-Eigenschaften (Layer, Renovation-Status, benutzerdefinierte Properties) zuerst `properties_get_all_property_ids_of_elements` aufrufen, dann mit den zurückgegebenen IDs `properties_get_property_values_of_elements`. Beide Calls mit der Fenster-GUID `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` als `elementId`.
```

---

## Worked Example — Sill-Höhe ändern (GDL-Parameter-Update)

Die Sill-Höhe (Brüstungshöhe) des Test-Fensters `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` wird von 0.9 m auf 1.0 m angehoben.

**Voraussetzung:** GDL-Parameter wurden in Schritt 3 des Read-Beispiels bereits gelesen. Wir wissen: Parameter `SILL` hat Index `"1"`, Typ `"Length"`, aktueller Wert `0.9`.

### SAFE-01 — Confirm-Dialog

Bevor der MCP-Call abgesetzt wird, zeigen wir dem User:

```
Ich werde folgendes ändern:
- Fenster 7185f21a  „Fenster EG Süd"  SILL (Brüstungshöhe): 0.9 m → 1.0 m
  Host-Wand: f1101930-e0bd-7044-a1f2-fdb20e520e21

Ausführen? (ja / nein / details / abbrechen)
```

### Update-Call nach Bestätigung

```
mcp__archicad__archicad_call_tool(
  name="elements_set_gdl_parameters_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithGDLParameters": [
        {
          "elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"},
          "gdlParameters": [
            {
              "index": "1",
              "type": "Length",
              "value": 1.0
            }
          ]
        }
      ]
    }
  }
)
```

<!-- VERIFY — Schema aus SCHEMAS.md abgeleitet, nicht live getestet -->

Erwartete Response bei Erfolg:

```json
{
  "executionResult": {
    "succeeded": true
  }
}
```

### Türschwenk-Richtung ändern (Variante)

Das Prinzip ist identisch — zuerst per `elements_get_gdl_parameters_of_elements` den Index des `DOOR_SWING`-Parameters ermitteln, dann denselben `elements_set_gdl_parameters_of_elements`-Call mit dem neuen Wert absetzen. Der Wert ist typischerweise ein Integer-Enum (0 = links, 1 = rechts — Werte im Library-Objekt nachschlagen).

---

## Worked Example — Öffnung löschen

Das Test-Fenster `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` wird gelöscht.

**Warum kein Hosted-Element-Pre-Check?** Öffnungen (Fenster, Türen, Wandöffnungen) hosten selbst keine weiteren Elemente. SAFE-04 ist hier nicht anwendbar — wir brauchen keinen `elements_get_connected_elements`-Vorab-Call. Anders als bei Wänden (die Fenster und Türen hosten) oder Stützen gibt es keine transitive Abhängigkeit.

**Aber:** Als schreibende Operation (Delete) gilt SAFE-01 — wir zeigen den Confirm-Dialog.

### SAFE-01 — Confirm-Dialog

```
Ich werde folgendes löschen:
- Fenster 7185f21a  „Fenster EG Süd"
  Host-Wand: f1101930-e0bd-7044-a1f2-fdb20e520e21 (Wand bleibt erhalten)

Ausführen? (ja / nein / details / abbrechen)
```

> Hinweis: Die Host-Wand wird beim Löschen der Öffnung **nicht** mitgelöscht. Die Wandfläche schließt sich wieder. Das ist das erwartete Archicad-Verhalten.

### Delete-Call nach Bestätigung

```
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"}}
      ]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_delete_elements funktioniert -->

Erwartete Response:

```json
{
  "executionResult": {
    "succeeded": true
  }
}
```

**Nach dem Delete:** Die GUID `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` ist ungültig. Nicht mehr in Folge-Calls verwenden. Wenn die Öffnung neu gezeichnet wird, bekommt sie eine neue GUID.

---

## Worked Example — Öffnung klassifizieren (Tür / Fenster)

Das Test-Fenster `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` wird im Klassifikations-System `SAB_Klassifizierung_29` als „Fenster" klassifiziert.

### Schritt 1 — Aktuelle Klassifikation lesen

Zuerst prüfen, ob schon eine Klassifikation gesetzt ist:

```
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [
        {"elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"}}
      ],
      "classificationSystemIds": [
        {"classificationSystemId": {"guid": "<sab-system-guid>"}}
      ]
    }
  }
)
```

> Die GUID des aktiven Klassifikations-Systems `SAB_Klassifizierung_29` ist projektspezifisch — sie liegt im Memory unter `project_archicad_user_setup.md`, nicht im Skill. Beim Warm-up aus dem Projekt laden oder per `attributes_get_classification_systems` abfragen.

### Schritt 2 — Klassifikations-Item-GUID für „Fenster" ermitteln

Das Klassifikations-Item (z. B. „Fenster" als Klasse unter `SAB_Klassifizierung_29`) hat eine eigene GUID. Diese per `attributes_get_classification_items_of_classification_system` oder per Discovery ermitteln, wenn noch nicht bekannt.

### Schritt 3 — SAFE-01 Confirm-Dialog

```
Ich werde folgendes klassifizieren:
- Fenster 7185f21a  → SAB_Klassifizierung_29: „Fenster"
  (bisher: keine Klassifikation / anderer Wert)

Ausführen? (ja / nein / details / abbrechen)
```

### Schritt 4 — Klassifikation setzen

```
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementClassifications": [
        {
          "elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"},
          "classificationItemIds": [
            {"classificationItemId": {"guid": "<fenster-item-guid>"}}
          ]
        }
      ]
    }
  }
)
```

<!-- VERIFY — Schema aus Discovery abgeleitet -->

**Für Türen:** Dieselbe Sequenz mit einer anderen `elementId` (Tür-GUID) und dem Klassifikations-Item-GUID für „Tür". Das Muster ist identisch.

---

## Bulk-Klassifizierung (Stub)

Alle Fenster einer Story automatisch als „Fenster" und alle Türen als „Tür" klassifizieren — das vollständige Worked Example kommt in Phase 5. Das universelle Muster (Read → Filter → Group → Confirm → Apply) ist in [`../reference/bulk-operations.md`](../reference/bulk-operations.md) beschrieben.

**Grundstruktur für Bulk-Öffnungs-Klassifizierung:**

1. Alle Elemente des Typs `Window` per `elements_get_elements_by_type` mit Filter `OnActualFloor` listen — ggf. paginiert.
2. Alle Elemente des Typs `Door` analog listen.
3. Aktuelle Klassifikation pro Element per `elements_get_classifications_of_elements` lesen (Batch-Call, alle GUIDs auf einmal).
4. Elemente ohne korrekte Klassifikation filtern.
5. Confirm-Dialog: „N Fenster → Klassifikation ‚Fenster', M Türen → Klassifikation ‚Tür'".
6. Nach `ja`: `elements_set_classifications_of_elements` in Batches ausführen.

Für den Klassifikations-Sub-Typ (Innen/Außen) bei Öffnungen: Innen-/Außen-Bestimmung läuft über die Host-Wand (Wand-Klassifikation auslesen, auf Öffnung übertragen). Das ist Phase-5-Logik.

---

## Gotchas

1. **Öffnung wird mit Host-Wand mitgelöscht.** Wenn die Host-Wand per `elements_delete_elements` gelöscht wird, verschwinden alle gehosteten Fenster, Türen und Wandöffnungen automatisch — ohne separaten Delete-Call, ohne Warnung im MCP-Response. Das `wall-operations.md` Delete-Beispiel zeigt den vorgeschriebenen `elements_get_connected_elements`-Vorab-Check (SAFE-04).

2. **Sill-Höhe ist relativ zur Wand-Unterkante, nicht zur Story-Höhe.** Ein Fenster mit `SILL = 0.9` sitzt 90 cm über dem Wand-Fuß — bei einer Wand mit `bottomOffset = 0.15` (Sockel) ergibt sich eine lichte Höhe über Fertigboden von 1.05 m.

3. **`elements_get_details_of_elements` ist in AC29 unzuverlässig (bekannter Bug).** Die MCP-Wrapper-Validierung (`extra='forbid'`) blockiert Responses, wenn AC29 neue Felder wie `structureType`, `geometryType` oder `arcAngle` zurückgibt. Workaround: Typ via `elements_get_types_of_elements`, Properties via `properties_get_property_values_of_elements`, GDL-Parameter via `elements_get_gdl_parameters_of_elements`. Siehe Memory-Eintrag `issue_archicad_mcp_get_details_bug.md`.

4. **GDL-Parameter-Index ist Library-objekt-spezifisch.** Index `"1"` für `SILL` gilt nur für das Standard-Library-Fenster. Bei Dritthersteller- oder angepassten GDL-Objekten abweichen. Immer zuerst `elements_get_gdl_parameters_of_elements` aufrufen und Indices aus der Response lesen — nie von einem anderen Element übertragen.

5. **Türschwenk-Richtung ist ein GDL-Parameter, kein Wand-Setting.** Kein `elements_set_details_of_elements`-Call kann sie ändern. Nur `elements_set_gdl_parameters_of_elements` funktioniert — mit dem konkreten Index aus der vorherigen GDL-Abfrage.

6. **`elements_get_connected_elements` ist direktional.** Wand → Window liefert die Fenster der Wand. Fenster → Host-Wand funktioniert nicht direkt. Für SAFE-05-Threading immer von der bekannten Wand-GUID ausgehen.

7. **Opening-Typ vs. Window/Door-Typ.** `"Opening"` ist eine leere geometrische Wandöffnung ohne Library-Objekt; `"Window"` und `"Door"` sind Library-Objekte mit GDL-Parameterraum. `elements_get_elements_by_type` akzeptiert jeweils einen `elementType`-Wert pro Call — drei separate Calls für alle drei Typen.

---

## Verwandte Recipes

- [`wall-operations.md`](wall-operations.md) — Host-Wand: Read, Update, Delete (inkl. SAFE-04 Hosted-Element-Pre-Check). Pflichtlektüre vor Wand-Delete-Operationen.
- [`library-objects.md`](library-objects.md) — GDL-Objekte im Allgemeinen: Library-Suche, Parameter-Schema lesen, Objekt-Varianten. Fenster und Türen sind spezielle Library-Objekte.
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Read → Filter → Group → Confirm → Apply-Muster für Bulk-Klassifizierungs-Workflows.
