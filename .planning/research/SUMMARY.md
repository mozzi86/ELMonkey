# Research Summary: Archicad-Skill für Claude Code

**Domain:** Claude Code skill wrapping discovery-driven MCP server for Archicad BIM operations
**Researched:** 2026-05-19
**Overall confidence:** HIGH — all four dimensions sourced from official Anthropic docs (STACK) or high-confidence domain expertise (FEATURES, ARCHITECTURE, PITFALLS). The one systemic low-confidence area is MCP-level support for specific sub-operations (surfaces Create, curtain-wall sub-elements, spatial queries), which live-verification will resolve.

---

## Executive Summary

The core design decisions in the spec — hub-and-spoke file structure, discovery-first tool resolution, asymmetric safety model, and self-improvement via reflection trigger — are all validated. No structural rethink is needed. What research added is precision: specific anti-patterns to avoid, mandatory protocol steps that were implied but not explicit in the spec, and a significant feature gap list that will land on day one if not addressed.

The most consequential single finding is the **description anti-pattern**: the spec's proposed skill description includes workflow summary text ("Includes per-job warm-up, asymmetric safety..."), which Anthropic's own testing shows causes Claude to skip the SKILL.md body and use the description as a shortcut. This must be fixed before any other implementation work starts. A wrong description invalidates the entire skill.

Feature research surfaced a meaningful gap between what the spec commits to and what a practicing architect will reach for in week one. Three missing primitives — story management read, attribute listing, and zones — are not exotic features; they are prerequisites for basic multi-story work, correct property assignment, and area scheduling. Adding them does not change the architecture, but they must appear in the roadmap as first-class work items, not later additions.

Pitfalls research produced 12 specific failure modes, nine of which are HIGH-confidence and all of which require design-in during implementation (not patching after). The four most dangerous: classification GUID cross-system pollution (corrupts BIM data irreversibly), hosted-element orphaning on wall delete (invisible model corruption), Create+Delete safety bypass (defeats the asymmetric model silently), and pagination cut-off in bulk reads (silent dataset truncation). All four require explicit rules written into SKILL.md and the reference files before any recipe work.

---

## Key Findings

**Stack:** Hub-and-spoke Markdown, ≤500-line SKILL.md, on-demand reference files; description must be triggering conditions only (no workflow summary); fully qualified `mcp__archicad__*` tool names throughout; one-level-deep references only; TOC required in any reference file >100 lines.

**Architecture:** Structure is correct and validated as-is; the one gap is that SKILL.md must explicitly state that element IDs from Create operations are retained and threaded across recipe calls within a task — without this instruction, multi-element workflows (wall → opening with host ID) will fail silently.

**Critical features missing from spec:**
1. Story management read (list all stories, elevations, heights) — prerequisite for every multi-story operation
2. Attribute listing (layers, surfaces, composites, fills, line types) — prerequisite for correct property assignment in all recipes
3. Zone creation + basic properties — area schedules are the most common non-geometry deliverable an architect produces

**Critical pitfalls to design-in:**
1. Classification GUID must be scoped to the active system from Warm-up step 7 — a global string search returns GUIDs from wrong classification systems, corrupting BIM data with no undo
2. Delete of a wall must pre-query hosted elements (windows, doors) and list them in the confirm dialog — without this, openings become orphaned floating elements that corrupt IFC exports silently
3. Create+Delete must never be used as an implementation of Update — explicit rule required in SKILL.md; without it Claude will naturally choose this path when the Update MCP interface is complex, silently bypassing confirm

---

## Implications for Spec

Changes the spec needs before implementation begins:

- **Fix the skill description:** Remove workflow summary from the description field. Description should end after "Triggers on any task involving the Archicad MCP server (`mcp__archicad__*`)." Move warm-up/safety/bulk detail to SKILL.md body.
- **Add element-ID threading rule to SKILL.md:** "Element IDs returned by Create operations are retained in working memory for the duration of the task. Use them as host/reference parameters in subsequent recipe calls without re-querying."
- **Add invisible-layer Create exception to SKILL.md safety section:** "Create is free *unless* the target layer is not visible — in that case, pause and confirm before proceeding."
- **Add Create+Delete prohibition to SKILL.md safety section:** "Never implement Update or Delete as a Create+[Delete|overwrite] sequence. If no Update endpoint exists, surface this to the user; do not destroy and recreate."
- **Add `self-improvement.md` to spec's reference file list:** Spec lists 3 reference files; research confirms 4 are needed. Self-improvement rules must exist before live-verification sessions begin.
- **Classify Story field as volatile in workflow-context.md:** Story ID must be re-verified whenever the user mentions a floor/level name in their request, not treated as session-stable.
- **Add P1 feature requirements to PROJECT.md:** STORY-01 (story management read), ATTR-01 (attribute listing), ZONE-01 (zone creation + properties) as Active requirements.
- **Add hosted-element pre-check to mcp-conventions.md design:** Before Delete confirm of any host element (wall), query and surface child elements in the confirm dialog.

---

## Implications for Roadmap

