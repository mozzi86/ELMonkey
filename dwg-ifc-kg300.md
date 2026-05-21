# DWG → IFC Pipeline für KG 300 (Bauwerk: Baukonstruktionen)

Verallgemeinerung der Lageplan-Pipeline ([`dwg-ifc-import.md`](dwg-ifc-import.md)) auf die Baukonstruktions-Komponenten der KG 300 nach DIN 276. Gleiche 4 Pipeline-Stufen (DWG → DXF → Pro-Klasse Geometrie-Recovery → Pro-Klasse IFC-Bau → STEP-Text-Merge → Archicad-Hotlink), aber pro Cost-Group andere IFC-Entity, andere Polygonization-Strategie, andere Layer-Annahmen, andere Z-Schemata.

**Plus: umgekehrte Richtung** — Bulk-Zuweisung einer KG-Nummer-Property auf bereits in Archicad modellierte Elemente, damit Schedules/Stücklisten nach Cost-Group filterbar werden.

<!-- VERIFY — nur Lageplan/KG-500-Spezialfall ist live verifiziert (2026-05-12). Die KG-300-Sektionen sind Pattern-Vorlagen; werden bei erster realer Anwendung pro KG mit Datums-Marker bestätigt. Bulk-KG-Zuweisung folgt dem in bulk-operations.md verifizierten Pattern. -->

## Inhalt

