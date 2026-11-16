# Handoff — Diff-Checker Archicad ↔ Plattform (Stand 2026-11-16)

Zweig `elm-diff-plattform` (Worktree `~/.scratch/elmonkey/diff-plattform-wt`), Quick-Task `.planning/quick/261116-dpc-*`.
Doku: [`skills/archicad/reference/diff-plattform.md`](../../skills/archicad/reference/diff-plattform.md).

## Was steht

| Teil | Stand |
|---|---|
| `ELM_SAB.GetDiffDataELM` (Add-on 0.11.13) | gebaut, CI 8/8 grün (AC27–30 × Mac/Win), **nicht live** |
| `elmonkey.diff_plattform` (Kern 0.4.0) + `elmonkey.abgleich` (Paket 10.11.) | 66 + 18 Tests grün, synthetische Daten |
| Skript `plattform_abgleich.py` (+ Befehl `diff` als Hülle um den Kern) | 10 Tests grün (nachgestelltes Archicad) |
| MCP `elmonkey_diff_plattform` (0.3.0) | 14 Tests grün (Archicad-Attrappe) |
| Plattform-Monkey | Gegenstück seit Plattform-PR #338 gemergt (Box im Stand, Geometrie-Vorschläge); hier nicht angefasst |
| Zweig `elm-0.11.13-diff-checker` | zusammengeführt (siehe unten), wird **nicht** gemergt, bleibt stehen |

## Sofort als Nächstes (gemeinsam mit dem Nutzer)

1. Bundle aus `ci-status` bzw. dem Lauf des Zweigs in ein **Test**-Archicad (nicht in ein Teamwork-Projekt, an dem
   gearbeitet wird); `ELM_SAB.GetAddOnVersion` = 0.11.13 prüfen.
2. Projektkopie: `ELM_SAB.GetDiffDataELM {limit: 50}` lesen — Felder, `context.no3DModelCodes`, Dauer je Seite.
3. Box-Prüfung: für ~10 Wände (L-/T-Anschlüsse, Mehrschicht) und ~5 Decken die Box aus dem Befehl gegen die Box aus
   einem IFC-Export derselben Elemente legen. Weicht sie ab: in `diff-plattform.md` Abschnitt 4 festhalten, Weg anpassen.
4. `archicad_abgleich_lesen` → `elmonkey_diff_plattform {stand: <stand_link>}` → Zahlen mit dem Komplett-IFC-Weg
   vergleichen (Fallen-Tabelle Zeile für Zeile).
5. Danach PR mergen, Release, `uv tool install` des MCP, Spiegel.

## Plattform (Stand PR #338, 16.11.2026)

`stand.json` trägt je Element `bbox` (Archicad-Meter, mm), ersatzweise `bbox_fehlt` mit Grund, `teil_von` für Teile von
Plattform-Gruppen und den Kopf `geometrie` (`hoeheNN`, `nullniveauNN`, Toleranz); IFC-Schichten sind schon beim Import
in ihr Bauteil eingerechnet. `basis.json` führt `geometrie`; `archicad_abgleich_melden` macht aus `diff.geometrie`
Vorschläge und meldet Nicht-Übertragbares unter `nicht_automatisch`. Der Kern nimmt die Box des Stands direkt.
Nachzuziehen im Plattform-Monkey nur noch: Normalisierung `$`/`*`, `.TRUE.`/`.FALSE.`, `['A','B']` als Liste.

## Zusammengeführt aus `elm-0.11.13-diff-checker` (Commit `3d2eed0`)

Übernommen: IFC-Kennung extern vor eigen (C++ gibt beide aus, Kern paart so, Skript liest `elementIFCIds` statt
`ifcIds`), Hinweis „3D-Fenster nie offen“, `$`/`*`/`.TRUE.`/Listen mit Komma, Liste ausgeblendeter Paare, Hinweis bei
doppeltem Stempel, Skript-Befehl `diff` als Hülle. Nicht übernommen: eigener Befehl `GetSolidBounds` und eigener Kern
`elmonkey.diff` (doppelt zu `GetDiffDataELM`/`diff_plattform`), Rückfall auf `Get3DBoundingBoxes` im Skript (zweiter
Lesepfad mit roher Box), `plattform.geometrie` im Ergebnis (die Plattform liest `diff.geometrie`). Der Zweig bleibt
unverändert stehen und wird nicht gemergt.
