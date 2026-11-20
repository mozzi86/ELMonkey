---
quick_id: 261120-eux
type: quick
autonomous: true
files_modified:
  - skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
  - skills/archicad/scripts/plattform_abgleich/sab_zuordnung.json
  - core/tests/test_plattform_abgleich_skript.py
  - skills/archicad/recipes/plattform-abgleich.md
  - skills/archicad/SKILL.md
---
# Quick 261120-eux: Klassifizierungssystem wählbar + drei Klassen-Zuordnungen

Live UNA 20.11.2026: Die SAB-Bürovorlage heißt seit der Umbenennung „Graphisoft Klassifizierung“ (gleiche
Klassen, 324 Stück). `sab_klassen()` sucht fest „SAB_Klassifizierung_29“ → `bauen` bricht ab. Außerdem fehlen
in `sab_zuordnung.json` IfcWall, IfcWindow, IfcDiscreteAccessory (UNA: 46 von 172 Teilen). Mudi hat die
Zuordnung am 20.11.2026 freigegeben.

## Aufgabe 1 (Test zuerst)
- `SAB_SYSTEME = ("SAB_Klassifizierung_29", "Graphisoft Klassifizierung")`; `sab_klassen(ac, name=None)`:
  ist `name` gesetzt, genau diesen suchen (Fehler wie bisher); sonst das erste vorhandene aus SAB_SYSTEME.
  Rückgabe zusätzlich der gefundene Name (Signatur `(system_id, klassen, name)` — alle Aufrufer anpassen),
  der Name kommt in Trockenlauf und Ergebnis unter `"klassifizierung"`.
  Fehlertext nennt beide gesuchten Namen und die im Projekt vorhandenen Systeme.
- `bauen` CLI: `--system NAME` (optional), durchgereicht.
- `sab_zuordnung.json`: `"IfcWall": "Wand"`, `"IfcWindow": "Fenster"`, `"IfcDiscreteAccessory": "Zusatzgerät / Einbauteil"`;
  `_hinweis` um „UNA 20.11.2026: Wall/Window/DiscreteAccessory freigegeben (Mudi); System auch ‚Graphisoft Klassifizierung‘“ ergänzen.
- Tests mit gefälschtem Archicad: nur „Graphisoft Klassifizierung“ vorhanden → gefunden; `name` explizit falsch →
  Fehler mit beiden Listen; Zuordnung enthält die drei neuen Schlüssel.

## Aufgabe 2
- Rezept `plattform-abgleich.md`: kurzer Punkt mit `<!-- 2026-11-20 -->`: System wird automatisch gefunden
  (beide Namen), `--system` erzwingt; fehlende Eigenschaften lassen sich per `TapirCommand.CreatePropertyDefinitions`
  anlegen, nur mit `isEditable: true` und `defaultValue {basicDefaultValue {type: string, status: userUndefined}}`
  (sonst „failed to create the property“), Verfügbarkeit = alle Klassen-Items — nur nach Ja des Zeichners.
- `skills/archicad/SKILL.md`, Abschnitt „Nach jedem Create — KI-Stempel“: den Punkt „Fehlt die Property im
  Projekt: User … bitten … Nicht still weglassen, nicht selbst per API anlegen (…fragil…)“ ersetzen durch die
  neue Weisung (Mudi, 20.11.2026, Marker `<!-- 2026-11-20 -->`): Claude darf fehlende Eigenschaften selbst anlegen
  und unpassende anpassen (Typ, Gruppe, Verfügbarkeit, Name) — per `TapirCommand.CreatePropertyDefinitions` /
  `UpdatePropertyDefinitions`, Rezept wie oben (isEditable, defaultValue userUndefined, Verfügbarkeit alle
  Klassen-Items), danach Rücklese (GetPropertyIds + GetDetailsOfProperties) und dem Nutzer in einem Satz sagen,
  was angelegt/geändert wurde. Löschen von Eigenschaften bleibt Confirm-pflichtig (SAFE-01). Nicht still weglassen.
  Auch `files_modified` im Frontmatter um SKILL.md ergänzen. Commit dieser Änderung separat.
- `PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests -q` grün.
- Commit mit `git -c user.email=mozzi666@gmail.com`, Trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Nicht pushen.
