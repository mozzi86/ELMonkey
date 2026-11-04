# Surfaces, Building Materials und Composites

Dieses Recipe deckt die drei Materialschichten in Archicad ab: **Surfaces** (visuelle Oberflächen),
**Building Materials** (BIM-Materialien mit physikalischen Eigenschaften) und **Composites**
(Mehrschicht-Aufbauten). Create ist für alle drei Typen via MCP verfügbar (live verifiziert
2026-05-21): `attributes_create_surfaces`, `attributes_create_building_materials`,
`attributes_create_composites`.

**Kritische Einschränkung (live verifiziert 2026-05-21):** Die **Zuweisung** eines existierenden
Composites/Building-Materials/Surfaces an ein bereits modelliertes Element ist via MCP v29
NICHT möglich — `elements_set_details_of_elements.typeSpecificDetails` ist hart auf
`WallSettings` (geometrische Wand-Felder) festgenagelt und enthält weder `structureType`
noch `compositeIndex`/`buildingMaterialIndex`/`surfaceIndex`. Workarounds für Material-Zuweisung:
(a) Archicad-UI (Element-Einstellungen), (b) Composite-Definition mit
`overwriteExisting: true` neu schreiben (ändert ALLE Elemente, die dieses Composite nutzen).

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

**Abgedeckt (via MCP v29 möglich, alle live-verifiziert 2026-05-21):**

