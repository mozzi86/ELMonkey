# Konturen → Wände

Erzeugt **Polygon-Wände** (Polywand, `APIWtyp_Poly`) aus geschlossenen 2D-Konturen —
die Automatisierung des Handgriffs „Wand-Werkzeug, Geometriemethode Polygon, Magic Wand
in jede Kontur klicken".

**Trigger-Phrasen:** „geschlossene Linien als Wand", „Konturen zu Wänden",
„2D-Umrisse in Wände", „Polywände aus Zeichnung".

## Voraussetzungen

- ELM_SAB_Add-On **≥ v0.3** geladen (Befehl `ELM_SAB.CreatePolygonWalls`)
- Für DXF-Modus: `ezdxf` + `shapely` (vorhanden, siehe dwg_to_ceilings)

## Pipeline

1. **Sammeln** — geschlossene Polylinien einer Ebene, live (Grundriss oder Arbeitsblatt
   via `--worksheet`) oder aus DXF-Datei (`--dxf`, Linien-Suppe wird per shapely
   polygonisiert — wie im dwg_to_ceilings-Tool)
2. **Klassifizieren** — Dicken-Schätzung `d = 2·Fläche/Umfang`; nur dünne, längliche
   Polygone (Default 0.05–0.65 m) werden Wände. **Raum-Polygone werden übersprungen
   statt hinterher gelöscht.** Mini-Artefakte (< 0.01 m²) fliegen raus.
3. **Erzeugen** — `CreatePolygonWalls` in Batches, undoable, mit Höhe/Geschoss/
   Ziel-Ebene/Aufbau als Parameter.

## Anwendung

```bash
# Erst IMMER die Vorschau:
python3 konturen_zu_waende.py --layer "EBENE" --worksheet "BSN_NAGEL 00" --dry-run

# Dann erzeugen:
python3 konturen_zu_waende.py --layer "EBENE" --worksheet "BSN_NAGEL 00" \
    --height 3.2 --floor 0 --ziel-ebene "A_01_TRAGWAND" --composite "Kalksandstein" --yes
```

## Bekannte Grenzen (v1, 2026-07-13)

- Live-Modus liest nur **Polylinien** (Tapir liefert für Line/Arc keine Geometrie —
  „Not yet supported"). Konturen aus Einzellinien → DXF-Modus nutzen.
- Ein Polygon = eine Wand. L-/U-förmige Wandzüge werden EINE Polywand (meist gewollt).
- Wände entstehen lagegleich (Koordinaten 1:1) im Ziel-Geschoss — Arbeitsblatt muss
  lagerichtig sein.
- Claude-Workflow: erst `--dry-run` zeigen, User bestätigt, dann mit `--yes` ausführen
  (SAFE-01: Create ist frei, aber Massen-Erzeugung immer mit Zahlen ankündigen).
