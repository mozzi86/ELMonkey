# Roadmap: Archicad-Skill für Claude Code

## Overview

Von Skill-Gerüst bis produktionsreifem Archicad-MCP-Skill in 8 Phasen. Jede Phase produziert eine eigenständig validierbare Teilfähigkeit, am laufenden Archicad live verifiziert. Phasenreihenfolge folgt den Abhängigkeiten aus Architecture-Research: Foundation → Warm-up → Strukturelle Recipes → Komplexe Recipes → Bulk-Infrastruktur → 2D + Materials → Cross-Recipe-Integration → Gap-Close.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, ..., 8): geplante Implementierungsphasen
- Decimal phases (z. B. 2.1): später eingeschobene Korrekturen (INSERTED-Marker)

- [x] **Phase 1: Skill-Gerüst + Safety** — Verzeichnisstruktur, SKILL.md mit allen Safety-Rules, 4 Reference-Skeletons, ohne Live-Archicad ✅ 2026-05-19
- [x] **Phase 2: Warm-up live + Story/Attribute-Listing** — MCP-Tool-Namen für alle 7 Warm-up-Felder live verifiziert (Feld 3 als MCP-Gap dokumentiert); STORY-01 + ATTR-01 implementiert; Capability-Tabelle für AC29 ergänzt ✅ 2026-05-19

**Backlog (post-v1.0):**
- 📋 **Phase 9 / v1.1-Milestone — DWG-zu-BIM-Pipeline** — Workflow um DWG-Importe in BIM-Elemente zu überführen (AmpliFY-Wrapper, DXF-Parser, oder semi-automatisches Nachzeichnen). Umgeht MCP-Wall-Create-Limit über User-zeichnet-Workflow. Details in REQUIREMENTS.md § v2 Requirements.
- [x] **Phase 3: Recipes Reihe 1 — Strukturelle Elemente + Zonen** — wall-operations, openings, slabs-columns-beams, zones (CRUD soweit MCP zulässt; Beam ohne Create, Wand/Öffnung/Curtain-Wall/Fill ohne Create). 1885 Zeilen, live-validated. ✅ 2026-05-20
- [x] **Phase 4: Recipes Reihe 2 — Komplexere Elemente** — curtain-walls (768 Z., 6 Element-Typen, Sub-Element-Hierarchie via `get_subelements_of_hierarchical_elements` live verifiziert), library-objects (683 Z., Library-Listing live verifiziert, Property-Bulk-Pattern integriert). 1451 Zeilen, Live-validated. ✅ 2026-05-20
- [~] **Phase 5: Bulk-Operations live + Klassifikations-Infrastruktur** — CAP-01+02 live verifiziert (Property-Set-Schema {value:string}, kein Modify-Endpoint), bulk-operations.md +3 Sektionen, 3. Wrapper-Bug für get_classifications_of_elements dokumentiert. **STATUS:** partial-complete. CAP-03+04 partial. CLASS-Worked-Examples in 5 Recipes deferred zu Phase 8. ✅ partial 2026-05-21
- [~] **Phase 6: Recipes Reihe 3 — 2D + Materials** — surfaces-materials, fills-hatches, lines-polylines. **STATUS:** Schema-only abgeschlossen (1848 Zeilen, alle 3 Recipes mit VERIFY-Markern). Live-Validation ausstehend sobald Archicad verfügbar. ✅ schema-only 2026-05-20
- [ ] **Phase 7: Cross-Recipe Integrationstests** — INT-01 (Wand + Fenster + Material + Linie), INT-02 (vollständige Projekt-Klassifizierung)
- [ ] **Phase 8: Smoke-Test + Gap-Close** — alle `<!-- ÜBERPRÜFEN -->`-Marker aus Phasen 2–7 abarbeiten; Definition-of-Done-Verifikation
- [ ] **Phase 10: THN-Modellabgleich — offene Punkte** — Sammel-Phase der laufenden THN-Produktivarbeit (2026-07-14), damit nichts vergessen wird; Details unten

## Phase Details