- Building Materials, Composites, Surfaces vollständig auflisten (paginiert).
- Building-Material-Detailwerte + physikalische Eigenschaften lesen.
- Composite-Detailwerte (Skins + Separators) lesen via `attributes_get_composite_attributes`.
- Surface-Detailwerte (Farbe, Reflektion, Textur) lesen via `attributes_get_surface_attributes`.
- Neues Building Material erstellen.
- Neues Composite erstellen mit Skins + Separators.
- Neue Surface erstellen mit SurfaceType-Enum (General/Simple/Matte/Metal/Plastic/Glass/Glowing/Constant) + Textur-Binding.
- Attribut-Ordner (Folder-Hierarchie) erstellen, lesen, verschieben, löschen.
- Composite-Definition mit `overwriteExisting: true` umschreiben (siehe Abschnitt „Composite-Zuweisung umgehen").

**NICHT abgedeckt (live-verifiziert 2026-05-21 als nicht-möglich in MCP v29):**

- **Composite/Building-Material an existierendes Element zuweisen** — `elements_set_details_of_elements.typeSpecificDetails` ist WallSettings-only. Keine `structureType`, kein `compositeIndex`, kein `buildingMaterialIndex`.
- **Surface-Override an existierendem Element setzen** — gleiche Einschränkung. Kein `surfaceIndex` im Schema.
- **Material-Override pro Element-Face** (z. B. einzelne Seitenfläche einer Wand) — separater Detailgrad, nicht in MCP v29.
- **Profile erstellen oder zuweisen** — separates Thema (`PROFILE`-Attributtyp), nicht in Phase 6.
- **Composite-Editor-Dialog** — der modale UI-Dialog blockiert MCP (siehe Gotcha 7).

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

| Operation | Discovery-Query | Tool-Name |
|-----------|-----------------|-----------|
| Attribute nach Typ listen (paginiert) | `"get attributes by type"` | `attributes_get_attributes_by_type` <!-- 2026-05-21 verifiziert AC29 --> |
| Building-Material-Details | `"get building material attributes"` | `attributes_get_building_material_attributes` <!-- 2026-05-21 verifiziert AC29 --> |
| Physikalische Eigenschaften (BM) | `"get building material physical properties"` | `attributes_get_building_material_physical_properties` <!-- 2026-05-21 verifiziert AC29 --> |
| Composite-Details (Skins/Separators) | `"get composite attributes"` | `attributes_get_composite_attributes` <!-- 2026-05-21 verifiziert AC29 — Schema bestätigt --> |
| Surface-Details (Farbe/Textur) | `"get surface attributes"` | `attributes_get_surface_attributes` <!-- 2026-05-21 verifiziert AC29 — Schema bestätigt --> |
| Fill-Details (Schraffur) | `"get fill attributes"` | `attributes_get_fill_attributes` <!-- 2026-05-21 verifiziert AC29 --> |
| Line-Details | `"get line attributes"` | `attributes_get_line_attributes` <!-- 2026-05-21 verifiziert AC29 --> |
| Surface erstellen | `"create surface"` | `attributes_create_surfaces` <!-- 2026-05-21 verifiziert AC29 — Schema bestätigt: SurfaceType-Enum + ColorRGB + optional Texture --> |
| Building Material erstellen | `"create building materials"` | `attributes_create_building_materials` <!-- 2026-05-21 verifiziert AC29 --> |
| Composite erstellen | `"create composites"` | `attributes_create_composites` <!-- 2026-05-21 verifiziert AC29 --> |
| Attribut-Ordner-Struktur lesen | `"get attribute folder structure"` | `attributes_get_attribute_folder_structure` <!-- 2026-05-21 verifiziert AC29 --> |
| Attribut-Ordner erstellen | `"create attribute folders"` | `attributes_create_attribute_folders` <!-- 2026-05-21 verifiziert AC29 --> |
| Attribute/Ordner verschieben | `"move attributes and folders"` | `attributes_move_attributes_and_folders` <!-- 2026-05-21 verifiziert AC29 --> |
| Element modifizieren (NUR Wand-Geometrie) | `"set details of elements"` | `elements_set_details_of_elements` <!-- 2026-05-21 verifiziert AC29 — typeSpecificDetails=WallSettings-only, KEIN Composite-/Material-/Surface-Index --> |
| Elemente nach Typ listen | `"get elements by type"` | `elements_get_elements_by_type` <!-- 2026-05-21 verifiziert AC29 — paginiert --> |
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` <!-- 2026-05-21 verifiziert AC29 --> |
| Property-Werte holen | `"get property values of elements"` | `properties_get_property_values_of_elements` <!-- 2026-05-21 verifiziert AC29 --> |

---

> **User sagt:** „Welche Building Materials gibt's im Projekt?"

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

**Schritt 2 — Folgeseiten abrufen, bis kein Token mehr** <!-- 2026-05-21 verifiziert AC29 — page_token ist Top-Level-Argument, NICHT in params -->:

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={
    "port": 19723,
    "params": {"attributeType": "BuildingMaterial"},
    "page_token": "tok_abc123"
  }
)
```

**Pagination-Gotcha:** `page_token` ist ein TOP-LEVEL-Feld der `arguments` (neben `port` und `params`), NICHT innerhalb von `params`. Falsche Position → ignoriert, gleiche Seite kommt zurück.

Ergebnisse akkumulieren. Erst wenn kein `next_page_token` mehr zurückkommt, sind alle Building Materials geladen. Niemals auf einer Teilseite weitermachen.

**Schritt 3 — Detail-Abfrage für ausgewählte Materialien** <!-- 2026-05-21 verifiziert AC29 -->:

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

**Physikalische Eigenschaften separat** <!-- 2026-05-21 verifiziert AC29 -->:

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

> **User sagt:** „Zeig mir alle Composites mit ihren Schichten."

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

**Schritt 2 — Composite-Details inklusive Skin-Struktur lesen** <!-- 2026-05-21 verifiziert AC29 — attributes_get_composite_attributes EXISTIERT (Discovery-Schema bestätigt) -->:

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_composite_attributes",
  arguments={
    "port": 19723,
    "params": {"attributeIds": [{"attributeId": {"guid": "cm-guid-1"}}]}
  }
)
```

**Hinweis zur Schema-Konvention:** `attributeIds` ist ein Array von `{"attributeId": {"guid": "..."}}`-Wrappern — die GUID steckt einen Wrapper tiefer als bei `properties`-Calls. Bei „invalid argument"-Fehler immer Wrapper-Tiefe prüfen.

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

> **User sagt:** „Welche Surfaces sind fürs Rendering verfügbar?"

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

**Detail-Abfrage für Surfaces** <!-- 2026-05-21 verifiziert AC29 — attributes_get_surface_attributes EXISTIERT -->:

```
mcp__archicad__archicad_call_tool(
  name="attributes_get_surface_attributes",
  arguments={
    "port": 19723,
    "params": {"attributeIds": [{"attributeId": {"guid": "sf-guid-1"}}]}
  }
)
```

Erwartete Response-Felder pro Surface: `name`, `materialType` (Enum: General/Simple/Matte/Metal/Plastic/Glass/Glowing/Constant), `ambientReflection`/`diffuseReflection`/`specularReflection` (0-100), `transparency` (0-100), `shine` (0-10000), `surfaceColor`/`specularColor`/`emissionColor` (ColorRGB 0.0-1.0), optional `texture` mit Filename + Skalierung + Mirror-Flags + Alpha-Channel-Bits, optional `fillId` für gekoppelten Fill.

---

> **User sagt:** „Leg ein neues Building Material für Hochleistungsbeton C40/50 an."

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
      "buildingMaterialDataArray": [{
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

<!-- 2026-05-21 verifiziert AC29 — attributes_create_building_materials, Schema-Param ist buildingMaterialDataArray (NICHT buildingMaterials) -->

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

> **User sagt:** „Erstell ein neues Composite für eine 36,5 Poroton-Außenwand mit Putz."

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
      "compositeDataArray": [{
        "name": "AW 365 Poroton T14 + Putz",
        "useWith": ["Wall"],
        "skins": [
          {
            "type": "Finish",
            "buildingMaterialId": {"attributeId": {"guid": "bm-guid-aussenputz"}},
            "framePen": 2,
            "thickness": 0.020
          },
          {
            "type": "Core",
            "buildingMaterialId": {"attributeId": {"guid": "bm-guid-poroton"}},
            "framePen": 2,
            "thickness": 0.365
          },
          {
            "type": "Finish",
            "buildingMaterialId": {"attributeId": {"guid": "bm-guid-innenputz"}},
            "framePen": 2,
            "thickness": 0.015
          }
        ],
        "separators": [
          {"lineTypeId": {"attributeId": {"guid": "lt-guid-solid"}}, "linePen": 2},
          {"lineTypeId": {"attributeId": {"guid": "lt-guid-solid"}}, "linePen": 1},
          {"lineTypeId": {"attributeId": {"guid": "lt-guid-solid"}}, "linePen": 1},
          {"lineTypeId": {"attributeId": {"guid": "lt-guid-solid"}}, "linePen": 2}
        ]
      }],
      "overwriteExisting": false
    }
  }
)
```

