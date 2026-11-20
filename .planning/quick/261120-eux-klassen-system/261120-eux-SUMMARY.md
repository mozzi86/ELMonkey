# Quick 261120-eux Summary

Klassifizierungssystem wählbar (SAB_Klassifizierung_29 oder Graphisoft Klassifizierung, `--system` erzwingt) plus drei neue IFC-Zuordnungen; Skill erlaubt selbst angelegte Eigenschaften.

## Commits
- cfe0970 feat: `sab_klassen(ac, name=None)` gibt `(system_id, klassen, name)` zurück; Fehlertext nennt gesuchte und vorhandene Systeme; `bauen --system`; Name unter `klassifizierung` in Trockenlauf und Ergebnis; `sab_zuordnung.json` um IfcWall, IfcWindow, IfcDiscreteAccessory ergänzt; 3 neue Tests, Bestandstest angepasst.
- docs-Commit: SKILL.md (KI-Stempel-Punkt ersetzt, 155 Zeilen) und Rezept plattform-abgleich.md.

## Tests
core/tests: 187 passed, 2 skipped.

## Deviations
None. `SAB_SYSTEM` bleibt als Alias auf das erste System erhalten (Bestandstests nutzen es). Die Befehlsnamen `CreatePropertyDefinitions` / `UpdatePropertyDefinitions` stimmen mit PropertyCommands.cpp überein.
