# Quick 261119-w9h: Verbünde als Kopf + Unterteile bauen

**Ergebnis:** `plattform_abgleich.py bauen` baut Verbünde standardmäßig als Kopf plus je ein Morph pro Unterteil (`--verbund einzeln`). `--verbund zusammen` verhält sich wie der alte Code.

## Commits (Branch fix-aggregation-eltern, nicht gepusht)
- 6a1d18a: `ifc_teile()`, `ifc_netze()` darauf aufgesetzt, `bauplan(verbund=)`
- Zweiter Commit (HEAD): `bauen(verbund=)`, Plattform-Verbund-Stempel, `kopf` in den JSON-Dateien, Zähler, CLI `--verbund`

Geändert wurden nur `skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py` und `core/tests/test_plattform_abgleich_skript.py`.

## Umsetzung
- `ifc_teile(pfad)` liefert `(eigene, kinder, info)`. `info` enthält Name (`\X2\`-dekodiert) und Klasse in IFC-Schreibweise. `ifc_netze` bildet die Verbund-Netze daraus, mit Zyklenschutz (T-w9h-01).
- `bauplan` (einzeln):
  - Unterteil-Klasse = Zuordnung der Unterteil-Klasse (Schlüssel ohne Beachtung der Groß-/Kleinschreibung), sonst Klasse des Kopfs.
  - Name = „Kopf / Kind“. Ebene und Status kommen vom Kopf.
  - `nur` und `ohne` greifen auf Kopf-GUID und auf Unterteil-GUIDs.
  - Jeder Plan-Eintrag hat `kopf` (bei normalen Teilen None).
- `bauen`:
  - Plattform-GUID = eigene GUID des Unterteils.
  - Plattform-Verbund = Kopf-GUID, nur wenn die Eigenschaft existiert, mit Rücklese in `stimmt`.
  - Fehlt die Eigenschaft, steht im Ergebnis der Hinweis „Plattform-Verbund fehlt – Kopf nur in gebaut.json“, ohne Fehler.
  - `verbuende` und `unterteile` stehen im Trockenlauf und im Endergebnis.

## Tests
`PYTHONPATH=core python -m pytest core/tests -q` ergibt 170 passed, 2 skipped (inklusive 6 neuer Tests: ifc_teile, bauplan einzeln/zusammen/Filter, Trockenlauf, Ausführung mit Verbund-Stempel, ohne Verbund-Eigenschaft, CLI-Vorgabe).

## Abweichungen
- In der Umgebung war kein pytest installiert, und `core` ist nicht installiert. Die Tests liefen deshalb mit `uv run --no-project --with pytest` und `PYTHONPATH=core`. Das ist der übliche Testlauf, ohne Änderung im Repo.
- Der Arbeitsbaum steht auf `fix-aggregation-eltern`, wie vom Auftrag verlangt. Die Worktree-Branch-Prüfung `worktree-agent-*` wurde dafür bewusst nicht angewendet.
- Kleine Randabweichung: Unterteile ohne GUID werden in `ifc_netze` nicht mehr mitgezählt. Im Plattform-IFC haben alle Produkte eine GlobalId.

## Known Stubs
Keine.

## Self-Check: PASSED
