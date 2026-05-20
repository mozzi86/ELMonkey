# Surfaces, Building Materials und Composites

Dieses Recipe deckt die drei Materialschichten in Archicad ab: **Surfaces** (visuelle Oberflächen),
**Building Materials** (BIM-Materialien mit physikalischen Eigenschaften) und **Composites**
(Mehrschicht-Aufbauten). Der Fokus liegt auf der **Zuweisung existierender Materialien an Elemente**
— Create ist für Building Materials und Composites verfügbar (live verifiziert Phase 2), aber
seltener gebraucht als Zuweisung und Update.

**Surface-Erstellung ist nicht via MCP bekannt** — vermutlich nur über Archicads UI-Materialeditor.
Surfaces über `attributes_get_attributes_by_type` lesen und dann an Elementen zuweisen ist
der übliche Workflow.

## Inhaltsverzeichnis

1. [Umfang und Abgrenzung](#umfang-und-abgrenzung)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [Discovery-Anker](#discovery-anker)
4. [Worked Example — Alle Building Materials auflisten (paginiert)](#worked-example--alle-building-materials-auflisten-paginiert)
5. [Worked Example — Alle Composites auflisten + Skin-Details lesen](#worked-example--alle-composites-auflisten--skin-details-lesen)
6. [Worked Example — Alle Surfaces auflisten](#worked-example--alle-surfaces-auflisten)
7. [Worked Example — Neues Building Material erstellen](#worked-example--neues-building-material-erstellen)
8. [Worked Example — Neues Composite erstellen (Skins + Separators)](#worked-example--neues-composite-erstellen-skins--separators)
9. [Worked Example — Composite an eine Wand zuweisen](#worked-example--composite-an-eine-wand-zuweisen)
10. [Worked Example — Surface-Override an einem Object zuweisen](#worked-example--surface-override-an-einem-object-zuweisen)
11. [Worked Example — Bulk-Material-Zuweisung per Layer-Filter](#worked-example--bulk-material-zuweisung-per-layer-filter)
12. [Bulk-Klassifizierungs-Stub](#bulk-klassifizierungs-stub)
13. [Gotchas](#gotchas)
14. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang und Abgrenzung

**Abgedeckt:**

- Building Materials, Composites, Surfaces vollständig auflisten.
- Building-Material-Detailwerte und physikalische Eigenschaften lesen.
- Composite an Wand oder Decke zuweisen (via `elements_set_details_of_elements`).
- Surface-Override an Library-Objects zuweisen.
- Bulk-Zuweisung pro Layer-Filter.
- Neues Building Material erstellen (verifiziert).
- Neues Composite erstellen mit Skins + Separators (verifiziert).

**Nicht abgedeckt:**

- Surface erstellen — kein MCP-Tool bekannt. <!-- VERIFY: mglw. via attributes_create_surfaces? -->
- Material-Override pro Element-Face (z. B. einzelne Seitenfläche einer Wand) — unklar ob MCP
  diesen Detailgrad unterstützt. v1.x.
- Profile erstellen oder zuweisen — separates Thema (`PROFILE`-Attributtyp, kein eigenes Recipe
  in Phase 6).
- Composite-Editor-Dialog — der modale Dialog blockiert MCP (siehe Gotcha 7).

---

## Erforderlicher Warm-up-Kontext

Details in `../reference/workflow-context.md`.

| Feld | Warum es hier wichtig ist |
|------|---------------------------|
| **Port** | Jeder MCP-Call braucht `"port"`. |
| **Aktive Story** (floorIndex) | Bei Element-Updates (Composite/Surface-Zuweisung): prüfen ob Ziel-Element auf der aktiven Story liegt. |
| **Sichtbare Layer** | SAFE-03: Element-Layer nicht sichtbar → erst konfirmieren. |
| **Längeneinheit** | Composite-Schicht-Dicken in **Meter** übergeben. |

Für reine Attribut-Listings (Building Materials, Composites, Surfaces) brauchen wir nur den Port.
Die Story- und Layer-Felder sind erst relevant, wenn wir ein konkretes Element anfassen.

---

## Discovery-Anker

| Operation | Discovery-Query | Typischer Tool-Name |
|-----------|-----------------|---------------------|
| Attribute nach Typ listen | `"get attributes by type"` | `attributes_get_attributes_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Building-Material-Details | `"get building material attributes"` | `attributes_get_building_material_attributes` <!-- VERIFY --> |
| Physikalische Eigenschaften | `"get building material physical properties"` | `attributes_get_building_material_physical_properties` <!-- VERIFY --> |
| Building Material erstellen | `"create building materials"` | `attributes_create_building_materials` <!-- 2026-05-20 verifiziert AC29 --> |
| Composite erstellen | `"create composites"` | `attributes_create_composites` <!-- 2026-05-20 verifiziert AC29 --> |
| Attribute-Folder erstellen | `"create attribute folders"` | `attributes_create_attribute_folders` <!-- 2026-05-20 verifiziert AC29 --> |
| Element modifizieren / Material zuweisen | `"set details of elements"` | `elements_set_details_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Elemente nach Typ listen | `"get elements by type"` | `elements_get_elements_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` <!-- VERIFY --> |
| Property-Werte holen | `"get property values of elements"` | `properties_get_property_values_of_elements` <!-- VERIFY --> |

---

## Worked Example — Alle Building Materials auflisten (paginiert)

Building Materials sind via `attributes_get_attributes_by_type` mit `attributeType: "BuildingMaterial"`
paginiert abrufbar. Vollständig durchpaginieren, bevor wir mit einem Index arbeiten.

**Schritt 1 — Erste Seite abrufen:**

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "BuildingMaterial"}
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — attributes_get_attributes_by_type -->

Response:
```json
{
  "attributes": [
    {"attributeId": {"guid": "bm-guid-1"}, "index": 1, "name": "Beton C25/30"},
    {"attributeId": {"guid": "bm-guid-2"}, "index": 2, "name": "Mauerwerk Poroton"},
    ...
  ],
  "next_page_token": "tok_abc123"
}
```

**Schritt 2 — Folgeseiten abrufen, bis kein Token mehr:** <!-- VERIFY: genaues Token-Feld -->

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "BuildingMaterial", "page_token": "tok_abc123"}
  }
)
```

Ergebnisse akkumulieren. Erst wenn kein `next_page_token` mehr zurückkommt, sind alle
Building Materials geladen. Niemals auf einer Teilseite weitermachen.

**Schritt 3 — Detail-Abfrage für ausgewählte Materialien:** <!-- VERIFY -->

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_building_material_attributes",
  arguments={
    "port": 19723,
    "params": {
      "attributeIds": [
        {"guid": "bm-guid-1"},
        {"guid": "bm-guid-2"}
      ]
    }
  }
)
```

Erwartete Response-Felder pro Material: `name`, `id`, `manufacturer`, `description`,
`connPriority`, `cutFillIndex`, `cutFillPen`, `cutSurfaceIndex`.

**Physikalische Eigenschaften separat:** <!-- VERIFY -->

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_building_material_physical_properties",
  arguments={
    "port": 19723,
    "params": {"attributeIds": [{"guid": "bm-guid-1"}]}
  }
)
```

Felder: `thermalConductivity` (W/mK), `density` (kg/m³), `heatCapacity` (J/kgK),
`embodiedEnergy` (MJ/kg), `embodiedCarbon` (kgCO₂/kg).

---

## Worked Example — Alle Composites auflisten + Skin-Details lesen

Composites beschreiben Mehrschicht-Aufbauten. Jedes Composite besteht aus `skins` (Schichten mit
Building-Material-Referenz + Dicke) und `separators` (Linienstil zwischen und außen).

**Schritt 1 — Alle Composites listen:**

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "Composite"}
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — attributes_get_attributes_by_type -->

Response: `{attributes: [{attributeId: {guid}, index, name}], next_page_token?}`.
Paginierung wie bei Building Materials durchführen.

**Schritt 2 — Composite-Details inklusive Skin-Struktur lesen:** <!-- VERIFY -->

Es gibt vermutlich keinen dedizierten `attributes_get_composite_attributes`-Call.
Alternativ: Das Composite kann beim Erstellen einer Wand oder beim Lesen des Elements
implizit zurückgegeben werden. Wenn ein dedizierter Endpoint fehlt, die Skin-Daten
aus dem Element-Property-System holen (Wand mit diesem Composite laden, Properties lesen).

**Was ein Composite-Datensatz typischerweise enthält:**

| Feld | Beschreibung |
|------|-------------|
| `name` | Anzeigename, z. B. „AW 365 Poroton" |
| `useWith` | Array von Element-Typen, z. B. `["Wall", "Roof", "Slab"]` |
| `skins` | Array von Schichten (Aufbau von außen nach innen) |
| `skins[i].type` | `"Core"`, `"Finish"`, `"Other"` |
| `skins[i].buildingMaterialId` | GUID des zugeordneten Building Materials |
| `skins[i].framePen` | Stiftnummer für Schicht-Umrandung |
| `skins[i].thickness` | Schichtdicke in Metern |
| `separators` | Array von Trennlinien — **Länge immer `skins.length + 1`** |
| `separators[i].lineTypeId` | GUID des Linientyps |
| `separators[i].linePen` | Stiftnummer für Trennlinie |

**Separator-Anzahl-Regel:** Bei N Skins gibt es N+1 Separators (eine außen, eine zwischen
jeder Schicht, eine innen). Falsche Anzahl führt zu einem Validierungsfehler beim Create.
Mehr dazu in Gotcha 2.

---

## Worked Example — Alle Surfaces auflisten

Surfaces sind visuelle Oberflächenmaterialien — für Rendern und Visualisierung. Im Unterschied zu
Building Materials haben sie keine physikalischen BIM-Eigenschaften.

**Surfaces listen:**

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "Surface"}
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — attributes_get_attributes_by_type -->

Response: `{attributes: [{attributeId: {guid}, index, name}], next_page_token?}`.

Typische Surfaces: `"Beton Sichtbeton"`, `"Putz weiß"`, `"Holz Eiche hell"`,
`"Glas transparent"`. Die Surface-GUIDs und -Indizes sind projekt-spezifisch.

**Detail-Abfrage für Surfaces:** <!-- VERIFY: ob attributes_get_surface_attributes existiert -->

Ein dedizierter `attributes_get_surface_attributes`-Endpoint ist aus Phase-2-Discovery nicht
bekannt. Für Surface-Eigenschaften (Textur, Farbe, Glanz-Wert) bleibt vorerst der
Archicad-Materialeditor der primäre Weg. Index aus der Listing-Response ist ausreichend
für Zuweisung an Elemente.

---

## Worked Example — Neues Building Material erstellen

Neues Building Material mit physikalischen Eigenschaften anlegen.

**Pre-Check — Modal-Dialog-Warnung:**
Vor dem Create: sicherstellen, dass der Archicad-Materialeditor und der Composite-Editor
geschlossen sind. Offene modale Dialoge blockieren den MCP-Call (Code 4001, siehe Gotcha 7).

```
mcp__archicad__archicad_call_tool(
  name="attributes_create_building_materials",
  arguments={
    "port": 19723,
    "params": {
      "buildingMaterials": [{
        "name": "Hochleistungsbeton C40/50",
        "id": "BETON-C40",
        "manufacturer": "Projektspezifisch",
        "description": "Hochfester Beton für Kellerdecken",
        "connPriority": 600,
        "cutFillIndex": 3,
        "cutFillPen": 2,
        "cutSurfaceIndex": 15,
        "thermalConductivity": 2.1,
        "density": 2400,
        "heatCapacity": 1000,
        "embodiedEnergy": 1.11,
        "embodiedCarbon": 0.159
      }],
      "overwriteExisting": false
    }
  }
)
```

<!-- 2026-05-20 verifiziert AC29 — attributes_create_building_materials -->

Response enthält die neue `attributeId: {guid}` des erstellten Materials. Diese GUID sofort
im Arbeitsgedächtnis festhalten (SAFE-05) — sie wird beim Composite-Create als
`buildingMaterialId` referenziert.

**`overwriteExisting: true`** aktualisiert ein bestehendes Material gleichen Namens, statt
einen Fehler zu werfen. Default ist `false` — sicherer für neue Materialien.

**CutFill-Pen-Hinweis:** `cutFillPen` bestimmt die Stiftnummer im Schnittschraffur-Muster.
Wert `0` = Pen-Override aus dem Pen-Set; Wert 2 = explizit Stift 2. Im Projekt-Template
„Schwarz Architekturbüro" (6 Pen-Tables) ist Stift 2 typisch für Schnitt-Konturen.
Mehr in Gotcha 5.

---

## Worked Example — Neues Composite erstellen (Skins + Separators)

Composite für eine 36,5 cm Außenwand aus Poroton mit Innen- und Außenputz erstellen.
Drei Skins → vier Separators (Regel: N+1).

**Voraussetzung:** Building-Material-GUIDs für alle Skins müssen bekannt sein
(entweder aus vorherigem Listing oder aus dem Create-Example oben).

```
mcp__archicad__archicad_call_tool(
  name="attributes_create_composites",
  arguments={
    "port": 19723,
    "params": {
      "composites": [{
        "name": "AW 365 Poroton T14 + Putz",
        "useWith": ["Wall"],
        "skins": [
          {
            "type": "Finish",
            "buildingMaterialId": {"guid": "bm-guid-aussenputz"},
            "framePen": 2,
            "thickness": 0.020
          },
          {
            "type": "Core",
            "buildingMaterialId": {"guid": "bm-guid-poroton"},
            "framePen": 2,
            "thickness": 0.365
          },
          {
            "type": "Finish",
            "buildingMaterialId": {"guid": "bm-guid-innenputz"},
            "framePen": 2,
            "thickness": 0.015
          }
        ],
        "separators": [
          {"lineTypeId": {"guid": "lt-guid-solid"}, "linePen": 2},
          {"lineTypeId": {"guid": "lt-guid-solid"}, "linePen": 1},
          {"lineTypeId": {"guid": "lt-guid-solid"}, "linePen": 1},
          {"lineTypeId": {"guid": "lt-guid-solid"}, "linePen": 2}
        ]
      }],
      "overwriteExisting": false
    }
  }
)
```

<!-- 2026-05-20 verifiziert AC29 — attributes_create_composites -->

**Separator-Zählung:** 3 Skins → 4 Separators (außen, zwischen Skin1/2, zwischen Skin2/3, innen).
Falsche Anzahl → Validierungsfehler. Mehr in Gotcha 2.

**`useWith`-Array** steuert, in welchen Element-Typ-Dialogen das Composite erscheint.
Mögliche Werte: `"Wall"`, `"Roof"`, `"Slab"`, `"Shell"`. Leeres Array = überall sichtbar.

**Linientyp-GUIDs für `lineTypeId`** per `attributes_get_attributes_by_type` mit
`attributeType: "Line"` ermitteln. Nicht raten oder hartkodieren.

Response enthält die neue `attributeId: {guid}` des Composites — sofort für den
nächsten Schritt (Zuweisung an Wand) festhalten.

---

## Worked Example — Composite an eine Wand zuweisen

Ein existierendes oder frisch erstelltes Composite wird einer Wand zugewiesen.
Dies ist ein Update-Call — SAFE-01 Confirm-Dialog ist Pflicht.

**Schritt 1 — Composite-Index ermitteln** (falls nur GUID bekannt):

Der `index`-Wert aus `attributes_get_attributes_by_type` wird als Float in `typeSpecificDetails`
gebraucht (z. B. `"compositeIndex": 5.0`). <!-- VERIFY: genaues Feld-Mapping -->

Alternativ: GUID direkt als `compositeAttributeId` übergeben, falls der Server das unterstützt.

**Schritt 2 — Confirm (SAFE-01):**

```
Ich werde folgendes ändern:
- Wand f1101930-e0bd-7044-a1f2-fdb20e520e21  (Story 0)
  Composite: <aktuell> → "AW 365 Poroton T14 + Putz"

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 3 — Composite-Zuweisung nach `ja`:** <!-- VERIFY: genaue Feldnamen in typeSpecificDetails -->

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"},
        "details": {
          "typeSpecificDetails": {
            "structureType": "Composite",
            "compositeIndex": 5.0
          }
        }
      }]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_set_details_of_elements (Basisaufruf) -->
<!-- VERIFY: structureType + compositeIndex Feldnamen für Wand-Composite-Zuweisung -->

**Für Decken (Slabs)** ist das Schema analog, aber der Wrapper-Typ lautet `SlabSettings`.
Über `elements_set_details_of_elements` mit derselben Struktur — `typeSpecificDetails`
enthält dann Slab-spezifische Felder.

**Wand auf single-Layer BuildingMaterial umstellen:** `structureType: "Basic"` und
`buildingMaterialIndex` statt `compositeIndex`. <!-- VERIFY -->

---

## Worked Example — Surface-Override an einem Object zuweisen

Library-Objects (Möbel, Sanitär, GDL-Objekte) können Surface-Overrides erhalten —
die Original-GDL-Oberfläche wird durch eine Projekt-Surface ersetzt.

**Schritt 1 — Surface-Index aus Listing ermitteln:**

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "Surface"}
  }
)
```

Response: `{attributes: [{attributeId: {guid}, index: 12, name: "Putz weiß"}, ...]}`.
Den `index`-Wert der gewünschten Surface notieren (z. B. `12`).

**Schritt 2 — Aktuellen Surface-Override des Objects lesen:** <!-- VERIFY -->

```
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "obj-guid-1234"}}],
      "properties": [{"propertyId": {"guid": "<surface-property-guid>"}}]
    }
  }
)
```

Surface-Property-GUID per `properties_get_all_property_ids_of_elements` ermitteln.

**Schritt 3 — Confirm (SAFE-01):**

```
Ich werde folgendes ändern:
- Object obj-guid-1234  "Wohnzimmer Sofa 3-sitzig"
  Surface-Override: <kein Override> → "Putz weiß" (Index 12)

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 4 — Surface-Override setzen nach `ja`:** <!-- VERIFY: genaue Feldnamen -->

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [{
        "elementId": {"guid": "obj-guid-1234"},
        "details": {
          "typeSpecificDetails": {
            "surfaceIndex": 12.0
          }
        }
      }]
    }
  }
)
```

<!-- VERIFY: surfaceIndex vs. surfaceAttributeId — welches Feld akzeptiert der Server? -->

**Hinweis — Schedule-Implikation:** Surface-Overrides erscheinen in Archicad-Schedules
anders als Composite-zugeordnete Materialien. Für korrekte Materialauswertung in
Raumbüchern sind Composites mit BuildingMaterial-Referenzen zuverlässiger als
Surface-Overrides allein. Mehr in Gotcha 4.

---

## Worked Example — Bulk-Material-Zuweisung per Layer-Filter

Alle Wände auf dem Layer `A_AW-Außenwände` sollen das Composite
`"AW 365 Poroton T14 + Putz"` erhalten. 6-Schritt-Pattern aus
`../reference/bulk-operations.md`, angepasst für Material-Zuweisung.

**Schritt 1 — READ: Alle Wände laden (paginiert):**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": 19723,
    "params": {"elementType": "Wall"}
  }
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Vollständig durchpaginieren. Annahme: 148 Wände gesamt.

**Schritt 2 — ATTR-01: Alle Layer mit ihren Indizes laden:**

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "Layer"}
  }
)
```

