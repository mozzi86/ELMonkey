---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: executing
last_updated: "2026-09-03T00:00:00.000Z"
last_activity: 2026-10-13 — Quick: Doku ELM_SAB 0.9.22–0.9.25 live verifiziert (261013-lmd)
progress:
  total_phases: 8
  completed_phases: 6
  total_plans: 0
  completed_plans: 0
  percent: 75
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-19)

**Core value:** Claude kann jederzeit in einem laufenden Archicad-Projekt arbeiten — neue Elemente erstellen, Bestand abfragen, Bulk-Operationen ausführen — ohne dass Tool-Namen oder Parameter zur Laufzeit erfunden werden müssen.
**Current focus:** Phase 1 (Skill-Gerüst + Safety) — bereit zur Planung

## Current Position

Phase: Phase 6 Live-Promotion complete (alle 50 VERIFY-Marker in 3 Recipes ersetzt, 7 neue MCP-v29-Findings dokumentiert, cross-recipe Schema-Bug in 5 weiteren Recipes gefixt). Ready for Phase 7 Integrationstests oder Phase 8 Gap-Close.
Plan: User-Wahl im nächsten Session-Auftakt.
Status: Skill v1.0-rc1. Phasen 1+2+3+4+5+6 done. Phase 7+8 pending. Out-of-Scope-Extensions: dwg-ifc-import.md (Lageplan live), dwg-ifc-kg300.md (KG 300 pattern) + Annotation-Parsing-Backlog dokumentiert.

Progress: [████████████░░░] 75% (6 von 8 Phasen + 2 Out-of-Scope-Extensions)
Last activity: 2026-05-21 — Phase 6 Live-Promotion: 50 VERIFY-Marker durch Live-Verification ersetzt (lines-polylines 12, surfaces-materials 21, fills-hatches 17). 7 MCP-v29-Findings in Memory: Typo `get2_d` ohne Unterstrich; `set_details.typeSpecificDetails=WallSettings-only`; 3 Create-Endpoints (surfaces, composites, building_materials) existieren mit korrigierten Param-Namen; `set_classifications` korrektes Schema = `elementClassifications+classificationId`-Single (NICHT elementsWithClassifications+classifications-Array, alte Form ergibt Pydantic-Error); pagination `page_token` ist Top-Level; `attributes_get_composite_attributes`+`_surface_attributes` existieren; `move_attributes_and_folders` für Folder-Workflow. Cross-recipe Schema-Bug in 5 weiteren Recipes (wall-operations, slabs-columns-beams, zones, library-objects, curtain-walls) gefixt — Recipes waren wegen falscher Code-Snippets seit Phase 3-5 nicht live anwendbar.

Commits dieser Session: 9 atomic (DWG-IFC Lageplan + KG-300 + 3× Phase-6-Live-Promotion + Schema-Cross-Fix).

## Quick Tasks Completed

