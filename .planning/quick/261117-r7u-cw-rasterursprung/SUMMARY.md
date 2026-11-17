---
status: complete
---
# 261117-r7u – Rasterursprung bei Fassaden (SetCurtainWallSegmentELM columnOrigin/rowOrigin)

**Anlass (live THN, 17.11.2026):** Beim nachträglichen Ändern des Spaltenrasters bleibt ein Pfosten am Rasterursprung stehen (Beispiel 27BB6F2C bei 0,986 m); ungleiche Bestandsteilungen wie [0.85, 1.2, 1.2, 0.9, 1.2, 1.2, 0.87] waren nicht treffbar (29 von 46 Fassaden).

**Umsetzung:** `SetCurtainWallSegmentELM` nimmt optional `columnOrigin`/`rowOrigin` (`start`, `startFromCenter`, `alignToCenter`, `end`) und setzt `gridOriginType` am Spalten-/Zeilenraster; Antwort liest den Ist-Wert zurück. Version 0.11.14 (main stand schon auf 0.11.13). MCP 0.2.6 mit Suchbegriffen „ursprung/rasterursprung/raster“, 11 Tests grün. Skill-Eintrag in `recipes/curtain-walls.md`, als „noch nicht live getestet“ markiert.

**Prüfungen:** Feldnamen in DevKits 27.3001, 28.3001, 29.2001, 29.3100, 30.3000 gleich; JSON-Schemas parsen; CI-Lauf 44010592819: 8/8 grün (AC27–30, Mac/Win).

**Commits:** 91897cf, 58cafb6, 294bd01, d8b88eb (Zweig `elm-0.11.14-cw-rasterursprung`).

**Offen:** Live-Test AC29 Mac (Installation braucht Archicad-Neustart, Entscheidung des Nutzers); danach alte Skill-Aussagen im Abschnitt bereinigen; Merge nach main + Release + Spiegeln nach -archicad.
