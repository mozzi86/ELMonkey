---
quick_id: 261013-kl1
type: quick
status: complete
plan: 261013-kl1
branch: main
base: a79a36e743ae9854a579ac5de303a1122859f520
commits:
  - 50c3d5d  # fix(ELM_SAB): 0.9.25 CaptureView – Echtfarbtiefe statt FromSourceImage (APIERR_BADID)
  - 86309a4  # docs(ELM_SAB): 0.9.25 CaptureView Farbtiefe – Ursache, Fix, Testplan
tasks_done: [1, 2, 3]
tasks_open: []
gate: "Push vom Nutzer ausdruecklich freigegeben (Vorbedingungen A und B erfuellt)"
pushed: true
push_range: "a79a36e..86309a4"
commit: 86309a4
ci_sha: 86309a4
ci_run: 38402663622
ci_status_commit: "Build-Status 86309a4 (Lauf 38402663622)"
fix_rounds: 0
duration: 20min
completed: 2026-10-13
---

# Quick 261013-kl1: ELM_SAB 0.9.25 — CaptureView: APIERR_BADID beim PNG-Export (Farbtiefe)

**Ergebnis in einem Satz:** `CaptureView` speichert das PNG jetzt mit der Echtfarbtiefe der Plattform (Mac `APIColorDepth_MiC`,
Windows `APIColorDepth_TC24`, sonst `256C`) statt mit `APIColorDepth_FromSourceImage` (laut DevKit nur für die Bildkonvertierung
definiert, wahrscheinlichste Ursache des live gemessenen -2130313115), versucht bei APIERR_BADID genau einmal `256C` und meldet die
benutzte Tiefe in `colorDepth`. Gepusht (`a79a36e..86309a4`), CI 6/6 grün beim ersten Push, keine Fix-Runde, Inhaltsprobe des
AC29-Mac-Binarys bestanden. **Noch NICHT live verifiziert. Archicad nicht angesprochen, nichts installiert oder getauscht.**

## Commits (Branch `main`, Basis `a79a36e`)

| Task | SHA | Betreff | Dateien |
|---|---|---|---|
| 1 | `50c3d5d` | `fix(ELM_SAB): 0.9.25 CaptureView – Echtfarbtiefe statt FromSourceImage (APIERR_BADID)` | CaptureViewCommand.hpp, AddOnVersion.hpp, AddOnMain.cpp |
| 2 | `86309a4` | `docs(ELM_SAB): 0.9.25 CaptureView Farbtiefe – Ursache, Fix, Testplan` | reference/mcp-extension.md, ELM_SAB_Tapir/README.md |

Beide Messages enden mit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Keine Löschungen. `git config` nicht angefasst
(Autor Mudi, mozzi666@gmail.com). Task 3 hat keinen eigenen Commit (kein Fix nötig).
Nicht committet (wie angewiesen): diese SUMMARY, `261013-kl1-PLAN.md`, `syntax_check_0925.sh` (liegen untracked im Quick-Ordner).

## Task 1 — Prüfergebnisse

