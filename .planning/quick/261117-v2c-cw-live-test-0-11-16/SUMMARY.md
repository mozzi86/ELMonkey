---
status: complete
---
# 261117-v2c – 0.11.16 live getestet

**Befund (AC29 Mac, 17.11.2026):** `GetCurtainWallFramesELM` liefert Klasse und Lage je Pfosten; der „feste“ Pfosten bei 1,016 m ab Segmentanfang ist ein normaler Rasterpfosten (`class0`). `SetCurtainWallSegmentELM` mit `gridOriginAtStart`/`gridOrigin` meldet `success`, erreicht den Ursprung aber nicht (`gridOriginReached: false`, Rücklese (0,0,0), Pfosten unverändert), weder per Memo noch per Segment-Element. Die Rasterlage bestehender Fassaden ist per API (DevKit 29) nicht verschiebbar; ungleiche Bestandsteilungen nur per Neubau. Rückbau mit `columnWidths: [1.2]` stellt den Ausgangszustand exakt her.

**Geändert:** `skills/archicad/recipes/curtain-walls.md` (0.11.16-Eintrag ersetzt, Vorbehalt aufgelöst). Nur Doku.

**Offen:** Neubau-Weg (`CreateCurtainWallFromAxes`) für ungleiche Bestandsteilungen, vorher Freigabe.