Mapping bauen: `"A_AW-Außenwände" → layerIndex 7`.

**Schritt 3 — FILTER: Nur Wände auf Layer 7:**

Property-Werte der Wände abrufen (`properties_get_property_values_of_elements`),
`Layer`-Property filtern → nur Wände mit `layerIndex: 7.0` behalten.
Annahme: 43 Wände.

**Schritt 4 — Composite-Index ermitteln:**

Aus dem Composite-Listing (Schritt im vorangehenden Example) den Index des
Composites `"AW 365 Poroton T14 + Putz"` herauslesen — z. B. `index: 5`.

**Schritt 5 — CONFIRM (SAFE-01):**

```
Ich werde folgendes ändern:
- 43 Wände (Layer "A_AW-Außenwände")
  Composite: jeweils aktuell → "AW 365 Poroton T14 + Putz" (Index 5)

Ausführen? (ja / details / abbrechen)
```

`details` zeigt die 43 GUIDs + aktuelles Composite pro Wand.

**Schritt 6 — APPLY: Pro Wand setzen nach `ja`:** <!-- VERIFY -->

```
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "<wand-guid-1>"},
          "details": {"typeSpecificDetails": {"structureType": "Composite", "compositeIndex": 5.0}}
        },
        {
          "elementId": {"guid": "<wand-guid-2>"},
          "details": {"typeSpecificDetails": {"structureType": "Composite", "compositeIndex": 5.0}}
        }
        /* ... alle 43 Wände */
      ]
    }
  }
)
```