- [Pipeline-Pattern (generisch)](#pipeline-pattern-generisch)
- [Vier Polygonization-Strategien](#vier-polygonization-strategien)
- [KG 310 — Baugrube / Erdbau](#kg-310--baugrube--erdbau)
- [KG 320 — Gründung](#kg-320--gründung)
- [KG 330 — Außenwände](#kg-330--außenwände)
- [KG 340 — Innenwände](#kg-340--innenwände)
- [KG 350 — Decken](#kg-350--decken)
- [KG 360 — Dächer](#kg-360--dächer)
- [KG 370 — Treppen und Rampen](#kg-370--treppen-und-rampen)
- [KG 380 — Außenfenster und -türen](#kg-380--außenfenster-und--türen)
- [KG 390 — Innenabschlüsse und -türen](#kg-390--innenabschlüsse-und--türen)
- [KG 395 — Sonstige Baukonstruktionen](#kg-395--sonstige-baukonstruktionen)
- [Bulk-KG-Zuweisung auf bestehende Archicad-Elemente](#bulk-kg-zuweisung-auf-bestehende-archicad-elemente)
- [Mapping KG 300 ↔ SAB_Klassifizierung_29](#mapping-kg-300--sab_klassifizierung_29)
- [Was die Pipeline NICHT kann](#was-die-pipeline-nicht-kann)
- [Anwendungs-Hinweise aus realen Werkplänen](#anwendungs-hinweise-aus-realen-werkplänen)

## Pipeline-Pattern (generisch)

Identische 4 Stufen wie Lageplan-Pipeline:

1. **DWG → DXF** via LibreDWG `dwg2dxf`
2. **Pro-Klasse Geometrie-Recovery** — Pickle pro Klasse (Strategie 1-4 unten)
3. **Pro-Klasse IFC-Bau** — passende IFC-Entity, eigenes `step_build_*.py`-Modul pro Strategie
4. **STEP-Text-Merge** in eine IFC-Datei → Archicad-Hotlink

Skripte aus dem Lageplan-Repo (Pfad in [`dwg-ifc-import.md` § Skript-Standort](dwg-ifc-import.md#skript-standort)) sind der Ausgangspunkt — `step_polygonize_v4.py` deckt Strategie S1 (Plan-Polygon) ab; für S2-S4 sind neue Module nötig.

## Vier Polygonization-Strategien

| Nr | Strategie | Wann | IFC-Konstrukt | Beispiele |
|---|---|---|---|---|
| **S1** | **Plan-Polygon + Z-Extrude** | Element flach, XY-Außenkante geschlossen, gleichmäßige Höhe | `IfcSlab`, `IfcFooting`, `IfcCovering` (Boden) | Bodenplatte, Rohbaudecke, Flachdach, Bodenbelag |
| **S2** | **Centerline + Z-Extrude** | Element linear, Centerline + Wandstärke bekannt | `IfcWall` (StandardCase) | Außenwand, Innenwand |
| **S3** | **Point + Z-Extrude** | Element punktuell, Profil bekannt | `IfcColumn` | Stütze, Pfeiler |
| **S4** | **Profile-Sweep** | Element folgt Pfad mit fixem Querschnitt | `IfcBeam`, `IfcStairFlight`, `IfcRailing` | Träger, Treppenlauf, Handlauf |

**S1 ist live verifiziert** (Lageplan-Pipeline, 5.385 Slabs). **S2-S4 sind Pattern-Vorlagen** — bei erster realer Anwendung verifizieren.

Für S2 (Wände) ist die kritische Frage: woher kommt die Centerline?
- **(a)** Eigener DWG-Layer mit Wand-Achslinien (selten gepflegt)
- **(b)** Aus dem Wand-Polygon (Außenkante + Innenkante) → Centerline = Skelett (Shapely Voronoi/Polygon-Skelett)
- **(c)** Aus Linien-Paaren (Außenfläche-Linie + Innenfläche-Linie auf separaten Layern) → Centerline = Mittellinie der Paare

(a) am einfachsten falls verfügbar. (c) am robustesten wenn Office-Konvention Außen-/Innenflächen auf separaten Layern führt.

## KG 310 — Baugrube / Erdbau

DIN 276: Bodenaushub, Verbau, Wasserhaltung, Erdtransport.

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Aushub-Kubatur | (nicht direkt; Differenz IfcSite − Soll) | S1 | `Erdbau_*`, `Aushub_*` | Gelände-OK → Aushub-Sohle |
| Spundwand / Verbau | `IfcWall` mit `ObjectType="Spundwand"` | S2 | `Verbau_*`, `Spundwand_*` | Sohle − 1 m → Gelände + 1 m |
| Wasserhaltung | nicht modelliert (Bau-Logistik) | — | — | — |

**Pipeline-Anwendung:** Selten. Baugrube wird meist als IfcSite + Geländemodell modelliert, nicht aus DWG-Plan-Polygonen.

**SAB_29-Mapping:** Nicht abgedeckt — Property `KG_Nummer = "310"` nötig.

<!-- VERIFY -->

## KG 320 — Gründung

DIN 276: Streifenfundamente, Einzelfundamente, Plattenfundamente, Bodenplatten, Unterfangungen.

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Streifenfundament | `IfcFooting` `PredefinedType="STRIP_FOOTING"` | S2 oder S1 | `Fundament_Streifen`, `KG_Fundament` | Sohle → OK Fundament |
| Einzelfundament | `IfcFooting` `PredefinedType="PAD_FOOTING"` | S3 | `Fundament_Einzel` | Sohle → OK |
| Bodenplatte | `IfcSlab` `PredefinedType="BASESLAB"` | S1 | `Bodenplatte`, `BPL_*` | OK Sauberkeitsschicht → OK BPL |
| Unterfangung | `IfcFooting` `ObjectType="Unterfangung"` | S1 oder S2 | `Unterfangung_*` | Bestand-Sohle − 0.5 m → Bestand-OK |

**Pipeline-Anwendung:** Gut geeignet für Sanierung — Bestands-Fundament-Pläne sind oft nur 2D-DWG. Pipeline rekonstruiert XY-Außenkante; Höhe muss aus Schnittplan extrahiert ODER vom User pro Layer-Convention gesetzt werden.

**SAB_29-Mapping:** Keine Fundament-Klasse — Property `KG_Nummer = "320"` + Composite (z.B. „Stahlbeton C25/30") nötig.

<!-- VERIFY -->

## KG 330 — Außenwände

DIN 276: tragende (MW, STB), nichttragende (Vorhangfassade, Leichtbau), Bauteile (Stützen, Pfeiler, Brüstungen, Außenputz, Fassadenbekleidung).

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Tragende Außenwand (MW/STB) | `IfcWall` (StandardCase) | S2 | `Wand_AW_*`, `AW_tragend`, `A_30_Aussenwand` | OK-FFB → UK-Decke (Geschosshöhe) |
| Nichttragende Außenwand | `IfcWall` mit `LoadBearing=false` | S2 | `Wand_AW_*_nichttragend` | wie oben |
| Vorhangfassade (Pfosten-Riegel) | `IfcCurtainWall` | S2 (Fassaden-Achse) | `Fassade_*`, `Fassade_PfRi_*` | OK-FFB → OK Attika |
| Außenputz / Bekleidung | NICHT als eigene Geometrie — Composite-Layer | — | (aus Annotation wie „SPALTKLINKERPL BIS 2,13M") | — |
| Brüstung | `IfcRailing` oder Teil der Wand | S2 | `Brüstung_*` | OK-FFB → BRH (typ. 0.90-1.10 m) |

**Pipeline-Anwendung:** Wahrscheinlich die wichtigste KG-Klasse für 2D→3D-Reconstruction. Außenwand-Polygone sind in Grundriss-DWGs meist sauber gezeichnet.

**Kritisch — Mehrschicht-Composites:** Pipeline rekonstruiert nur die GEOMETRIE (Außenkante + Innenkante → Centerline + Dicke). Die Schichten-Struktur (MW + Dämmung + Vorhangschale, wie typischerweise in Archicad als Composite „MW Kal …" modelliert) ist NICHT aus 2D-Linien ableitbar und muss in Archicad NACH dem Hotlink-Import per Composite-Zuweisung gesetzt werden. Anhaltspunkte für die Zuweisung: Office-Layer-Konvention oder Annotation-Text auf dem Plan („MW Kal 24 + WDVS 16").

**SAB_29-Mapping:**
- Tragend MW → `ELEMENTE > Wand > MW - Wand` (GUID `fd317914-3c36-604d-b094-ecf824666301`)
- Tragend Beton → `ELEMENTE > Wand > Betonwand` (`84893816-736c-5d42-be74-bcf148487b27`)
- Vorhangfassade → `ELEMENTE > Fassade` (`ea87e04f-7e66-5b4f-85b3-eefc34202e9c`)
- Innen-/Außen-Unterscheidung: NICHT als SAB_29-Klasse — separate Property nötig (siehe Bulk-KG-Zuweisung).

<!-- VERIFY -->

## KG 340 — Innenwände

DIN 276: tragende (MW, Beton), nichttragende (Ständerwerk, GK), Bauteile (Innenstützen, Türöffnungen, Innenputz).

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Tragende Innenwand | `IfcWall` mit `LoadBearing=true` | S2 | `Wand_IW_tragend`, `IW_tragend` | OK-FFB → UK-Decke |
| Nichttragende Innenwand (MW) | `IfcWall` mit `LoadBearing=false` | S2 | `Wand_IW_*` | wie oben |
| GK-Ständerwerkswand | `IfcWall` `ObjectType="Trockenbauwand"` | S2 | `Wand_GK_*`, `Trockenbau_*` | wie oben |
| Innenstütze | `IfcColumn` | S3 | `Stütze_*`, `Innenstütze` | OK-FFB → UK-Decke |
| Innenputz | NICHT als Geometrie — Composite-Layer | — | — | — |

**SAB_29-Mapping:**
- Tragend MW → MW-Wand (`fd317914-3c36-604d-b094-ecf824666301`)
- Tragend Beton → Betonwand (`84893816-736c-5d42-be74-bcf148487b27`)
- Trockenbau → Trockenbauwand (`8187be07-ed29-3847-b190-e87d63e70644`)
- Trennwand → Trennwand (`932504f3-7f7f-494c-8ea1-92be4dbff0f3`)
- Innenstütze → Stütze (`dd96d02a-dd21-1148-bc5e-35c0f500c0fe`)

<!-- VERIFY -->

## KG 350 — Decken

DIN 276: Rohbaudecken (STB, Holzbalken, Element), Deckenbekleidungen (abgehängt, Putz), Balkone.

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Rohbaudecke STB | `IfcSlab` `PredefinedType="FLOOR"` | S1 | `Decke_Roh_*`, `Decke_STB_*` | UK-Decke → OK Rohbau |
| Holzbalkendecke | `IfcSlab` + `IfcBeam`-Schar | S1 + S4 | `Decke_Holz_*`, `Balken_*` | UK Tragwerk → OK Rohbau |
| Abgehängte Decke | `IfcCovering` `PredefinedType="CEILING"` | S1 | `Decke_abgehängt_*`, `GK_Decke` | UK abgehängt → OK abgehängt (typ. 5-15 cm) |
| Balkon (vorgefertigt) | `IfcSlab` `ObjectType="Balkon"` | S1 | `Balkon_*` | UK Balkon → OK FFB Balkon |

**Pipeline-Anwendung:** Sehr gut — Decken sind die idealsten Pipeline-Kandidaten (flach, polygonal, klare XY-Außenkante). Lageplan-Pipeline (`step_polygonize_v4.py` + `step_build_single.py`) ist direkt anwendbar.

**SAB_29-Mapping:**
- Rohbaudecke → Rohbaudecke (`3de2bfef-f58b-7b4f-8006-1434684195db`)
- Abgehängte Decke → Abgehängte Decke (`295ade7e-34f1-2a46-bf01-c4c07d767028`)
- Decke allgemein → Decke (`96a3a735-0beb-4948-b009-09fd98d60ceb`)

<!-- VERIFY -->

## KG 360 — Dächer

DIN 276: Dachkonstruktionen (Stuhl, Pfetten, Sparren), Dacheindeckungen, Dachabdichtungen, Dachöffnungen.

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Flachdach (Tragwerk) | `IfcSlab` `PredefinedType="ROOF"` | S1 | `Dach_FD_*`, `Dach_Roh_*` | OK Attika − Dachaufbau → OK Attika |
| Geneigtes Dach | `IfcRoof` (multi-segment) | S1 pro Dachfläche | `Dach_*`, `Sparren_*` | Traufe-OK → First-OK (geneigt) |
| Sparren / Pfette | `IfcBeam` `ObjectType="Sparren"` | S4 | `Dach_Sparren`, `Dach_Pfette` | folgt Dachneigung |
| Dachöffnung (Fenster) | `IfcWindow` mit `HostElement=IfcRoof` | NICHT pipeline-fähig (siehe § NICHT) | — | — |
| Dacheindeckung | NICHT als Geometrie — Material auf IfcRoof | — | — | — |

**Pipeline-Anwendung:** Flachdach trivial (wie Decke). Geneigtes Dach schwierig — Pipeline produziert nur XY-Polygon, Neigung muss aus Schnittplan oder separatem Höhenraster kommen. Realistisch: Pipeline liefert Hülle, User-Modifikation in Archicad nach Hotlink.

**SAB_29-Mapping:** Keine Dach-Klasse — Property `KG_Nummer = "360"`.

<!-- VERIFY -->

## KG 370 — Treppen und Rampen

DIN 276: Treppenläufe (STB, Holz, Stahl), Treppenpodeste, Rampen.

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Treppenlauf | `IfcStairFlight` | S4 (Sweep mit Steigungs-Profil) | `Treppe_Lauf`, `Treppe_STR` | Antritt-OK → Austritt-OK |
| Treppenpodest | `IfcSlab` `ObjectType="Podest"` | S1 | `Treppe_Podest` | UK → OK |
| Rampe | `IfcRamp` | S4 (geneigter Sweep) | `Rampe_*` | Antritt → Austritt |

**Pipeline-Anwendung:** Erheblich eingeschränkt — Treppen brauchen Steigungshöhe + Auftrittstiefe, die aus 2D-Grundriss nicht ableitbar sind (Steigungslinien in DWGs sind nur 2D-Symbole). Realistisch: Pipeline überspringt KG 370, User modelliert in Archicad direkt nach Hotlink.

**SAB_29-Mapping:** Keine Treppen-Klasse — Property `KG_Nummer = "370"`.

<!-- VERIFY -->

## KG 380 — Außenfenster und -türen

DIN 276: Fenster (Holz, Kunststoff, Alu), Außentüren, Sonnenschutz/Tore.

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Fenster | `IfcWindow` mit `HostElement=IfcWall` | NICHT direkt pipeline-fähig | `Fenster_*`, `F_*` | BRH → BRH + Fensterhöhe |
| Außentür | `IfcDoor` mit `HostElement=IfcWall` | NICHT direkt | `Tür_AT_*`, `T_*` | 0 → Türhöhe (typ. 2.01 m) |
| Sonnenschutz / Tor | `IfcShadingDevice`, `IfcDoor` | NICHT direkt | `Sonnenschutz_*`, `Tor_*` | variabel |

**Pipeline-Anwendung:** Stark eingeschränkt — Fenster/Türen brauchen Host-Wand-ID (die bei der Pipeline-Außenwand-Reconstruction erst entsteht) UND Brüstungshöhe (aus 2D nicht ableitbar — auf Werkplänen als „BRH 0.94"-Annotation, also erst nach Annotation-Parsing — siehe TODO).

**Realistischer Workflow:**
1. Pipeline produziert nur die Wände (KG 330).
2. Nach Hotlink: Fenster/Türen werden in Archicad anhand der Grundriss-DWG (als 2D-Referenz im Hintergrund) per Hand gesetzt.
3. ODER (zukünftig, falls Annotation-Parsing-Backlog umgesetzt): Pipeline extrahiert Fenster-Position + BRH aus dem Plan und schreibt eine Tabelle für strukturierte Wandmodifikation.

**SAB_29-Mapping:**
- Fenster → Fenster (`8a46db63-2597-5942-bd5b-6ffb8bff3c30`)
- Tür → Tür (`765aed84-6644-0247-9a22-f0ccb633d601`)
- Innen-/Außen-Unterscheidung der Türen: NICHT in SAB_29 — separate Property.

<!-- VERIFY -->

## KG 390 — Innenabschlüsse und -türen

DIN 276: Innentüren, Innenfenster, sonstige Abschlüsse (Schiebetüren, Brandschutztüren).

Identische Pipeline-Limits wie KG 380 — Host-Wand + Höhen-Annotation erforderlich.

**SAB_29-Mapping:** Tür-Klasse identisch zu KG 380. Brandschutztür-Unterscheidung über Property.

<!-- VERIFY -->

## KG 395 — Sonstige Baukonstruktionen

DIN 276: Raumbildende Einbauten, Bekleidungen/Bodenbeläge, Abdichtungen.

| Item | IFC-Entity | Strategie | DWG-Layer-Hints | Z-Schema |
|---|---|---|---|---|
| Bodenbelag | `IfcCovering` `PredefinedType="FLOORING"` | S1 | `Bodenbelag_*`, `MIPOLAM_*`, `Estrich_*` | OK Rohbau → OK FFB |
| Wandbekleidung | `IfcCovering` `PredefinedType="CLADDING"` | NICHT direkt (Wand-Layer) | — | — |
| Bauwerksabdichtung | NICHT als Geometrie (Folien) | — | — | — |
| Raumbildende Einbauten | `IfcFurniture` / `IfcBuildingElementProxy` | S1 oder S3 | `Einbau_*`, `Tresen_*` | variabel |

**Pipeline-Anwendung:** Bodenbeläge gut pipeline-fähig (flache Polygone pro Raum, Material-Annotation per Raum-Code → siehe Annotation-Parsing-Backlog).

**SAB_29-Mapping:**
- Sanitäreinrichtung → Sanitäreinrichtung (`ed7be860-81d2-b345-ade0-c2d31dfb00cd`)
- Möbel → Möbel (`5d3e7d17-4a79-f348-a770-5a2bb167fadb`)
- Bodenbelag selbst: nicht in SAB_29 — Property `KG_Nummer = "395"`.

<!-- VERIFY -->

## Bulk-KG-Zuweisung auf bestehende Archicad-Elemente

**Umgekehrte Richtung:** Elemente sind bereits in Archicad modelliert. Wir wollen sie nach KG 300 stempeln, damit Schedules/Stücklisten nach Cost-Group filterbar sind.

**Voraussetzung:** UserDefined-Property `KG_Nummer` (String, optional Enum mit Werten `310, 320, …, 395`) ist im Projekt angelegt. Falls nicht: neue Property-Group + `properties_create_property_definitions` (siehe [`bulk-operations.md`](bulk-operations.md) — Achtung: `defaultValue` ist Pflicht, kein Modify-Endpoint).

**Workflow (Read → Filter → Group → Confirm → Apply → Verify):**

1. **Read** — Element-Set holen via `mcp__archicad__elements_get_elements_by_classification` mit SAB_29-Klassen (z.B. alle Wände → MW-Wand + Betonwand + Trockenbauwand + Trennwand + Elementwand kombiniert).
2. **Filter** — pro KG-Bucket gruppieren via Mapping-Tabelle unten. Beispiel: `ELEMENTE > Wand > *`-Subtypen → entweder KG 330 (Außen) oder KG 340 (Innen). Innen/Außen-Unterscheidung über (a) Layer-Konvention (z.B. `A_30_*` = außen), (b) bereits gesetzte „Außen"/„Innen"-Property, oder (c) Geometrie-Heuristik (Wand grenzt an Aussenraum-Zone → außen).
3. **Group + Confirm** — Summary pro KG-Bucket vorzeigen: „KG 330: 47 Außenwände · KG 340: 89 Innenwände · KG 350: 12 Decken · …" mit `details`-Option für Element-Aufzählung. Pattern aus [`bulk-operations.md`](bulk-operations.md).
4. **Apply** — `mcp__archicad__properties_set_property_values_of_elements` mit `propertyValue.value = "330"` (String, auch für Enum-Property). Batch-Calls pro Bucket. Wenn ein Element nicht klassifiziert ist (Pre-Check für Property-Availability): erst `elements_set_classifications_of_elements` aufrufen.
5. **Verify (Pflicht)** — pro KG-Bucket nochmal `properties_get_property_values_of_elements` und Counts vergleichen. Trust-but-verify-Pattern: silent-success-without-success ist bei Property-Set wie bei Klassifikation möglich (siehe `bulk-operations.md` § Klassifikations-Set: success ist nicht success).

**Pseudocode für Verify-Loop:**
```python
for kg, element_ids in buckets.items():
    set_property(element_ids, "KG_Nummer", kg)
    verify_values = get_property(element_ids, "KG_Nummer")
    sticked = sum(1 for v in verify_values if v == kg)
    if sticked != len(element_ids):
        warn(f"KG {kg}: {sticked}/{len(element_ids)} sticked, "
             f"{len(element_ids)-sticked} silent-rejected")
```

**Reverse-Lookup für Stückliste:** Schedule in Archicad anlegen mit Filter `KG_Nummer = "330"` → liefert alle Außenwände inklusive Mengen (Fläche, Volumen). Export als XLSX für Kostenermittlung. Siehe [`schedule-pipeline.md`](schedule-pipeline.md) für Export-Workflow.

## Mapping KG 300 ↔ SAB_Klassifizierung_29

System-GUID SAB_Klassifizierung_29: `cca1f9aa-ca6c-3444-9bfb-363c6145327e`

| KG | Konzept | SAB_29-Klasse | ClassificationItem-GUID |
|---|---|---|---|
| 320 | Fundament | (nicht in SAB_29) | — |
| 330 | Außenwand tragend (MW) | MW-Wand | `fd317914-3c36-604d-b094-ecf824666301` |
| 330 | Außenwand tragend (STB) | Betonwand | `84893816-736c-5d42-be74-bcf148487b27` |
| 330 | Fassade (Vorhang) | Fassade | `ea87e04f-7e66-5b4f-85b3-eefc34202e9c` |
| 330 | Fassade-Paneel | Fassade Paneel | `6226536b-0f5d-a748-a73c-73fa09ec0e40` |
| 330 | Fassade-Profil | Fassade Profil | `f33ed439-539c-024d-867a-8aa142506bc3` |
| 340 | Innenwand tragend (MW) | MW-Wand | `fd317914-3c36-604d-b094-ecf824666301` |
| 340 | Innenwand tragend (STB) | Betonwand | `84893816-736c-5d42-be74-bcf148487b27` |
| 340 | Innenwand GK/Trockenbau | Trockenbauwand | `8187be07-ed29-3847-b190-e87d63e70644` |
| 340 | Trennwand | Trennwand | `932504f3-7f7f-494c-8ea1-92be4dbff0f3` |
| 340 | Elementwand | Elementwand | `64df12c7-801a-fa4d-9b99-0d66dba285b9` |
| 340 | Innenstütze | Stütze | `dd96d02a-dd21-1148-bc5e-35c0f500c0fe` |
| 350 | Rohbaudecke | Rohbaudecke | `3de2bfef-f58b-7b4f-8006-1434684195db` |
| 350 | Decke (allgemein) | Decke | `96a3a735-0beb-4948-b009-09fd98d60ceb` |
| 350 | Abgehängte Decke | Abgehängte Decke | `295ade7e-34f1-2a46-bf01-c4c07d767028` |
| 350 | Träger | Träger / Balken / Unterzug | `7391f247-bbf7-6844-8768-472e1b6da80c` |
| 360 | Dach | (nicht in SAB_29) | — |
| 370 | Treppe | (nicht in SAB_29) | — |
| 380 | Fenster | Fenster | `8a46db63-2597-5942-bd5b-6ffb8bff3c30` |
| 380 | Dachfenster | Dachfenster | `02c43b88-6a94-854f-ab0c-eeca58ced47e` |
| 380 | Außentür | Tür | `765aed84-6644-0247-9a22-f0ccb633d601` |
| 380 | Fenstertür | Fenstertür | `e67e0dd4-7d5d-5646-99ee-4e93790757a8` |
| 390 | Innentür | Tür | `765aed84-6644-0247-9a22-f0ccb633d601` |
| 395 | Möbel / Einbau | Möbel | `5d3e7d17-4a79-f348-a770-5a2bb167fadb` |
| 395 | Sanitäreinrichtung | Sanitäreinrichtung | `ed7be860-81d2-b345-ade0-c2d31dfb00cd` |

**Lücken** (KG ohne SAB_29-Korrespondenz): 310 Baugrube, 320 Fundamente, 360 Dächer, 370 Treppen. Für diese KGs ist die Property `KG_Nummer` die einzige strukturierte Markierung. SAB_29 ist primär bauteil-typ-orientiert, KG 300 ist kostengruppen-orientiert — die Schnittmenge deckt die Hauptmenge der Bauelemente ab, aber nicht alle.

**Innen-/Außen-Unterscheidung** (KG 330 vs. 340 bei MW/Betonwand): SAB_29 hat KEINE Innen-/Außen-Klasse für Wände. Auflösung über (a) Layer-Konvention, (b) separate Property „Wand_Position" mit Enum {Außen, Innen}, oder (c) Geometrie-Heuristik (Außenraum-Adjazenz). Dies ist bereits in [`user setup memory`] (Schwarz Architekturbüro) als Lücke dokumentiert.

## Vorsicht: Klassifikation steuert NICHT das IFC-Typ-Mapping

<!-- 2026-05-21 live-verifiziert AC29 (AFW-WC Teamwork-Projekt, Schwarz-Office-Template) -->

**Befund:** Die SAB-Klassifikation (oder generell jede Archicad-Klassifikation) bestimmt NICHT automatisch, wie ein Element ins IFC exportiert wird. Klassifikation und IFC-Typ-Mapping sind ZWEI getrennte Systeme im Archicad:

- **Klassifikation** (via `elements_set_classifications_of_elements`): SAB-/Uniclass-/etc.-Klasse, sichtbar in Schedules und Properties — beeinflusst KEINEN IFC-Export-Typ.
- **IFC-Typ-Mapping**: gesteuert vom **IFC-Übersetzer** (Datei → Interoperabilität → IFC → IFC-Übersetzer einstellen). Die Mapping-Tabelle dort sagt: Archicad-Element-Typ X → IFC-Entity Y. Per-Element-Override ist im „IFC-Eigenschaften verwalten"-Dialog NICHT möglich (die IFC-Typ-Zeile hat keine Editier-Checkbox).

**Häufiger Fehler (Schwarz-Office-Template, live verifiziert AFW-WC 2026-05-21):** Übersetzer mappt ALLE Elemente pauschal auf `IfcBuildingElementProxy`, ignoriert SAB-Klasse. Resultat: IFC-Export mit 822 Proxies, 0× IfcWall/IfcDoor/IfcWindow/etc. Klassifizieren der Elemente via MCP ändert daran NICHTS — der Übersetzer ist Klassifikations-unsensitiv.

**Fix:** Nur in Archicad-UI im **IFC-Übersetzer-Dialog** die Mapping-Tabelle korrigieren. Siehe Memory-Eintrag `issue_archicad_ifc_translator_proxy_bug.md` für die vollständige Mapping-Liste.

**MCP-v29-Limit:** Keine Endpoints für IFC-Übersetzer-Settings, Werkzeug-Standardeinstellungen, oder per-Element IFC-Daten-Properties. Diese Settings liegen außerhalb der von MCP exponierten Property-Domäne. Klassifikations-Set über MCP ist nützlich für Stücklisten und Schedules — aber NICHT als Pre-Step für korrekten IFC-Export.

## Was die Pipeline NICHT kann

Strikte Scope-Grenze für ehrliche User-Erwartung:

- **Annotation-Parsing** — Raumnummern (WB.013, VORRAUM188), BRH-Werte (BRH 0.94), Höhen-Codes (OK=297.88, UK=OKRF, OK=OKBS), Material-Spezifikationen im Plantext („WÄNDE: SPALTKLINKERPL BIS 2,13M / DARÜBER TROCKENPUTZ BIS 2,50") werden NICHT extrahiert. Realistische Werkpläne enthalten 60-80 % der Bauinformation als Text, nicht als Geometrie. → Ist als eigener Backlog-Punkt erfasst.
- **Schichten-Composites** rekonstruieren — Pipeline gibt einschichtige Wände/Decken. Multi-Layer-Aufbauten (MW + WDVS + Vorhangschale) müssen in Archicad NACH dem Hotlink per Composite-Zuweisung gesetzt werden.
- **Schnitt-Plan-Auswertung** — Höhen (Geschosshöhe, Decken-OK/UK, Treppensteigung, Dachneigung) sind nur im Schnittplan; die Pipeline nimmt sie aus einem hartkodierten Z-Schema pro Klasse. Variable Höhen erfordern User-konfigurierbares Höhen-Mapping pro Layer/Klasse.
- **Host-Beziehungen** — Fenster/Tür-zu-Wand-Hosting kommt nicht aus 2D-Linien. KG 380/390 sind Post-Hotlink-Handarbeit.
- **Bewehrung, Detail-Anschlüsse, Stoßausführung** — zu fein für 2D-Linien-Reconstruction; ohnehin Statik- bzw. Schaltisch-Domain.
- **Brandschutz-Zonen, Fluchtwege** (die roten Bänder auf Werkplänen) — Symbol-Layer, würde Pipeline als Slabs interpretieren wenn nicht ausgeschlossen. Im Layer-Mapping `Brandschutz_*`, `Fluchtweg_*` EXPLIZIT excluden. Erkennung der Zonen-Semantik selbst: Backlog (Annotation-Parsing).

## Anwendungs-Hinweise aus realen Werkplänen

Schwarz-Architekturbüro-Werkpläne (live-gesehen 2026-05-21) haben charakteristisch:
- Hochdichte Bemaßung + Höhenannotation (BRH, OK/UK, OKBS, OKRF, OKFF, FB-KANAL)
- Office-spezifische Wand-/Sturz-Codes (H WD, L WD, S WD, S BD mit Profil-Dimensionen 50/40 etc.)
- Raum-Stempel mit Codes (WB.013, VORRAUM188) + NHN-Höhen
- Material-Spezifikation als Plantext in Räumen
- Fluchtweg-Bänder rot, Brandschutz-Wände separat markiert
- Spezial-Symbole (FB-KANAL, GK/D, GK/E, MIPOLAM) auf eigenen Layern

**Konsequenz für Pipeline-Layer-Mapping:** Vor der ersten Pipeline-Anwendung auf einen Werkplan dieses Detailgrads MUSS eine **Layer-Audit-Phase** laufen — User listet alle relevanten Layer, ordnet sie zu Pipeline-Klassen UND markiert ausdrücklich die Layer, die GEFILTERT werden müssen (Bemaßung, Annotation, Symbole, Brandschutz-Bänder).

**Realistische Erwartung:** Pipeline produziert die GROBE 3D-Massen-Struktur (Wände, Decken, Stützen) korrekt. Detail-Composites, Öffnungen, Höhen-Variationen, Material-Zuweisungen sind Post-Hotlink-Handarbeit in Archicad. **Zeitersparnis gegenüber komplettem Neu-Modellieren: realistisch 60-80 %, nicht 100 %.**
