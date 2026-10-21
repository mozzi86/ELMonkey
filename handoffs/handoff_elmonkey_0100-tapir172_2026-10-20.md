# Handoff — ELMonkey 0.10.0 (Tapir 1.7.2 + Archicad 30), Stand 2026-10-21 mittags

Vorgänger: `handoff_elmonkey_0933-demo-wunschliste_2026-10-20.md` (Demo, 0.9.33–0.9.35).
Repo `~/.claude/repos/archicad-skill` (= `~/.claude/skills/archicad`), Änderungen über /gsd-quick. `main` = `8d94dba`.

## Endstand

| Version | Wo | Stand |
|---|---|---|
| 0.9.35 | abgelöst (war Release 20.10.) | live: leer 40/41, lokal 8/8 |
| **0.10.0** | **Release** `elm-sab-tapir-latest` „ELMonkey 0.10.0 — AC27/28/29/30 (CI)“ (main-CI 39508968282: 8/8 grün), **bei Mudi eingebaut** (AC29 Mac) | **live getestet 21.10.: 59/60** — `gegenprobe_0100.py leer` 11/11, `gegenprobe_0934.py leer 0.10.0` 40/41, `gegenprobe_0100.py datei` 8/8 (Speichern aus 3D). Offen nur **B6** |

Rückbau falls nötig: `~/.scratch/elm-bundle-0.10.0/backup_alt/ELM_SAB_AC29_Mac.bundle` (= 0.9.35), davor `~/.scratch/elm-bundle-0.9.35/backup_0.9.34/`.

## Was 0.10.0 ist (Details: reference/mcp-extension.md § 0.10.0)

