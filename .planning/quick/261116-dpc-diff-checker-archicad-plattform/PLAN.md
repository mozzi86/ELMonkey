---
slug: 261116-dpc
status: planned
---
# Diff-Checker Archicad ↔ Plattform ohne IFC-Export (ELM_SAB 0.11.13, Kern 0.4.0, MCP 0.3.0)

Anforderung 16.11.2026 (Büro, nach einem Live-Abgleich per Komplett-IFC). Ziel: das offene Archicad-Modell
direkt mit dem Stand der Plattform (`stand.json` aus `archicad_abgleich_lesen`) vergleichen und nur die
Unterschiede liefern — neu / weg / Eigenschaften / Geometrie je Seite (x−/x+/y−/y+/z−/z+, Toleranz 3 mm) —
im Format `sab-archicad-abgleich/1` (`archicad_abgleich_melden`), ohne IFC-Datei.

Ausgangslage: `origin/main` = `beb82d5`; eigener Zweig `elm-diff-plattform` im Worktree. Repo öffentlich: keine
Projekt-, Personen-, Ortsnamen, keine Projekt-Höhenwerte. Kein lokaler Build (CI AC27–30 × Mac/Win).
Kein Release, kein Tag, kein Spiegeln, keine Installation, kein Zugriff auf ein laufendes Archicad.

## Tasks

### 1. Grundlage: Paket „Plattform-Abgleich" vom 10.11. übernehmen
- **files:** `core/elmonkey/abgleich.py`, `core/tests/test_abgleich.py`, `skills/archicad/recipes/plattform-abgleich.md`,
  `skills/archicad/scripts/plattform_abgleich/*`, `skills/archicad/SKILL.md` (eine Zeile)
- **action:** Inhalt unverändert übernehmen (das Gegenstück der Plattform `core/plattform_monkey/abgleich.py` nennt
  `elmonkey.abgleich` als Partner, liegt aber noch nicht in main); Beispiel-Projektnummer neutralisieren.
- **verify:** `uv run --with pytest pytest -q` in `core/` und im Skriptordner grün.

### 2. Archicad-Monkey: Lesebefehl `ELM_SAB.GetDiffDataELM`
- **files:** `archicad-addon/ELM_SAB_Tapir/Sources/GetDiffDataCommandELM.{hpp,cpp}`, `AddOnMain.cpp`, `AddOnVersion.hpp`
- **action:** je Element (alle Elemente der Grundriss-Datenbank, ohne Sichtbarkeits-/Umbaufilter, 2D-Typen
  ausgelassen, Paging) GUID, IFC-GlobalId, IFC-Klasse, IFC-Attribute und -Eigenschaften (Vorschau des Übersetzers),
  Geschoss, Umbau-Status, Element-ID, Ebene (+ ausgeblendet), Aufbau, Oberflächen, Klassifizierung, Zone,
  Wirt von Öffnungen, Geometrie als Box aus den Körpern des 3D-Modells (nach Verschneidung; Mehrschicht = ein Körper)
  mit Quelle; Kontext (Datenbank umgeschaltet + zurückgestellt, 3D-Schnittebenen aktiv, ohne 3D-Modell).
  Ändert nichts.
- **verify:** Raw-JSON-Schemas parsen (`check_raw_json_schemas.py`), CI 8/8.

### 3. Python-Monkey: `elmonkey.diff_plattform` (Kern 0.4.0)
- **files:** `core/elmonkey/diff_plattform.py`, `core/tests/test_diff_plattform.py`, `core/elmonkey/__init__.py`,
  `core/pyproject.toml`
- **action:** reine Funktionen: Archicad-Antwort → Abgleich-Bauteile, Wert-Normalisierung, Mengen raus,
  Öffnungen über Tür/Fenster, Zonen getrennt, Bauteilteile zusammenfassen, Geometrie je Seite mit 3 mm,
  Paare „gebaut", Ergebnis im Format `sab-archicad-abgleich/1` (über `abgleich.planen`/`entscheiden`) plus
  Erweiterungsteil `diff`. Tests je Zeile der Fallen-Tabelle mit synthetischen Daten.
- **verify:** `uv run --with pytest pytest -q` in `core/`.

### 4. MCP: Werkzeug `elmonkey_diff_plattform` (0.3.0)
- **files:** `mcp/elmonkey-mcp/src/elmonkey_mcp/addons/archicad/diff.py`, `…/addons/archicad/__init__.py`,
  `…/catalog.py` (Synonyme), Tests, `pyproject.toml`, `__init__.py`
- **action:** `stand_link`/Pfad + optional Basis → Befehl seitenweise lesen → Vergleich → `ergebnis.json`
  (Datei) + Zusammenfassung. Links nur https.
- **verify:** `uv run --with pytest pytest -q` in `mcp/elmonkey-mcp` (Archicad-Aufrufe durch Attrappe ersetzt).

### 5. Doku, Versionen, Handoff, Push, CI, PR
- **files:** Rezept `plattform-abgleich.md` (Abschnitt Diff-Checker), `skills/archicad/reference/diff-plattform.md`,
  READMEs, `docs/UMBAU.md`, `.planning/STATE.md`, SUMMARY, Handoff
  `.planning/handoffs/handoff_elmonkey_diff-plattform_2026-11-16.md`
- **verify:** CI 8/8 grün auf `elm-diff-plattform`; PR gegen main offen, nicht gemergt.