Batch-Größe: keine Obergrenze (User-Präferenz). Einzel-Fehler weiterlaufen lassen,
Report am Ende: „43 verarbeitet, 42 erfolgreich, 1 Fehler bei ID X — Grund: ...".

---

## Bulk-Klassifizierungs-Stub

Bulk-Zuweisung von Materialien kombiniert mit Klassifizierung (z. B. alle Außenwände
klassifizieren UND gleichzeitig das Außenwand-Composite zuweisen) ist ein häufiger
Workflow. Das Muster:

1. `elements_get_elements_by_type` (alle Wände paginiert).
2. Layer-Filter + ggf. Klassifikations-Filter.
3. Pro Gruppe: Composite-Index + Klassifikations-GUID ermitteln.
4. CONFIRM mit kombinierter Summary: `"84 Wände → Composite X + Klasse Außenwand"`.
5. APPLY: `elements_set_details_of_elements` + `elements_set_classifications_of_elements`
   in zwei getrennten Batches (oder prüfen ob kombinierbar). <!-- VERIFY -->

Vollständige Live-verifizierten Patterns: `../reference/bulk-operations.md`.

---

## Gotchas

1. **Composite ≠ Surface — zwei unterschiedliche Konzepte.** Composites definieren
   den physikalischen und statischen Schichtaufbau eines Elements (Wandaufbau, Dachabdichtung)
   mit Building-Material-Referenzen und Schichtdicken. Surfaces sind rein visuelle
   Oberflächenmaterialien für Rendern und Visualisierung. Eine Wand hat einen Composite
   (Aufbau) und kann zusätzlich Surface-Overrides haben (Darstellung). Diese nicht
   verwechseln — ein falsch zugewiesener Surface-Override macht den Composite nicht kaputt,
   aber der User sieht einen ungeplanten Rendering-Look. Im Umgekehrten: ein falscher
   Composite ändert Flächen- und Materialmengen-Berechnungen und taucht im IFC-Export auf.