<!-- 2026-05-21 verifiziert AC29 — Schema-Korrekturen: Param ist compositeDataArray (NICHT composites), buildingMaterialId+lineTypeId sind AttributeIdArrayItem-Wrapper {"attributeId": {"guid": ...}} (eine Ebene tiefer als zuvor dokumentiert) -->

**Separator-Zählung:** 3 Skins → 4 Separators (außen, zwischen Skin1/2, zwischen Skin2/3, innen).
Falsche Anzahl → Validierungsfehler. Mehr in Gotcha 2.

**`useWith`-Array** steuert, in welchen Element-Typ-Dialogen das Composite erscheint.
Mögliche Werte: `"Wall"`, `"Roof"`, `"Slab"`, `"Shell"`. Leeres Array = überall sichtbar.

**Linientyp-GUIDs für `lineTypeId`** per `attributes_get_attributes_by_type` mit
`attributeType: "Line"` ermitteln. Nicht raten oder hartkodieren.

Response enthält die neue `attributeId: {guid}` des Composites — sofort für den
nächsten Schritt (Zuweisung an Wand) festhalten.

---

## Composite-Zuweisung an existierende Wand — NICHT direkt via MCP v29

<!-- 2026-05-21 verifiziert AC29 via Discovery-Schema-Inspektion -->

