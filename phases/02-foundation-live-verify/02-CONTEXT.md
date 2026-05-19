# Phase 2: Foundation live verifizieren - Context

**Gathered:** 2026-05-19 (im Verlauf der Phase-1-Smoke-Tests aufgenommen)
**Status:** Complete (Live-Verifikation am realen Archicad MCP v29 durchgeführt)

<domain>
## Phase Boundary

Phase 2 verifiziert die MCP-Tool-Namen für alle 7 Warm-up-Felder am laufenden Archicad und füllt `reference/workflow-context.md` mit live-verifizierten Inhalten. Zusätzlich werden STORY-01 (alle Stories) und ATTR-01 (Attribute-Listings) implementiert.

**In scope:** Discovery + Live-Verifikation aller 7 Warm-up-Felder, STORY-01, ATTR-01, dokumentation in workflow-context.md.

**Out of scope:** Recipes (Phase 3+), Bulk-Operationen-Live (Phase 5), konkrete Element-CRUD-Workflows.
</domain>

<decisions>
## Implementation Decisions (live aufgenommen)

### Live-Probe-Methodik
- **D-10:** Phase 2 wurde **inline mit Phase 1** durchgeführt, nicht als separates Subagent-Spawn. Begründung: Sub-Agents im Claude-Code-Session-Registry nicht verfügbar; Live-MCP-Tools sind direkt aufrufbar; effizienter als Restart-Zyklus.
- **D-11:** Pro Feld: 1-2 Discovery-Queries + Direct-Call, Ergebnis in workflow-context.md mit `<!-- 2026-05-19 verifiziert AC29 -->`-Datums-Marker (D-08-Pattern angewandt umgekehrt: TODO wird zu verifizierter Eintrag).

### Feld 3 (Längeneinheit) — MCP-Gap
- **D-12:** Length-Unit ist nicht über einen direkten MCP-Endpoint verfügbar (verifiziert durch Discovery: kein „project units / preferences / working units"-Tool). Workaround: alle MCP-Coordinate-Aufrufe verwenden **Meter** als Default (empirisch verifiziert mit `elements_create_zones` 5×3m → 5×3m im Modell). Bei User-Erwähnung anderer Einheiten (cm, mm, ft): client-side umrechnen vor MCP-Call.

### Feld 5 (Sichtbare Layer) — zweistufig
- **D-13:** Layer-Listing über `attributes_get_attributes_by_type` (universell für alle Attribute-Typen). „Aktive Sichtbarkeit" indirekt über `navigator_get_view_settings` → `layerCombination` → `attributes_get_layer_combination_attributes` für Detail.

### Klassifizierungs-System-Identifikation
- **D-14:** Es gibt **keinen Endpoint, der das aktive Klassifikations-System markiert**. Wenn mehrere Systeme vorhanden, User fragen (z. B. der User dieses Setups hat `SAB_Klassifizierung` + `SAB_Klassifizierung_29`). Wenn nur eines, dieses verwenden.

### Projekt-spezifische Strings nicht im Skill
- **D-15:** Live-Probe hat projekt-spezifische Werte gezeigt (`SAB_Klassifizierung_29` GUID, `Z_*` / `A_*` Layer-Konvention, Pen-Tables-Namen). Diese gehören **in Memory**, nicht in den Skill (Self-Improvement-Grenze, siehe `reference/self-improvement.md`). → Memory-File `project_archicad_user_setup.md` angelegt.

</decisions>

<canonical_refs>
## Canonical References

### Project-Level
- `.planning/phases/01-skill-ger-st-safety/01-CONTEXT.md` — Phase-1-Decisions (D-01..D-09), Vorgänger.
- `.planning/REQUIREMENTS.md` — WARM-01, WARM-02 sind die formalen Phase-2-Items.
- `.planning/research/PITFALLS.md` § P5 — Klassifikations-System-Cross-Pollution (Feld 7 Begründung).

### Skill Output
- `reference/workflow-context.md` — alle 7 Felder live dokumentiert mit Datums-Markern; ATTR-01 universal-Tool dokumentiert.
- `reference/mcp-conventions.md` § Capability-Tabelle — was creatable ist und was nicht.

### Memory
- `~/.claude/projects/-Users-ap/memory/project_archicad_mcp_capabilities.md` — Capability-Surface generisch.
- `~/.claude/projects/-Users-ap/memory/project_archicad_user_setup.md` — User-spezifisch (Klassifikations-Systeme, Layer-Konvention, Pen-Tables).
</canonical_refs>

<success_signals>
## Success Signals (verifiziert)

1. ✅ Alle 7 Warm-up-Felder live geprüft (1 Gap dokumentiert).
2. ✅ STORY-01 — vollständige Story-Liste via `project_get_stories` zurückbekommen (10 Stories, EG=0).
3. ✅ ATTR-01 — universal-Tool `attributes_get_attributes_by_type` mit 12 unterstützten Typen verifiziert (Layer, PenTable durch tatsächliche Aufrufe getestet).
4. ✅ Klassifikations-Systeme abrufbar (2 im Test-Projekt vorhanden).
5. ✅ Real-World-Test: Zone + Polylinie im Archicad erfolgreich angelegt.

</success_signals>
