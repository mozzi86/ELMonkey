# Schwarz Architekturbüro — Office-spezifische Fakten

Konsolidierte projekt-/template-spezifische Befunde aus dem Schwarz-Architekturbüro-Setup, die für alle Kollegen mit dem gleichen Template relevant sind. Diese Fakten ändern sich nur bei Template-Updates des Büros (selten) und sind sonst stabil über alle Schwarz-Projekte hinweg.

<!-- konsolidiert 2026-05-22 aus User-Memory (Mudi) — bei Template-Update neu verifizieren -->

## Inhalt

- [Klassifikations-Systeme](#klassifikations-systeme)
- [SAB_Klassifizierung_29 — ClassificationItem-GUIDs](#sab_klassifizierung_29--classificationitem-guids)
- [Layer-Naming-Konvention (Z_/A_-Schema)](#layer-naming-konvention-z_a_-schema)
- [Pen-Tabellen](#pen-tabellen)
- [Bekannte Schwarz-Template-Bugs](#bekannte-schwarz-template-bugs)
- [Teamwork-spezifische Stolperfallen](#teamwork-spezifische-stolperfallen)
- [Typische Workflows](#typische-workflows)

## Klassifikations-Systeme

Zwei aktive Systeme im Schwarz-Template:

| Name | GUID | Source | Version | Status |
|---|---|---|---|---|
| `SAB_Klassifizierung` | `0eae85a4-23e0-5243-b1cc-d6465a28dfc9` | ELM | 01 (2025-03-06) | Legacy, in älteren Projekten aktiv (z.B. AFW-WC) |
| `SAB_Klassifizierung_29` | `cca1f9aa-ca6c-3444-9bfb-363c6145327e` | ELM + Schwarz-Extensions + Capmo | 02 (2026-05-18) | Aktuell, für neue Projekte ab AC29 |

**Bei Projekt-Auftakt prüfen:** Welches System ist im konkreten Projekt aktiv? Per MCP via `classifications_get_all_classification_systems`. Beim AFW-WC-Projekt z.B. ist es das Legacy-System — die GUIDs aus SAB_29-Memory passen NICHT.

## SAB_Klassifizierung_29 — ClassificationItem-GUIDs

System-GUID: `cca1f9aa-ca6c-3444-9bfb-363c6145327e`

| Konzept | ClassificationItem-GUID | Pfad |
|---|---|---|
| Wand (Oberkategorie) | `d543ae8f-cf98-9a4c-99ed-35e4aef73565` | ELEMENTE > Wand |
| MW-Wand (Mauerwerk) | `fd317914-3c36-604d-b094-ecf824666301` | ELEMENTE > Wand > MW-Wand |
| Betonwand | `84893816-736c-5d42-be74-bcf148487b27` | ELEMENTE > Wand > Betonwand |
| Trennwand | `932504f3-7f7f-494c-8ea1-92be4dbff0f3` | ELEMENTE > Wand > Trennwand |
| Trockenbauwand | `8187be07-ed29-3847-b190-e87d63e70644` | ELEMENTE > Wand > Trockenbauwand |
| Elementwand | `64df12c7-801a-fa4d-9b99-0d66dba285b9` | ELEMENTE > Wand > Elementwand |
| Fenster | `8a46db63-2597-5942-bd5b-6ffb8bff3c30` | ELEMENTE > Fenster > Fenster |
| Dachfenster | `02c43b88-6a94-854f-ab0c-eeca58ced47e` | ELEMENTE > Fenster > Dachfenster |
| Fenstertür | `e67e0dd4-7d5d-5646-99ee-4e93790757a8` | ELEMENTE > Fenster > Fenstertür |
| Tür | `765aed84-6644-0247-9a22-f0ccb633d601` | ELEMENTE > Tür > Tür |
| Innenraum | `877f3785-b278-9b4a-b63c-dc8c7f679d9e` | ELEMENTE > Raum > Innenraum |
| Außenraum | `b63f4790-baea-0648-b0fb-c845375a7c53` | ELEMENTE > Raum > Aussenraum |
| Decke | `96a3a735-0beb-4948-b009-09fd98d60ceb` | ELEMENTE > Decke |
| Rohbaudecke | `3de2bfef-f58b-7b4f-8006-1434684195db` | ELEMENTE > Decke > Rohbaudecke |
| Abgehängte Decke | `295ade7e-34f1-2a46-bf01-c4c07d767028` | ELEMENTE > Bekleidung/Belag > Abgehängte Decke |
| Stütze | `dd96d02a-dd21-1148-bc5e-35c0f500c0fe` | ELEMENTE > Stütze > Stütze |
| Träger | `7391f247-bbf7-6844-8768-472e1b6da80c` | ELEMENTE > Träger / Balken / Unterzug |
| Fassade (Top-Level) | `ea87e04f-7e66-5b4f-85b3-eefc34202e9c` | ELEMENTE > Fassade |
| Fassade Paneel | `6226536b-0f5d-a748-a73c-73fa09ec0e40` | ELEMENTE > Fassade > Fassade Paneel |
| Fassade Profil | `f33ed439-539c-024d-867a-8aa142506bc3` | ELEMENTE > Fassade > Fassade Profil |
| Möbel | `5d3e7d17-4a79-f348-a770-5a2bb167fadb` | ELEMENTE > Möbel |
| Sanitäreinrichtung | `ed7be860-81d2-b345-ade0-c2d31dfb00cd` | … > Endgerät > Sanitäreinrichtung |

**Wichtige Lücke:** „Innenwand" / „Außenwand" gibt es als Klassen NICHT in SAB_29 — nur „Wand" als Oberkategorie + Material-Subtypen. Innen/Außen-Unterscheidung muss über separate Property ODER Layer-Konvention (Z_/A_-Prefix) abgebildet werden. Räume sind dagegen sauber getrennt (Innenraum vs. Aussenraum).

## SAB_Klassifizierung (Legacy) — Top-Level-GUIDs

System-GUID: `0eae85a4-23e0-5243-b1cc-d6465a28dfc9`

| Konzept | ClassificationItem-GUID |
|---|---|
| Wand | `6ac0f2d7-6912-204a-ad86-3c68142401ea` |
| Tür / Tor | `7d57a1c6-236c-a64a-8c92-0d880023caf1` |
| Fenster / Dachfenster / Lichtkuppel | `450a964f-61d8-fc4e-a03e-572dc6812066` |
| Decke | `90b19b0b-a2ec-c041-b834-6f7d374e0749` |
| Dach | `4f7047a3-9fee-c141-b617-7c830e3595cc` |
| Stütze / Pfeiler | `53346f40-a8a6-654c-b7db-bfede2a74199` |
| Träger / Balken / Unterzug | `38592abd-0026-7f48-8fc1-0e4053bb41f6` |
| Treppe | `023c6d7f-852f-d544-a8d2-993e095eed06` |
| Möbel | `7acf20eb-f145-964a-8b67-997f69f49177` |
| Leuchte | `9554d137-4a8a-a941-939e-ebdf432112b8` |
| Raum (Oberkategorie) | `6e1ad0c1-b5b5-a14c-90f0-e8751ae2617d` |
| Fassade | `e431c33b-e470-5e42-98c9-f403fad93100` |
| Geländer | `8d7fb6d6-3311-e845-9f60-33a82a2b0fa5` |
| Fundament | `a1ed9b97-dc3a-d64d-8742-091cf5f51ab6` |
| Bekleidung / Belag | `0f8e2d99-42c2-834e-b42f-2b5adf0b9d5a` |
| Durchbrüche / Schlitze | `f3c25b11-e8a2-f14d-b959-52abce86c73e` |
| Rampe | `dec3aecb-7f77-fc47-8f28-83761fd62d44` |
| Schornstein | `5de8075d-f291-a64c-b0d7-899cf0ee994b` |
| Sonnenschutz | `4ad45d3e-70cb-374b-ac77-46304ad7fce3` |

## Layer-Naming-Konvention (Z_/A_-Schema)

Strukturierte 2-Prefix-Konvention:

- **`Z_<NN>_<NAME>`** — Plan/sheet-level Layers (Z_06_PLANKOPF, Z_10_LEGENDE, Z_01_HILFSLINIE, Z_05_PLANRAHMEN, Z_03_BEMASSPKT, …)
- **`A_<NN>_<NAME>`** — Architektur/Model-level Layers mit numerischen Mid-Prefixen:

| Range | Domain |
|---|---|
| `A_2X` | Möbel/Einbau (A_25_MOEBEL, A_26_MOEBEL_EINBAU, A_27_EINBAU_BESOND, A_28_EINBAU_SANIT, A_29_EINBAU_HEIZ) |
| `A_3X` | Räume und Flächen (A_301_ADTRAUM, A_311_UMGRENZ, A_32_FLAECHENART, A_33_ZONE, A_34_BGF_DIN277, A_35_BRI_DIN277) |
| `A_4X` | Massenelemente (A_431_MASSENELEMENT, A_432_MASSENGRUPPE, A_433_MASSENSCHNITT, A_401_BAUTEILTABELLE) |
| `A_5X` | Aufmaß/Scan (A_50_AUFMASS, A_50_SCAN, A_50_SCAN_RASTER) |
| `A_6X` | Schnitt, Gelände, Mensch, Auto, Schatten, Baum |
| `A_7X` | Brandschutz, Kunst |
| `A_8X` | Symbole, Topographie, Kontrolle |

**Suffix-Konventionen:**
- `_BS` = Bestand
- `_BM` = (unklar, prüfen)
- `_ABZUG` = Abzug

Plus:
- `X_REF` — externe Referenzen
- `Defpoints` — AutoCAD-Kompatibilität

## Pen-Tabellen

Sechs Pen-Tabellen im Template, alle scale-basiert benannt:

- „ArchiCAD 1:100/200"
- „ArchiCAD 1:20/50"
- „S/W 1:100/200" (S/W = Schwarz/Weiß)
- „S/W 1:20/50"
- „AutoCAD Stifte"
- „AutoCAD Farben"

Bei Plan-Erstellung: passende Pen-Table je Plan-Maßstab wählen.

## Bekannte Schwarz-Template-Bugs

### IFC-Übersetzer mapped ALLES als IfcBuildingElementProxy (kritisch)

<!-- 2026-05-21 live-verifiziert am AFW-WC-Projekt -->

Der IFC-Übersetzer („Allgemeiner Übersetzer IFC4") im Schwarz-Template hat die Typ-Zuordnung auf **„Klassifikations-basiert"** stehen, aber das **Quell-Klassifizierungssystem ist nicht gesetzt** („Nicht verfügbar"). Folge: alle Bauteile (Wände, Türen, Fenster, Decken, Stützen, …) fallen in die Default-Kategorie „Nicht klassifizierte Elemente" und werden als `IfcBuildingElementProxy` exportiert. Die SAB-Klassifikation der Elemente wird ignoriert.

**Symptom:** IFC-Export enthält 0× IfcWall/IfcDoor/IfcWindow/IfcSlab, alles ist IfcBuildingElementProxy mit Pset_BuildingElementProxyCommon statt Pset_WallCommon. Auch im Archicad-internen „IFC-Eigenschaften verwalten"-Dialog zeigt jede Wand `IfcBuildingElementProxy` — selbst nach SAB-Klassifikation.

**Fix (Archicad UI, NICHT via MCP):**

1. `Datei → Interoperabilität → IFC → IFC-Übersetzer einstellen`
2. „Allgemeiner Übersetzer IFC4" wählen
3. Auf `…` rechts neben **„Typ-Zuordnung"** klicken
4. Im Dialog: Radio-Button von **„Klassifizierung"** auf **„Element-Typ"** umschalten
5. Die Element-Typ-Mapping-Regeln prüfen — sollten standardmäßig korrekt sein (Wand→IfcWallStandardCase, Tür→IfcDoor, etc.); falls nicht: pro Zeile korrigieren

**Empfohlene Ziel-Mappings:**

| Archicad-Typ | IFC-Typ | PredefinedType |
|---|---|---|
| Wand | `IfcWallStandardCase` | STANDARD |
| Tür | `IfcDoor` | — |
| Fenster | `IfcWindow` | — |
| Decke (Rohbaudecke) | `IfcSlab` | FLOOR |
| Decke (Bodenplatte) | `IfcSlab` | BASESLAB |
| Decke (abgehängt) | `IfcCovering` | CEILING |
| Dach | `IfcRoof` | — |
| Stütze | `IfcColumn` | — |
| Träger / Unterzug | `IfcBeam` | BEAM |
| Treppe | `IfcStair` | — |
| Geländer | `IfcRailing` | — |
| Vorhangfassade | `IfcCurtainWall` | — |
| Zone | `IfcSpace` | — |
| Möbel | `IfcFurnishingElement` | — |
| Sanitäreinrichtung | `IfcSanitaryTerminal` | — |
| Leuchte | `IfcLightFixture` | — |

Diesen Fix sollte das Office einmal zentral im Vorlagen-Template machen, dann profitieren alle künftigen Projekte.

## Teamwork-spezifische Stolperfallen

### Reservierungs-Pflicht für alle Write-Operationen

<!-- 2026-05-21 verifiziert AC29 -->

In Teamwork-Projekten **muss jedes Element vom aktuellen User reserviert sein**, bevor man via MCP `set_classifications`, `set_property_values`, `set_details` etc. aufrufen kann. Ohne Reservierung kommt Fehler-Code `-2130312310` („Failed to set classification item for element").

**Workaround:** Immer vorher `mcp__archicad__teamwork_reserve_elements` auf die Ziel-GUIDs aufrufen. Idempotent — schadet nicht falls schon reserviert.

**Workflow-Pattern:**
1. Element-Inventar holen
2. Reserve in einem Bulk-Call (alle GUIDs)
3. Set-Operation
4. Optional: `teamwork_release_elements` nach Abschluss

### Übersetzer-/Attribut-Reservierungs-Konflikte

IFC-Übersetzer, Klassifikations-Systeme, Eigenschaften können von einem User reserviert sein. Vor dem Editieren prüfen:
- Roter Punkt im Teamwork-Status → von jemand anders reserviert; „Anfragen" klicken
- Blauer Punkt „Frei zur Reservierung" → kann reserviert werden
- Grüner Punkt „Bearbeitbar" → eigene Reservierung aktiv

### Pagination-Session-TTL

`page_token` von `elements_get_elements_by_type` & Co. verfällt nach kurzer Zeit. Fehler: „Pagination session expired". Pagination muss **sequenziell und zeitnah** durchgezogen werden — nicht parallel zu vielen anderen Calls. Alternative für große Inventare: IFC-Export als statische Quelle nutzen (siehe `dwg-ifc-import.md`).

## Typische Workflows

### Bodenbelag-Sync (66-Räume-Projekt)

User Mudi pflegt regelmäßig Property/GDL-Stempel-Sync für Bodenbeläge in Raum-Schedules (z.B. „00-02 Räume Nordflügel+Neubau"). Workflow-Details siehe Memory `workflow_archicad_bodenbelag_sync.md` (intern beim User) — Kernpunkt: Expression-Linking ist eleganter als Bulk-Copy.

### Bulk-Klassifizierung (Innen/Außen, tragend/nicht-tragend)

Wiederkehrender Kern-Workflow. Pattern: Read → Filter → Group → Confirm → Apply → Verify (reverse-lookup wegen silent-success-Bug). Details: `bulk-operations.md`.

### DWG-Lageplan → IFC-Decken-Hotlink

Externe Lagepläne (Liegenschaftspläne, Bestandsaufnahmen) kommen oft als 2D-DWG. Konvertierung zu IFC4 mit gestaffelten IfcSlab-Decken: siehe `dwg-ifc-import.md`. Übertragung auf KG 300 Baukonstruktionen: `dwg-ifc-kg300.md`.

---

**Bei Änderungen am Schwarz-Template:** Diesen File aktualisieren + Datums-Marker setzen. Bei Klassifikations-System-Update auch die GUID-Tabellen neu live-extrahieren.