### Phase 1: Skill-Gerüst + Safety
**Goal**: Vollständiges Skill-Verzeichnis mit `SKILL.md` (alle Safety-Rules + Warm-up-Skelett + Index) und allen 4 Reference-Dateien als Skelette. Ausführbar ohne laufendes Archicad. Description-Feld korrekt (nur Trigger-Bedingungen).
**Depends on**: Nothing (first phase)
**Requirements**: FOUND-01, FOUND-02, FOUND-03 (Skelett), FOUND-04 (Skelett), FOUND-05, SAFE-01, SAFE-02, SAFE-03, SAFE-04, SAFE-05, SELF-01, SELF-02, SELF-03
**Success Criteria** (what must be TRUE):
  1. `~/.claude/skills/archicad/SKILL.md` existiert, ≤500 Zeilen, Description ist Trigger-only.
  2. Alle 4 Reference-Dateien existieren mit TOC bei >100 Zeilen, voll-qualifizierten `mcp__archicad__*`-Toolnamen, ein Level tief verlinkt.
  3. SKILL.md enthält explizit: Element-ID-Threading-Regel, Create+Delete-Verbot, Invisible-Layer-Ausnahme, Hosted-Element-Delete-Pre-Check-Hinweis.
  4. Self-Improvement-Mechanik (Reflection-Trigger, Datums-Marker) ist in `reference/self-improvement.md` dokumentiert.
**Plans**: 3 plans
- [ ] 01-01: SKILL.md schreiben (Sicherheit + Warm-up + Discovery-Pattern + Index)
- [ ] 01-02: Reference-Dateien skelettieren (mcp-conventions, workflow-context, bulk-operations, self-improvement) inkl. TOCs
- [ ] 01-03: Description-Feld validieren + Skelett-Smoke-Test (Claude lädt Skill ohne Fehler in einer neuen Session)

### Phase 2: Warm-up live + Story/Attribute-Listing
**Goal**: Alle 7 Warm-up-Felder am laufenden Archicad live verifiziert; STORY-01 und ATTR-01 als arbeitsfähige Sub-Recipes (oder Sektionen in `reference/workflow-context.md`).
**Depends on**: Phase 1
**Requirements**: FOUND-03 (Inhalt), WARM-01, WARM-02
**Success Criteria** (what must be TRUE):
  1. Echte MCP-Tool-Namen für Port-Holen, Projekt-Info, Längeneinheit, aktive Story, sichtbare Layer, Pen-Set, Klassifikations-System sind in `workflow-context.md` dokumentiert mit Datums-Marker.
  2. STORY-01: Claude listet erfolgreich alle Stories des Test-Projekts mit ID/Name/Höhe.
  3. ATTR-01: Claude listet erfolgreich Layer/Surfaces/Composites/Fills/Line-Types als ID/Name-Listen.
  4. Story-Feld in `workflow-context.md` ist als „volatil" markiert (re-verify bei jeder Floor-Erwähnung).
  5. Paginierungs-Verhalten verifiziert (mit/ohne `totalCount`-Feld).
**Plans**: 3 plans
- [ ] 02-01: Warm-up-Felder 1–4 live verifizieren (Port, Projekt-Info, Längeneinheit, Story)
- [ ] 02-02: Warm-up-Felder 5–7 live verifizieren (Layer, Pen-Set, Klassifikations-System) + STORY-01 + ATTR-01 implementieren
- [ ] 02-03: Paginierungs-Pattern in `mcp-conventions.md` einarbeiten (totalCount-Erkennung)

### Phase 3: Recipes Reihe 1 — Strukturelle Elemente + Zonen
**Goal**: Vier Recipe-Dateien (walls, openings, slabs-columns-beams, zones) mit vollständigem CRUD + Worked Examples am laufenden Archicad bestanden. Reihenfolge: walls vor openings (Host-ID-Threading), dann slabs-columns-beams parallel, zones zuletzt.
**Depends on**: Phase 2
**Requirements**: WALL-01, OPEN-01, STRU-01, ZONE-01
**Success Criteria** (what must be TRUE):
  1. `recipes/walls.md` Worked Example „6m Außenwand im EG" am Archicad erfolgreich; Composite-Zuweisung + Reference-Line-Position dokumentiert.
  2. `recipes/openings.md` Worked Example „1.20×1.40m Fenster mittig" hostet sich korrekt auf der Wand aus 3.1; Sill-Höhe + Schwenk-Richtung als Sub-Op dokumentiert.
  3. `recipes/slabs-columns-beams.md` Worked Examples für Decke, Stütze, Träger live; Slab-Offset + Profil-Sub-Ops dokumentiert.
  4. `recipes/zones.md` (oder integriert) Worked Example „Raum 25m²" erfolgreich; Name/Nummer/Kategorie/Fläche-Properties zugänglich.
  5. Asymmetrische Sicherheit greift in mindestens einem Update-Test pro Recipe (Update-Confirm angezeigt).
