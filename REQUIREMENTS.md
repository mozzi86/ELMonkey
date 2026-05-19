# v1 Requirements — Archicad-Skill für Claude Code

**Datum:** 2026-05-19
**Quelle:** Synthese aus Design-Spec (`docs/superpowers/specs/2026-05-19-archicad-skill-design.md`) + Domain-Research (`.planning/research/SUMMARY.md`).

## Übersicht

v1 liefert einen produktionsreifen Claude-Code-Skill, der Claude befähigt, in einem laufenden Archicad-Projekt sämtliche gängigen Modellier-, Abfrage- und Bulk-Operationen mit asymmetrischer Sicherheit und selbst-verbesserndem Verhalten durchzuführen.

---

## v1 Requirements

### Foundation (FOUND)

- [ ] **FOUND-01**: `SKILL.md` — Hub-Datei (≤500 Zeilen, ≤1.500 Tokens). Enthält: Description (nur Trigger-Bedingungen, kein Workflow-Summary), Sicherheitsregeln (asymmetrisch + Create+Delete-Verbot + Layer-not-visible-Ausnahme + Element-ID-Threading-Regel), Warm-up-Checkliste, Discovery-Pattern-Kurzfassung, Index zu Reference + Recipes.
- [ ] **FOUND-02**: `reference/mcp-conventions.md` — Discovery-Strategien, Fehlerklassen, Paginierungs-Handling (mit `totalCount`-Vergleich oder visibilisierten Count), Port-Handling. TOC am Anfang. Voll-qualifizierte MCP-Toolnamen (`mcp__archicad__*`).
- [ ] **FOUND-03**: `reference/workflow-context.md` — Live-verifizierte MCP-Tool-Namen für alle 7 Warm-up-Felder. Story-Feld als „volatil" markiert.
- [ ] **FOUND-04**: `reference/bulk-operations.md` — Read → Filter → Group → Confirm → Apply-Muster + Klassifikations-Spezifika (System-scoped GUID-Lookup, Mehrdeutigkeits-Handling).
- [ ] **FOUND-05**: `reference/self-improvement.md` — Reflection-Trigger, Lern-Klassen, Format-Konventionen (Datums-Marker), Verification-Loop. **Muss VOR Live-Verifikations-Sessions existieren**.

### Warm-up Erweiterungen (WARM)

- [ ] **WARM-01 (= STORY-01)**: Story-Management-Read — alle Stories listen (ID, Name, Höhe, Elevation). Voraussetzung für korrekte Mehrgeschoss-Operationen.
- [ ] **WARM-02 (= ATTR-01)**: Attribute-Listing — Layer, Surfaces, Composites, Fills, Line-Types als ID/Name-Listen abfragbar. Voraussetzung für korrekte Property-Zuweisung in allen Recipes.

### Zonen / Räume (ZONE — neu aus Research)

- [ ] **ZONE-01**: Recipe `zones.md` oder Integration in `recipes/zones-rooms.md` — Erstellen + Listen + Modifizieren von Zonen mit Basis-Properties (Name, Nummer, Kategorie, Fläche). Voraussetzung für Flächenermittlung/Schedulings.

### Element-Recipes (CRUD)

- [ ] **WALL-01**: Recipe `walls.md` — Gerade, geschwungene, polygonale Wände. CRUD vollständig. **Composite-Zuweisung + Reference-Line-Position** als Sub-Operations dokumentiert. Worked Example live verifiziert.
- [ ] **OPEN-01**: Recipe `openings.md` — Fenster, Türen, Wandöffnungen. CRUD vollständig. **Sill-Höhe + Türschwenk-Richtung** als Sub-Operations dokumentiert. Host-Wand-ID-Threading.
- [ ] **STRU-01**: Recipe `slabs-columns-beams.md` — Decken, Stützen, Träger. CRUD vollständig. **Slab-Offset von Home-Story + Beam/Column-Profil** als Sub-Operations dokumentiert.
- [ ] **CURT-01**: Recipe `curtain-walls.md` — Fassaden / Pfosten-Riegel. CRUD am Top-Level-Element. Sub-Element-API (Panel-Typ pro Zelle) als „bei Verfügbarkeit" markiert; ggf. v1.x-Split.
- [ ] **LIB-01**: Recipe `library-objects.md` — Platzierte Objects (Möbel, Sanitär, GDL-Library-Items). CRUD vollständig. Subtype-Erkennung.
- [ ] **SURF-01**: Recipe `surfaces-materials.md` — Surfaces, Building Materials, Composites. Umfang explizit im „Umfang"-Abschnitt: Create live verifiziert; Zuweisung/Read sicher. **Composite-vs-Surface-Disambiguierung** als Gotcha dokumentiert.
- [ ] **FILL-01**: Recipe `fills-hatches.md` — 2D-Schraffuren. CRUD vollständig. Orientierung/Winkel als Sub-Op.
- [ ] **LINE-01**: Recipe `lines-polylines.md` — 2D-Linien, Polylinien, Bögen, Splines. CRUD vollständig.

### Klassifizierung & Bulk (CLASS)

- [ ] **CLASS-01**: Bulk-Klassifizierungs-Worked-Examples pro relevantem Recipe — `walls.md` (Innen/Außen), `openings.md` (Tür/Fenster), `slabs-columns-beams.md` (tragend/nicht-tragend), `library-objects.md` (Möbel/Sanitär/Beleuchtung). Jeweils mit konkreter Ableitungs-Logik (Layer + Geometrie + Raumzugehörigkeit).
- [ ] **CLASS-02**: System-scoped Klassifikations-GUID-Lookup im Bulk-Operations-Pattern — verhindert Cross-System-Pollution (Pitfall 5).

