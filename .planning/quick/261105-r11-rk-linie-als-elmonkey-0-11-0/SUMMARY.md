---
slug: 261105-r11
status: complete
---
# Ergebnis

- Zweig `elm-0.11.0-rk-integration`: 11 RK-Commits (0.10.1–0.10.8) über Hauptlinie 0.10.4, Version 0.11.0.
- Einziger Konflikt `AddOnVersion.hpp`; keine doppelten Befehlsnamen; 0.10.4-Ebenen-Fixes unverändert.
- Neutralisiert (User 05.11.): Autor RK → `RK <rk@users.noreply.github.com>`, BFW aus Kommentaren.
- AC30-Fix: `LibPartEdit0104ELM.hpp` `ToLowerCase` → `GetLowerCased` (Makro `ELM_LOWER_CASED`).
- CI Lauf 42036133079: 8/8 grün (AC27–30 × Mac/Win), Kopf `e918db7`.
- Skill: Abschnitt „ELMonkey 0.11.0" in `reference/mcp-extension.md` (RKs Fallen + AC30-Falle).
- Offen: nicht in `main` gemergt, nicht installiert, gemeinsamer Live-Test beider Linien steht aus.

## Nachtrag 05.11.2026 abends — Polygonwände (0.11.1 → 0.11.11)

Ursache des CreatePolygonWalls-Absturzes/Normalwand-Problems: Archicad nimmt `APIWtyp_Poly` nur an, wenn die
Wand-Werkzeug-Voreinstellung der Datei Basis-Struktur hat (Diff vor/nach Handwand: einziges Feld
`modelElemStructureType`). Fix: `ACAPI_Element_ChangeDefaults` auf Basis vor `GetDefaults`, danach zurück.
Zehn CI-Builds (0.11.2–0.11.11), live verifiziert in frischer INT-Datei (AC29 5101), Gegenprobe AC28 (7006).
Details: skills/archicad/reference/mcp-extension.md, Abschnitt „0.11.1–0.11.11 Polygonwände, GELÖST".