**Befund:** `elements_set_details_of_elements.params.elementsWithDetails[].details.typeSpecificDetails` ist im MCP v29-Server hart auf `WallSettings` festgenagelt. `WallSettings` enthält ausschließlich geometrische Felder (begCoordinate, endCoordinate, height, bottomOffset, offset, begThickness, endThickness) — KEINE `structureType`, KEIN `compositeIndex`, KEIN `buildingMaterialIndex`. Die Composite-Zuweisung an ein bereits modelliertes Element ist deshalb via diesen Endpoint **nicht möglich**.

### Workaround A — User-UI (empfohlen für Einzelelemente)

```
Bitte in Archicad öffnen:
  1. Wand f1101930-... selektieren (z.B. Werkzeug → Auswahl, GUID via Element-ID-Werkzeug zeigen lassen)
  2. Einstellungen-Dialog (Strg/Cmd+T)
  3. Struktur → Composite → "AW 365 Poroton T14 + Putz" wählen
  4. OK

(MCP v29 hat keinen programmatischen Endpoint dafür.)
```

### Workaround B — Composite-Definition überschreiben (für Bulk-Effekt)

Wenn ALLE Wände, die ein bestimmtes Composite (z.B. „AW 365") verwenden, einen neuen Aufbau bekommen sollen: Composite-Definition via `attributes_create_composites` mit `overwriteExisting: true` neu schreiben. Das ändert das Composite-Attribut selbst — alle zugewiesenen Wände erben den neuen Aufbau automatisch.

**Confirm (SAFE-01 ist hier ÜBERLEBENSWICHTIG — kaskadiert auf alle Wände):**

```
Ich werde folgendes ändern:
- Composite "AW 365 Poroton T14 + Putz" (Index 5, GUID cm-...)
  Aufbau: 3 Skins (Außenputz 2cm + Poroton 36,5cm + Innenputz 1,5cm)
       → 4 Skins (Klinker 11,5cm + Dämmung 16cm + Poroton 17,5cm + Innenputz 1,5cm)

ACHTUNG: Diese Änderung wirkt sich auf ALLE 47 Wände aus, die dieses
Composite zugewiesen haben (Stand: elements_get_elements_by_type +
property-filter "Layer:A_AW-Aussenwand" → 47 Treffer).

Ausführen? (ja / nein / details / abbrechen)
```

Aufruf nach `ja` siehe „Worked Example — Neues Composite erstellen" oben — gleicher Call, nur `overwriteExisting: true` und `attributeId` der existierenden Composite-GUID setzen.

### Workaround C — Wand löschen + neu erstellen — NICHT möglich

Wände sind in MCP v29 NICHT erstellbar (siehe Capability-Tabelle in `mcp-conventions.md`). Workaround „delete + recreate" wie bei PolyLine ist daher für Wand-Composite-Wechsel nicht verfügbar. Workaround A (UI) ist alternativlos für Einzelelemente.

---

## Surface-Override an einem Object zuweisen — NICHT direkt via MCP v29

<!-- 2026-05-21 verifiziert AC29 via Discovery-Schema-Inspektion -->

**Befund:** Identische Einschränkung wie bei der Composite-Zuweisung — `set_details.typeSpecificDetails` ist WallSettings-only. Es gibt kein `surfaceIndex`-Feld im Schema und keinen anderen Endpoint zum Setzen eines Surface-Overrides pro Element. Library-Object-Surface-Overrides können via MCP v29 nicht gesetzt werden.

### Workaround — User-UI

```
Bitte in Archicad öffnen:
  1. Object obj-guid-1234 selektieren
  2. Einstellungen-Dialog (Strg/Cmd+T)
  3. Modell-Darstellung → Oberflächen-Override → "Putz weiß" wählen
  4. OK

(MCP v29 hat keinen programmatischen Endpoint für Surface-Override-Set.)
```

### Was via MCP geht (Surface-Domain)

- Alle Surfaces auflisten: `attributes_get_attributes_by_type` (Type "Surface").
- Surface-Details lesen: `attributes_get_surface_attributes`.
- Neue Surface erstellen: `attributes_create_surfaces` (mit SurfaceType-Enum + Farbe + Textur).
- Bestehende Surface umdefinieren: `attributes_create_surfaces` mit `overwriteExisting: true` → wirkt auf alle Elemente, die diese Surface nutzen.

**Hinweis — Schedule-Implikation:** Surface-Overrides erscheinen in Archicad-Schedules anders als Composite-zugeordnete Materialien. Für korrekte Materialauswertung in Raumbüchern sind Composites mit BuildingMaterial-Referenzen zuverlässiger als Surface-Overrides allein. Mehr in Gotcha 4.

---

## Bulk-Material-Zuweisung per Layer-Filter — alternative Wege

<!-- 2026-05-21 verifiziert AC29 -->

Ein-zu-eins-Composite-Zuweisung an N existierende Wände ist via MCP v29 NICHT möglich (siehe vorherigen Abschnitt). Wenn aber alle N Wände dasselbe **bestehende** Composite tragen sollen und das Ziel ein **neuer** Aufbau ist, gibt es zwei tragfähige Wege:

### Weg 1 — Bulk-Composite-Definition überschreiben

Falls alle 43 Ziel-Wände bereits dasselbe Composite (z.B. „AW 365 alt") tragen, das umdefiniert werden soll → siehe „Workaround B" im vorherigen Abschnitt. Ein einziger `attributes_create_composites`-Call mit `overwriteExisting: true` ändert das Composite-Attribut, alle 43 Wände erben den neuen Aufbau automatisch. Keine Batch-Schleife nötig.

### Weg 2 — UI-Workflow vorbereiten

Wenn die 43 Wände unterschiedliche aktuelle Composites haben (Mischbestand), funktioniert Weg 1 nicht — der einzige Pfad bleibt UI-Bulk:

**Schritt 1 — READ + FILTER programmatisch vorbereiten:**

```
# Alle Wände laden (paginiert)
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={"port": 19723, "params": {"elementType": "Wall"}}
)
# Annahme: 148 Wände, paginierung durchziehen

# Layer-Indizes auflisten
mcp__archicad__archicad_call_tool(
  name="attributes_get_attributes_by_type",
  arguments={"port": 19723, "params": {"attributeType": "Layer"}}
)
# Mapping bauen: "A_AW-Außenwände" → layerIndex 7

# Filter: Property-Werte der Wände lesen, Layer-Property = 7 behalten
# Resultat: 43 GUIDs der Ziel-Wände
```

**Schritt 2 — User-Anweisung mit GUID-Liste vorbereiten:**

```
Die 43 zu ändernden Wände (Layer "A_AW-Außenwände"):
  - f1101930-... (Story 0, Länge 4.20 m)
  - a2b3c4d5-... (Story 0, Länge 3.85 m)
  - ... (41 weitere)

Bitte in Archicad:
  1. Bearbeiten → Suchen+Auswählen → nach GUID-Liste (Skript im Workflow,
     oder nach Layer "A_AW-Außenwände" filtern)
  2. Mit allen 43 selektiert: Strg/Cmd+T → Struktur → Composite
     "AW 365 Poroton T14 + Putz" → OK
  3. Bestätigen, wenn fertig.
```

**Schritt 3 — Verifikation nach User-Bestätigung:**

Nach „fertig" via `properties_get_property_values_of_elements` die Composite-Property der 43 Wände lesen und prüfen, ob alle den neuen Aufbau zeigen. Nicht-vertrauen-Pattern (siehe `bulk-operations.md` § silent-success-without-success).

**Honest-Limit:** Vollautomatischer Bulk-Composite-Wechsel ist in MCP v29 für Mischbestände nicht möglich. Diese Lücke ist als Backlog-Item dokumentiert; ggf. bei MCP-Server-Update neu verifizieren.

---

## Bulk-Klassifizierungs-Stub

Bulk-Zuweisung von Materialien kombiniert mit Klassifizierung (z. B. alle Außenwände
klassifizieren UND gleichzeitig das Außenwand-Composite zuweisen) ist ein häufiger
Workflow. Das Muster:

1. `elements_get_elements_by_type` (alle Wände paginiert).
2. Layer-Filter + ggf. Klassifikations-Filter.
3. Pro Gruppe: Composite-Index + Klassifikations-GUID ermitteln.
4. CONFIRM mit kombinierter Summary: `"84 Wände → Composite X + Klasse Außenwand"`.
5. APPLY: nur `elements_set_classifications_of_elements` ist als programmatischer Bulk-Call verfügbar (siehe `bulk-operations.md`). Composite-Set: NICHT via MCP — siehe Abschnitt „Composite-Zuweisung an existierende Wand" oben. Daher in der Praxis zwei getrennte Schritte: (a) MCP-Bulk-Klassifizierung, (b) Composite-Wechsel via UI oder Composite-Definition-Überschreibung.

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

6. **Surface-Create geht via MCP** <!-- 2026-05-21 verifiziert AC29: attributes_create_surfaces existiert -->.
   Korrektur zur älteren Skill-Version: `attributes_create_surfaces` IST im Discovery zu finden
   und unterstützt vollständige Surface-Definition: SurfaceType-Enum (General/Simple/Matte/Metal/
   Plastic/Glass/Glowing/Constant), Ambient/Diffuse/Specular/Transparency/Shine-Werte,
   ColorRGB für surfaceColor/specularColor/emissionColor, optional Texture-Block mit Filename,
   Skalierung, Mirror-Flags, Alpha-Channel-Bindings. Required-Felder: name, materialType,
   alle Reflection-Werte, alle Color-Felder. Beim Update existierender Surfaces:
   `overwriteExisting: true`.

7. **Modal-Dialog-Warnung beim Material- und Composite-Editor.** Der Archicad-Materialeditor
   (Optionen → Element-Attribute → Baumaterialien) und der Composite-Editor sind modale Dialoge.
   Wenn sie offen sind, frieren MCP-Calls ein oder timeouten mit Code 4001. Vor jedem
   `attributes_create_building_materials` oder `attributes_create_composites` sicherstellen,
   dass diese Dialoge in Archicad geschlossen sind. Symptom: Call hängt > 10 Sekunden.
   Reaktion: User informieren, Dialog schließen lassen, dann wiederholen. Nie mehrfach
   auto-retry. Mehr in `../reference/mcp-conventions.md § Modal-Dialoge`.

8. **Attribute-Folder-Hierarchie — Zwei-Schritt-Workflow.** <!-- 2026-05-21 verifiziert AC29 — Discovery zeigt keine Folder-Felder in den Create-Schemas. -->
   Building Materials, Composites, Surfaces können in Ordnern organisiert werden, aber
   NICHT direkt beim Create. Workflow ist zwei Schritte:
   (1) Attribut anlegen via `attributes_create_*` (landet erstmal in Root)
   (2) Attribut in Ordner verschieben via `attributes_move_attributes_and_folders` mit
   `targetFolderId` (Folder-GUID aus `attributes_get_attribute_folder_structure`).
   Ordner erstellen vorher mit `attributes_create_attribute_folders` (nimmt nur `path[]` als
   String-Liste). Ohne diesen Move-Schritt bleiben neue Attribute in Root, was bei großen
   Projekten unübersichtlich wird.

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
