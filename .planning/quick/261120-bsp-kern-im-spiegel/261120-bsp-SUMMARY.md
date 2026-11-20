# Quick 261120-bsp: Kern im Spiegel Summary

Der gespiegelte Skill bringt `elmonkey` jetzt unter `scripts/_kern/elmonkey` mit. `plattform_abgleich.py` lädt den Kern per Kaskade (installiert, Repo-`core/`, Spiegel-`_kern`) und bricht ohne Kern mit deutscher Meldung ab.

## Tasks

1. Import-Kaskade in `plattform_abgleich.py` (`_kern_laden()`, Längenprüfung statt `parents[4]`-Zugriff) plus Tests `test_spiegel_layout_mit_kern` und `test_spiegel_layout_ohne_kern`. RED bestätigt (beide scheiterten), danach GREEN.
2. Workflow `skill-spiegeln.yml`: Trigger `core/elmonkey/**`, rsync des Kerns nach der Schleife (wegen `--delete`), `HERKUNFT.txt` mit `${GITHUB_SHA}` nur als Shell-Variable. Recipe-Hinweis mit Marker 2026-11-20.

Commit: einer, lokal (Hash per `git log -1`), Autor-Mail mozzi666@gmail.com, kein Push.

## Verifikation

- `pytest core/tests`: 174 passed, 2 skipped (ifcopenshell fehlt, unabhängig). Beide neuen Spiegel-Tests liefen.
- YAML-Check: ok.

## Deviations

None - plan executed as written.

## Self-Check: PASSED
