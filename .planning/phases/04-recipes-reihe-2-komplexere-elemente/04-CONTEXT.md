# Phase 4: Recipes Reihe 2 — Komplexere Elemente - Context

**Gathered:** 2026-05-20
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 4 liefert zwei Recipe-Dateien für komplexere Architektur-Element-Typen:
- `recipes/curtain-walls.md` — Fassaden / Pfosten-Riegel-Konstruktionen mit allen 5 Sub-Element-Typen.
- `recipes/library-objects.md` — Platzierte Library-Objects (Möbel, Sanitär, Beleuchtung, GDL-Items) inkl. Subtype-Erkennung + Property-Bulk-Update-Pattern.

**In scope:**
- Live-Verifikation gegen Teamwork-Projekt „Futurelab_BI_Umbau_ARZ_OBL_V29" (Port 19724) — **Read-Only**.
- CurtainWall: Top-Level + Sub-Elemente (CurtainWallSegment, Frame, Panel, Junction, Accessory) — alle 6 Typen mit Worked Examples.
- Library-Objects: aus Teamwork existierende lesen, Subtype-Discovery, Property-Bulk-Pattern (6-Schritt) wie in zones.md.
- Update SKILL.md Recipe-Index.

**Out of scope:**
- Test-Element-Create im Sandkasten (Phase 4 ist Real-Read-Only).
- CurtainWall-Create (Capability-Gap MCP v29).
- Object-Create-Worked-Example (defer Phase 5 oder spätere Session — wir lesen existierende, schreiben keine neuen).
- Bulk-Klassifizierungs-Worked-Examples (Phase 5).
- IFC-Specifics, Stair/Railing/Morph (v2).
</domain>

<decisions>
## Implementation Decisions

