# DWG → IFC Decken-Pipeline

Wandelt eine Lageplan-DWG mit Straßen, Gehwegen und Gebäuden in ein **lückenfreies IFC4-Modell**, in dem alle Flächen als gestaffelte `IfcSlab`-Decken extrudiert sind. Entwickelt für den BuPol-Lageplan (1.7 × 2.7 km, 81 k DXF-Entities), übertragbar auf andere Liegenschaftspläne.

**Trigger-Phrasen (für Claude):** „convert dwg to ceilings", „dwg in ifc decken", „lückenlose außenanlagen aus dwg", „lageplan zu ifc"

---

## Quickstart

```bash
cd ~/.claude/repos/claude-skills/archicad/scripts/dwg_to_ceilings/
chmod +x run_pipeline.sh step_1_dwg_to_dxf.sh

# Komplette Pipeline in einem Schritt
./run_pipeline.sh /pfad/zur/lageplan.dwg

# Mit Output-Pfad und Auto-Import ins offene Archicad
./run_pipeline.sh /pfad/zur/lageplan.dwg /pfad/zum/output.ifc -i
```

Ergebnis: `lageplan_Decken.ifc` (schema-validiert, in BlenderBIM/Solibri/Archicad öffenbar).

---

## Voraussetzungen

| Komponente | Installation |
|---|---|
| **Python 3** (≥ 3.10) | meistens vorhanden |
| `ezdxf` | `pip install ezdxf` |
| `shapely` | `pip install shapely` (braucht `geos` lib) |
| `ifcopenshell` | `pip install ifcopenshell` |
| **LibreDWG** (`dwg2dxf`) | `brew install libredwg` (Mac) oder Source bauen |

---

## Höhen-Schema

```
 -0.30 ──── Strasse_Fill  Unterkante  ┐
 -0.20 ──── Strasse_Fill  Oberkante   ┘  Lückenfüller (Site-Hülle)
 -0.30 ──── Strasse       Unterkante  ┐
 -0.15 ──── Strasse       Oberkante   ┘  Asphalt detailliert
 -0.15 ──── Bordstein     Unterkante  ┐
  0.00 ──── Bordstein     Oberkante   ┘  Gehweg / Bordstein
  0.00 ──── Gebäude       Sohle       ┐
 +6.00 ──── Gebäude       Oberkante   ┘  Dummy-Volumen
```

Straße sitzt 15 cm **unter** Bordsteinoberkante → realistische Geländekante. Hintergrund-Füller liegt weitere 5 cm tiefer.

---

## Pipeline-Stufen (was die Skripte tun)

### Step 1 — `step_1_dwg_to_dxf.sh`
DWG → DXF via LibreDWG `dwg2dxf`. Wrapper-Bashscript.

### Step 2 — `step_2_polygonize.py` (= ehemals `step_polygonize_v4.py`)
Pro IFC-Klasse alle `LINE`/`ARC`/`LWPOLYLINE` sammeln und **zweistufig** polygonisieren:

- **Stage 1: SNAP_TIGHT = 0.10 m** → exakte Polygone aus sauber gezeichneten Konturen
- **Stage 2: layerspezifische loose tolerance** (0.30 – 1.00 m) auf den übrig gebliebenen Linien → schließt Lücken

Output: ein Pickle pro Klasse (`(label, outer_pts, holes)`-Tupeln).

```bash
python3 step_2_polygonize.py LAGEPLAN.dxf Strasse   strasse_v4.pkl
python3 step_2_polygonize.py LAGEPLAN.dxf Bordstein bordstein_v4.pkl
python3 step_2_polygonize.py LAGEPLAN.dxf Gebaeude  gebaeude_v4.pkl
```

### Step 3 — `step_3_fill_gaps.py` (= ehemals `step_fill_gaps.py`)
Komplement-Füller für Lückenfreiheit. Berechnet `concave_hull` aller Linien-Endpunkte (`ratio=0.05`), vereinfacht topologie-erhaltend, schreibt `fill_v5.pkl`.

```bash
python3 step_3_fill_gaps.py LAGEPLAN.dxf
```

### Step 4 — `step_4_build_ifc.py` (= ehemals `step_build_single.py`)
Pro Klasse ein eigenes IFC4. Jeder Polygon wird `IfcSlab` mit `IfcExtrudedAreaSolid`, eingeordnet unter `IfcBuildingStorey`.

```bash
python3 step_4_build_ifc.py strasse_v4.pkl   Strasse      lageplan_Strasse.ifc
python3 step_4_build_ifc.py bordstein_v4.pkl Bordstein    lageplan_Bordstein.ifc
python3 step_4_build_ifc.py gebaeude_v4.pkl  Gebaeude     lageplan_Gebaeude.ifc
python3 step_4_build_ifc.py fill_v5.pkl      Strasse_Fill lageplan_StrasseFill.ifc
```