**Plans**: 4 plans
- [ ] 03-01: walls.md — gerade + geschwungene + polygonale Wände live verifizieren
- [ ] 03-02: openings.md — Fenster, Türen, Wandöffnungen live verifizieren mit Host-ID-Threading
- [ ] 03-03: slabs-columns-beams.md — alle drei strukturellen Elemente live
- [ ] 03-04: zones.md — Zonen-Recipe live

### Phase 4: Recipes Reihe 2 — Komplexere Elemente
**Goal**: curtain-walls und library-objects live verifiziert mit vollständigem CRUD.
**Depends on**: Phase 3
**Requirements**: CURT-01, LIB-01
**Success Criteria** (what must be TRUE):
  1. `recipes/curtain-walls.md` Worked Example „4×3m Pfosten-Riegel" am Top-Level-Element live; Sub-Element-API-Verfügbarkeit dokumentiert (oder als v1.x-Split markiert).
  2. `recipes/library-objects.md` Worked Example „Tisch + Stuhl platzieren" live; Subtype-Erkennung dokumentiert.
  3. Beide Recipes haben Gotchas-Sektion mit mindestens 2 verifizierten Stolperfallen.
**Plans**: 2 plans
- [ ] 04-01: curtain-walls.md live verifizieren inkl. Sub-Element-API-Klärung
- [ ] 04-02: library-objects.md live verifizieren mit Subtype-Behandlung

### Phase 5: Bulk-Operations live + Klassifikations-Infrastruktur
**Goal**: `reference/bulk-operations.md` vollständig live verifiziert. Klassifikations-System-Discovery + System-scoped GUID-Lookup funktioniert. Bulk-Klassifizierungs-Worked-Examples werden in alle bisher existierenden Recipes (walls, openings, slabs, library-objects) nachgezogen.
**Depends on**: Phase 4
**Requirements**: FOUND-04 (Inhalt), CLASS-01, CLASS-02
**Success Criteria** (what must be TRUE):
  1. Aktives Klassifikations-System wird via Discovery erkannt; falsches System wird vermieden (System-scoped Query).
  2. Klartext → GUID-Mapping funktioniert für mindestens 5 Klassen aus dem aktiven System.
  3. CLASS-Worked-Example pro betroffenes Recipe live verifiziert: walls Innen/Außen, openings Tür/Fenster, slabs-columns-beams tragend/nicht-tragend, library-objects Kategorie.
  4. Mehrdeutige Fälle (Wand zwischen Innen- und Außenraum) werden separat aufgelistet, NICHT geraten.
  5. Asymmetrische Sicherheit greift bei Massenklassifikation (Summary + `details`-Option).
**Plans**: 3 plans
- [ ] 05-01: bulk-operations.md Read→Filter→Group→Confirm→Apply-Muster live verifizieren
- [ ] 05-02: Klassifikations-System-Discovery + GUID-Lookup live
- [ ] 05-03: Bulk-Klassifizierungs-Worked-Examples in alle vorherigen Recipes nachziehen

