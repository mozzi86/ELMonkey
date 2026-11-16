# Handoff — Diff-Checker Archicad ↔ Plattform (Stand 2026-11-16)

Zweig `elm-diff-plattform` (Worktree `~/.scratch/elmonkey/diff-plattform-wt`), Quick-Task `.planning/quick/261116-dpc-*`.
Doku: [`skills/archicad/reference/diff-plattform.md`](../../skills/archicad/reference/diff-plattform.md).

## Was steht

| Teil | Stand |
|---|---|
| `ELM_SAB.GetDiffDataELM` (Add-on 0.11.13) | gebaut, CI 8/8 grün (AC27–30 × Mac/Win), **nicht live** |
| `elmonkey.diff_plattform` (Kern 0.4.0) + `elmonkey.abgleich` (Paket 10.11.) | 53 + 18 Tests grün, synthetische Daten |
| MCP `elmonkey_diff_plattform` (0.3.0) | 14 Tests grün (Archicad-Attrappe) |
| Plattform-Monkey | nicht angefasst (anderes Repo) |

## Sofort als Nächstes (gemeinsam mit dem Nutzer)

1. Bundle aus `ci-status` bzw. dem Lauf des Zweigs in ein **Test**-Archicad (nicht in ein Teamwork-Projekt, an dem
   gearbeitet wird); `ELM_SAB.GetAddOnVersion` = 0.11.13 prüfen.
2. Projektkopie: `ELM_SAB.GetDiffDataELM {limit: 50}` lesen — Felder, `context.no3DModelCodes`, Dauer je Seite.
3. Box-Prüfung: für ~10 Wände (L-/T-Anschlüsse, Mehrschicht) und ~5 Decken die Box aus dem Befehl gegen die Box aus
   einem IFC-Export derselben Elemente legen. Weicht sie ab: in `diff-plattform.md` Abschnitt 4 festhalten, Weg anpassen.
4. `archicad_abgleich_lesen` → `elmonkey_diff_plattform {stand: <stand_link>}` → Zahlen mit dem Komplett-IFC-Weg
   vergleichen (Fallen-Tabelle Zeile für Zeile).
5. Danach PR mergen, Release, `uv tool install` des MCP, Spiegel.

## Plattform-Monkey nachziehen

`bauteil()` mit `bbox` (Archicad-Koordinaten) und `teil_von` für `IfcBuildingElementPart`; `basis_neu` behält
`basis.geometrie`; `archicad_abgleich_melden` übersetzt `diff.geometrie` in Geometrie-Ops (oder ein eigenes Werkzeug);
Kern 0.4.0 (`abgleich`, `diff_plattform`) in den Plattform-Monkey übernehmen.

## Achtung

Im Haupt-Arbeitsbaum `~/.claude/repos/ELMonkey` lag beim Start ein fremder, ungespeicherter Stand auf Zweig
`elm-0.11.13-diff-checker` (dasselbe 10.11.-Paket, ab 16.11. 12:10). Nicht angefasst — vor dem Merge klären, damit
nicht zwei 0.11.13 entstehen.
