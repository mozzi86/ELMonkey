# Handoff — ELMonkey 0.10.0 (Tapir 1.7.2 + Archicad 30), Stand 2026-10-20 abends

Vorgänger: `handoff_elmonkey_0933-demo-wunschliste_2026-10-20.md` (Demo, 0.9.33–0.9.35).
Repo `~/.claude/repos/archicad-skill` (= `~/.claude/skills/archicad`), Änderungen über /gsd-quick.

## Versionsstand

| Version | Wo | Stand |
|---|---|---|
| 0.9.35 | **Release** (`elm-sab-tapir-latest`), main bis 867d400 | live getestet: leer 40/41 (offen B6), lokal 8/8; README zweisprachig mit Demo-Bildern; MIT-Lizenzhinweise Tapir + Lorántfy |
| **0.10.0** | **Release** seit 21.10. (main `c855bb1`, CI-Lauf 39508968282: 8/8 grün, Release „ELMonkey 0.10.0 — AC27/28/29/30 (CI)“), bei Mudi eingebaut | Tapir 1.7.2 + AC30; **live getestet 21.10.: 11/11 + 40/41 (offen nur B6), keine Regression**; offen: Speichern aus 3D (`gegenprobe_0100.py <port> datei`) |

Mudis Auftrag 20.10.: „watcher stellen und addon bei mir installieren handoff schreiben und live gehen“ → Wächter `tools/watch_ci_install.sh 3c4b713 0.10.0` baut das AC29-Mac-Bundle bei Grün ein (nur wenn Archicad zu ist; Sicherung `~/.scratch/elm-bundle-0.10.0/backup_alt/`); danach Merge Branch → main (= Release 0.10.0).

## Was der Merge gemacht hat (Details: reference/mcp-extension.md § 0.10.0)

- Drei-Wege-Merge im Arbeitsrepo `~/.scratch/elmonkey/tapir-merge` (Zweige base/ours/theirs), Basis Upstream `18734069` (nicht Tag 1.5.4 – unser Import war ein Zwischenstand vom 15.07.).
- Tapirs Lösung statt unserer: Decken-Absturz (#452), isSlanted (#508), GDL Tür/Fenster, SaveProject (#681 Grundriss-Wechsel). **Unser 0.9.34-Speicherfix war gefährlich** (überschreibt schreibgeschützt geöffnete Projekte) → SaveProjectCopy nutzt jetzt den Grundriss-Weg.
- Behalten/eingebaut: Vorprüfung `[CODE]`, layerName, zReference, Rücklese, Delete mit Teamwork-Reservierung (Antwort jetzt **je Element**), VersionChecker gegen unser Release (Hintergrund-Thread von Tapir), Tapir-Selbst-Update per Laufzeit-Sperre aus.
- CI-Fallen: macOS `-Werror -Wunused-function` + MSVC C4505 bei `#if 0`-Code → Laufzeit-Sperre; AC30 `ToLowerCase` → `GetLowerCased`; Mac-Runner-Engpass → Jobs „cancelled“ (kein Code-Fehler, neu auslösen).

## Sofort als Nächstes

1. **Live-Test 0.10.0** (Archicad 29, Mac):
   - leeres Projekt aus der Bürovorlage: `python3 ~/.scratch/elmonkey/tools/gegenprobe_0934.py <port> leer 0.10.0` und `python3 ~/.scratch/elmonkey/tools/gegenprobe_0100.py <port> leer`
   - gesichertes lokales Projekt (z. B. `~/Desktop/training session.pln`): `gegenprobe_0100.py <port> datei` (Speichern aus dem 3D-Fenster)
   - Port per `lsof -nP -a -p <PID> -iTCP -sTCP:LISTEN` (meist 19723; 65050 ist NICHT die JSON-API). Zweite Instanz = 19724.
2. Bei Fehlern: Rückbau `~/.scratch/elm-bundle-0.10.0/backup_alt/` (= 0.9.35) und Fix als 0.10.1.
3. Kollegen informieren (Text unten). Windows-Installer + AC30 sind nur per CI gebaut, nicht live getestet.
4. B6 (Dach-/Schalen-Favoriten der Bürovorlage → APIERR_BADINDEX) mit Tapir 1.7.2 erneut prüfen; sonst feldweise eingrenzen.
5. Recherche-Lücken (`~/.scratch/elmonkey/research/archicad-addons-landschaft_2026-10-20.md`): Schutzschicht im elmonkey-mcp (Probelauf, readonly), Jobs mit Wartefrist, Modellprüfung, GDL-Werkzeuge. Weisung: nachbauen, außer Lizenz erlaubt Übernahme.
6. Lizenzhinweis auch in die Bundles/Release-Assets und den Über-Dialog (MIT verlangt ihn bei Weitergabe).

## Nachricht an die Kollegen (von Mudi gekürzt)

> ELMonkey 0.9.35 ist da (Archicad 27/28/29, Mac + Windows). Update: gebt eurem Claude diesen Satz: „Aktualisiere meinen archicad-Skill und ELMonkey nach https://github.com/mozzi86/-archicad/blob/main/recipes/initial-setup.md“ Danke an Maxl, Roman, Sophia und Michi fürs Mithelfen! Details: https://github.com/mozzi86/-archicad — Demnächst 0.10.0 mit Tapir 1.7.2 und Archicad 30.

Nach Release 0.10.0 dieselbe Nachricht mit „0.10.0 … Archicad 27/28/29/30“.

## Werkzeuge

`~/.scratch/elmonkey/tools/`: `ac.py` (Mini-Client), `syn1.sh` (Syntax je Datei, `STRICT=1` = macOS-CI-Schalter; DevKits 27.3001/28.3001/29.3100 unter `~/SchwarzPlatform/devkit/`), `conflict.py` (Merge-Konflikte), `watch_ci_install.sh`, `gegenprobe_0934.py`, `gegenprobe_0100.py`. CI ohne Token: `curl -s https://api.github.com/repos/mozzi86/-archicad/actions/runs?per_page=5`; Build-Logs fehlgeschlagener Läufe liegen auf Zweig `ci-status` unter `logs/`. Python-urllib gegen api.github.com scheiterte zeitweise an SSL → curl nehmen.
