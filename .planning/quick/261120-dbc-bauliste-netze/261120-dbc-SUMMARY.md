---
phase: quick-261120-dbc
plan: 01
subsystem: plattform_abgleich
tags: [bauliste, netze, morph, plattform]
key-files:
  modified:
    - skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
    - core/tests/test_plattform_abgleich_skript.py
    - skills/archicad/recipes/plattform-abgleich.md
metrics:
  completed: 2026-11-20
---

# Quick 261120-dbc: bauen mit Bauliste-Netzen

`plattform_abgleich.py bauen` baut Morphs aus den echten Netzen der neuen Bauliste (sab-bauliste/1, parameter.koerper). IFC ist optional und gilt nur noch für Verbund-Unterteile mit eigener GUID.

## Commits (lokal, nicht gepusht, Branch bauliste-netze)
- 903b19e feat: Code und Tests
- 5c7d51f docs: Rezept

## Umgesetzt
- `bauliste_laden` (Pfade und https-Links, Zusammenführen per plattformGuid, spätere gewinnt), `bauliste_netze`, `nicht_bearbeitet_pruefen` (Abbruch vor jedem Archicad-Aufruf, `--trotzdem`).
- `bauplan`: Quellen-Priorität status fehler, IFC-Unterteile, Bauliste-Netz (ein Morph über alle Körper), IFC-Netz, sonst "nur Hülle". Neue Felder `quelle` und `netz_kennzahlen`. `netz_pruefung` liefert quelle-Zählung, Summen und `netz_pruefen` (auch `netz_pruefen.json`).
- CLI: `--bauliste` mit mehreren Werten, `--trotzdem`, `--ifc` optional.
- Rezept Schritt 7 neu, alter Hüllen-Rat als überholt markiert.
- Tests: 184 passed, 2 skipped (bestehende unverändert grün, 10 neue).

## Offline-Check mit echten Daten (stichprobe.json, kein Archicad)
- 51 Bauteile, alle geometrie "netz", status "offen", nicht_bearbeitet 0.
- bauplan (alle Klassen auf eine Dummy-Klasse gemappt): 51 im Plan, 0 ausgelassen, quelle bauliste 51, ifc 0.
- Dreiecke gesamt (morph_body, nach zerlegen): 44.577.
- Kanten mit mehr als 2 Flächen nach zerlegen: bei keinem Bauteil (0 von 51). Vor zerlegen hatten 8 Bauteile je 1 solche Kante (Zuluftstutzen, Fallrohr, Steigrohr, Zuluftleitung Ø315).
- Plattform-Kennzahlen aus der Bauliste: nicht_mannigfaltige_kanten 1.170 (Summe), offene_kanten 21; 25 Bauteile in netz_pruefen.
- Auffälligkeit: Index-basiert gezählt haben viele Leitungen offene Kanten (z. B. Heizungsleitung Vorlauf Strang Nord 435, Zuluftleitung Ø315 224 bereits in den Rohdaten vor zerlegen), obwohl die Plattform nur 3 bzw. 0 offene Kanten meldet. Die Plattform zählt offenbar nach Position, die Netze sind über getrennte Indizes verschweißt. Ob Archicad diese Körper als geschlossen annimmt (Rücklese `geschlossen`), zeigt erst der Live-Lauf.

## Abweichungen
- Task 1 und 2 in EINEM Commit statt zwei (gleiche Datei, Implementierung und Tests zusammen geschrieben, Tests liefen nach der Implementierung grün; RED-Phase nicht separat committet).
- Zusätzlich: Netz ohne Dreiecke wird mit Grund "Netz ohne Dreiecke" ausgelassen (Schutz vor IndexError bei leerer Box).

## Nicht live gelaufen
Kein Archicad-Aufruf, kein Netz.

## Self-Check: PASSED
Dateien und beide Commits vorhanden, Arbeitsbaum außer Planordner und uv.lock sauber.
