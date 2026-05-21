# DWG → IFC Decken-Pipeline (Lageplan-Import)

Workflow zum Wandeln einer Lageplan-DWG (Straßen, Gehwege, Gebäude — meist nur als 2D-Linien ohne saubere Konturen) in ein **lückenfreies IFC4-Modell**, in dem alle Flächen als gestaffelte `IfcSlab`-Decken extrudiert sind. Das fertige IFC wird in Archicad als **Hotlink** eingebunden — saubere 3D-Decken statt 2D-Linien-Soup.

Entwickelt am BuPol-Lageplan (1.7 × 2.7 km, 81 k DXF-Entities → 5.385 Slabs, 9 MB IFC). Übertragbar auf andere Liegenschaftspläne mit drei Anpassungen (siehe § Übertragung).

<!-- live verifiziert 2026-05-12, BuPol-Lageplan -->

## Inhalt
- [Wann diese Pipeline](#wann-diese-pipeline)
- [Höhen-Schema](#höhen-schema)
- [Pipeline (4 Stufen)](#pipeline-4-stufen)
- [Layer-Mapping](#layer-mapping)
- [Bekannte Einschränkungen](#bekannte-einschränkungen)
- [Performance-Richtwerte](#performance-richtwerte)
- [Übertragung auf andere Projekte](#übertragung-auf-andere-projekte)
- [Archicad-Integration via MCP](#archicad-integration-via-mcp)
- [Skript-Standort](#skript-standort)

## Wann diese Pipeline

Trigger: User hat einen Lageplan als DWG/DXF, will ihn als 3D-Hintergrund-Modell in Archicad nutzen (Hotlink oder IFC-Import), und die DWG enthält nur 2D-Linien — keine geschlossenen Polygone, keine Höhen.

NICHT geeignet für:
- DWGs mit sauberen Closed-Polylines + Höhen → direkter IFC-Export reicht
- 3D-Mesh-Geländemodelle → eigener Workflow (TIN-Import)
- Reine Symbol-Pläne (Bäume, Beleuchtung) ohne Flächenbedarf → DWG bleibt 2D-Hotlink

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

Straße sitzt 15 cm **unter** Bordsteinoberkante → realistische Geländekante zwischen Fahrbahn und Gehweg. Hintergrund-Füller liegt weitere 5 cm tiefer, damit detaillierte Straßenpolygone darüber visuell dominieren.

## Pipeline (4 Stufen)

### 1. DWG → DXF Konvertierung
LibreDWG `dwg2dxf` (lokal aus Source kompiliert, weil Sandbox ohne ODA-Converter):

```bash
dwg2dxf -y EINGABE.dwg
```

### 2. Pro-Klasse Polygonisierung (zweistufig)

`step_polygonize_v4.py` — sammelt pro Layer alle `LINE` / `ARC` / `LWPOLYLINE` und macht zwei Durchläufe:

- **Stage 1: SNAP_TIGHT = 0.10 m** → exakte Polygone aus sauber gezeichneten Konturen
- **Stage 2: layerspezifische loose tolerance** (0.30 – 1.00 m) auf den übrig gebliebenen Linien (Mittelpunkt nicht in Stage-1-Polygon) → schließt Lücken

```python
LAYER_LOOSE = {
    "road_site":   1.00,
    "sidewalks":   0.75,
    "BASE":        0.75,
    "Cement_Street": 0.50,
    "buildings":   0.30,
    "GARAGE":      0.20,
}
```

Polygonisierung mit `shapely.set_precision(grid_size=tol)` + `unary_union` + `polygonize`. Output: ein Pickle pro Klasse mit `(label, outer_pts, holes)`-Tupeln.

```bash
python3 step_polygonize_v4.py BuPol.dxf Strasse   strasse_v4.pkl
python3 step_polygonize_v4.py BuPol.dxf Bordstein bordstein_v4.pkl
python3 step_polygonize_v4.py BuPol.dxf Gebaeude  gebaeude_v4.pkl
```

### 3. Komplement-Füller (für Lückenfreiheit)

`step_fill_gaps.py` — berechnet die Site-Hülle und schreibt einen einzelnen, vereinfachten Filler-Slab:

```python
# 184 k Linien-Endpunkte aus allen relevanten Layern
site_hull = shapely.concave_hull(MultiPoint(pts), ratio=0.05, allow_holes=False)
fill = site_hull.simplify(tolerance=0.2, preserve_topology=True)
```

`ratio` steuert die Engigkeit der Hüllung:
- `0.00` → Konvexe Hülle (sehr großzügig)
- `0.05` → moderat eng (Default für kompakte Liegenschaften)
- `0.20` → eng an der Bebauung (zergliederte Anlagen)
- `1.00` → maximal eng (kann Lücken in der Hülle erzeugen — Vorsicht)

### 4. IFC-Bau & Merge

`step_build_single.py` baut pro Klasse ein eigenes IFC4. Dann `step_text_merge.py` verkettet die IFCs auf STEP-Textebene (in Subsekunden), mit ID-Remapping und Mapping der Singletons (Project, Site, Building, Storey, OwnerHistory, Units, RepresentationContext) auf die der Hauptdatei.

```bash
python3 step_build_single.py strasse_v4.pkl   Strasse       BuPol_Strasse.ifc
python3 step_build_single.py bordstein_v4.pkl Bordstein     BuPol_Bordstein.ifc
python3 step_build_single.py gebaeude_v4.pkl  Gebaeude      BuPol_Gebaeude.ifc
python3 step_build_single.py fill_v5.pkl      Strasse_Fill  BuPol_StrasseFill.ifc

python3 step_text_merge.py BuPol_Decken_v5.ifc \
    BuPol_StrasseFill.ifc \
    BuPol_Strasse.ifc \
    BuPol_Bordstein.ifc \
    BuPol_Gebaeude.ifc
```

## Layer-Mapping

Beispiel-Mapping aus dem BuPol-Lageplan — pro Projekt anpassen:

| Klasse | DWG-Layer | Z (Oberkante) | Dicke | Farbe |
|---|---|---|---|---|
| Straße | `road_site`, `Cement_Street`, `parking area 305/325 *`, `Zaunstrasse_BUPOL`, `A_01_Strasse_neu`, `60 Straße`, `Apron#` | −0.15 m | 0.15 m | dunkelgrau |
| Bordstein | `sidewalks`, `BORDSTEIN`, `BETON`, `RINNE`, `Concrete_area`, `BASE` | 0.00 m | 0.15 m | hellgrau |
| Gebäude | `buildings`, `GARAGE`, `Bunker`, `Gazebo`, `Antenna`, `Tanks`, `WashRack`, `HAZERDOUS_WASTE`, `assigned_Bldgs`, alle `Gebäude_PhaseX_*` | +6.00 m | 6.00 m | lehmbraun |
| Füller | Site-Hülle (alle Endpunkte) | −0.20 m | 0.10 m | sehr dunkelgrau |

## Bekannte Einschränkungen

- **`ratio` der konkaven Hülle ist projektabhängig.** Bei kompakten Liegenschaften 0.05 OK; bei zergliederten Anlagen 0.10 – 0.20 setzen.
- **`MIN_AREA = 1 m²`** filtert Schraffur-Rauschen weg, kann aber legitime Mini-Polygone (z. B. Verkehrsinseln) verlieren.
- **`MAX_AREA = 80.000 m²`** filtert versehentlich riesige Außenpolygone weg, die bei der Linien-Polygonisation entstehen.
- **Layer-Konflikt-Resolution ist deaktiviert** (Straße vs. Bordstein) — Slabs überlappen, aber die Z-Staffel macht das visuell sauber.
- **Keine echten `IfcSitePart` / `IfcBuilding`-Hierarchien**: alles hängt unter EINER Storey, ist aber per `ObjectType` filterbar.
- **Bordstein-Layer (`BORDSTEIN`) selbst hat oft nur wenige Linien** — Workflow nutzt stattdessen `sidewalks` und `BASE` als Hauptquelle für Gehwegflächen.

## Performance-Richtwerte

Gemessen auf Apple M-Klasse / Linux ARM64 Sandbox, BuPol-Datensatz (8.5 MB DWG, 81 k DXF-Entities):

| Schritt | Zeit |
|---|---|
| DWG → DXF (LibreDWG dwg2dxf) | ~3 s |
| Polygonize Strasse (12 k Linien) | 1 s |
| Polygonize Bordstein (18 k Linien) | 1 s |
| Polygonize Gebäude (7 k Linien) | 0.5 s |
| Concave Hull (184 k Punkte, ratio=0.05) | 2.5 s |
| IFC-Bau Strasse (2098 Slabs) | 32 s |
| IFC-Bau Bordstein (2040 Slabs) | 27 s |
| IFC-Bau Gebäude (1246 Slabs) | 10 s |
| Text-Merge alle IFCs | <1 s |

Gesamt für 5.385 Slabs: ~80 s. Bei deutlich kleineren Liegenschaften (< 10 k Entities) entsprechend weniger.

## Übertragung auf andere Projekte

Drei Stellen anpassen:

1. **`LAYER_MAP`** in `step_polygonize_v4.py` und `step_fill_gaps.py` an die Layer-Konvention des neuen Projekts anpassen. Vorher: DWG kurz inspizieren, welche Layer Straße/Bordstein/Gebäude enthalten.
2. **`LAYER_LOOSE`** kalibrieren: typische Lücken­breite in den Layern messen, +20 % aufschlagen.
3. **`CLASS_SPECS`** in `step_build_single.py`: Z-Höhen und Dicken nach Projektkonvention (z. B. wenn Bordsteine 12 cm statt 15 cm hoch sind).

Versionshistorie als Kalibrierungs-Referenz:

| Version | Strategie | Slabs gesamt | Lücken? |
|---|---|---|---|
| v1 | Nur geschlossene LWPolylines | 1.970 | viele |
| v3 | + Linien-Snap-Polygonize @ 10 cm | 4.240 | weniger |
| v4 | + zweistufige Toleranz (10 cm + bis 1 m) | 5.384 | wenige |
| **v5** | + Site-Hüllen-Komplement-Füller | **5.385** | **keine** |

## Archicad-Integration via MCP

Sobald die IFC-Datei vorliegt, kann sie in Archicad als **Hotlink** eingebunden werden — entweder UI-seitig (*Datei → Interoperabilität → Hotlink-Manager → IFC*) oder via MCP, falls ein passender Discovery-Tool-Treffer existiert.

Discovery-Versuch:
1. `mcp__archicad__archicad_discover_tools` mit Query `"create hotlink from ifc file"` oder `"import ifc as hotlink"`.
2. Wenn ein Treffer kommt (typisch: `teamwork_*` oder `file_open_*` Familie): Schema lesen, Pfad zur IFC + Ziel-Storey + ggf. Anchor-Point setzen.
3. Wenn kein Treffer: User informieren, UI-Workflow vorschlagen.

**Vorteile gegenüber direktem DWG-Hotlink:**
- 3D-Slabs statt 2D-Linien — sichtbar in Schnitten und 3D-Fenster
- Klassen-Filterung über `ObjectType` (`Strasse` / `Bordstein` / `Gebaeude` / `Strasse_Fill`) → Layer-Sichtbarkeit pro Klasse steuerbar
- Höhenstaffelung passt sich an Geländemodell an, wenn man die IFC später entlang einer Mesh-Oberfläche projiziert

**Achtung:** Hotlink kann den Discovery-Pattern-Aufruf eines Update- oder Delete-Endpoints triggern, wenn ein bestehender Hotlink ersetzt wird. Dann greift SAFE-01 (Confirm-Schleife) — siehe SKILL.md.

## Skript-Standort

Die fünf Python-Scripts liegen NICHT im Skill (bewusste Entscheidung — sie sind ~50 kB Code und Layer-Konventions-spezifisch), sondern in der ursprünglichen Local-Agent-Mode-Session, in der die Pipeline entwickelt wurde:

```
/Users/ap/Library/Application Support/Claude/local-agent-mode-sessions/
  d7558e7f-c488-4613-a4f1-2fb19e30e2f5/
    6847b14d-add0-4bc2-90f9-f11ac6ac2775/
      local_65685fda-4d49-4e5b-b418-694d2a36d0f5/
        outputs/
          step_polygonize_v4.py
          step_build_single.py
          step_fill_gaps.py
          step_text_merge.py
          dwg2ifc.py            # älterer Monolith, v4 split ersetzt ihn
        uploads/
          BuPol_BS_Gesamtdatei_umgebung.dwg
          Layer_Freianlagen_AutoCAD2000.dwg
```

Bei einem neuen Lageplan-Projekt: Scripts in einen Arbeitsordner kopieren, anpassen (siehe § Übertragung), laufen lassen. Bei Verlust der Originale: Pipeline ist anhand dieser Reference + Stundenarbeit rekonstruierbar — die Algorithmen (`shapely.set_precision` + `polygonize` für Stage 1+2; `concave_hull(ratio=0.05)` für Füller; STEP-Text-Merge mit Singleton-Mapping für IFC-Zusammenführung) sind hier vollständig dokumentiert.

## Was nicht zu tun ist

- **Nicht** in Archicad selbst polygonisieren wollen — Archicads Polylinien-Recovery aus DWG-Linien ist nicht robust genug für Lagepläne mit zig-tausend Linien.
- **Nicht** den Füller weglassen, „weil ja eh alles abgedeckt ist" — die konkaven Lücken zwischen Straßen-Polygonen sind im Renderer sichtbar als Löcher.
- **Nicht** alle Klassen in eine einzige IFC bauen ohne Z-Staffel — Slabs verschmelzen visuell und der Höhen-Unterschied zwischen Straße und Bordstein geht verloren.
- **Nicht** `ratio` der konkaven Hülle nach Augenmaß setzen — immer mit `0.05` starten und nur bei sichtbaren Hüllen-Lücken erhöhen.