### Step 5 — `step_5_merge.py` (= ehemals `step_text_merge.py`)
Verkettet die Einzel-IFCs auf STEP-Textebene (in Subsekunden), mit ID-Remapping und Mapping der Singletons (Project, Site, Building, Storey, OwnerHistory, Units, RepresentationContext) auf die der Hauptdatei.

```bash
python3 step_5_merge.py lageplan_Decken.ifc \
  lageplan_StrasseFill.ifc \
  lageplan_Strasse.ifc \
  lageplan_Bordstein.ifc \
  lageplan_Gebaeude.ifc
```

### Step 6 (optional) — Import nach Archicad
Im `run_pipeline.sh` mit `-i`-Flag aktivierbar. Macht `open -a "Archicad 29" lageplan_Decken.ifc` (macOS).

Alternativ — sag Claude:
> Importiere `lageplan_Decken.ifc` ins offene Archicad-Projekt als Hotlink.

Claude nutzt dann den Archicad-Skill (MCP-Server) für den Import.

---

## Übertragung auf andere Projekte

Drei Stellen anpassen — aktuell **hartkodiert in den Skripten**, später via `config.json` refactorbar:

1. **`LAYER_MAP`** in `step_2_polygonize.py` + `step_3_fill_gaps.py` an die Layer-Konvention des neuen Projekts anpassen
2. **`LAYER_LOOSE`** (`step_2_polygonize.py`) kalibrieren — typische Lücken­breite messen, +20 %
3. **`CLASS_SPECS`** (`step_4_build_ifc.py`) — Z-Höhen + Dicken nach Projektkonvention

Eine Template-Datei mit allen Konfigurations-Werten liegt als [`config.json`](config.json) bei. Sync nach Bedarf manuell in die Skripte.

---

## Versionshistorie (Strategie-Entwicklung)

| Version | Strategie | Slabs (BuPol) | Lücken? |
|---|---|---|---|
| v1 | Nur geschlossene LWPolylines | 1.970 | viele |
| v3 | + Linien-Snap-Polygonize @ 10 cm | 4.240 | weniger |
| v4 | + zweistufige Toleranz (10 cm + bis 1 m) | 5.384 | wenige |
| **v5** | + Site-Hüllen-Komplement-Füller (aktuelle Pipeline) | **5.385** | **keine** |

---

## Bekannte Einschränkungen

- **`ratio` der konkaven Hülle ist projektabhängig.** Bei kompakten Liegenschaften 0.05 OK; bei zergliederten Anlagen 0.10 – 0.20 setzen (`CONCAVE_HULL_RATIO` in `config.json`, hartkodiert in `step_3_fill_gaps.py`).
- **`MIN_AREA = 1 m²`** filtert Schraffur-Rauschen weg, kann aber legitime Mini-Polygone (z. B. Verkehrsinseln) verlieren.
- **`MAX_AREA = 80.000 m²`** filtert riesige Außenpolygone weg, die bei der Linien-Polygonisation entstehen.
- **Layer-Konflikt-Resolution ist deaktiviert** (Straße vs. Bordstein) — Slabs überlappen, aber die Z-Staffel macht das visuell sauber.
- **Keine echten `IfcSitePart` / `IfcBuilding`-Hierarchien** — alles hängt unter EINER Storey, ist aber per `ObjectType` filterbar.
- **`step_4` erwartet** drei spezifische Pickle-Pfade (`strasse.pkl`, `bordstein.pkl`, `gebaeude.pkl`) als globalen Origin-Anchor. `run_pipeline.sh` legt dafür Symlinks neben den `_v4.pkl`-Dateien.

---

## Performance (Apple M-Klasse / Linux ARM64)

| Schritt | Zeit |
|---|---|
| DWG → DXF (8.5 MB DWG) | ~3 s |
| Polygonize Strasse (12 k Linien) | 1 s |
| Polygonize Bordstein (18 k Linien) | 1 s |
| Polygonize Gebäude (7 k Linien) | 0.5 s |
| Concave Hull (184 k Punkte, ratio=0.05) | 2.5 s |
| IFC-Bau Strasse (2098 Slabs) | 32 s |
| IFC-Bau Bordstein (2040 Slabs) | 27 s |
| IFC-Bau Gebäude (1246 Slabs) | 10 s |
| Text-Merge alle IFCs | <1 s |

---

## Verwandte Doku im Skill

- [`../../reference/dwg-ifc-import.md`](../../reference/dwg-ifc-import.md) — Workflow-Doku Lageplan/KG 500
- [`../../reference/dwg-ifc-kg300.md`](../../reference/dwg-ifc-kg300.md) — Verallgemeinerung auf KG 300 Baukonstruktionen
- [`../../reference/schwarz-office-facts.md`](../../reference/schwarz-office-facts.md) — Z_/A_-Layer-Konvention, Schwarz-Office-Template-Bugs

---

**Autor:** Mudi · **Office:** Schwarz Architekturbüro Nürnberg GbR · **Stand:** Juni 2026 · **Pipeline-Version:** v5
