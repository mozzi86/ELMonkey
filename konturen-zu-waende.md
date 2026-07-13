# Konturen → Wände (2D-Linienwerk zu Polygon-Wänden)

*Live-verifiziert 2026-07-13, THN EG-Bestandsplan: 2.032 Wände aus 16.468 Segmenten.*

Automatisiert den Handgriff „Wand-Werkzeug → Geometriemethode Polygon → Magic Wand
in jede geschlossene Kontur". Script: [`../scripts/konturen_zu_waende/`](../scripts/konturen_zu_waende/).
Braucht **ELM_SAB_Add-On ≥ v0.4**.

## Pipeline

1. **Geometrie lesen** — `ELM_SAB.Get2DGeometryOfElements` (Line/Arc/Circle/PolyLine
   inkl. Bogensegmente; schließt die Tapir-Lücke „Not yet supported"). Quelle: Ebene
   in Arbeitsblatt/Grundriss oder aktuelle Selektion.
2. **Polygonisieren** — shapely `unary_union` + `polygonize` (Magic-Wand-Äquivalent);
   Bögen werden zu Segmenten geplättet (8 Punkte/Radiant).
3. **Klassifizieren** — Dicken-Schätzung `d = 2·Fläche/Umfang`; Wand wenn 0.05–0.65 m
   (Parameter). Raum-Polygone werden ÜBERSPRUNGEN statt erzeugt+gelöscht.
   Mini-Artefakte < 0.01 m² fliegen raus.
4. **Erzeugen** — `ELM_SAB.CreatePolygonWalls` (APIWtyp_Poly), Batch 200, undoable.

## Regeln (SAB-Konvention, User-definiert 2026-07-13)

- **Quell-Ebene = Ziel-Ebene**: Wände landen auf der Ebene ihrer Konturlinien.
- Wandtyp nach Ebene: `A_01_TRAGWAND` → Tragwände, `A_02_LEICHTWAND` /
  `A_021_TRENNWAND` → Leichtbau. Nur Wand-Ebenen verarbeiten —
  `_SCHR`/`AUSSPAR`-Ebenen sind KEINE Wandkonturen.
- Protokoll: erst Smoke-Test (nur geschlossene Polylinien, wenige Wände) →
  User prüft → dann Massenlauf. Immer `--dry-run` vor `--yes`.

## Gotchas (alle live erlebt)

- **Wand-Erzeugung braucht aktives Grundriss-Fenster** (Wände = Modellelemente).
  Lesen geht aus jedem Fenster/DB-Kontext.
- **„Ich sehe keine Wände"**: erst Filter prüfen (Tapir `FilterElements` mit
  IsVisibleByRenovation/IsVisibleByLayer/OnActualFloor) — wenn alle grün:
  Zoom-Problem → `ChangeSelectionOfElements` + User macht „Zoom auf Auswahl".
- **Markierungsrahmen ist per API nicht lesbar** — Workaround: Rahmen aktiv +
  Cmd+A = Selektion nur im Rahmen; Script-Scope = Selektion.
- **Duplikat-Falle**: Konturen bereits erzeugter Wände werden beim Neu-Lauf wieder
  polygonisiert → vorher alte Testwände löschen (GUIDs sichern!).
- ~4 % der Polygone scheitern in ACAPI_Element_Create (degenerierte/selbst-
  schneidende Konturen) — normal, Fehlerliste prüfen reicht.
- Median-Dicke im Bestandsplan wirkt dünn (10–11 cm): Öffnungslinien zerteilen
  Wandbänder; mit `--min-dicke` schärfen, wenn zu viel Kleinkram.

## Worked Example (THN EG, 2026-07-13)

| Ebene | Segmente | Regionen | Wände | Raum übersprungen |
|---|---|---|---|---|
| A_01_TRAGWAND | 16.468 | 6.313 | 2.083 | 1.135 |
| A_02_LEICHTWAND | 623 | 68 | 22 | 35 |
| A_021_TRENNWAND | 117 | 14 | 12 | 2 |

Ergebnis: 2.032 erzeugt, 85 degeneriert fehlgeschlagen, H=4.20, EG, ein Undo-Schritt pro Batch.