| Datum | Slug | Ergebnis |
|-------|------|----------|
| 2026-09-03 | datumskorrektur | Datumsmarker der drei Tageseinträge 01.09. → 03.09. (Sitzungstag), keine Inhaltsänderung |
| 2026-09-03 | teamwork-6001-diagnose | 6001 = zuerst Ebene ausgeblendet/Hotlink prüfen (Rezept ein-/ausblenden); 4001 nennt Dialog; API-Selektion ortsgebunden (3 Dateien) |
| 2026-09-07 | 260907-k1t | Rezept „Gelände aus Punktwolke" (E57 → Bodenraster → Tapir CreateMeshes) + drei parametrisierte Skripte in SKILL.md verlinkt |
| 2026-09-03 | element-preview-render | Tapir `GetElementPreviewImage` als Bildquelle für Objekte ohne Export-Vorschau; Renderer-Hänger bei Legacy-Meshes + Port→PID-Diagnosefalle (3 Dateien, 30 Zeilen) |
| 2026-09-03 | schedule-export-lernpunkte | GDL-Preise vs. leere Property + eingebettete Summenzeilen in Schedule-Exports dokumentiert (3 Dateien, 32 Zeilen) |
| 2026-09-06 | 260905-ro7 | ELM_SAB.SetStoryVisibilityOfElements/GetStoryVisibilityOfElements (Auf Geschossen zeigen, Object+Lamp) neu registriert v0.9.15; CI-Build ausstehend |
| 2026-09-06 | 260906-hs0 | layerIndex-Gotcha (Index ≠ Listenposition, `API.GetAttributesIndices`); Geschosslogik+Beschriftungsgrammatik der Durchbruch-Symbole (Verweis statt Duplikat); 3 MCP-Betriebs-Fallen (600-s-Zeitgrenze, Aktionsbefehle nie leer testen, Archicad nie per kill beenden) — 3 Dateien
| 2026-09-06 | 260906-jw7 | Rotation-Zeile in library-objects.md korrigiert (RotateElements statt SetGDLParametersOfElements); Host-Deckung-Punkt-3 präzisiert + Mittig-Kriterium/Reihenfolge Drehen→Tiefe→Zentrieren/Schlitze/Wandkreuzungen ergänzt; Bauteilname-Fallback ohne Zonen in zones.md; 3 Betriebs-Fallen an mcp-conventions.md angehängt — 4 Dateien
| 2026-09-07 | 260907-lme | THN-SuD-Kampagne-Lehren: Nachlese-Regeln HL-/Trassenpläne (11 Punkte, u.a. „/"-Falle verlor 40% der Funde, Rohrhülsen, Steigschacht-Abgrenzung) in oeffnungen-aus-konturen.md; Teamwork-Konflikt-trotz-success + Werkzeug-Hygiene (9 Punkte) in mcp-conventions.md — 2 Dateien |
| 2026-09-09 | 260909-0tg | GDL-Steuerparameter der SAB-Durchbruch-Symbole (`gs_hole_width`/`gs_hole_depth`/`gs_slab_thickness`/`gs_hole_diameter`; A/B/ZZYZX sind abgeleitet, THN 2110 betroffen/1856 repariert) + Nachtrag sitzungsweites leeres `executionResults` (Neustart-Abhilfe, Verweis auf 0.9.16-Diagnose) — 3 Dateien |
| 2026-10-06 | 261006-io0 | ELM_SAB 0.9.19: `CreateLabels` (floorInd respektiert, beg/mid/end, templateLabelId), `GetLabelsOfElements` (ein Durchlauf), `SetLayerOfElements`, `Get2DGeometryOfElements` mit ShapePrims-Fallback für Tür/Fenster/Objekt/Etikett; CI 6/6 grün (`e1e886b`), Live-Test nach Bundle-Tausch offen |
| 2026-10-06 | 261006-j8h | ELM_SAB 0.9.20: `DeleteElements` reserviert in Teamwork selbst (`reserve`, Default true) und prüft Überlebende je GUID → `success:false` statt stillem No-Op; Doku 0.9.19 live verifiziert (THN), Türaufschlag-Kalibrierung + Pipetten-Falle im Türen-Rezept; CI 6/6 grün (`896b982`) |
| 2026-10-06 | 261006-lod | ELM_SAB 0.9.21: `CreateWallOpenings` meldet `sourceDeleted:true` nur nach Existenz-Rücklese; fremd reservierte Quellen werden nicht gelöscht (eigener Grund), Freigabe nur eigener Überlebender; Doku 0.9.20-Lücke → behoben; CI 6/6 grün (`2ef26c2`) |
| 2026-10-06 | 261006-m19 | Doku: ELM_SAB 0.9.20 + 0.9.21 live verifiziert (THN): DeleteElements Standard/`reserve:false`, CreateWallOpenings `sourceDeleted` mit eigener Testwand; offene Testschritte markiert; Lehren KI-Stempel nur klassifiziert + Teamwork-Löschungen senden — 1 Datei |
| 2026-10-12 | 261012-on6 | ELM_SAB 0.9.22: `GetDetailsOfElements` liefert Wand-`buildingMaterialId` (Basic) / `profileId` (Profile), Composite wie bisher; noch nicht live verifiziert; CI 6/6 grün (`b1651e8`) |
| 2026-10-13 | 261013-fel | ELM_SAB 0.9.23: neuer Lesebefehl `FindElements` (and/or/not beliebig tief über 18 Kriterien, immer Grundriss-Datenbank, `scope` projekt/wie3D/sichtbar/aktuellesGeschoss, `return`, `criteriaStats`, `select` mit Rücklese) + `CaptureView`-Fix (Fehlerantwort schemakonform statt 4009, Fenstertyp + Archicad-Code, Layout versucht/abgelehnt); 6 weitere Befehle mit gleichem Schemafehler nur gelistet; `RunCriteriaSet` per API nicht möglich; noch nicht live verifiziert; CI 6/6 grün (`4e804b3`) |
| 2026-10-13 | 261013-jom | ELM_SAB 0.9.24: Antwortschema von `CreateWallOpenings`, `SetLayerOfElements`, `SetObjectParametersForce`, `SetPenOfElements`, `SetStoryVisibilityOfElements`, `SetTextSizeOfElements` auf `ELMResponseSchemaWithError` (oneOf Erfolg|Fehler, Erfolgsschema byte-gleich), 9 Top-Level-Fehler über `CreateErrorResponseELM` (Code/Text unverändert) → Aufruffehler kommen lesbar statt 4009; Push erst nach Live-Bestätigung des CaptureView-Musters (0.9.23 eingebaut, Gegenprobe bestanden); Scan: 0 Kandidaten; noch nicht live verifiziert; CI 6/6 grün beim ersten Push (`247aa63`, Lauf 38398262095) |
| 2026-10-13 | 261013-kc2 | Doku: ELM_SAB 0.9.23 live verifiziert (WiSchu): Türenabgleich in EINEM FindElements-Aufruf GUID-genau = modell_abgleich.py (267), scope = Tapir mit gleichen Flags, Layout vorn FindElements 267 / Tapir 0, Fehler mit Namen; Falle „Element ID" doppelt → `ID und Kategorien/Element ID`; Abbruch-Türen (Renovierungsfilter) nicht auswählbar; CaptureView-Fehler jetzt lesbar = APIERR_BADID (Grundriss + Layout, nie funktionsfähig) — 1 Datei (`d84ca99`) |
| 2026-10-13 | 261013-kl1 | ELM_SAB 0.9.25: `CaptureView` APIERR_BADID-Ursache offline geklärt (DevKit 29.3100): `colorDepth = APIColorDepth_FromSourceImage` (-1) ist nur für die Bildkonvertierung definiert (APIdefs_Goodies.h Z. 280), seit 0.9.12 im Code → CaptureView lieferte nie ein Bild. Fix: Echtfarbtiefe je Plattform (Mac `MiC`, Win `TC24`), bei BADID genau ein Rückfall auf `256C` (DevKit-Beispiel Do_Save_TiffFile), Antwort meldet `colorDepth`; Eingabeschema byte-gleich. 0.9.24-Bundles vorab gesichert (`~/.scratch/elm-bundle-0.9.24/ci_247aa63/`); Bundle enthält 0.9.24 mit (Differenz zu eingebautem 0.9.23: `7c07600` + `50c3d5d`). Noch nicht live verifiziert; CI 6/6 grün beim ersten Push (`86309a4`, Lauf 38402663622), Inhaltsprobe AC29-Mac = 0.9.25 |
| 2026-10-13 | 261013-lmd | Doku: ELM_SAB 0.9.22–0.9.25 live verifiziert (WiSchu, Bundle 0.9.25 eingebaut): FindElements alle Kriterien GUID-genau gegen unabhängige Referenzen (nur designOption-Namen am WiSchu nicht prüfbar); 0.9.22 Wand-Baustoff 56/56; 0.9.24 Fehlerpfade lesbar (3 Proben vom Eingabeschema abgefangen, Tabelle korrigiert); 0.9.25 CaptureView liefert PNG (MiC) für Grundriss auch verdeckt, Layout, 3D; Korrektur Geschoss 0 → 1. OG; Falle Klassifizierung code statt name — 1 Datei (`35cd3d5`) |
| 2026-10-13 | 261013-n2y | ELM_SAB 0.9.26: neuer Befehl `Create2DElements` (Line/Arc/Circle/PolyLine/Spline/Hatch, ein Undo-Schritt, Rücklese je Element mit layer/floor/coordinatesMatch, Hatch area+perimeter über GetQuantities) + Spline-Lesezweig in `Get2DGeometryOfElements`; Fallen: Spline-coords 0-basiert, Tapir `GetPolyArcs` verschiebt Bogenindizes. CI 6/6 grün (`3f38981`), eingebaut + Versionsrücklese ok (WiSchu 19723); 0.9.25 gesichert in `~/.scratch/elm-bundle-0.9.26/backup_0.9.25`. Live-Abnahme siehe Folgecommit — 3 Code/Doku-Commits (`e7ec585`, `d5fa36a`, `3f38981`) |

## Performance Metrics

**Velocity:**

- Total plans completed: 0
- Average duration: — min
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| — | — | — | — |
| 2026-10-13 | 261013-q27 | ELM_SAB 0.9.27 nach Live-Abnahme 0.9.26 (WiSchu: 6 Typen ok, Fläche/Umfang exakt, Rundweg punktgenau, Fehler je Element lesbar): Circle-Rücklese akzeptiert Arc+whole; Hatch mit Loch → klare Meldung statt APIERR_IRREGULARPOLY; Get2DGeometryOfElements liefert subPolyEnds. Noch nicht eingebaut |
| 2026-10-13 | 261013-q27 (Live) | ELM_SAB 0.9.27 eingebaut + live verifiziert (WiSchu): Circle success:true, Loch-Schraffur klar abgelehnt, subPolyEnds [22,29]/[25], Außenkontur angelegt (Fläche = Shoelace); Testelemente gelöscht |
| 2026-10-13 | 261013-q28 | ELM_SAB 0.9.28: Create2DElements legt Schraffuren mit Löchern an (subPolyEnds wie Lesebefehl, Prüfung + Rücklese je Teilpolygon, holesArea). Noch nicht eingebaut |
| 2026-10-14 | 261013-q28 (Live) | ELM_SAB 0.9.28 eingebaut + live verifiziert (WiSchu): Loch-Schraffur Rundweg netto 6,2426 / Loch 0,2734 = Shoelace, Quadrat mit Loch 12/4/24, Fehlerfälle lesbar; Testelemente gelöscht |

**Recent Trend:**

- Last 5 plans: —
- Trend: —

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.

Recent decisions affecting current work:

- **Brainstorming → GSD-Workflow:** Hub-and-Spoke + Discovery-verifiziert; asymmetrische Sicherheit; keine Batch-Obergrenze; Self-Improvement-Pattern; B1-volle GSD-Variante mit Research-Subagenten.
- **Research-Synthese (2026-05-19):** Description anti-pattern fixed (Trigger-only); Element-ID-Threading-Regel ergänzt; 3 Pitfall-Safety-Rules (SAFE-02/03/04) hinzugefügt; STORY-01 + ATTR-01 + ZONE-01 als neue Requirements; Roadmap auf 8 Phasen erweitert; Bulk-Operations vor Surfaces/Materials.

### Pending Todos

(none yet — captured during sessions in `.planning/todos/pending/`)

## Session Continuity

Last session: 2026-07-15
Stopped at: Resume aus HANDOFF.json (Phase 10 THN-Modellabgleich, pausiert 2026-07-14 abends)
Resume file: .planning/.continue-here.md (bleibt bis Phase-10-Abschluss liegen)