| Prüfung | Ergebnis | Beleg |
|---|---|---|
| Ausgangslage | OK | `git status --short -- ELM_SAB_Tapir reference` leer, `git diff --stat a79a36e HEAD -- ELM_SAB_Tapir reference` leer, HEAD = origin/main = `a79a36e` |
| RED: `syntax_check_0925.sh a79a36e` am unveränderten Stand | scheitert wie gefordert | Exit 1, 14 × `FEHLER` (C: noch 1 × `FromSourceImage`, 0 × `TrySave (`, 0 × `APIERR_BADID`, 0 × `colorDepth`-Antwort, keine Plattformzweige; D: `properties` nur `imagePath, view2D`, `colorDepth` fehlt; E: Diff leer, Version/Registrierung alt). Stufen A und B waren schon OK (17 × OK) |
| GREEN: `syntax_check_0925.sh a79a36e` | **bestanden** | Exit 0, **31 × OK, 0 × FEHLER**, „ERGEBNIS: alle Stufen OK (A Syntax AC29/Mac c++20, B Plattform, C Struktur, D Schema, E Diff)" |
| A Syntax | OK | `clang++ -std=c++20 -fsyntax-only -Dmacintosh=1 … -Werror` auf `AddOnMain.cpp` (bindet `CaptureViewCommand.hpp` ein), DevKit 29.3100, 0 Fehler |
| B Plattform-Kontrolle | OK | `APIColorDepth_MiC` kompiliert unter `-Dmacintosh=1`, `APIColorDepth_TC24` scheitert dort (Beleg für den Pflicht-`#if`), `APIColorDepth_256C` kompiliert; DevKit-Header: `256C=none MiC=macintosh TC24=WINDOWS` |
| C Struktur (Nicht-Kommentarzeilen) | OK | 0 × `APIColorDepth_FromSourceImage`, 0 × `keepSelectionHighlight`, 1 × `ACAPI_ProjectOperation_Save`, 3 × `TrySave (` (Definition + 2 Aufrufe), 2 × `APIERR_BADID`, 1 × `response.Add ("colorDepth"`, 1 × `pict.colorDepth = depth`; MiC nur im `macintosh`-, TC24 nur im `WINDOWS`-Zweig, 256C im `#else` |
| D Schema | OK | Raw-String endet sauber vor `));`, `properties` = imagePath, view2D, colorDepth, `colorDepth.type` = string, `required` = [imagePath, view2D], `additionalProperties` = false, Schema ASCII, per `ELMResponseSchemaWithError`-Text zusammengesetztes `oneOf` ist gültiges JSON mit 2 Zweigen, Eingabeschema **byte-gleich** zur Basis (467 Bytes) |
| E Diff gegen Basis | OK | genau 3 Dateien; keine C++20-Konstrukte; `!=` nur gegen `APIColorDepth_*`/`NoError`/`APIERR_*`; AddOnMain.cpp und AddOnVersion.hpp je „1 1"; `ELM_SAB_VERSION "0.9.25"`, `ADDON_VERSION "1.5.4"`; Registrierung bleibt `"0.9.12"`; neue Registrierungszeile = alte Zeile + 0.9.25-Satz vor dem letzten `"` |
| Plan-Verify Task 1 (komplette Befehlskette) | OK | `TASK1_VERIFY_OK`, 3 Dateien im Commit, Co-Authored-By vorhanden, `origin/main..HEAD` für `ELM_SAB_Tapir reference` = 1, Arbeitsbaum dort sauber |

Selbst gelesen (`git diff a79a36e -- CaptureViewCommand.hpp`): Eingabeschema unberührt, Layout-Hinweis vorhanden, Rücklese vorhanden (mit Tiefe), genau 2 `TrySave`-Aufrufe, der zweite nur im Zweig `err == APIERR_BADID && primaryDepth != APIColorDepth_256C`.

## Task 2 — Prüfergebnisse

`TASK2_VERIFY_OK` (volle Plan-Befehlskette): Überschrift `## ELM_SAB 0.9.25 — CaptureView: Farbtiefe statt APIERR_BADID <!-- 2026-10-13 -->` genau 1 ×;
alle 24 geforderten Begriffe im Abschnitt; Z. 1580 (0.9.23 Offener Punkt) unverändert als Präfix, am Ende der Verweis
„Ursache gefunden, Fix in 0.9.25 (nicht live verifiziert)"; Korrekturvermerk im 0.9.12/0.9.13-Abschnitt; im `mcp-extension.md`-Diff
genau 1 entfernte Zeile (die verlängerte Z. 1580), README 0 entfernte Zeilen; 2 Dateien im Commit; `origin/main..HEAD` = 2.

Zusätzlich (Orchestrator-Vorgaben, im Abschnitt umgesetzt):
* Tabelle „Auswertung des Live-Tests", Zeile „MiC … -2130313115, Rueckfall 256C … -2130313115": zwei nächste Kandidaten, je in einer EIGENEN späteren Version: (a) Dateityp TIFF wie `Do_Save_TiffFile`, (b) API-Datenbank ≠ vorderes Fenster (`ACAPI_Database_GetCurrentDatabase` gegen `ACAPI_Window_GetCurrentWindow`, Kandidat 1 aus `~/.scratch/elmonkey/handoff_elmonkey_captureview-badid_2026-10-13.md`).
* „### Stand" und Testplan Schritt 8: Das Bundle enthält gegenüber dem eingebauten 0.9.23 (CI `4e804b3`) auch die ungetesteten 0.9.24-Änderungen. Commit-Differenz benannt: `7c07600` (0.9.24-Schemafix, sechs Befehle) + `50c3d5d` (0.9.25-CaptureView-Fix); beide Blöcke getrennt testen, der 0.9.24-Testplan steht im Abschnitt „ELM_SAB 0.9.24".