- Tapir 1.7.2 (314 Befehle) + 44 ELM_SAB-Befehle, Archicad 27–30. Drei-Wege-Merge im Arbeitsrepo `~/.scratch/elmonkey/tapir-merge`, Basis Upstream `18734069` (unser Import vom 15.07. war ein Zwischenstand vor Tag 1.5.4).
- Tapir statt eigener Lösung: Decken-Absturz (#452), isSlanted (#508), GDL Tür/Fenster/Oberlicht, SaveProject (#681, Grundriss kurz aktiv). **Unser 0.9.34-Speicherfix war gefährlich** (überschrieb schreibgeschützt geöffnete Projekte) → SaveProjectCopy nutzt den Grundriss-Weg.
- Behalten: Vorprüfung `[CODE]`, layerName (alle Create-Schemas), zReference, Rücklese, Delete mit Teamwork-Reservierung (Antwort jetzt **je Element**), VersionChecker gegen unser Release im Hintergrund-Thread, Tapir-Selbst-Update per Laufzeit-Sperre aus.
- Neu durch Tapir nutzbar: `floorIndex` bei CreateSlabs/Columns (löst das Heimatgeschoss-Problem der Demo), `favoriteName` je Element, Mengen, 3D-Geometrie, MEP, Keynotes, grafische Überschreibungen, CreateHatches/Lines/Arcs (parallel zu unserem Create2DElements).
- CI-Fallen: macOS `-Werror -Wunused-function` + MSVC C4505 bei `#if 0` → Laufzeit-Sperre; DevKit 30 `ToLowerCase` → `GetLowerCased`; Mac-Runner-Engpass → Jobs „cancelled“ (kein Code-Fehler, neu auslösen).
- Lizenz: `ELM_SAB_Tapir/LICENSE-TAPIR` (MIT, Enzyme APD) + `LICENSE-ARCHICAD-ADDITIONAL-JSON-COMMANDS` (MIT, Lorántfy), Hinweise in beiden READMEs.

## Offene Punkte (Reihenfolge = Vorschlag)

1. **Kollegen-Rollout:** Nachricht unten verschicken. Michi (0.9.18), Maxl (noch tapir-archicad-mcp), Roman/Sophia (eigene Bundles) brauchen den Claude-Satz. Windows-Installer, Windows-Bundles und AC30 sind nur per CI gebaut, **nicht live getestet** → Rückmeldungen sammeln.
2. **B6:** Dach-/Schalen-Favoriten der Bürovorlage → APIERR_BADINDEX in allen Varianten (auch mit Tapir 1.7.2); Decke/Wand ok, im Projekt erzeugter Dach-Favorit ok → Feld im gespeicherten Favoriten feldweise eingrenzen. Umweg dokumentiert (CreateRoofs `compositeName`).
3. **Lizenzhinweis in die Bundles** (Release-Asset + Über-Dialog) – MIT verlangt ihn bei Weitergabe.
4. **Recherche umsetzen** (`research/archicad-addons-landschaft_2026-10-20.md`, 108 Funde): Schutzschicht in elmonkey-mcp (Probelauf, readonly, Risikoliste), lange Befehle als Jobs mit Wartefrist, Instanz-Erkennung über Projekt statt Port, Modellprüfung, GDL-Werkzeuge, CaptureView wartet auf 3D. Weisung: nachbauen, außer Lizenz erlaubt Übernahme (MIT ok mit Hinweis, GPL/ohne Lizenz nur nachbauen).
5. Backlog: lotrechter Sparren-Firstschnitt (`APIMemoMask_AssemblySegmentCut`), CaptureView-RGB-Farben, Lizenz-/Kompatibilitätsvermerk zu unseren elmonkey-mcp-Werkzeugnamen (gleich wie SzamosiMate, eigener Code).

## Nachricht an die Kollegen

```text
ELMonkey 0.10.0 ist da (Archicad 27/28/29/30, Mac + Windows) – jetzt mit Tapir 1.7.2.

Update: gebt eurem Claude diesen Satz:
„Aktualisiere meinen archicad-Skill und ELMonkey nach https://github.com/mozzi86/-archicad/blob/main/recipes/initial-setup.md“

Danke an Maxl, Roman, Sophia und Michi fürs Mithelfen!

Details: https://github.com/mozzi86/-archicad
```

## Arbeitsweise / Fallen dieser Sitzung

- **Mehrere Archicad-Instanzen** (21.10.: 19723 leeres Testprojekt, 19724 **THN-Teamwork „Gesamtplanung TH Nürnberg“ – nie anfassen**, 19725 training session). Immer erst Port → Projekt per `GetProjectInfo` prüfen; Testskripte brechen bei Teamwork ab, „leer“ nur in unbenannten Projekten.
- Leere Archicad-Instanzen nach dem Schließen: nur wenn `API.IsAlive` „no open project“ meldet, regulär per `NSRunningApplication.terminate` beenden (kein kill). Offener „Änderungen sichern“-Dialog = Projekt noch offen.
- Bundle-Tausch nur bei geschlossenem Archicad; Version im Binary per `strings … | grep -x <Version>` prüfen.
- Unteragenten mit `curl -o <relativer Pfad>` schreiben ins Arbeitsverzeichnis (= Skill-Repo): So entstand `rel021.md` (Versionshinweise Boti-Ormandi/archicad-mcp v0.2.1, Recherche-Unteragent 20.10. 18:27, nie eingecheckt), am 21.10. gelöscht. In Aufträgen an Unteragenten immer absolute Zielpfade vorgeben. Der zweite unversionierte Ordner `.planning/quick/261013-mb5-…` (13.10.) ist nicht aus dieser Sitzung, Herkunft ungeprüft, nicht angefasst.

## Werkzeuge

`~/.scratch/elmonkey/tools/`: `ac.py` (Mini-Client), `syn1.sh <27|28|29> <datei>` (`STRICT=1` = macOS-CI-Schalter; DevKits 27.3001/28.3001/29.3100 unter `~/SchwarzPlatform/devkit/`), `conflict.py` (Merge-Konflikte), `watch_ci_install.sh <sha> <version>` (wartet auf CI, baut AC29-Mac-Bundle ein), `gegenprobe_0934.py <port> datei|leer [version]`, `gegenprobe_0100.py <port> datei|leer`. CI ohne Token: `curl -s https://api.github.com/repos/mozzi86/-archicad/actions/runs?per_page=5`; Build-Logs fehlgeschlagener Läufe auf Zweig `ci-status` unter `logs/`. Python-urllib gegen api.github.com scheiterte zeitweise an SSL → curl nehmen. Release wird nur von `main` aktualisiert; Doku-only-Pushes (`*.md`, außerhalb `ELM_SAB_Tapir/`) lösen keinen Build aus.