### D-21: Live-Test gegen Teamwork-Real-Projekt (Read-Only)
- **Port:** 19724 (`Futurelab_BI_Umbau_ARZ_OBL_V29`).
- **Modus:** Read-Only — keine Schreib-Operationen im Real-Projekt. Asymmetrische Sicherheit garantiert das (Create + Read frei, aber wir vermeiden auch Create in einem Real-Projekt).
- **Begründung:** Real-Projekt hat vermutlich echte CurtainWalls und Library-Objects in produktiver Konfiguration — Worked Examples gegen echte Strukturen sind pädagogisch wertvoller als gegen Sandkasten-Test-Elemente.
- **Risiko-Mitigation:** Worked Examples zeigen die MCP-Calls mit Platzhalter-GUIDs (`<curtain-wall-guid>`), nicht hartkodierte Real-Projekt-IDs. Reale GUIDs für Wave-0-Verifikation in TEST-SET.md (gitignored später, oder als „nur Phase-4-Run"-Hinweis markiert).
- **Sandkasten (19723) bleibt verfügbar** — falls Read im Real-Projekt etwas zerstört oder unklar ist, fallback dort.

### D-22: CurtainWall voll ausgearbeitet (alle 6 Element-Typen)
- Recipe `curtain-walls.md` deckt: `CurtainWall` (Top-Level) + `CurtainWallSegment` + `CurtainWallFrame` + `CurtainWallPanel` + `CurtainWallJunction` + `CurtainWallAccessory`.
- Pro Sub-Element-Typ: eigener Worked-Example-Block für Read + Update (wo modifizierbar) + Klassifikation. Delete oft nur auf Top-Level sinnvoll (Sub-Elemente sterben mit).
- **Erwartete Datei-Länge:** ~600-700 Zeilen. TOC besonders wichtig.
- **Begründung:** User-Wunsch („voll ausgearbeitet"). CurtainWalls sind im Fassaden-Workflow zentral — der Mehraufwand jetzt zahlt sich aus.
- **Gotchas-Sektion** dokumentiert die Hierarchie-Beziehung (Top-Level → Segments → Frames+Panels → Junctions). Element-ID-Threading (SAFE-05) ist hier prominent — Sub-Elemente brauchen Top-Level-Referenz.

### D-23: library-objects.md — Read-Only aus Teamwork, Subtype-Fokus
- Recipe-Inhalt aus tatsächlich im Teamwork-Projekt vorhandenen Objects abgeleitet.
- **Subtype-Discovery** ist das Kern-Konzept: Library-Objects unterscheiden sich vor allem im Subtype (Library-Pfad). Worked Example zeigt: alle Objects listen → nach Subtype gruppieren → repräsentative pro Gruppe lesen.
- **Kein Object-Create-Worked-Example in Phase 4.** Create ist via MCP verifiziert, aber wir testen es nicht im Real-Projekt — und das Sandkasten-Object-Create-Pattern ist trivial (siehe SCHEMAS.md aus Phase 3, `elements_create_objects`). Defer zu Phase 5 oder späterer Session, falls Bedarf.
- **Begründung:** Real-Projekt hat ~typischerweise 100+ Library-Objects in vielfältigen Subtypes — das ist die realistische Grundlage für Worked Examples.

### D-24: Property-Bulk-Pattern in library-objects.md spiegeln
- Genauso wie das in zones.md eingebaute 6-Schritt-Pattern „Property setzen (Bodenbelag-Pattern)" wird library-objects.md ein Worked-Example „Property-Bulk-Update für Library-Objects" bekommen.
- **Beispiel-Use-Case:** Möbel-Inventar-Properties wie „Hersteller", „Modell-Nummer", „Anschaffungsjahr" — solche Properties müssen oft bulk-aktualisiert werden.
- Verweis auf `../reference/bulk-operations.md` Pre-Flight-Check + Identifier-Mapping bleibt gleich.
- **Begründung:** Konsistenz im Skill. Wenn der User „Bulk-Update Property X auf 50 Möbeln" sagt, soll das Pattern aus library-objects.md erkennbar parallel zu zones.md sein — eine einzige Self-Improvement-Verbesserung in einem Recipe sollte ins andere übertragen werden.

### D-25: SAFE-Regeln gelten unverändert weiter
Phase-4-Recipes respektieren SAFE-01..05 unverändert. Hosted-Element-Pre-Check (SAFE-04) ist bei CurtainWall extra wichtig — Sub-Elemente hängen am Top-Level. Element-ID-Threading (SAFE-05) ist im CurtainWall-Sub-Hierarchie-Read prominent.

### D-26: Subagent-Architektur wie Phase 3
- **Wave 1 (Orchestrator):** MCP-Cleanup unnötig (kein Sandkasten-Create). Discovery + Schema-Sammlung für CurtainWall + Library-Object und Sub-Elemente. Listing in Teamwork-Projekt zum Aufspüren von Test-Elementen (deren GUIDs nur Wave-1-intern verwendet werden).
- **Wave 2 (2 parallele Subagents):**
  - Subagent A: `curtain-walls.md`
  - Subagent B: `library-objects.md`
- **Wave 3 (Orchestrator):** SKILL.md-Index-Update (zwei Recipe-Links bereits drin, nur Hinweis-Text feinjustieren), Konsistenz-Checks, Sample-Live-Validation gegen Teamwork (Read-Only-Calls).

</decisions>

<canonical_refs>
## Canonical References

### Project-Level
- `.planning/PROJECT.md`
- `.planning/REQUIREMENTS.md` — CURT-01 + LIB-01
- `.planning/ROADMAP.md` § Phase 4

### Prior Phases
- `.planning/phases/03-recipes-reihe-1-strukturelle-elemente-zonen/03-CONTEXT.md` — D-16..D-20 (Patterns aus Phase 3)
- `.planning/phases/03-recipes-reihe-1-strukturelle-elemente-zonen/SCHEMAS.md` — Schema-Sammlungs-Format
- `.planning/phases/03-recipes-reihe-1-strukturelle-elemente-zonen/03-06-SUMMARY.md` — Wave-3-Validation-Pattern

### Skill Foundation
- `SKILL.md` — Recipe-Index
- `reference/mcp-conventions.md` — Capability-Tabelle + Modal-Dialog + AC29-Bugs
- `reference/bulk-operations.md` — Pre-Flight + Identifier-Mapping (für D-24)
- `reference/schedule-pipeline.md` + `reference/property-expression-linking.md` — Bulk-Workaround-Pfade

### Memory (Real-Projekt-Kontext)
- `project_archicad_user_setup.md` — SAB_Klassifizierung_29, Layer-Konventionen
- `project_archicad_mcp_capabilities.md` — Capability-Surface
- `issue_archicad_mcp_get_details_bug.md` — `get_details_of_elements` + `gdl_parameters_of_elements` Bugs

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- SKILL.md Recipe-Index hat bereits Verweise auf `curtain-walls.md` und `library-objects.md` — keine Änderung nötig, nur Hinweis-Text feinjustieren.
- Phase-3-Recipe-Template (Umfang → Warm-up → Discovery-Anker → Typische Parameter → 4 Worked Examples → Bulk-Klassifizierungs-Stub → Gotchas → Verwandte Recipes) wird wiederverwendet.
- Phase-3-SCHEMAS.md-Format bleibt — wir erweitern es für CurtainWall + Library-Object.

### Established Patterns
- Read-via-Property-Workaround bei `get_details`-Bug (siehe wall-operations.md / openings.md / zones.md).
- Datums-Marker `<!-- 2026-05-20 verifiziert AC29 -->` vs. `<!-- VERIFY -->` Konvention.
- 6-Schritt-Property-Bulk-Workflow aus zones.md (für D-24).

### Integration Points
- `curtain-walls.md` → `wall-operations.md` (verwandt — beide modifizieren Fassaden-Elemente)
- `library-objects.md` → `openings.md` (Türen + Fenster sind GDL-Library-Objects, aber im openings-Recipe behandelt — Recipe verweist auf Abgrenzung)
- Beide Recipes → `reference/bulk-operations.md` § Pre-Flight + Identifier-Mapping

</code_context>

<deferred>
## Noted for Later

- **Object-Create-Worked-Example** im Sandkasten (`elements_create_objects` mit Standard-Library-Part) → Phase 5 oder spätere Session, falls Bedarf für „neuen Stuhl erzeugen"-Workflow.
- **CurtainWall-Create-Workaround** — falls in v1.x doch ein Pattern entsteht (User zeichnet Top-Level, wir modifizieren Sub-Elements) → eigene Phase oder v1.1.
- **Hotlink / XREF-Handling** für Library-Objects → v2 (siehe REQUIREMENTS.md § v2).
- **Real-Projekt-spezifische Subtype-Pfade** (z. B. „Schwarz Architekturbüro/Möbel/Sitzmöbel") werden in den Worked Examples als Platzhalter-Beispiele dokumentiert, NICHT als hartkodierte Strings — Memory ist der Ort dafür.

</deferred>

<success_signals>
## Success Signals (for Plan-Phase to verify)

Phase 4 ist erfolgreich, wenn:

1. `recipes/curtain-walls.md` existiert mit:
   - TOC (>100 Zeilen)
   - 6 Sub-Element-Typen abgedeckt (Top + Segment + Frame + Panel + Junction + Accessory)
   - Pro Typ: Read + (Update wo möglich) + Classify, plus Top-Level Delete mit Hosted-Pre-Check
   - SAFE-05 Element-ID-Threading prominent in Sub-Element-Hierarchie
   - Bulk-Klassifizierungs-Stub
   - Gotchas-Sektion mit Sub-Element-Hierarchie-Erklärung

2. `recipes/library-objects.md` existiert mit:
   - TOC
   - Subtype-Discovery + Subtype-Gruppierung als Kern-Worked-Example
   - Read + Update + Delete + Classify
   - **Property-Bulk-Update-Worked-Example** (6-Schritt-Pattern wie zones.md)
   - Bulk-Klassifizierungs-Stub
   - Klare Abgrenzung zu openings.md (Türen/Fenster sind GDL-Objects, aber dort behandelt)

3. SKILL.md Recipe-Index unverändert (Pfade bleiben gleich) oder Hinweis-Text feinjustiert.

4. Live-Sample-Validation gegen Teamwork-Port 19724:
   - 1 CurtainWall-Listing-Call (`elements_get_elements_by_type` mit `CurtainWall`) — Anzahl > 0 erwartet.
   - 1 Library-Object-Listing-Call mit Subtype-Gruppierung — diverse Subtypes erwartet.

5. SAFE-01..05 in Update/Delete-Beispielen sichtbar.

6. Voll-qualifizierte MCP-Toolnamen, one-level-deep References, D-02 Ton, Datums-Marker.

</success_signals>
