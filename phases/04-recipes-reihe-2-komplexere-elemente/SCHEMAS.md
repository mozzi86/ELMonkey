# Phase 4 SCHEMAS

Konsolidierte MCP-Schemata für CurtainWall + 5 Sub-Element-Typen + Library-Object. Live verifiziert am 2026-05-20 gegen Archicad 29 / Port 19723 / „Ohne Titel"-Sandkasten.

## Inhaltsverzeichnis

1. [CurtainWall-Hierarchie: das Schlüssel-Tool](#curtainwall-hierarchie-das-schlüssel-tool)
2. [CurtainWall (Top-Level)](#curtainwall-top-level)
3. [CurtainWallSegment](#curtainwallsegment)
4. [CurtainWallFrame](#curtainwallframe)
5. [CurtainWallPanel](#curtainwallpanel)
6. [CurtainWallJunction](#curtainwalljunction)
7. [CurtainWallAccessory](#curtainwallaccessory)
8. [Library-Object](#library-object)
9. [Workaround-Patterns](#workaround-patterns)

---

## CurtainWall-Hierarchie: das Schlüssel-Tool

**`mcp__archicad__elements_get_subelements_of_hierarchical_elements`** <!-- 2026-05-20 verifiziert AC29 -->

Live-getestet mit unserer Test-Curtain-Wall: liefert in einem Call alle Sub-Elemente, sauber gruppiert.

**Aufruf:**
```python
mcp__archicad__archicad_call_tool(
  name="elements_get_subelements_of_hierarchical_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<curtain-wall-guid>"}}]
    }
  }
)
```

**Response-Struktur:**
```json
{
  "subelements": [{
    "cWallSegments": [{"elementId": {"guid": "..."}}],
    "cWallFrames": [{"elementId": {"guid": "..."}}, ...],
    "cWallPanels": [{"elementId": {"guid": "..."}}, ...]
    // ggf. cWallJunctions, cWallAccessories
  }]
}
```

**Live-Test-Resultat:** 1 CurtainWall → 1 Segment + 31 Frames + 12 Panels in einer Antwort. Junctions + Accessories waren in dieser Test-Fassade nicht enthalten — sind aber im Schema vorgesehen.

**SAFE-05 (Element-ID-Threading):** Diese Liste ist der kanonische Weg, um an Sub-Elemente eines Top-Level-CurtainWall zu kommen. **Nicht** `elements_get_connected_elements` versuchen — das funktioniert für Window-auf-Wall, aber NICHT für CurtainWall-Hierarchie (live verifiziert leer).

**Wichtig:** Das Tool funktioniert auch für andere hierarchische Element-Typen — Stair (mit Riser/Tread/StairStructure-Sub-Elementen), Railing (mit Toprail/Handrail/Post/Baluster-Subs), Beam (mit BeamSegments), Column (mit ColumnSegments). Erweitert das Pattern auch für Phase 6 Lines/Polylines nicht (das sind keine hierarchischen Elemente), aber für künftige Phase-Erweiterungen relevant.

---

## CurtainWall (Top-Level)

### Create

**NICHT VERFÜGBAR in MCP v29.** User zeichnet manuell mit dem Curtain-Wall-Werkzeug (nicht das normale Wand-Werkzeug — die sind unterschiedliche Tools in Archicad).

### Read

Mehrstufig:
1. `mcp__archicad__elements_get_elements_by_type` mit `CurtainWall` → alle Top-Level-GUIDs.
2. `elements_get_subelements_of_hierarchical_elements` (oben) → Sub-Element-Map.
3. Property-basiertes Detail-Read (Standard-Workaround wegen `get_details`-Bug).

### Update

`elements_set_details_of_elements` mit Top-Level-Settings. Schema noch nicht im Detail durchgespielt — `<!-- VERIFY -->` für konkrete Settings-Felder.

### Delete + SAFE-04 Hosted-Pre-Check

**Sub-Elemente sterben mit dem Top-Level-Delete.** Vor Delete:

1. Sub-Elements zählen via `get_subelements_of_hierarchical_elements`.
2. Confirm-Dialog: „CurtainWall hat N Segmente, M Frames, K Panels — alle werden mit-gelöscht."
3. `elements_delete_elements` mit Top-Level-GUID.

### Classify

Standard via `elements_get_classifications_of_elements` + Set-Tool. **Klassifikation propagiert NICHT automatisch auf Sub-Elemente** — bei Bedarf separat klassifizieren.

---

## CurtainWallSegment

### Read

`elements_get_elements_by_type` mit `CurtainWallSegment` → alle Segmente projektweit. ODER aus Sub-Element-Map (oben).

### Update

`elements_set_details_of_elements`. Segment-Settings beeinflussen Geometrie + Frame/Panel-Layout — `<!-- VERIFY -->`.

### Delete

Sub-Element — wird mit Top-Level mitgelöscht. Direktes Delete unüblich, aber mechanisch möglich.

---

## CurtainWallFrame

Pfosten + Riegel + Border-Frames.

### Read

`elements_get_elements_by_type` mit `CurtainWallFrame`. Live-Test: 31 Frames in einer einfachen Default-Curtain-Wall (also keine triviale Anzahl).

### Update

Frame-Profil-Zuweisung über `elements_set_details_of_elements`. Profile-Attribute über `attributes_get_attributes_by_type` mit `Profile`-Typ vorab ermitteln.

---

## CurtainWallPanel

Glas/Füll-Paneele zwischen den Frames.

### Read

`elements_get_elements_by_type` mit `CurtainWallPanel`. Live: 12 Panels in der Test-Curtain-Wall.

### Update

Panel-Material/Composite zuweisen. Standard-Pattern: Composite-vs-Surface-Disambiguierung (siehe `surfaces-materials.md`).

---

## CurtainWallJunction

Knotenpunkte zwischen Frames.

**Live-Test:** 0 Junctions in der Default-Test-Curtain-Wall. Möglicherweise nur bei komplexen Curtain-Wall-Geometrien (Ecken, T-Stöße) generiert. `<!-- VERIFY -->`.

---

## CurtainWallAccessory

Zubehör (Beschläge, Sonderelemente). Library-Object-basiert.

**Live-Test:** 0 Accessories in der Default-Test-Curtain-Wall. Erscheint nur bei explicit-angelegten Accessories.

Read/Update folgt Library-Object-Pattern (unten).

---

## Library-Object

### Create

**`mcp__archicad__elements_create_objects`** <!-- 2026-05-19 verifiziert AC29 -->

Schema aus Phase 2:
```python
{"port": <port>, "params": {"objectsData": [{
  "libraryPartName": "<library-pfad-string>",
  "coordinates": {"x": <m>, "y": <m>, "z": <m>},
  "dimensions": {"x": <m>, "y": <m>, "z": <m>}  // optional
}]}}
```

**Wichtig:** `libraryPartName` muss einem geladenen Library-Part entsprechen (siehe `library_get_libraries` für aktive Libraries).

### Read + Subtype-Identifikation

Standard-Listing + Property-Read. Built-in-Property-Set ist groß (~230 Properties pro Object — live verifiziert). Subtype-Identifikation erfolgt über:
- Library-Part-Name (via Property oder vermutlich Detail-API)
- Subtype-Property aus dem Built-in-Property-Set

### Library-Listing

`mcp__archicad__library_get_libraries` <!-- 2026-05-20 verifiziert AC29 -->

Liefert alle aktiven Libraries mit `{name, path, type, available, readOnly, twServerUrl, urlWebLibrary}`. Library-Types: `BuiltInLibrary`, `EmbeddedLibrary`, `LocalLibrary`, `ServerLibrary`. Live-Test: 19 Libraries im Test-Projekt, darunter BIMcloud-Server-Library.

### Update

`elements_set_details_of_elements` für Position, Rotation, Dimensions. GDL-Parameter via `elements_set_gdl_parameters_of_elements` für Library-spezifische Eigenschaften (Sill-Höhe bei Fenstern, Schwenk-Richtung bei Türen, etc.).

**Caveat:** `elements_get_gdl_parameters_of_elements` hat Pydantic-Validierungs-Bug bei großen Param-Sets (~4000 Errors bei Zonen mit 849 Params, siehe Memory). Bei Library-Objects mit weniger Params ggf. funktional, aber riskant.

### Delete

Standard-Delete. Library-Objects hosten typischerweise nichts (außer Sub-Library-Items in komplexen GDLs) — kein Pre-Check nötig.

---

## Workaround-Patterns

### Sub-Element-Hierarchie ohne `connected_elements`

Für CurtainWall + Stair + Railing + Beam + Column: **`elements_get_subelements_of_hierarchical_elements`** ist das Tool. `connected_elements` ist NUR für Host-Hosted-Beziehung (Wall→Window, etc.) — nicht für Container-Sub-Element-Hierarchien.

### Property-basiertes Lesen (Bug-Workaround)

Wegen `get_details_of_elements`-Bug in AC29:

1. `properties_get_all_property_ids_of_elements` mit `propertyType: "BuiltIn"` oder `"UserDefined"`.
2. `properties_get_property_values_of_elements` mit den Property-IDs.

Object-Built-in-Properties (live verifiziert): ~230 IDs pro Object. Inkl. Klassifikations-System-Refs für aktive Systeme (SAB_Klassifizierung + SAB_Klassifizierung_29).

### Modal-Dialog-Defense (Code 4001)

**LIVE VERIFIZIERT 2026-05-20** — wir haben den Error in dieser Session selbst gefangen:

```
Error 4001: Invalid program status (there is an open modal dialog: Attribute-Manager)
```

User muss den Dialog schließen, bevor MCP weiter funktioniert. Siehe `../reference/mcp-conventions.md` § Modal-Dialoge.
