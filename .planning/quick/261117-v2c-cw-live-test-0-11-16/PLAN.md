---
quick_id: 261117-v2c
date: 2026-11-17
---
# Skill: ELMonkey 0.11.16 live getestet (Fassaden-Rasterursprung)

Live-Befund AC29 Mac, ELMonkey 0.11.16: `GetCurtainWallFramesELM` funktioniert, der feste Pfosten war ein normaler Rasterpfosten; `SetCurtainWallSegmentELM` mit `gridOriginAtStart`/`gridOrigin` setzt den Ursprung weder über Memo noch über Segment-Element (`gridOriginReached: false`).

Zweig `elm-0.11.14-cw-rasterursprung`.

1. `skills/archicad/recipes/curtain-walls.md`: 0.11.16-Eintrag „noch nicht live getestet“ durch den Befund ersetzen, Vorbehalt zum Ursprungspunkt auflösen (Marker 2026-11-17).
2. Push, kein Merge.

## Nicht in diesem Task

Code-Änderungen, Merge nach main, Release.