### Phase 6: Recipes Reihe 3 — 2D + Materials
**Goal**: surfaces-materials (riskantestes Rezept; Create-Verfügbarkeit live klären), fills-hatches, lines-polylines. Klassifikations-Infrastruktur aus Phase 5 ist Voraussetzung für saubere Composite-vs-Surface-Disambiguierung.
**Depends on**: Phase 5
**Requirements**: SURF-01, FILL-01, LINE-01
**Success Criteria** (what must be TRUE):
  1. `recipes/surfaces-materials.md` Umfang explizit dokumentiert (Create live verifiziert: ja/nein; Zuweisung sicher). Composite-vs-Surface-Disambiguierung mit konkretem Beispiel.
  2. Material-Zuweisung an eine Wand aus Phase 3 erfolgreich live; Composite-Variante getestet.
  3. `recipes/fills-hatches.md` Worked Example „Schraffur auf Grundriss-Bereich" live; Orientierung/Winkel als Sub-Op.
  4. `recipes/lines-polylines.md` Worked Example „Hilfslinie + Polygon" live für alle 4 Typen (Linie, Polylinie, Bogen, Spline).
**Plans**: 3 plans
- [ ] 06-01: surfaces-materials.md live verifizieren (Create-Klarheit + Composite-Disambiguierung)
- [ ] 06-02: fills-hatches.md live verifizieren
- [ ] 06-03: lines-polylines.md live verifizieren (alle 4 Untertypen)

### Phase 7: Cross-Recipe Integrationstests
**Goal**: Zwei realistische Workflows, die mehrere Recipes orchestrieren, am laufenden Archicad sauber durchlaufen.
**Depends on**: Phase 6
**Requirements**: INT-01, INT-02
**Success Criteria** (what must be TRUE):
  1. INT-01 vollständig live: 6m Außenwand → 1.20×1.40m Fenster mittig → Material „Mauerwerk Klinker" zugewiesen → 2D-Hilfslinie 50cm parallel außerhalb. Endergebnis im Archicad sichtbar und korrekt.
  2. INT-02 vollständig live: Klassifizierung aller Wände/Öffnungen/Stützen/Library-Objects im Test-Projekt; Pro-Klasse-Counts + unklare Fälle berichtet vorab; nach `ja` ausgeführt.
  3. Bei beiden Tests werden Lücken (z. B. unerwartete Discovery-Fehlschläge, fehlende Sub-Operationen) im `<!-- ÜBERPRÜFEN -->`-Format markiert für Phase 8.
**Plans**: 2 plans
- [ ] 07-01: INT-01 (Wand + Fenster + Material + Linie) live
- [ ] 07-02: INT-02 (vollständige Klassifizierung) live

### Phase 10: THN-Modellabgleich — offene Punkte (Stand 2026-07-14)
**Goal**: Alle offenen Arbeitspunkte aus der THN-Session 2026-07-14 abarbeiten. Scratchpad-Daten liegen in der Session vom 14.07. (Scripts + Pläne sind deterministisch reproduzierbar).
**Kontext**: Referenzmodell im selben Teamwork-Projekt, Versatz Referenz→generiert dx=0.001/dy=349.820. **Referenzmodell ist READ-ONLY** (User-Regel, siehe Memory).
**Fortschritt 2026-07-14 abends:** 1 ✓ (Reservierung), 2 ✓ (4.500 Wände klassifiziert+Properties), 3 ✓ teilweise (307 Türen via Türwand-Pattern, 153 nummeriert, Properties übertragen; Rest hängt an Wandlücken), 4 ✓ (334 Glas-Fassaden, 104 Abhangdecken, 859 Deckenstreifen, 632 Zonen in Arbeit, Umgebung/Gelände mit DWG-Kalibrierung dx=-252.896/dy=+277.480), 5 ✓ teilweise (254/256 nachgezogen; +46 Öffnungen; 337 Öffnungen + ~340 Türen bleiben = Knautsch-/OG2-Lücken), 9b ✓ (20 Treppenläufe, Steigung 17,5 cm, 4 Deckenausschnitte). Tapir-Issue-ENTWÜRFE in Session-Scratchpad tapir_issues/ (noch nicht gepostet).

