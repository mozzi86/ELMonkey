# Wand-Operationen

Dieses Recipe deckt Lesen, Modifizieren, Löschen und Klassifizieren von Wänden ab.
**Update 2026-07-13:** Seit Tapir ≥ 1.5.3 / tapir-archicad-mcp ≥ 0.4.3 ist Wand-Erstellung
verfügbar: `elements_create_walls` (Pflicht: `begCoordinate`, `endCoordinate`, `zCoordinate`,
`height`, `thickness`; optional `referenceLineLocation`, `structureType`, `compositeId`,
`profileId`, `buildingMaterialId`). Ebenso neu: `elements_modify_walls`, `create_windows`,
`create_doors`, `create_openings`, `rotate_elements`, `get_element_preview_image`.
Die Aussage „kein Create per MCP v29" galt nur bis Tapir 1.4.0 — Details und Update-Prozedur
in [`../reference/mcp-extension.md`](../reference/mcp-extension.md).
(Worked Example für Create folgt nach erstem Live-Einsatz.)

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [Discovery-Anker](#discovery-anker)
4. [Typische Parameter](#typische-parameter)
5. [Worked Example — Wand lesen](#worked-example--wand-lesen)
6. [Worked Example — Wand modifizieren (Höhe ändern)](#worked-example--wand-modifizieren-höhe-ändern)
7. [Worked Example — Wand löschen (mit Hosted-Element-Check)](#worked-example--wand-löschen-mit-hosted-element-check)
8. [Worked Example — Wand klassifizieren](#worked-example--wand-klassifizieren)
9. [Worked Example — Wand in Schichten aufteilen (Kern + Schalen)](#worked-example--wand-in-schichten-aufteilen-kern--schalen)
10. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
11. [Gotchas](#gotchas)
12. [Verwandte Recipes](#verwandte-recipes)
13. [Überlappungen, Doppelungen und gerundete Ecken bereinigen](#überlappungen-doppelungen-und-gerundete-ecken-bereinigen)
14. [Wände gegen den Bestandsplan abgleichen (Einpassung, Wandstärke)](#wände-gegen-den-bestandsplan-abgleichen-einpassung-wandstärke)

---

## Umfang

- Alle Wände einer Story oder des gesamten Projekts auflisten.
- Wand-Eigenschaften lesen (via Property-System-Workaround — `elements_get_details_of_elements` ist in AC29 unzuverlässig, siehe Gotchas).
- Wand-Eigenschaften modifizieren: Höhe, Dicke, Geometrie (Anfangs-/Endpunkt), Layer, Boden-Offset.
- Wand löschen, mit vorangehendem Hosted-Element-Pre-Check (SAFE-04).
- Wand-Klassifikation lesen oder setzen (im aktiven Klassifikations-System).

Wand erstellen: seit Tapir 1.5.3 via `elements_create_walls` möglich (siehe Hinweis oben); Worked Example folgt.

---

## Erforderlicher Warm-up-Kontext

Details in `../reference/workflow-context.md`.

| Feld | Warum es hier wichtig ist |
|------|---------------------------|
| **Port** | Jeder MCP-Call braucht `"port"`. |
| **Aktive Story** (floorIndex) | Filter `OnActualFloor` greift per Default; bei anderen Geschossen explizit angeben. |
| **Längeneinheit** | Alle Koordinaten und Höhen in **Meter** (verifiziert). User-Eingaben in cm/mm selbst umrechnen. |
| **Sichtbare Layer** | SAFE-03: Ziel-Layer nicht sichtbar → erst konfirmieren. |
| **Klassifikations-System** | Nur bei Klassifizierungs-Auftrag — System-GUID ist Pflicht für Classify-Calls. |

---

## Discovery-Anker

| Operation | Discovery-Query | Verifizierter Tool-Name |
|-----------|-----------------|------------------------|
| Wände listen | `"get elements by type wall"` | `elements_get_elements_by_type` <!-- 2026-05-19 verifiziert AC29 --> |
| Element-Typ prüfen | `"get type of element"` | `elements_get_types_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` <!-- verifiziert 2026-07-14 --> |
| Property-Werte holen | `"get property values of elements"` | `properties_get_property_values_of_elements` <!-- verifiziert 2026-07-14 --> |
| Bounding Box | `"get 2d bounding boxes of elements"` | `elements_get2_d_bounding_boxes` (kein Unterstrich in „get2"!) <!-- verifiziert 2026-07-14 --> |
| Wand modifizieren | `"set details of elements"` | `elements_set_details_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Wand löschen | `"delete elements"` | `elements_delete_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Hosted Elemente prüfen | `"get connected elements"` | `elements_get_connected_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` <!-- 2026-05-19 verifiziert AC29 --> |
| Klassifikation setzen | `"set classification of elements"` | `dev_set_classifications_of_elements` (Präfix `dev_`, nicht `elements_`!) <!-- verifiziert 2026-07-14 --> |
| Klassifikations-Systeme | `"get all classification systems"` | `classifications_get_all_classification_systems` <!-- 2026-05-19 verifiziert AC29 --> |

---

## Typische Parameter

### WallSettings (für `typeSpecificDetails` in `elements_set_details_of_elements`)

Alle Felder optional — nur die zu ändernden angeben.

| Feld | Typ | Beschreibung |
|------|-----|-------------|
| `begCoordinate` | `{x: m, y: m}` | Startpunkt (Coord2D, kein Z). |
| `endCoordinate` | `{x: m, y: m}` | Endpunkt (Coord2D, kein Z). |
| `height` | `number` (m) | Wandhöhe in Metern. |
| `bottomOffset` | `number` (m) | Abstand Wand-Fuß zu Story-Level (Standard: 0). |
| `offset` | `number` (m) | Versatz Wand-Mittellinie zur Reference Line. |
| `begThickness` | `number` (m) | Wanddicke am Anfangspunkt. |
| `endThickness` | `number` (m) | Wanddicke am Endpunkt (bei gerader Wand = `begThickness`). |
| `structureType` | `string` | live gesehen: `"Basic"`, `"Composite"`. <!-- verifiziert 2026-07-14 --> |
| `geometryType` | `string` | live gesehen: `"Straight"`, `"Polygonal"`. <!-- verifiziert 2026-07-14 --> |

Details-Rahmen: `{"floorIndex": 0, "layerIndex": 3.0, "drawIndex": 1.0, "typeSpecificDetails": {...}}`.
`layerIndex` ist ein Float (1-basiert, nicht 0-basiert).

---

> **User sagt:** „Lies mir die Eigenschaften der Wand — Höhe, Dicke, Composite."

## Worked Example — Wand lesen

Wir lesen die Eigenschaften der Test-Wand `f1101930-e0bd-7044-a1f2-fdb20e520e21` über das
Property-System. `elements_get_details_of_elements` schlägt in AC29 wegen eines
Pydantic-Wrapper-Bugs fehl (Details: Gotcha 1 + Memory-Eintrag).

**Schritt 1 — Property-IDs holen:**

```
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]
    }
  }
)
```

Response: `{"propertyIds": [{"propertyId": {"guid": "aaa1..."}}, ...]}`.
Typische Wand-Properties: `Height`, `Thickness`, `Layer`, `Composite`, `BuildingMaterial`.
Property-IDs sind projekt-spezifisch.

**Schritt 2 — Werte für gewünschte Properties holen:** <!-- verifiziert 2026-07-14 --> (4.500er-Lauf)

```
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}],
      "properties": [{"propertyId": {"guid": "aaa1..."}}, {"propertyId": {"guid": "bbb2..."}}]
    }
  }
)
```

Response enthält pro Property Typ und Wert (z. B. `{"type": "length", "value": 2.8}`).

**Alternative — Typ + Lage ohne Property-System:**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_types_of_elements",
  arguments={"port": 19723, "params": {"elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]}}
)
```

<!-- 2026-05-19 verifiziert AC29 — liefert {"typesOfElements": [{"type": "Wall"}]}, auch wo get_details scheitert -->

Für die räumliche Lage: `elements_get_2_d_bounding_boxes` mit derselben GUID — gibt
`xMin`, `yMin`, `xMax`, `yMax` in Metern zurück. <!-- verifiziert 2026-07-14 -->

**Element-ID-Threading (SAFE-05):** Die GUID `f1101930-e0bd-7044-a1f2-fdb20e520e21` bleibt
im Arbeitsgedächtnis für alle Folge-Operationen dieser Session. Nicht erneut suchen.

---

> **User sagt:** „Ändere die Höhe dieser Wand von 2,80 auf 3,20 Meter."

## Worked Example — Wand modifizieren (Höhe ändern)

Wir ändern die Höhe der Test-Wand von 2,80 m auf 3,20 m. Gemäß SAFE-01 zeigen wir
zuerst den Confirm-Dialog.

**Confirm (SAFE-01) — vor dem Aufruf:**

```
Ich werde folgendes ändern:
- Wand f1101930-e0bd-7044-a1f2-fdb20e520e21  (Story 0, gerade, ca. 4m)
  Höhe: 2.80m → 3.20m

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
        "elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"},
        "details": {
          "typeSpecificDetails": {"height": 3.20}
        }
      }]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_set_details_of_elements -->

Response: `{"editedElements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]}`.
Die GUID bleibt nach dem Update stabil — für Folge-Operationen dieselbe ID weiterverwenden.

Weitere `typeSpecificDetails`-Beispiele: Dicke ändern (`"begThickness": 0.365, "endThickness": 0.365`),
Endpunkte verschieben (`"begCoordinate": {"x": 2.5, "y": 0.0}, "endCoordinate": {"x": 6.5, "y": 0.0}`).
Hinweis: Koordinaten haben kein `z`-Feld (Coord2D) — vertikale Lage über `floorIndex` + `bottomOffset`.

---

> **User sagt:** „Lösch die Wand, sag mir aber vorher welche Fenster und Türen mit weggehen."

## Worked Example — Wand löschen (mit Hosted-Element-Check)

SAFE-04 und SAFE-01 greifen hier gemeinsam. Die Test-Wand hostet das Fenster
`7185f21a-ca8f-6b44-a8a3-28d0610f0d82`. Archicad löscht gehostete Elemente beim Entfernen
der Wirt-Wand automatisch mit — das ist Archicads eigenes Verhalten. Wir müssen den User
daher informieren, bevor der Delete-Call abgesetzt wird.

**Schritt 1 — Hosted-Element-Pre-Check: Fenster suchen (SAFE-04):**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_connected_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}],
      "connectedElementType": "Window"
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_get_connected_elements -->

Response: `{"connectedElements": [{"elementId": {"guid": "f1101930..."}, "connectedElements": [{"elementId": {"guid": "7185f21a-ca8f-6b44-a8a3-28d0610f0d82"}}]}]}`.
Optional: separater Call mit `"connectedElementType": "Door"` und `"Opening"` falls relevant.

**Schritt 2 — Confirm mit Hosted-Element-Information (SAFE-01):**

```
Ich werde folgendes löschen:
- Wand f1101930-e0bd-7044-a1f2-fdb20e520e21  (Story 0, gerade, ca. 4m)
  Hostet folgende Elemente, die von Archicad automatisch mit-gelöscht werden:
  - Fenster 7185f21a-ca8f-6b44-a8a3-28d0610f0d82

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 3 — Delete erst nach ausdrücklichem `ja`:**

```
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 — elements_delete_elements -->

Das Fenster muss nicht separat gelöscht werden. Beim Wand-Löschen mitgerissene Öffnungen
können in IFC-Exporten stille Datenfehler erzeugen — der Pre-Check schützt davor.

---

> **User sagt:** „Klassifizier alle Wände nach SAB Innen/Außen."

## Worked Example — Wand klassifizieren

Wir setzen für die Test-Wand eine Klassifikation im Projekt-Klassifikations-System
(`SAB_Klassifizierung_29` im User-Setup — System-GUID ist projekt-spezifisch).

**Schritt 1 — Klassifikations-Systeme listen (Warm-up, falls System-GUID unbekannt):**

```
mcp__archicad__archicad_call_tool(
  name="classifications_get_all_classification_systems",
  arguments={"port": 19723, "params": {}}
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Response enthält `{"classificationSystemId": {"guid": "<system-guid>"}, "name": "SAB_Klassifizierung_29", ...}`.
System-GUID nie hartkodieren — immer per diesem Call ermitteln.

**Schritt 2 — Bestehende Klassifikation der Wand lesen:**

```
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elements": [{"elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"}}],
      "classificationSystemIds": [{"classificationSystemId": {"guid": "<system-guid>"}}]
    }
  }
)
```

<!-- 2026-05-19 verifiziert AC29 -->

Response zeigt aktuelle Klasse oder `null`. Damit wissen wir den Ist-Zustand für den Confirm.

Falls die Klassen-GUID der Zielklasse (z. B. „Außenwand") noch unbekannt ist:
`classifications_get_all_classifications_in_system` mit der System-GUID aufrufen — gibt die
vollständige Klassenhierarchie mit GUIDs zurück (Projekt-A: 299 Items, eine Antwort). <!-- verifiziert 2026-07-14 -->

**Schritt 3 — Confirm (SAFE-01):**

```
Ich werde folgendes klassifizieren:
- Wand f1101930-e0bd-7044-a1f2-fdb20e520e21
  Klassifikation (SAB_Klassifizierung_29): <alt oder leer> → "Außenwand"

Ausführen? (ja / nein / details / abbrechen)
```

**Schritt 4 — Klassifikation setzen nach Bestätigung** <!-- 2026-05-21 live verifiziert AC29 — Schema-Form korrigiert; alte Form "elementsWithClassifications + classifications[]" wird vom Server mit Pydantic-Error abgelehnt -->:

```
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": 19723,
    "params": {
      "elementClassifications": [{
        "elementId": {"guid": "f1101930-e0bd-7044-a1f2-fdb20e520e21"},
        "classificationId": {
          "classificationSystemId": {"guid": "<system-guid>"},
          "classificationItemId": {"guid": "<klassen-guid>"}
        }
      }]
    }
  }
)
```

`<klassen-guid>` ist die GUID der konkreten Klasse (z. B. „Außenwand"), nicht die System-GUID.
Beide GUIDs stammen aus den vorherigen Schritten — nie raten.

---

## Worked Example — Wand in Schichten aufteilen (Kern + Schalen) <!-- 2026-11-02 -->

Anlass: Bestandswand mit Befund „Mauerwerk + Klinker zum Flur + Trockenbau zum Raum“
soll als Einzelwände je Schicht im Modell stehen. Live Projekt-A 02.11.2026: 67 Wände,
94 neue Schalen, 16 Leere Türöffnungen, jede Wand auf 1 mm nachgemessen.

**Grundsatz: Die Bestandswand bleibt der Kern.** Ihre GUID, Klassifizierung und die
darin sitzenden Türen bleiben erhalten. Nur ihre Dicke und Lage ändern sich; die
Schalen kommen als neue Wände daneben. (Löschen + neu bauen wäre SAFE-02-widrig und
würde die Türen verlieren.)

Ablauf je Wand (Update am Bestand → vorher Summary + `ja` des Users):

1. **Vorprüfung, noch nichts ändern:** `ELM_SAB.GetUIState` (kein modaler Dialog,
   richtiges Projekt), `ELM_SAB.GetElementEditState` (bearbeitbar, keine Fremd-
   Reservierung), Dicke laut Modell = Dicke laut Befund, keine Fenster in der Wand,
   Wand ist nicht polygonal.
2. **Ist-Lage messen:** `floorPlanPolygons` der Wand auf die Normale der
   Originalachse projizieren (links positiv) → Kanten `lo`/`hi`. Diese Erstlesung
   sichern — sie ist die Referenz für alles Weitere (auch für einen Wiederanlauf).
3. `TapirCommand.ReserveElements` auf den Kern.
4. Kernbaustoff per `ELM_SAB.SetStructureOfElements` (`reserve:false`, Basic).
5. **Kern verschmälern:** `TapirCommand.ModifyWalls` mit `thickness`, danach neu
   messen und auf die Soll-Kanten schieben — bei Bezugslinie Mitte über die Achse,
   sonst über `offset` (siehe Gotcha 9).
6. **Schalen anlegen:** `TapirCommand.CreateWalls` je Schicht, Achse parallel zur
   Originalachse in Schalenmitte, `referenceLineLocation:"Center"`, `offset:0`,
   gleiches `floorIndex`/`zCoordinate`(=bottomOffset des Kerns)/`height`,
   `structureType:"Basic"` + `buildingMaterialId`.
7. Schalen **wie den Kern klassifizieren** (`API.SetClassificationsOfElements`),
   **KI-Stempel**, Ebene per `ELM_SAB.SetLayerOfElements` (CreateWalls kennt keine Ebene).
8. **Türen durchziehen:** je Kerntür in jede Schale `TapirCommand.CreateDoors` mit
   `libraryPartName:"Leere Türöffnung 27"`, gleichem `centerOffset`/`width`/`height`/
   `sillHeight`; Element-ID über die eingebaute Property `General_ElementID`
   („<Tür-ID> Schale <Material>“), Klassifizierung wie die Kerntür.
9. **Nachmessen** Kern + jede Schale gegen Soll-Intervall (Toleranz 1 mm), Baustoff,
   Stempel, Öffnungs-IDs — einmal vor und einmal nach `TapirCommand.TeamworkSend`.
10. `ReleaseElements` auf Kern **und alle neu angelegten Elemente** (Gotcha 10).
11. CSV-Protokoll + GUID-Register (Kern → neue Wände, Türöffnungen) ablegen.

Schichtregeln, Materialnamen und Ebenen sind projektspezifisch → Memory/Handoff,
nicht hier.

---

## Bulk-Klassifizierung (Stub)

Bulk-Klassifizierung aller Wände nach Innen/Außen ist ein wiederkehrender Kern-Workflow.
Die vollständigen Worked Examples folgen in Phase 5 nach Live-Verifikation. Bis dahin:
`../reference/bulk-operations.md` dokumentiert das universelle
Read → Filter → Group → Confirm → Apply-Pattern.

Kurzfassung für Wände: `elements_get_elements_by_type` (paginiert) → Properties +
Bounding-Box lesen → Layer/Zone-Zugehörigkeit via `elements_get_elements_related_to_zones`
auswerten → Gruppen bilden → Confirm (Summary-Format für > 10 Elemente) →
`elements_set_classifications_of_elements`.

**Unterklasse-Ableitung (Konstruktionstyp Beton-/Trockenbau-/MW-Wand): per MCP nur eingeschränkt möglich.** <!-- 2026-06-19 -->
Der Baustoff einer **Basic-Wand** ist auf Element-Ebene **nicht** lesbar:
`SurfaceAndMaterials_ComponentBuildingMaterialName` (514C85CB-…) liefert
`status: notAvailable` (es ist eine Pro-Komponenten-Property, kein Element-Wert).
`Construction_CompositeName` (704E9212-…) liefert **nur bei Composite-Wänden** einen
Namen, bei Basic-Wänden `null`. Folge: feine Unterklassen lassen sich für reine
Basic-Wände nicht zuverlässig ableiten. **Fallback-Heuristik: Layer-Name**
(`ModelView_LayerName`) — z. B. Layer „… Trennwand …" → Klasse „Trennwand".
Wo keine eindeutige Heuristik greift (Theke/Entwurf/SEO/2D/3D-Sammelsurium),
ehrlich auf die **Basisklasse „Wand"** klassifizieren statt zu raten.
Hinweis: `General_HotlinkAndElementID` ohne Hotlink-Präfix = lokales, klassifizierbares Element.

**Baustoff lesen/setzen:** Lesen geht über Tapir `GetDetailsOfElements` (`buildingMaterialId`), Setzen nur über `ELM_SAB.SetStructureOfElements` — Tapir `SetDetailsOfElements` quittiert, schreibt aber nicht. Rezept + „fehlt“-Fall: [`surfaces-materials.md`](surfaces-materials.md#wand-baustoff-ersetzen-basic-wände). <!-- 2026-11-12 -->

---

## Gotchas

1. **`elements_get_details_of_elements` ist in AC29 unzuverlässig.** Der MCP-Server
   blockiert die Response wegen eines Pydantic-Bugs (`extra='forbid'`): Archicad 29 gibt neue
   Felder zurück (`structureType`, `geometryType`, `arcAngle`, `slantAngle`,
   `verticalCurveHeight`), die das Schema nicht kennt. Stattdessen: Property-System,
   `elements_get_types_of_elements` oder `elements_get_2_d_bounding_boxes` verwenden.
   Memory: `~/.claude/projects/-Users-ap/memory/issue_archicad_mcp_get_details_bug.md`.
   <!-- 2026-05-19 -->

2. **Wand-Erstellung nicht via MCP v29.** `elements_create_walls` existiert nicht.
   User zeichnet manuell; wir holen die resultierende GUID und arbeiten damit weiter.
   Vollständige Capability-Tabelle in `../reference/mcp-conventions.md`.

3. **`begCoordinate`/`endCoordinate` sind Coord2D — kein Z-Feld.** Vertikale Lage
   wird über `floorIndex` + `bottomOffset` gesteuert. Ein `z`-Feld würde einen
   Argument-Fehler erzeugen.

4. **`layerIndex` ist ein Float (1-basiert).** Wert `3.0` = dritter Layer in der
   Projekt-Liste. `0.0` wäre ungültig. Index aus `attributes_get_attributes_by_type`
   mit `attributeType: "Layer"` ermitteln.

5. **Hosted-Element-Mit-Delete ist Archicad-Verhalten.** Wir löschen nur die Wand;
   das Fenster verschwindet durch Archicads Hosting-Logik. Trotzdem: SAFE-04-Pre-Check
   ist Pflicht, damit der User die Konsequenz kennt. Beim IFC-Export können verwaiste
   Öffnungs-Einträge zu stillen Datenfehlern führen (Pitfall P5 / Research).

6. **Klassifikations-System-GUIDs sind projekt-spezifisch.** Nie aus einem anderen
   Projekt übernehmen oder raten. Immer per `classifications_get_all_classification_systems`
   ermitteln — Cross-Pollution führt zu stillen Fehlklassifizierungen.

7. **Element-ID bleibt nach Update stabil (SAFE-05).** Nach `elements_set_details_of_elements`
   verändert sich die Wand-GUID nicht. Folge-Operationen (z. B. direkt danach klassifizieren)
   können dieselbe GUID weiterverwenden.

8. **Offene Wände ≠ Tragwerksbestand.** Bei „klassifiziere alle offenen Wände" zuerst
   per `ModelView_LayerName` gruppieren — der unklassifizierte Rest ist erfahrungsgemäß
   ein Sammelsurium (Theke-Entwurf, Bauzaun/Baustelleneinrichtung, Solid-Element-Operator-
   Hilfskörper, 2D-Darstellung, 3D-Massenstudie). Nicht blind auf eine Konstruktions-
   Unterklasse zwingen — SEO-Operatoren oder Bauzaun als „Betonwand" wäre sachlich falsch.
   Eindeutige Layer (Trennwand) → Unterklasse, Rest → Basis „Wand". <!-- 2026-06-19 -->

9. **`offset` wirkt nicht bei Bezugslinie Mitte.** `TapirCommand.ModifyWalls` mit
   `offset` meldet Erfolg, die Wand bleibt aber liegen, wenn
   `referenceLineLocation == "Center"`. Dann die Achse selbst senkrecht verschieben:
   `begCoordinate`/`endCoordinate` jeweils um `delta · Normale` versetzen (die Türen
   wandern mit, weil ihr `centerOffset` längs der Achse gilt). Bei Außen-/Innenkante
   als Bezugslinie geht `offset` — das Vorzeichen per Probe (+/−) und Nachmessung
   ermitteln. Immer nach dem Schieben aus `floorPlanPolygons` nachmessen.
   Live Projekt-A: ein Kern blieb deshalb verschmälert, aber unverschoben stehen. <!-- 2026-11-02 -->

10. **Neu angelegte Elemente bleiben auf dich reserviert.** In Teamwork gehört jedes
    per API erstellte Element (Wand, Tür, …) dem Anleger und bleibt reserviert, bis
    es nach `TeamworkSend` ausdrücklich per `ReleaseElements` freigegeben wird. Wer
    nur die vorher reservierten Bestandselemente freigibt, lässt die neuen gesperrt
    für die Kollegen — und meldet fälschlich „freigegeben“. GUIDs aus jedem Create
    sofort in die Freigabeliste aufnehmen; Abschluss per `GetElementEditState` prüfen.
    <!-- 2026-11-02 -->

11. **Wand ohne `floorPlanPolygons` → vor jeder Änderung abbrechen.** Manche Wände
    liefern keinen Grundrissumriss (verdeckt/von einem anderen Bauteil geschluckt).
    Ohne Ist-Kanten gibt es keine Soll-/Ist-Messung → nichts reservieren, nichts
    ändern, Wand dem User zur Sichtung melden. Bricht ein Lauf ab, nachdem der Kern
    schon verschmälert ist, mit den **Kanten aus der Erstlesung** fortsetzen, nicht
    neu messen — die aktuelle Wand ist ja schon dünner. <!-- 2026-11-02 -->

12. **`TapirCommand.ModifyWalls`: Felder direkt neben `elementId`.** Form
    `{"elementId":{…},"offset":0,"begCoordinate":{…}}`; ein `details`-Objekt gibt 4002.
    <!-- 2026-11-17 -->

13. **Referenzlinie auf die Wandseite legen, ohne die Wand zu bewegen.** Bei Versatz ≠ 0
    `offset:0` setzen und die Achse quer um den alten Versatz verschieben; bei Bezugslinie
    Mitte `referenceLineLocation:"Outside"` setzen und die Achse nachführen. Immer beide
    Wandkanten vorher und nachher messen und bei Abweichung über 1 mm zurücksetzen
    (live Projekt-A: 201 + 757 Wände, 3 zurückgesetzt). <!-- 2026-11-17 -->

14. **Stärke ändern mit fester Seite.** `thickness` setzen, Kanten messen, Achse quer
    nachschieben, bis die feste Kante wieder stimmt. <!-- 2026-11-17 -->

15. **„Failed to modify wall" (-2130312913) trotz eigener Reservierung = Ebene gesperrt.**
    `ELM_SAB.GetElementEditState` zeigt `layerLocked: true`. Das betrifft dann meist alle
    Ebenen (Ebenenkombination); der Nutzer muss umschalten. <!-- 2026-11-17 -->

---

## Verwandte Recipes

- [`openings.md`](openings.md) — Fenster, Türen, Wandöffnungen (hosten auf Wänden).
- [`zones.md`](zones.md) — Zonen / Räume; Wand kann Zone-Grenze sein.
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Confirm-Format,
  Capability-Tabelle, Fehlerklassen.
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Bulk-Klassifizierungs-Pattern.
- [`../reference/workflow-context.md`](../reference/workflow-context.md) — Warm-up-Felder im Detail.

## CLASS-Worked-Example: 4.500 Wände (live 2026-07-14, Projekt-A)

Muster: Klasse „Wand" (Item-GUID aus System-Baum) via `SetClassificationsOfElements`
(300er-Batches) → DANACH Properties: Bauteilname (singleEnum, Gebäudeflügel per
kNN über Referenzpunkte) + SAB_Brandschutz (multiEnum, nächste Referenzwand
≤ 2,5 m, gleiche Etage, sonst ehrlich leer). Ergebnis 4.500/4.500 + 2.171
Brandschutz-Werte, 0 Fehler. Confirm-Schleife davor: Summary-Tabelle mit
Pro-Klasse-Counts, `ja` startet. Details: reference/referenzmodell-abgleich.md.

## Überlappungen, Doppelungen und gerundete Ecken bereinigen <!-- 2026-11-17 -->

Live verifiziert am Projekt-A-Teamwork, ELMonkey 0.11.12.1, AC29.

1. **Echter Wandumriss im Grundriss.** `ELM_SAB.Get2DGeometryOfElements` liefert je Wand
   Primitive. Der echte Umriss nach Verschneidung ist das Primitiv `kind=poly`,
   `closed=true`, `insideHatchBorder=true` (`subPolyEnds` beachten). Wände, die nicht
   geschnitten werden (Brüstungen, Sturzbänder, Wände über/unter der Schnittebene),
   liefern KEIN solches Polygon, nur Umrisslinien. Ersatz: kleinstes gedrehtes Rechteck
   um alle Linienpunkte. Ohne diesen Ersatz fehlen diese Wände in jeder
   Überlappungsprüfung (am Projekt-A 150 gerade Wände, dadurch 40 Höhenüberschneidungen
   zunächst übersehen). Der Grundriss muss das aktive Fenster sein.
2. **Überlappungen richtig deuten.** Von rund 1000 rechnerischen Überlappungen sind etwa
   die Hälfte normale Wandanschlüsse (Ende läuft in die Querwand bis zur Referenzlinie)
   und ein Viertel andere Ebenen (Beschriftung, Fugen, Baustelle). Echte Fälle: ganz
   enthalten (≥ 95 %), parallel übereinander, mit Polygonwand, Stoß < 10 cm. Erst
   klassifizieren, dann handeln.
3. **Bereinigen, ohne den Gesamtumriss zu ändern.**
   - (a) Gerade Wand am Ende kürzen (`ModifyWalls` `begCoordinate`/`endCoordinate`), nur
     wenn der wegfallende Teil ungefähr der Überlappung entspricht, sonst ist es eine
     seitliche Überlappung. Nie den Anfang einer Wand mit Tür/Fenster verschieben (Türen
     hängen am Wandanfang, `centerOffset`), das Ende nur mit Abstand zur Tür.
   - (b) Polygonwand = alte Fläche minus Nachbar neu bauen.
   - (c) Rangregel: Dämmung und Leichtwand ohne Öffnungen weichen Mauerwerk/Beton.
   - (d) Höhen: Steckt eine Wand im Grundriss ganz in einer Wand, die erst weiter oben
     beginnt, Höhe der unteren per `ModifyWalls {height}` bis zur Unterkante kürzen.
   - Nach jedem Schritt Umrisse neu lesen und Überlappung nachmessen, bei Abweichung
     zurücksetzen. Je Wand nur eine Aktion pro Lauf, danach neu auslesen (der Plan
     enthält sonst alte Koordinaten/GUIDs).
4. **Polygonwand-Umriss ändern geht nicht, daher neu bauen mit Datenübernahme.**
   `ELM_SAB.CreatePolygonWalls` (`polygonCoordinates` ohne Wiederholung des ersten
   Punkts, `floorIndex`, `height`, `bottomOffset`, `layerIndex`, `buildingMaterialIndex`
   bzw. `compositeIndex`; Attribut-Index aus Tapir `GetAttributesByType`). Danach:
   - Geschossbindung oben per Tapir `ModifyWalls {relativeTopStory, topOffset}` nachziehen.
   - Klassifizierung kopieren (`API.GetClassificationsOfElements` /
     `SetClassificationsOfElements`) und eine vom Werkzeug-Standard geerbte
     Klassifizierung ENTFERNEN, wenn die alte Wand keine hatte (`classificationId` nur
     mit `classificationSystemId`, ohne `classificationItemId`).
   - Alle benutzerdefinierten Eigenschaften mit `status=normal` kopieren, dazu die
     eingebauten `Category_StructuralFunction`, `Category_Position`,
     `Category_RenovationStatus`, `General_ElementID` (`API.GetPropertyIds`,
     `type: BuiltIn`, `nonLocalizedName`).
   - KI-Stempel setzen; Rücklese vergleichen (Höhe, Fußpunkt, Baustoff, Ebene, Stifte,
     Sichtbarkeit); erst dann die alte Wand löschen.
   - Auch gerade Wände ohne Öffnungen lassen sich so in Polygonwände umwandeln.
   - `CreatePolygonWalls` schlägt bei manchen sehr schmalen/entarteten Umrissen fehl
     (Code -2130313102): Fall auslassen.
5. **Durchbrüche (Opening): Wirtswand lesen.** `GetDetailsOfElements` meldet „Not yet
   supported element type“ und `API.Get3DBoundingBoxes` Fehler 6000, aber die Wirtswand
   lässt sich lesen: Tapir `GetConnectedElements {elements: <Wände>,
   connectedElementType: "Opening"}` (am Projekt-A 102 von 114 Durchbrüchen in 36 Wänden; der
   Rest sitzt in Decken). Vor jedem Löschen/Kürzen einer Wand prüfen; zusätzlich
   Türen/Fenster/Durchbrüche vor und nach jedem Löschen zählen.
6. **Gerundete Ecken aus dem Bestands-DXF.** Bögen stehen als `ARC` und als
   `LWPOLYLINE`-Bulge im DXF (auch in `INSERT`s); reine Linien-Auszüge enthalten sie
   nicht. Je Bogen: Mittelpunkt, Radius, Endpunkte, scharfe Ecke q = Schnitt der
   Tangenten.
   - Zwickel (q + Bogen) liegt in der Wand: abrunden.
   - Wandende gerade/abgeschrägt und der Bogen wölbt nach außen: Kreisabschnitt
     (Sehne–Bogen) anfügen.
   - Gerade Wand: Ende um das Eckstück kürzen und das Eckstück als Polygonwand mit
     Bogenpunkten (8 Segmente je Viertelkreis) bauen. Polygonwand: mit Rundung neu bauen.
   - Zwickel um einen 12-mm-Rand außerhalb der Tangenten erweitern, sonst bleiben
     Splitter stehen, wenn die Modellkante wenige mm neben dem Bestand liegt.
   - Vor dem Bauen Vorschau zeichnen: Gehrungs-Enden ergeben Pfeilformen (auslassen);
     liegt das Modell etwa 1 cm neben dem Bestand, entstehen Stufen im Umriss (Geschoss
     auslassen).
   - Befund am Projekt-A: von 574 Bestandsbögen waren nur etwa 100 direkt baubar, 208 ohne
     Modellwand, 132 mit Modellkante > 8 mm neben dem Bestand.
   - **Bögen an Türöffnungen (Leibungen)** liegen mitten in der Wand, nicht am Wandende:
     Dort ist kein Eckstück möglich, ohne die Türwand umzubauen. <!-- 2026-11-17 -->
   - Ein Wandumriss aus ShapePrims kann durch Türen in mehrere Polygone zerfallen; dann
     nur das Stück am Bogen betrachten. <!-- 2026-11-17 -->
   - Die Vorschau zeigt nur die betroffene Wand. Vorher prüfen, ob diese Wand selbst eine
     Doppelung in einer anderen Wand ist, sonst wird die Rundung an der falschen Wand
     gebaut. <!-- 2026-11-17 -->
7. **Python-Falle:** shapely `polygon.interiors` ist auch ohne
   Löcher „wahr“. Immer `len(poly.interiors) > 0` prüfen.

## Wände gegen den Bestandsplan abgleichen (Einpassung, Wandstärke) <!-- 2026-11-17 -->

Live am Projekt-A-Teamwork, AC29 Mac.

1. **Systematischer Versatz eines Geschosses: zuerst die Einpassung prüfen, nicht die
   Wände.** Weichen im Vergleich Modell ↔ Bestands-DXF fast alle Wände eines Geschosses
   gleichsinnig ab, den Ausgleich über alle geraden Wände rechnen: je Wand Abweichung
   quer zur Achse = n·t + w·((x−c)×n) (Verschiebung t, Drehung w um den Schwerpunkt c),
   robust (Ausreißer > 6 mm raus, zweiter Durchgang).
   - Projekt-A: 1. OG (−12,8/+5,5) mm und −0,18 mrad, 3. OG (+5,6/+6,3) mm und +0,12 mrad;
     Rest danach im Median 0,3–0,4 mm. Im 1. OG lagen danach 381 statt 107 Wände exakt
     auf dem Bestand (vorher 254 „verschieben“, danach 14).
   - Die Einpassung (Transformation des Bestandsplans) korrigieren und sichern; das
     Modell bleibt unberührt.
2. **Gleichmäßige 4–8 mm „Versatz“ gegen den Bestand sind meist Putzlinien**, keine
   falsch liegenden Wände: Im Bestand liegen dort zwei dicht benachbarte Linien. Nicht
   verschieben. Abweichungen von 3–5 cm sind meist abgetreppte Bestandsformen
   (Vorsprünge), keine Verschiebung.
3. **Wandstärke auf Bestand: Schutzregeln**, sonst greift die Messung die falsche Linie.
   - Eine Seite muss schon auf dem Bestand liegen (≤ 3 mm).
   - Änderung ≤ 6 cm.
   - Ergebnis < 2 cm verwerfen: Dann wurde eine Linie im Wandinneren erkannt (z. B.
     Plattenlage einer Ständerwand).
   - Klinker bleibt 11,5 cm (Bürovorgabe), auch wenn der Bestand 12,5 zeigt (Fuge/Putz).
   - Bekleidung ≤ 3 cm bleibt.
   - Keine neue Überlappung mit parallelen Nachbarwänden oder Polygonwänden
     (Querwände sind Anschlüsse und dürfen mehr überlappen).
   - Erst 5 Wände als Muster, dann Serie.
   - **Verbundwände** (`structureType` Composite): `ModifyWalls {thickness}` wirkt
     nicht, die Nachmessung setzt korrekt zurück. Stärke nur über den Verbund.
