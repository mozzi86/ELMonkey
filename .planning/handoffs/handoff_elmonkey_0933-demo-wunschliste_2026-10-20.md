# Handoff — ELMonkey 0.9.31–0.9.33, Wunschliste Michi, Demo (Stand 2026-10-20 abends)

Repo `~/.claude/repos/archicad-skill` (= `~/.claude/skills/archicad`), Änderungen über /gsd-quick (Quick-Ordner `.planning/quick/261020-*`).
Prüfwerkzeuge: `~/.scratch/elmonkey/tools/elm_syntax_check.sh <Sources> <datei>` (bei Fehlern in MigrationHelper: Datei mit Vorab-Include `APIEnvir.h`+`ACAPinc.h` prüfen), `elm_schema_scan.py <Sources>`, `ac.py` (Mini-Client).
CI-Status ohne Token: `curl -s https://api.github.com/repos/mozzi86/-archicad/actions/runs?per_page=4`; Bundles: `git fetch origin ci-status && git show FETCH_HEAD:bundles/ELM_SAB_Tapir_AC29_Mac.zip`. Release wird NUR von main aktualisiert.

## Versionsstand

| Version | Stand | Inhalt |
|---|---|---|
| 0.9.30 | Release bis 20.10. | Installer, Update-Knopf, ListCommands, elmonkey-mcp |
| 0.9.31 | in 0.9.32 aufgegangen | Decken-Absturz Maxl (2 Ursachen: Null-Handles; **sideMaterials aus Favoriten nie vergrößern**), GDL Tür/Fenster (Romans Bundle zerlegt + nachgebaut) |
| **0.9.32** | **main + Release** (`1691abb`), live abgenommen | Wunschliste A1–A6: Vorprüfung `[CODE]` (Move/Rotate/SetDetails/Delete/alle Create), `layerName` für alle Tapir-Create, `GetLayers`, `SetLayerAttributes`, `GetSlabLevels`, `zReference` |
| **0.9.33** | Branch `elm-0.9.33-luecken`, **bei Mudi eingebaut (AC29-Mac aus Lauf 1ae60d2)**, CI-Fix `02c61ff` läuft (AC27/28 `IO::fileSystem.Contains` 2-Parameter) | B2 `ChangePropertyDefinitions`, B3 `SetStructureOfElements`, B6 Dach/Schale-Favoriten ohne Memo, B7 `SaveProjectCopy`, B8 Auswahl als Sammelaufruf, **Beam `isSlanted`-Fix** (Sparren!). **Noch nicht live getestet.** Sicherung 0.9.32: `~/.scratch/elm-bundle-0.9.33/backup_0.9.32/` |

## Sofort als Nächstes

1. **Demo fertigstellen**: Mudi öffnet `~/Desktop/training session.pln` (gesicherte Demo). Dann `python3 ~/.scratch/elmonkey/demo/demo_sparren.py <port>` — testet erst einen Sparren (bricht bei falscher Lage ab), dann 30 Sparren 8/20, Dachhaut 10 cm neu, Giebel-SEO neu, Holz per SetStructureOfElements (= B3-Live-Test), Sparren tragend. Danach 3D-Bild (`ChangeWindow 3DModel`, `GetCurrentWindowType` prüfen, `FitInWindow`, `CaptureView`).
2. **0.9.33 live testen** (leeres Projekt): B6 Dach-Favorit „Unterdach+Dämmung 21,5cm“ (vorher APIERR_BADINDEX nachgestellt), B2 Enum-Wert anhängen + Verfügbarkeit, B3 Decke/Dach/Stütze, B7 SaveProjectCopy (lokale .pln; Teamwork/unbenannt müssen ablehnen), B8 >500 Elemente auswählen.
3. **B4/B5 fehlen noch** (geplant 0.9.34): `ELM_SAB.Modify2DElements` in `Create2DElementsCommand.cpp` (Helfer C2D* wiederverwenden): Schraffur fill/Stifte/RGB/Winkel (`hatchOrientation`, API_HatchRotated)/Polygon; Linie/Polylinie/Bogen Linientyp, Stift, Geometrie, **Pfeilspitzen aus** (Demo-Weg/Spline haben Standard-Pfeile).
4. **Weitere Fixes aus der Demo** (Backlog): CaptureView zeigt RGB-Schraffuren schwarz; `floorIndex`/`zReference` auch für CreateSlabs/Beams/Columns (Heimatgeschoss wird aus absoluter Höhe gewählt → Außenanlage landet im UG); `layerName` im CreateBeams-Schema (liegt nicht im ElementCreationCommands-Muster).
5. Nach Tests: Doku 0.9.33 in `reference/mcp-extension.md`, Branch → main (Release + Update-Knopf).

## Wunschliste Michi (`~/Downloads/ELM_SAB_Wunschliste.md`)

A1–A6 erledigt (0.9.32). B2/B3/B6/B7/B8 gebaut (0.9.33, ungetestet). B1 Lampen: `TapirCommand.CreateLamps` existiert — Test braucht Projekt mit Leuchten-Bibliothek. B4/B5 offen. C1–C4 Forschung, C5 durch `elmonkey-mcp` gelöst. Michi hat noch 0.9.18 → einmal Installer.

## Kollegen / Rückmeldungen

- Maxl: Absturz behoben (0.9.32). Nutzt noch `tapir-archicad-mcp` → auf `elmonkey-mcp` umstellen.
- Roman: eigenes Bundle „0.9.31“ (ZZ_Austausch/RK) nicht verwenden; Quellcode war im Scratchpad. Update auf Release.
- S.: Ansichtsbemaßung in 0.9.29; eigenes arm64-Bundle durch Release ersetzen.
- `ZZ_Austausch/ELM/mozzi-backend.env` liegt offen im Büro-OneDrive — Mudi informiert, ungeprüft.
- MCP-Registrierung bei Mudi steht noch auf `tapir-archicad-mcp`; `elmonkey-mcp` ist installiert (`~/.local/bin/elmonkey-mcp`), Umstellung braucht Mudis Okay.

## Lessons learned (auch im Skill: `recipes/gebaeude-und-aussenanlage.md`, Memory `feedback_demo_fachlich_3d`)

Fachlich planen wie ein Architekt, alles in 3D/BIM. Stille No-Ops gibt es weiter in Tapir (slantAngle) → jede Schreibung zurücklesen. Testelement vor Serie. Vor Bundle-Tausch unbenannte Projekte sichern lassen. Nicht auf Kontrollbilder hin „reparieren“ (CaptureView-Farben).