### Sicherheits-Rules (SAFE)

- [ ] **SAFE-01**: Asymmetrisches Sicherheits-Modell durchgängig — Create/Read frei; Update/Delete confirm. Keine Batch-Obergrenze.
- [ ] **SAFE-02**: Create+Delete-Verbot — Update/Delete dürfen nie als Create+Delete-Sequenz implementiert werden. Explizite Regel in `SKILL.md`. (Pitfall 10)
- [ ] **SAFE-03**: Invisible-Layer-Ausnahme — Create pausiert und konfirmiert, wenn Ziel-Layer nicht sichtbar ist. (Pitfall 4)
- [ ] **SAFE-04**: Hosted-Element-Pre-Check bei Delete — Bevor Wand-Delete konfirmiert wird, hosted Elemente (Fenster, Türen) abfragen und in Confirm-Dialog auflisten. (Pitfall 6)
- [ ] **SAFE-05**: Element-ID-Threading-Regel in `SKILL.md` — IDs aus Create werden für die Dauer des Auftrags im Working Memory behalten und als Host/Reference-Parameter in Folgeoperationen verwendet, ohne neue Discovery-Calls.

### Integrationstests (INT)

- [ ] **INT-01**: Cross-Recipe-Test „Wand + Fenster + Material + Hilfslinie" am laufenden Archicad. Deckt: walls + openings + surfaces-materials + lines-polylines + Warm-up + asymmetrische Sicherheit.
- [ ] **INT-02**: Cross-Recipe-Test „komplette Projekt-Klassifizierung" — alle Wände, Öffnungen, Stützen, Library-Objects klassifizieren. Deckt: bulk-operations + Klassifikations-System + asymmetrische Sicherheit bei großen Batches + Mehrdeutigkeits-Handling.

### Self-Improvement (SELF)

- [ ] **SELF-01**: Reflection-Trigger am Auftrags-Ende — Claude fragt einmal: „Lern-Check: War da was Neues, das in den Skill sollte?". Antworten: `nein` / `ja, <X>` / `check selbst`.
- [ ] **SELF-02**: Datums-Marker-Format — alle neuen Einträge mit `<!-- YYYY-MM-DD -->`. `<!-- ÜBERPRÜFEN -->` bei wiederholten Problemen; 3 stabile Sessions entfernen Marker.
- [ ] **SELF-03**: Self-Improvement-Verification — neue Tool-Namen / Parameter durch reale Ausführung im MCP validiert, bevor sie eingetragen werden. (Pitfall 9: Hallucination-Feedback-Loop verhindern)

---

## v2 Requirements (deferred)

Aus Research-Features hervorgegangen; nicht in v1, aber natürliche v1.x/v2-Erweiterungen:

- Zone-Flächen-Schedules (Tabellen-Output)
- Hotlink / Xref-Read
- Attribute-Remapping projektweit
- Stair / Railing Recipes
- Morph Recipe
- Mesh / Terrain Recipe
- IFC Pset Read/Write
- Clash Detection (LOW confidence MCP-Support)
- Bulk-Layer-Migration (wenn nicht in v1 via bulk-operations integriert)
- Story-Duplikation, Level-Shifts

---

## Out of Scope (mit Begründung)

- **IFC-Import/-Export** — separater Funktionsbereich; v2-Thema.
- **Layout-Buch / Pläne / Plot** — Output-Workflow, nicht Modellierung.
- **Erstellung neuer Views** (Schnitte, Ansichten, 3D-Dokumente) — Sichten ≠ Elemente.
- **Bemaßungen + Beschriftungs-Stile** — eigener Komplex, später.
- **GDL-Code schreiben** — separate Domäne (Library-Entwicklung).
- **BIMx, Renderings, Visualisierung** — Output, nicht Bearbeitung.
- **Mehrere Archicad-Instanzen parallel** — Komplexität nicht gerechtfertigt.
- **Teamwork / BIMcloud-Sync** — kollaborativer Workflow, eigene Sicherheits-Frage.
- **Auto-Save / Undo-Execution per Skill** — Archicad-Native; Claude soll nicht eingreifen.
- **Layer-/Story-Create/Delete** — strukturelle Projektänderungen, eigene Hochrisiko-Klasse.
- **Clipboard Copy/Paste per Skill** — verliert Host-Links, BIM-Daten-Risiko.
- **View/Layer-Combination-Switching per Skill** — UI-State, gehört zum User.
- **Batch-Delete ohne explizite ID-Liste** — verletzt asymmetrische Sicherheit.

---

## Traceability

| REQ-ID | Phase | Status |
|---|---|---|
| (wird von Roadmapper gefüllt) | | |

---

## Definition of Done (v1)

Der Skill gilt als v1-fertig, wenn:
1. Alle Foundation- + Reference-Files (FOUND-01..05) existieren und Sicherheitsregeln vollständig enthalten.
2. Alle 8+1 Recipes (WALL/OPEN/STRU/CURT/LIB/SURF/FILL/LINE/ZONE) Worked Examples am laufenden Archicad bestanden haben.
3. Bulk-Klassifizierungs-Worked-Examples (CLASS-01) für die 4 Kern-Element-Typen funktionieren.
4. Beide Integrationstests (INT-01, INT-02) am laufenden Archicad sauber durchlaufen.
5. Self-Improvement-Mechanik (SELF-01..03) ist im Skill verankert und mindestens einmal in einer realen Session genutzt worden.
6. Asymmetrische Sicherheit (SAFE-01..05) ist in mindestens einem Failure-Test bewiesen (falsche Element-ID, vage Aufgabe, unsinnige Discovery-Query).