2. **Separator-Anzahl = Skins.length + 1.** Bei einem Composite mit N Skins müssen
   exakt N+1 Separators übergeben werden. Zu wenige oder zu viele → Validierungsfehler.
   Eselsbrücke: Separator-Index 0 ist außen, Separator-Index N ist innen, dazwischen je
   einer pro Schichtgrenze.

3. **Building-Material Connection Priority (connPriority).** Wenn zwei Elemente sich
   schneiden (z. B. Wand trifft Wand), entscheidet die `connPriority`, welches Material
   visuell durchläuft. Höhere Zahl = höhere Priorität = läuft durch. Typische Werte:
   Beton 700, Mauerwerk 600, Holz 500, Putz 400. Falsche Priority → unerwartetes Schnittbild
   in Grundrissen und Schnitten.

4. **Surface-Override vs. Composite für Schedule-Auswertung.** Materialmengen-Schedules
   (Raumbuch, Bauteilmengen) werten Building Materials aus Composites aus. Ein Surface-Override
   an einem Object oder einer Wand erscheint im Rendering, aber nicht in der Material-
   mengenberechnung. Für korrekte BIM-Datenqualität: Composites mit BuildingMaterial-
   Referenzen verwenden, nicht nur Surface-Overrides.

5. **CutFill-Pen-Wert 0 vs. explizite Stiftnummer.** `cutFillPen: 0` bedeutet in Archicad
   „Pen-Override aktiv — Pen aus aktivem Pen-Set verwenden". `cutFillPen: 2` setzt explizit
   Stift 2. Im Template-Projekt „Schwarz Architekturbüro" (6 Pen-Tables) ist Stift 2 für
   Schnitt-Konturen reserviert, Stift 1 für Umrisslinien. Falsche Wahl → falsches
   Schnittschraffur-Erscheinungsbild bei Pen-Set-Wechsel.