Suggested phase structure adjustments. Our current 7-phase Bootstrap-Plan from the spec:
1. Skill-Gerüst
2. Foundation live verifizieren
3. Recipes Reihe 1 (walls, openings, slabs-columns-beams)
4. Recipes Reihe 2 (curtain-walls, library-objects)
5. Recipes Reihe 3 (2D + materials)
6. bulk-operations live
7. Cross-recipe Integrationstests

Based on research, recommended changes:

- **Add `self-improvement.md` to Phase 1 scope** (currently missing). All 4 reference files must exist before any live session.
- **Add STORY-01 + ATTR-01 to Phase 2 scope.** Story IDs and attribute IDs are prerequisites for accurate recipe smoke-tests. Without them, warm-up is incomplete.
- **Add ZONE-01 to Phase 3 scope** (alongside walls/openings/slabs). Zone is architecturally similar to a structural element and shares MCP patterns. Placing it here lets it use the bulk pattern from Phase 6 as a natural v1.x extension.
- **Move Phase 6 (bulk-operations live) before Phase 5 (Reihe 3).** Surfaces/materials live-verification depends on classification system and attribute GUID infrastructure. Pitfall 12 (composite vs. surface confusion) must be resolved before surfaces-materials.md is authored. Surfaces/materials is the riskiest recipe; bulk-operations context should precede it.
- **Add a pitfall-hardening checkpoint between Phase 1 and Phase 2.** Before any live-verification: confirm SKILL.md contains the invisible-layer exception, Create+Delete prohibition, element-ID threading rule, and hosted-element delete pre-check in mcp-conventions.md.

**Revised 8-phase structure:**
1. Skill-Gerüst (SKILL.md + all 4 reference files, pitfall rules baked in, description fixed)
2. Foundation live verifizieren + STORY-01 + ATTR-01 (warm-up, story management, attribute listing)
3. Recipes Reihe 1 + ZONE-01 (walls → openings → slabs-columns-beams → zones)
4. Recipes Reihe 2 (curtain-walls, library-objects)
5. bulk-operations live (classification system, GUID mapping, pagination exhaustion verification)
6. Recipes Reihe 3 (surfaces-materials with disambiguation, fills-hatches, lines-polylines)
7. Cross-recipe Integrationstests (INT-01: wall+window+material+line; INT-02: full project classification)
8. Smoke-test and gap-close (address VERIFY markers from previous phases)

**Phase ordering rationale:**
- Phase 1 before Phase 2: SKILL.md is the always-loaded hub; safety rules and element-ID threading must exist before any live call.
- Phase 2 before Phase 3: Story IDs and attribute IDs are prerequisites for accurate smoke-tests across all recipes.
- Phase 3 walls before openings: openings require wall element IDs as host references; the wall Create smoke-test produces the real ID format needed for the openings Worked Example.
- Phase 5 (bulk-operations) before Phase 6 (surfaces-materials): classification GUID infrastructure and composite/surface disambiguation must be established before the riskiest recipe is authored.
- Phase 7 integration tests last: they validate cross-recipe data flow which only works after all component recipes are individually stable.

**Research flags for phases:**
- Phase 3: needs phase-specific live-verification of MCP zone creation tool — no verified tool name exists yet
- Phase 5: needs phase-specific live-verification of classification system discovery and GUID lookup scope — core pitfall (P5) depends on this
- Phase 6: needs live-verification of surfaces Create support — currently flagged uncertain in PROJECT.md
- Phases 1, 2, 4, 7, 8: standard patterns, skip phase-level research

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Official Anthropic docs; all constraints confirmed including description anti-pattern, size limits, one-level-deep rule |
| Features | HIGH (needs) / MEDIUM (MCP support) | Architect workflow needs are high-confidence; MCP support for zone, stair, spatial queries unverified |
| Architecture | HIGH | Hub-and-spoke validated; build order confirmed; element-ID threading gap is small and well-defined |
| Pitfalls | HIGH (9/12) / MEDIUM (3/12) | Discovery ambiguity, schema drift, orphaned elements, pagination cut-off, GUID cross-system all HIGH |

---

## Gaps to Address

- **Zone creation MCP support:** Dedicated tool or generic createElement(type=zone)? Live-verify in Phase 3.
- **Surfaces Create support:** Still flagged uncertain (PROJECT.md Known Issues). Resolve in Phase 6; recipe Umfang must state outcome explicitly.
- **Classification GUID scoped query pattern:** Exact Discovery query for system-scoped GUID lookup (Pitfall 5) unverified. Placeholder in bulk-operations.md; populate during Phase 5.
- **Curtain-wall sub-element API:** Whether panel type per cell is accessible on the top-level element or requires a sub-element API determines whether CURT-01 needs splitting in v1.x.
- **Innen/Außen geometry derivation algorithm:** Spec flags "Layer-Heuristik + Raumzugehörigkeit + Geometrie" without committing to a specific algorithm. Design in Phase 5 against a real project.
- **Pagination totalCount field:** Pitfall 11 mitigation depends on comparing collected count against a totalCount response field. Verify in Phase 2; if absent, mitigation falls back to user-visible count in confirm dialog only.