## Task 3 — Vorbedingungen, Push, CI, Inhaltsprobe

**Vorbedingung A (gesicherte 0.9.24-Bundles, nur `test -s` / `ls`):** alle 6 Dateien in `~/.scratch/elm-bundle-0.9.24/ci_247aa63/` vorhanden und nicht leer
(AC27 Mac 2.004.869 B, AC27 Win 3.369.472 B, AC28 Mac 2.016.803 B, AC28 Win 3.365.888 B, AC29 Mac 2.132.307 B, AC29 Win 3.718.144 B). Ordner nach dem Push unverändert (9 Einträge).

**Vorbedingung B:** `git fetch -q origin main` → `origin/main` = `a79a36e743ae9854a579ac5de303a1122859f520`; `git rev-list --count origin/main..HEAD -- ELM_SAB_Tapir reference` = 2; `syntax_check_0925.sh` Exit 0.

**Push:** `git push origin main` → `a79a36e..86309a4  main -> main` (2 Commits). Gepushter SHA7 `86309a4`.

**CI (über Zweig `ci-status`, Polling als Hintergrund-Bash, ~4,5 min, Treffer in Runde 3):**

* `status.md` erste Zeile: „Gebaut aus `86309a4` — Basis Tapir 1.5.4 plus die SAB-eigenen Befehle."
* ci-status-Commit-Betreff: „Build-Status 86309a4 (Lauf 38402663622)".
* 6 Bundle-Namen in `status.md`, 6 Dateien unter `bundles/` (AC27/28/29 × Mac/Win), `**fehlgeschlagen**` = 0, 12 Logs unter `logs/`, Mac-AC29-Log 379.461 B, in keinem der 6 Build-Logs eine Zeile mit `CaptureViewCommand` + warning/error.
* **Fix-Runden: 0** (grün beim ersten Push). Die Plan-Verify-Kette von Task 3 lief komplett durch (`TASK3_VERIFY_OK`).

**Inhaltsprobe AC29-Mac-Binary** (neues leeres Verzeichnis im Session-Scratchpad, nur `strings`, nichts ausgeführt, kein `xattr`, Verzeichnis danach gelöscht; Pfad im Zip `ELM_SAB_AC29_Mac.bundle/Contents/MacOS/ELM_SAB_AC29_Mac`, 7.283.296 B):

| Suche | Treffer |
|---|---|
| `grep -cx '0.9.25'` | 2 (Soll ≥ 1) |
| `grep -cx '0.9.24'` | 0 (Soll 0) |
| `grep -cF '0.9.25: PNG-Export mit Echtfarbtiefe'` | 2 (Soll ≥ 1) |
| `grep -cF 'Tatsaechlich benutzte Farbtiefe'` (colorDepth-Schema) | 2 (Soll ≥ 1) |
| `grep -cF 'Rueckfall 256C: Fehler'` (Versuchsprotokoll) | 2 |

Globale Kontrollen: `git diff --stat a79a36e origin/main -- ELM_SAB_Tapir reference` zeigt genau die 5 Dateien (CaptureViewCommand.hpp, AddOnVersion.hpp, AddOnMain.cpp, mcp-extension.md, README.md; 129 + / 20 −);
Diff-Grep auf `ACAPI_Element_(Change|Create|Delete)`, `ACAPI_CallUndoableCommand`, `ACAPI_Teamwork_` in neuen Zeilen: keine Treffer. `~/.scratch/elm-bundle-*` unverändert, fremder Worktree `archicad-skill-stufe5` und Zweig `stufe5-openings` nicht berührt.

## Abweichungen vom Plan