6. **Surface-Create nicht via MCP bekannt.** `attributes_create_surfaces` wurde in der
   Phase-2-Discovery nicht gefunden. Surfaces werden in Archicad über Optionen → Element-
   Attribute → Oberflächen angelegt. Für neue Surfaces: Archicad-UI verwenden, dann die
   neue Surface per `attributes_get_attributes_by_type` auflisten und die GUID nutzen.
   <!-- VERIFY: ob ein Create-Endpoint existiert, der nicht in Discovery auftauchte -->

7. **Modal-Dialog-Warnung beim Material- und Composite-Editor.** Der Archicad-Materialeditor
   (Optionen → Element-Attribute → Baumaterialien) und der Composite-Editor sind modale Dialoge.
   Wenn sie offen sind, frieren MCP-Calls ein oder timeouten mit Code 4001. Vor jedem
   `attributes_create_building_materials` oder `attributes_create_composites` sicherstellen,
   dass diese Dialoge in Archicad geschlossen sind. Symptom: Call hängt > 10 Sekunden.
   Reaktion: User informieren, Dialog schließen lassen, dann wiederholen. Nie mehrfach
   auto-retry. Mehr in `../reference/mcp-conventions.md § Modal-Dialoge`.

8. **Attribute-Folder-Hierarchie für Organisation.** Building Materials und Composites
   können in Ordnern organisiert werden. Ordner erstellen via `attributes_create_attribute_folders`
   <!-- 2026-05-20 verifiziert AC29 -->, dann beim Create des Attributs den Folder-Pfad
   angeben. Ohne Folder landen neue Attribute in der Root-Liste, was bei großen Projekten
   unübersichtlich wird. <!-- VERIFY: genaues Feld für Folder-Zuweisung beim Create -->

---

## Verwandte Recipes

- [`wall-operations.md`](wall-operations.md) — Wand-Update allgemein; `elements_set_details_of_elements`-Muster.
- [`slabs-columns-beams.md`](slabs-columns-beams.md) — Composite-Zuweisung an Decken + Stützen.
- [`library-objects.md`](library-objects.md) — Surface-Override und GDL-Parameter an Objects.
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Capability-Tabelle
  (Building Materials + Composites Create ✅), Modal-Dialog-Warnung, Confirm-Format.
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Read → Filter → Group
  → Confirm → Apply-Pattern für Bulk-Material-Zuweisung.
- [`../reference/workflow-context.md`](../reference/workflow-context.md) — ATTR-01 für
  Attribut-Listings (Surface, BuildingMaterial, Composite, Layer, Line).
