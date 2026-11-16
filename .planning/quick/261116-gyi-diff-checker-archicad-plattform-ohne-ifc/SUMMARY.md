---
status: incomplete
date: 2026-11-16
---
# Diff-Checker Archicad ↔ Plattform ohne IFC

Branch `elm-0.11.13-diff-checker`, nur lokal, nicht gepusht.

Gebaut:
- Bernds Abgleich-Paket vom 10.11. eingebaut (`core/elmonkey/abgleich.py`, Skript, Rezept, SKILL-Verweis). Auftrag 09
  behoben: `elementIFCIds`, `externalIFCId` vor `ifcId`; Testattrappe auf die echte Antwortform gebracht.
- `core/elmonkey/diff.py` + 10 Tests: IFC-Schreibweisen, Mengen, Räume, Öffnungen, Plattform-eigene, Geometrie je
  Seite (3 mm), rohe Box wird nicht verglichen, `ergebnis.json`.
- Skript-Unterbefehl `diff` + 2 Tests (liest Archicad direkt, kein IFC-Export; Rückfall ohne `GetSolidBounds`).
- Add-on `ELM_SAB.GetSolidBounds` (header-only, in AddOnMain registriert), Version 0.11.13.
- Kern 0.4.0, MCP 0.2.4 (Suchbegriffe), READMEs, `mcp-extension.md`, Rezept.

Tests: core 94 bestanden, Skript 10 bestanden, MCP-Katalog 6 bestanden. `mcp/tests/test_server.py` lief lokal nicht
(Paket `mcp` fehlt im benutzten venv, unabhängig von dieser Änderung).

Offen (deshalb incomplete):
- Add-on ist nicht gebaut: C++ ohne lokalen Build geschrieben, die CI muss ihn erst übersetzen.
- Kein Live-Lauf gegen Archicad. Zu prüfen: liefert das 3D-Modell Körper, wenn das 3D-Fenster nie offen war;
  stimmt die Körper-Box mit der IFC-Geometrie überein (Gegenprobe AFWC).
- Plattform-Monkey: `bbox` (Archicad-Meter) je Bauteil in `stand.json`, `plattform.geometrie` in
  `archicad_abgleich_melden` → Geometrie-Operationen. Ohne das vergleicht der Diff keine Geometrie.
- Annahme aus Bernds Paket bleibt: Archicad „Gruppe.Name“ = IFC „Pset.Name“.
