# Quick 261119-tox: Plattform-Abgleich live-tauglich Summary

**Vier Live-Fehler im Abgleich-Skript behoben, Befehl `bauen` (Morphs mit echter Geometrie aus dem Plattform-IFC) aus der BFW07-Vorlage übernommen, Rezept und SKILL.md auf Live-Stand 19.11.2026.**

## Commits (Zweig elm-diff-plattform, nicht gepusht)

| Task | Commit | Inhalt |
|------|--------|--------|
| 1 | a4c0604 | Custom-Eigenschaften, Elemente je Typ (API.GetElementsByType), 4001-Warteschleife in `Archicad._rufen`, `Archicad.api`, curl-Fallback bei SSL, `stempeln` ohne Reservierung, volle Wertform, 6703 |
| 2 | f67285c | `bauen`: ifc_netze, zerlegen, volumen, morph_body, bauplan, sab_klassen, bauen, cmd_bauen; `sab_zuordnung.json` (cmp identisch zur Vorlage) |
| 3 | 2ac557e | Rezept (Kopf, Trigger, Schritt 7, Abschnitt Live-Lauf BFW07) und SKILL.md-Zeile |

## Tests

`python3 -m pytest core/tests -q` (mit `uv run --with pytest`, da pytest in keinem installierten Python liegt): **162 passed, 2 skipped**. Zusammen mit den Skript-Tests im Skriptordner: 172 passed, 2 skipped. Neue Testdatei `core/tests/test_plattform_abgleich_skript.py` (Fake `senden()`, keine Live-Aufrufe an Archicad). `bauen --help` zeigt alle Optionen.

## Deviations from Plan

**1. [Rule 3 - Blocking] Bestehende Skript-Tests angepasst**
- **Found during:** Task 1
- **Issue:** `skills/archicad/scripts/plattform_abgleich/test_plattform_abgleich.py` (nicht in files_modified) brach, weil das Skript jetzt `API.*` direkt sendet (GetElementsByType, Klassifizierung, SetPropertyValues) und „KI generiert <Datum>“ statt „ja“ schreibt.
- **Fix:** Fake um direkte API-Befehle erweitert, Erwartung „KI generiert …“ plus „Stempeln reserviert nicht“. Mit Task 1 committed (a4c0604).

**2. pytest nicht installiert:** Tests liefen über `uv run --with pytest` im Ordner core (ephemer, keine Projektdatei geändert).

## Offene Unsicherheit

Die Antwortform von `API.GetAllClassificationsInSystem` (Schlüssel `classificationItems`, je `classificationItem` + `children`) und `GetDetailsOfClassificationItems` (`classificationItemDetails`) ist nach Plan/Vorlage nachgebaut und nur gegen den Fake getestet, nicht live. `sab_klassen` ist tolerant gegenüber verschachtelter und flacher Form. Beim ersten echten `bauen`-Trockenlauf prüfen, ob Klassen gefunden werden.

## Known Stubs

Keine.

## Self-Check: PASSED

Commits a4c0604, f67285c, 2ac557e vorhanden; `sab_zuordnung.json` und Testdatei vorhanden; nur uv.lock-Dateien und Quick-Ordner ungetrackt.

## Nachtrag Orchestrator (19.11.2026)
Live-Trockenlauf `bauen` gegen BFW Kantinenküche_Hauptdatei: Klassensuche fand nichts (Kinder hängen am Item, Details-Schlüssel `classificationItems`). Fix + Regressionstest in `81863a8`. Danach Trockenlauf: 432 zu bauen / 35 ausgelassen, Klassen, Status (394 Abbruch, 38 Bestand) und 42.996 Dreiecke deckungsgleich mit dem Handlauf vom Nachmittag.
