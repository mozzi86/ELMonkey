# Konturen → Wände

Erzeugt **Polygon-Wände** (Polywand, `APIWtyp_Poly`) aus geschlossenen 2D-Konturen —
die Automatisierung des Handgriffs „Wand-Werkzeug, Geometriemethode Polygon, Magic Wand
in jede Kontur klicken".

**Trigger-Phrasen:** „geschlossene Linien als Wand", „Konturen zu Wänden",
„2D-Umrisse in Wände", „Polywände aus Zeichnung", „Markierungsrahmen Wände zeichnen".

## Voraussetzungen

- ELM_SAB_Add-On **≥ v0.4** geladen (`ELM_SAB.CreatePolygonWalls` + `Get2DGeometryOfElements`)
- Für DXF-Modus: `ezdxf` + `shapely` (vorhanden, siehe dwg_to_ceilings)

## Pipeline

1. **Sammeln** — 2D-Elemente (Line/Arc/Circle/PolyLine inkl. Bogensegmente) live via
   `ELM_SAB.Get2DGeometryOfElements`: ganze Ebene (`--layer`, optional `--worksheet`),
   **aktuelle Selektion** (`--selektion`) oder DXF-Datei (`--dxf`).
2. **Klassifizieren** — Dicken-Schätzung `d = 2·Fläche/Umfang`; nur dünne, längliche
   Polygone (Default 0.05–0.65 m) werden Wände. **Raum-Polygone werden übersprungen
   statt hinterher gelöscht.** Mini-Artefakte (< 0.01 m²) fliegen raus.
3. **Erzeugen** — `CreatePolygonWalls` in 200er-Batches, undoable, mit Höhe/Geschoss/
   Ziel-Ebene/Aufbau als Parameter. Antwort ist positionsgleich zum Request →
   Erfolg pro Polygon zuordenbar.

## Anwendung

```bash
# Erst IMMER die Vorschau:
python3 konturen_zu_waende.py --layer "EBENE" --worksheet "BSN_NAGEL 00" --dry-run

# Dann erzeugen:
python3 konturen_zu_waende.py --layer "EBENE" --worksheet "BSN_NAGEL 00" \
    --height 3.2 --floor 0 --ziel-ebene "A_01_TRAGWAND" --composite "Kalksandstein" --yes
```

## Selektions-Modus (Markierungsrahmen) <!-- 2026-07-14 -->

Der Markierungsrahmen ist per API nicht lesbar — Workaround: **Rahmen ziehen + Cmd+A**
(selektiert nur Inhalt des Rahmens), dann:

```bash
python3 konturen_zu_waende.py --selektion --dry-run   # Vorschau
python3 konturen_zu_waende.py --selektion --yes --guids-out neue_waende.json
```

Der Modus macht automatisch, was im Ebenen-Modus Parameter sind:

- Verarbeitet **alle SAB-Wand-Ebenen zugleich** (A_01_TRAGWAND, A_02_LEICHTWAND,
  A_021_TRENNWAND; mit `--layer` auf eine einschränkbar). Ziel-Ebene = Quell-Ebene.
- **Geschoss automatisch** = dominantes `floorIndex` der gelesenen Konturen.
- **Wandhöhe automatisch** = Level-Differenz zum nächsten Geschoss (Tapir `GetStories`);
  oberstes Geschoss übernimmt die Höhe des darunterliegenden (mit Hinweis).
- **Duplikat-Registry** (`--registry`, Default `konturen_registry.json` im
  Arbeitsverzeichnis): erfolgreich erzeugte Polygone werden pro Geschoss gemerkt und
  bei Folge-Läufen übersprungen — überlappende Rahmen erzeugen nichts doppelt.
  **Geschoss-bewusst**: übereinanderliegende Geschosse haben oft identische
  XY-Konturen, die sind KEINE Duplikate (live erlebt: hätte im 2. UG echte Wände
  verschluckt). Registry ist projekt-spezifisch → gehört nicht ins Repo.

Damit lässt sich ein Gebäude Rahmen für Rahmen abarbeiten: Rahmen ziehen, Cmd+A,
Script laufen lassen, nächster Rahmen — Geschoss-Wechsel erkennt das Script selbst.

## Bekannte Grenzen (Stand 2026-07-14)

- Ein Polygon = eine Wand. L-/U-förmige Wandzüge werden EINE Polywand (meist gewollt).
- Wände entstehen lagegleich (Koordinaten 1:1) im Ziel-Geschoss — Quelle muss
  lagerichtig sein.
- ~2–9 % der Polygone scheitern in `ACAPI_Element_Create` (degenerierte/selbst-
  schneidende Bestandskonturen) — normal, Fehlzahl wird gemeldet.
- Wand-Erzeugung braucht ein **aktives Grundriss-Fenster** (im richtigen Geschoss).
- Claude-Workflow: erst `--dry-run` zeigen, User bestätigt, dann mit `--yes` ausführen
  (SAFE-01: Create ist frei, aber Massen-Erzeugung immer mit Zahlen ankündigen).