1. **[Plan-Inkonsistenz, Prüfskript] ASCII-Prüfung der Registrierungszeile.** Der Plan verlangt „Alle neuen Zeilen sind ASCII" und zugleich „neue AddOnMain-Zeile = alte Zeile + Satz". Die alte CaptureView-Beschreibungszeile enthält aber schon einen Gedankenstrich („PNG — die visuelle Ruecklese"), also ist die geänderte Zeile zwangsläufig nicht rein ASCII. `syntax_check_0925.sh` Stufe E nimmt diese eine Zeile von der Nicht-ASCII-Prüfung aus und prüft den angehängten Satz separat auf ASCII (`SENT.isascii()`). Der Code ist so wie geplant (alt + Satz), nur das Skript wurde angepasst.
2. **[Plan-Zeilennummer]** Der letzte Satz des 0.9.12/0.9.13-CaptureView-Bullets steht in `mcp-extension.md` auf Z. 702 (Plan: 701). Der Korrekturvermerk wurde nach der tatsächlich letzten Zeile des Bullets eingefügt (Verankerung per Text, nicht per Nummer).
3. **[Kleine Doku-Ergänzung]** In der „Restunsicherheit" steht zusätzlich der geprüfte Befund, dass `APIFType_PNGFile` im DevKit nur in der Enum vorkommt (`APIdefs_Automate.h` Z. 131) und kein Beispiel PNG nutzt (per grep über `Support` und `Examples` belegt). Stützt den TIFF-Kandidaten, ändert nichts am Code.
4. **[Wartevorgang, ohne Repo-Auswirkung]** Das CI-Polling lief wie vorgeschrieben als `run_in_background`-Bash. Zusätzlich habe ich auf dessen Ausgabedatei mit einem Vordergrund-Befehl `tail -f | grep -m1` gewartet (kein `sleep`); BSD-`tail` puffert, der Befehl lief ins 595-s-Timeout und wurde automatisch in den Hintergrund verlegt. Der dabei zurückgebliebene eigene `tail`-Prozess (PID 12876) wurde beendet. Kein Einfluss auf Repo oder CI.

Keine Auth-Gates, keine Stubs, keine neuen Sicherheitsflächen (kein neuer Endpoint, keine Element-/Undo-/Teamwork-Aufrufe).

## Hinweis an den Nutzer

* Das Bundle `86309a4` liegt auf `ci-status` (`bundles/ELM_SAB_Tapir_AC29_Mac.zip` für den Mac). **Nichts eingebaut, nichts live getestet.**
* Einbau und Live-Test nach den Bundle-Regeln, Testplan im Abschnitt „ELM_SAB 0.9.25" in `reference/mcp-extension.md`: eingebautes 0.9.23 sichern, Archicad komplett beenden (nie kill), nur EINE Instanz, `ELM_SAB.GetAddOnVersion` = 0.9.25. **Commit-Differenz zum eingebauten 0.9.23 (CI `4e804b3`) benennen:** `7c07600` (0.9.24-Schemafix für sechs Befehle) + `50c3d5d` (0.9.25-CaptureView-Fix); beide getrennt testen (0.9.24: Testplan im Abschnitt „ELM_SAB 0.9.24", Schritte 2–4).
* Je CaptureView-Probe ein NEUER Dateiname (die Rücklese prüft nur die Existenz). Auswertung nach der Tabelle im Abschnitt. Bleibt BADID bei beiden Tiefen: nächste Kandidaten TIFF bzw. Datenbank ≠ Fenster, je in einer eigenen Version.
* Zum Fortschreiben im Orchestrator: STATE.md-Zeile für 261013-kl1 (CI grün `86309a4`, Lauf 38402663622, Live-Test offen).

## Self-Check: PASSED

* Dateien vorhanden: `CaptureViewCommand.hpp`, `AddOnVersion.hpp`, `AddOnMain.cpp`, `reference/mcp-extension.md`, `ELM_SAB_Tapir/README.md`, `syntax_check_0925.sh`.
* Commits vorhanden: `50c3d5d`, `86309a4` (`git log`), gepusht (`origin/main` = `86309a4`).
* CI-SHA `86309a4` auf `ci-status`, 6 Bundles, 0 × `**fehlgeschlagen**`.
