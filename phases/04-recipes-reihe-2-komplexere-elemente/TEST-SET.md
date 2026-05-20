# Phase 4 Test-Set

Live-Setup vom 2026-05-20 gegen Archicad 29 / Port 19723 / „Ohne Titel"-Sandkasten.

**Hinweis:** GUIDs sind Sandkasten-spezifisch. In Recipe-Worked-Examples als `<placeholder-guid>` referenzieren, nicht hartkodieren. Diese Datei dient nur als Wave-2-Subagent-Input.

## CurtainWall + Sub-Elemente

| Element-Typ | Anzahl | Beispiel-GUID |
|---|---|---|
| **CurtainWall (Top-Level)** | 1 | `5d5ad6b2-6bd5-d449-9d96-6c8f04139c84` |
| CurtainWallSegment | 1 | `ea9426d5-8fc3-b64b-bd1a-6ae32d5a78aa` |
| CurtainWallFrame | 31 | `784cb66b-6324-bc44-8247-ba8cded2c7ec` (1/31), … |
| CurtainWallPanel | 12 | `3a4643c7-6914-0847-b9f1-eeccd3c210c3` (1/12), … |
| CurtainWallJunction | 0 | — (nicht in Default-CurtainWall vorhanden) |
| CurtainWallAccessory | 0 | — (nicht in Default-CurtainWall vorhanden) |

**Hierarchie-Aufruf live verifiziert:**
- `elements_get_subelements_of_hierarchical_elements` mit Top-Level-GUID → liefert alle 44 Sub-Elemente gruppiert nach Typ (cWallSegments, cWallFrames, cWallPanels)

## Library-Objects

| Element-Typ | Anzahl | Beispiel-GUIDs |
|---|---|---|
| **Object** | 4 | `f4574e72-1399-ea41-a64e-32e992bb2efd` (Beispiel für Worked-Example), `2695f108-df62-a84b-af2a-9537bfe97806`, `fde581f6-70e1-634d-9a50-a02664a2b002`, `375a7aef-099d-b948-889c-9957084cec1b` |

**Live-Befund:** Object hat **~230 Built-in Properties** pro Element (riesiges Set für GDL-Library-Items). Inklusive Klassifikations-System-Refs für die zwei aktiven Systeme (SAB_Klassifizierung + SAB_Klassifizierung_29 GUIDs).

## Phase-3-Test-Elemente (noch im Projekt, falls Recipes darauf verweisen)

| Element | GUID | Notiz |
|---|---|---|
| Wall (Phase 3) | `f1101930-e0bd-7044-a1f2-fdb20e520e21` | User-gezeichnet Phase 3 |
| Wall (zusätzlich) | `917aa78c-f309-3c4c-9e9e-7b86f98e2054` | unklar — vermutlich versehentlich beim Test gezeichnet |
| Window | `7185f21a-ca8f-6b44-a8a3-28d0610f0d82` | Phase 3 |
| Slab | `01b90e9f-a241-dc45-a448-5acc06a186c4` | Phase 3 |
| Column (Phase 3) | `41adb22a-a347-784a-bcba-ac6137ce76e3` | Phase 3 |
| Column (zusätzlich) | `eb761a09-1bcc-f44f-af64-8ab8aa520e2e` | unklar |
| Zone „Test-Zone" T01 | `e0394527-a1fa-b24b-bab2-d75ffce4cc7e` | Phase 3 |

## Libraries (aktiv im Projekt)

19 Libraries geladen, darunter:
- BuiltInLibrary: BuiltInLibraryParts.libpack + diverse Add-On-Bundles
- EmbeddedLibrary: AutoSave-AC-ARM-29 (Emb_3305904328)
- LocalLibrary: Archicad Migrationsbibliotheken (AC28)
- **ServerLibrary: „BIBLIOTHEKEN 28"** auf https://schwarz.bimcloud.com (BIMcloud-gehostet, Read-Write)

## Wave-2-Subagent-Hinweise

- **Sub-Element-Hierarchie:** Verwende `elements_get_subelements_of_hierarchical_elements`, NICHT `elements_get_connected_elements` (letzteres liefert leer für CurtainWall, nur für Host-Hosted-Beziehungen).
- **Modal-Block live aufgetreten:** Code 4001 mit „Attribute-Manager" — der User hatte den Dialog versehentlich offen, mussten ihn schließen lassen. Recipe muss diesen Reflex bei Worked Examples erwähnen.
- **GUID-Format:** Alles UUID v4 mit Bindestrichen.
- **Placeholder in Recipes:** GUIDs aus dieser Test-Datei in Worked Examples als `<top-level-cw-guid>`, `<frame-guid>`, `<panel-guid>`, `<object-guid>` referenzieren.