**Offene Punkte** (Reihenfolge = Priorität):
  1. **Teamwork-Reservierung wiederherstellen** (Blocker für alles Folgende — Senden gab Reservierungen frei, Reserve-all hing).
  2. **Eigenschafts-Transfer ausführen** (Script fertig, idempotent): Klasse „Wand" + Bauteilname (WB/WE/WD/WG/WA per kNN-Flügelzuordnung) + **SAB_Brandschutz** (2.171 Werte, nächste Ref-Wand ≤2,5 m) auf 4.500 generierte Wände. Wichtig: Properties sind erst NACH Klassifizierung verfügbar (notAvailable-Falle). Staubschutz/Baustelleneinrichtung NICHT übernehmen.
  3. **Türen**: Smoke-Test wiederholen (Favorit „Bau_Tür" funktioniert; „T Innen…"-Favoriten schlagen per API fehl — Library-Problem?); centerOffset-Semantik in Polywänden klären (Probe landete Kantenmitte); dann 904 Türen aus Aufschlag-Bögen (Radius=Breite, Anschlag=Bogenzentrum) + Türnummern aus Referenzmodell übernehmen.
  4. **Fehlende Elemente aus Referenz kopieren** (−349,82 m): Glas-Fassadenwände (Kollegin: Fassade als Wand, Material Glas), Rampen, neue Abhangdecken; Gebäudefugen mit DWGs abgleichen/verfeinern. OHNE Asbest-/Baustellen-Staubschutz (explizit ausgenommen).
  5. **383 Wanddurchbrüche ohne Host-Wand** + **101 beim Crash verlorene Wände** nachziehen (Wandlücken schließen).
  6. **OG2/UG-2**: Linienwerk liegt auf Alt-Ebenen (2MWTRAG_0, 2NMWTRAG0, 2ITUER__0…) → Wände+Decken erzeugen, dann 72 Warteliste-Öffnungen setzen.
  7. **51 gedrehte BD/FBA** achsparallel gesetzt → nacharbeiten oder ELM_SAB v0.6 (Öffnung mit Rotation).
  8. **48 Wandschlitze** (WS/BS/HS-Texte) → ELM_SAB v0.6: Öffnung mit Tiefenbegrenzung.
  9. **Unterzüge** (615 Linien, Linienpaar→CreateBeams), **Dach** (CreateRoofs, Neigung vom User) → danach **Dachflächenfenster** (ELM_SAB, braucht Dächer), **Fenster** (Linien-Cluster, mittel-groß).
  9b. **Treppen aus 2D** (A_06_TREPPE/F_05_TREPPE): Lauflinie→CreateStairs-Baseline, Stufenlinien→stepNum, Umriss→flightWidth; kein Referenz-Vorbild (beide Modelle treppenlos!). Smoke-Test→Masse. Fahrtreppen (M_21) gesondert.
  10. **Bemaßung**: CreateWallThicknessDimensions + Maßketten (Witness-Points testen); Höhenkoten = Tapir-Lücke.
  11. **Tapir-Bugs an Maintainer melden**: ModifySlabs+polygonOutline = fataler Crash; holes:[] ignoriert; CreateDoors mit bestimmten Favoriten schlägt fehl.
**Success Criteria**: Punkte 1–5 erledigt und in Teamwork gesendet; Punkte 6–11 erledigt oder bewusst als v0.6/Backlog markiert; Skill-Doku je Erkenntnis aktualisiert.

### Phase 8: Smoke-Test + Gap-Close
**Goal**: Alle in den Phasen 2–7 entstandenen `<!-- ÜBERPRÜFEN -->`-Marker abarbeiten; Definition-of-Done-Verifikation durchführen; ggf. v2-Backlog auffüllen mit nicht abdeckbaren Punkten.
**Depends on**: Phase 7
**Requirements**: alle (Definition-of-Done-Check)
**Success Criteria** (what must be TRUE):
  1. Kein `<!-- ÜBERPRÜFEN -->`-Marker mehr im Skill-Tree (außer dokumentierten v2-Verschiebungen).
  2. Alle 6 Definition-of-Done-Kriterien aus REQUIREMENTS.md verifiziert.
  3. SAFE-01..05 sind durch je einen Failure-Test bewiesen (mind. 5 Test-Szenarios dokumentiert).
  4. SELF-Mechanik in mindestens einer realen Session erfolgreich genutzt; Beispiel-Lerneintrag im Skill nachvollziehbar.
**Plans**: 2 plans
- [ ] 08-01: ÜBERPRÜFEN-Marker abarbeiten + Failure-Tests dokumentieren
- [ ] 08-02: Definition-of-Done-Verifikation + Memory-Anker setzen (Skill ist v1-bereit)
