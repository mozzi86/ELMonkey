# Architecture Research

**Domain:** Claude Code Skill — Hub-and-Spoke Markdown, Archicad MCP integration
**Researched:** 2026-05-19
**Confidence:** HIGH on structure questions; MEDIUM on MCP-specific data flow (pre-live-verification)

---

## System Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│  SKILL.md  (always loaded, < 1500 tokens)                            │
│  Safety rules · Warm-up checklist · Discovery pattern · Link map     │
└──────────────────────────┬───────────────────────────────────────────┘
                           │ loads on demand
          ┌────────────────┼──────────────────────┐
          ▼                ▼                      ▼
┌──────────────────┐  ┌────────────────┐  ┌──────────────────────────┐
│  reference/      │  │  reference/    │  │  reference/              │
│  mcp-conventions │  │  workflow-     │  │  bulk-operations.md      │
│  .md             │  │  context.md    │  │                          │
│                  │  │                │  │  + reference/            │
│  Discovery strat │  │  Warm-up facts │  │  self-improvement.md     │
│  Error classes   │  │  Live tool     │  │                          │
│  Pagination      │  │  names         │  │  Read→Filter→Group→      │
│  Port handling   │  │  Port/Story/   │  │  Confirm→Apply           │
└──────────────────┘  │  Layer/Units   │  │  Classification GUIDs    │
                      └────────────────┘  └──────────────────────────┘
          ┌────────────────────────────────────────────────────┐
          │  recipes/  (on-demand, one per element type)       │
          ├─────────────┬──────────────┬────────────────────── ┤
          │ walls.md    │ openings.md  │ slabs-columns-        │
          │             │              │ beams.md              │
          ├─────────────┼──────────────┼───────────────────────┤
          │ curtain-    │ library-     │ surfaces-             │
          │ walls.md    │ objects.md   │ materials.md          │
          ├─────────────┼──────────────┼───────────────────────┤
          │ fills-      │ lines-       │                       │
          │ hatches.md  │ polylines.md │                       │
          └─────────────┴──────────────┴───────────────────────┘
```

---

## Q1 — Boundary check on the 8 recipe groupings

### curtain-walls.md: keep as one file

**Verdict: KEEP as one file.** Confidence: HIGH.

The spec groups Fassaden + Pfosten-Riegel under CURT-01 deliberately. The MCP interface for curtain walls treats them as a single element type with sub-element relationships (frame members, panels are children of the curtain wall element). Splitting into "facade-systems.md" vs "panel-frame-manipulation.md" would fragment what is a single MCP create/read/update cycle. The natural split — if it ever materialises — is between top-level curtain wall CRUD and sub-element manipulation, but the spec explicitly restricts v1 to CRUD on the element itself. Keep it one file until live-verification shows the sub-element API is substantially different enough to warrant separation.

### openings.md: keep as one file

**Verdict: KEEP as one file.** Confidence: HIGH.

Fenster (windows) and Türen (doors) share the same host-dependent mechanism: both require a wall host, both use the same placement logic (position on wall axis, sill height), both carry a "host-id" relationship in MCP operations, and both require the same safety precaution when the host wall is deleted. The spec explicitly lists them together under OPEN-01 with "Wandöffnungen" (plain wall openings) as the third member. The shared mechanism is load-bearing for the recipe — a single Discovery-Anker table covers both because the query pattern ("place opening on wall") is the same. Splitting would duplicate the warm-up, host-relationship, and safety sections without adding information. The only valid split trigger would be if MCP uses entirely distinct tool families for doors vs windows — verify at live-implementation, but current knowledge says they share the element API.

### surfaces-materials.md: keep as one file, but flag scope ambiguity

**Verdict: KEEP as one file; add explicit scope table.** Confidence: MEDIUM.

Surfaces, Building Materials, and Composites are three distinct Archicad objects, but from the MCP perspective they are all "attribute-type" entities (not geometry elements): they are queried via attribute-discovery calls and assigned to geometry elements. The recipe's job is the assignment workflow, not the creation of these attributes. The spec already flags that Create is uncertain (FOUND-05 known issue). A single file covering "how to discover, read, and assign surface/material attributes to elements" is coherent. What the file must NOT do is balloon into material-library management. The scope boundary — assignment yes, library CRUD uncertain — should be an explicit "Umfang" table at the top of the recipe. Do not collapse with fills-hatches.md (different element class, different visual domain) or lines-polylines.md.

---

## Q2 — Reference vs Recipe split: are 4 reference files over-abstraction?

**Verdict: No — the 4-file reference layer is the right size. Keep all four.** Confidence: HIGH.

Test: would the information fit naturally in SKILL.md if we removed reference/?

- `mcp-conventions.md` — Discovery retry logic, pagination token handling, port-multiplexing: too operational for SKILL.md which must stay < 1500 tokens. Has to live somewhere. Correct.
- `workflow-context.md` — Live-verified tool names for warm-up fields. These change as Archicad versions change. Isolating them makes the self-improvement target explicit. Correct.
- `bulk-operations.md` — The Read→Filter→Group→Confirm→Apply pattern + GUID-mapping is referenced from every recipe's "Bulk-Klassifizierung" section. Cross-cutting by design. Correct.
- `self-improvement.md` — Reflection trigger, verification loop, format conventions. This is instruction-to-Claude, not reference-to-MCP. Without it in its own file, the Reflection trigger would have to live in SKILL.md (inflating it) or be duplicated across recipes.

The only consolidation candidate: merge `workflow-context.md` into `mcp-conventions.md`. Argument against: `workflow-context.md` is the primary target of self-improvement writes (new live tool-names go here), while `mcp-conventions.md` holds structural patterns that rarely change. Mixing them muddies the write-target semantics. Keep them separate.

**Do not** add a fifth reference file. The only plausible addition would be a `reference/classification-systems.md` for GUID-mapping, but the spec explicitly assigns that content to `bulk-operations.md`. Keep it there.

---

## Q3 — Data flow during "erstelle Wand mit Fenster und Bemaßung"

Note: "Bemaßung" (dimensioning) is out of scope per PROJECT.md. The realistic multi-element task from the spec's Integration Test 1 is: Wand + Fenster + Material + Hilfslinie (construction line). This is the canonical case.

### Optimal load order for a multi-element task

```
[User: "EG Außenwand 6m + Fenster 1.20×1.40 mittig + Material Klinker + Hilfslinie"]
    │
    ▼
1. SKILL.md  (already loaded — always in context)
   → Triggers warm-up check
    │
    ▼
2. reference/workflow-context.md
   → Resolve: active Story ID, unit, visible layers, port
   (One-time per session, cached)
    │
    ▼
3. recipes/walls.md
   → Discovery-Anker for wall create
   → Execute: create wall, capture returned element ID
    │
    ▼
4. recipes/openings.md
   → Discovery-Anker for window create (host = wall element ID from step 3)
   → Execute: place window at mid-point of wall
    │
    ▼
5. reference/mcp-conventions.md  (only if error occurs in steps 3–4)
   → Consult error class, retry strategy
    │
    ▼
6. recipes/surfaces-materials.md
   → Discover attribute for "Mauerwerk Klinker"
   → CONFIRM (Update → asymmetric safety kicks in)
   → Assign to wall element ID from step 3
    │
    ▼
7. recipes/lines-polylines.md
   → Create construction line 50cm parallel to wall
    │
    ▼
8. reference/self-improvement.md  (end-of-task reflection trigger)
   → "War da was Neues?"
```

### Does the current structure enable this flow?

YES — with one gap. Steps 3 and 4 have a hard dependency: `openings.md` needs the wall element ID that `walls.md`'s Create operation returns. The structure enables this because:
- Both recipes are separate files loaded on demand (no inter-file dependency in the file system).
- The dependency is a runtime data dependency (element ID), not a file-structural one.
- SKILL.md's warm-up section naturally establishes that element IDs from one step flow into the next.

The gap: SKILL.md must explicitly state that **element IDs from Create operations are retained in working memory across recipe calls within the same task**. Without this instruction, Claude may not know to thread the wall ID into the openings recipe. This is a SKILL.md content requirement, not a structural change.

---

## Q4 — Build-order dependency: openings before or after walls?

**Verdict: walls.md BEFORE openings.md in build order.** Confidence: HIGH.

The dependency is asymmetric:
- `openings.md` requires `walls.md` conceptually: every window/door placement uses a wall host. A developer writing `openings.md` needs to know what a wall element ID looks like, how Create returns it, and what the host-relationship parameter is.
- `walls.md` does NOT reference openings. Walls exist independently.

However, "build order" here means implementation sequence, not file-system order. The argument is practical:
1. Build `walls.md` first, live-verify it end-to-end, and record the exact structure of a wall element ID as returned by the MCP.
2. Use that real element ID as the `hostId` in the `openings.md` Worked Example.

If `openings.md` is built before `walls.md` is live-verified, the Worked Example will have a placeholder host ID, the smoke-test will fail, and iteration is wasted. Build order: walls → openings.

This matches the spec's Bootstrap-Plan (step 3 = Reihe 1: walls.md, openings.md in that order). Confirming the spec is correct on this point.

---

## Q5 — Cross-cutting concerns: shared/ subdirectory or spread?

**Verdict: Keep in reference/ as-is. Do NOT create a shared/ subdirectory.** Confidence: HIGH.

The three cross-cutting concerns are:
- Asymmetric safety (confirm-before-update/delete)
- Self-improvement (reflection trigger, learning loop)
- Bulk-operations (Read→Filter→Group→Confirm→Apply)

Current location: asymmetric safety lives in SKILL.md (the always-loaded hub). Bulk-operations in `reference/bulk-operations.md`. Self-improvement in `reference/self-improvement.md`.

A `shared/` directory would be a renaming of `reference/` with no behavioral difference. It would add a navigation layer without adding information. The hub-and-spoke model already handles this: SKILL.md is the cross-cutting anchor, and `reference/` is its extended detail layer.

The one structural question worth considering: should safety rules move from SKILL.md into `reference/`? No. Safety must be in the always-loaded hub (SKILL.md) because it gates every operation. Moving it to reference/ would require Claude to load an additional file before knowing the safety model — defeating the purpose.

---

## Recommended Structure (validated against spec)

```
~/.claude/skills/archicad/
├── SKILL.md                        # Hub: safety + warm-up + discovery + links
├── reference/
│   ├── mcp-conventions.md          # Discovery strategies, errors, pagination, port
│   ├── workflow-context.md         # Live-verified warm-up tool names (self-improve target)
│   ├── bulk-operations.md          # Read→Filter→Group→Confirm→Apply + GUID mapping
│   └── self-improvement.md         # Reflection trigger, verification loop, format rules
└── recipes/
    ├── walls.md                    # Straight/curved/polygon walls; CRUD
    ├── openings.md                 # Windows + doors + wall openings; CRUD; host=wall
    ├── slabs-columns-beams.md      # Slabs, columns, beams; CRUD
    ├── curtain-walls.md            # Facade systems + post-and-rail; CRUD
    ├── library-objects.md          # Furniture, sanitary, GDL objects; CRUD
    ├── surfaces-materials.md       # Surfaces, Building Materials, Composites; assign+read
    ├── fills-hatches.md            # 2D hatches; CRUD
    └── lines-polylines.md          # 2D lines, polylines, arcs, splines; CRUD
```

No changes from spec. The structure is correct. No splits, no merges, no renames required.

---

## Build Order (with rationale)

| Step | File | Rationale |
|------|------|-----------|
| 1 | `SKILL.md` skeleton | Hub must exist before any spoke; defines warm-up + safety that all others reference |
| 2 | `reference/mcp-conventions.md` | Discovery pattern needed before any recipe is tested |
| 3 | `reference/workflow-context.md` | Warm-up fields needed for any live smoke-test; populated during live-verification |
| 4 | `reference/bulk-operations.md` | Can be written pre-live; live-verification populates GUID-mapping section |
| 5 | `reference/self-improvement.md` | No live dependency; write once, stable |
| 6 | `recipes/walls.md` | Foundation element; no upstream recipe dependency; provides element IDs for steps 7+ |
| 7 | `recipes/openings.md` | Depends on wall element ID format from step 6 |
| 8 | `recipes/slabs-columns-beams.md` | Independent of openings; parallel to step 7 in theory, but sequential in practice |
| 9 | `recipes/curtain-walls.md` | More complex parameter set; build after simpler elements are stable |
| 10 | `recipes/library-objects.md` | GDL placement complexity; after structural elements |
| 11 | `recipes/surfaces-materials.md` | Attribute assignment; requires live-verify of Create uncertainty |
| 12 | `recipes/fills-hatches.md` | Pure 2D; independent |
| 13 | `recipes/lines-polylines.md` | Pure 2D; independent; needed for Integration Test 1 |
| 14 | Integration Test 1 | Wand + Fenster + Material + Hilfslinie — validates steps 6, 7, 11, 13 in sequence |
| 15 | Integration Test 2 | Bulk-Klassifizierung — validates step 4 + all recipes' Klassifizierung sections |

---

## Architectural Patterns

### Pattern 1: Discovery-First, No Hardcoded Tool Names

**What:** Every recipe stores tool names as "Hinweis" (hint), not contract. Claude runs `archicad_discover_tools` before each new tool family.
**When to use:** Always, for every MCP call.
**Trade-off:** +1 MCP round-trip per operation; +robustness against Archicad version changes. At the skill's scale (single user, < 50 operations per session), the overhead is negligible.

### Pattern 2: Element-ID Threading

**What:** Create operations return element IDs; these flow as parameters into subsequent operations within the same task (openings use wall host IDs; material assignments use wall element IDs).
**When to use:** Any multi-element task.
**Gap to address:** SKILL.md must explicitly instruct Claude to retain and thread element IDs across recipe calls. This is not currently stated in the spec.

### Pattern 3: Asymmetric Safety at the SKILL.md Layer

**What:** Safety classification (free vs confirm) lives in SKILL.md (always loaded), not in individual recipes. Recipes reference it but do not re-define it.
**When to use:** Every operation that mutates existing elements.
**Trade-off:** If safety rules need to be recipe-specific (e.g., surface assignment has different risk than geometry update), SKILL.md needs a caveat or the recipe overrides. The spec does not define recipe-level safety overrides — acceptable for v1.

---

## Data Flow

### Single-Element Create Flow

```
User request
    → SKILL.md: safety check (free for Create)
    → SKILL.md: warm-up check (Story, Layer, Units resolved)
    → recipes/<type>.md: Discovery-Anker → archicad_discover_tools
    → archicad_call_tool (create)
    → Capture element ID
    → Report to user
```

### Multi-Element Flow (cross-recipe dependency)

```
User request (multi-step)
    → SKILL.md: warm-up (once)
    → recipes/walls.md: create wall → element ID W1
    → recipes/openings.md: create window, host=W1 → element ID O1
    → recipes/surfaces-materials.md: discover "Klinker" attribute → assign to W1 [CONFIRM]
    → recipes/lines-polylines.md: create line parallel to W1
    → reference/self-improvement.md: reflection trigger
```

### Bulk-Classification Flow

```
User request (classify all walls)
    → SKILL.md: warm-up (Story, Classification System)
    → reference/bulk-operations.md: READ all walls (paginated)
    → reference/bulk-operations.md: FILTER by Story/Layer
    → reference/bulk-operations.md: GROUP → derive Innen/Außen per element
    → SKILL.md: asymmetric safety → CONFIRM (summary format for N > 10)
    → reference/bulk-operations.md: APPLY per element (continue on single error)
    → Report: success/fail IDs
```

---

## Anti-Patterns

### Anti-Pattern 1: Hardcoding MCP Tool Names in Recipes

**What people do:** Write `archicad_call_tool(name="CreateWall", ...)` as the literal step in a recipe.
**Why it's wrong:** Tool names change between Archicad versions. The MCP server's tool registry is the source of truth. Hardcoded names become stale silently.
**Do this instead:** Document names as "Typischer Tool-Name (Hinweis)" in the Discovery-Anker table and always run `archicad_discover_tools` first.

### Anti-Pattern 2: Splitting Files by Archicad UI Concept

**What people do:** Split `openings.md` into `windows.md` + `doors.md` because Archicad's UI has separate palettes for them.
**Why it's wrong:** The MCP interface treats them as the same element class (both are hosted on walls, same placement mechanism). The UI split does not map to the API split.
**Do this instead:** Keep the recipe boundary at the MCP element-class level, not the UI panel level.

### Anti-Pattern 3: Cross-Recipe File Dependencies

**What people do:** Have `openings.md` `import` or `include` content from `walls.md`.
**Why it's wrong:** Claude Code skills are plain Markdown files. There is no include mechanism. Cross-references must be runtime instructions ("see walls.md for wall element ID format") not structural dependencies.
**Do this instead:** SKILL.md defines the data-threading convention. Recipes reference it by instruction, not by file inclusion.

---

## Integration Points

| Boundary | Communication | Notes |
|----------|---------------|-------|
| SKILL.md → recipes/ | Claude loads recipe on demand based on task type | SKILL.md must contain a clear link map so Claude knows which recipe covers which element type |
| SKILL.md → reference/ | Claude loads on demand or per warm-up trigger | workflow-context.md loaded every task (warm-up); others as needed |
| recipes/ → reference/bulk-operations.md | Instruction reference ("see bulk-operations.md") | Not a file dependency — runtime navigation |
| recipes/ → reference/mcp-conventions.md | On error only | Reduces token load on happy path |
| Archicad MCP server | archicad_discover_tools + archicad_call_tool only | No direct API calls; all via MCP tool interface |

---

## Open Questions for Implementation

1. **Element-ID threading instruction** — SKILL.md needs an explicit rule: "Retain element IDs returned by Create operations within a task session; use as host/reference parameters in subsequent operations." Not in current spec. LOW risk to forget, HIGH cost if omitted.

2. **surfaces-materials.md Create scope** — PROJECT.md flags this as "unsicher." Until live-verified, the recipe's "Umfang" section should explicitly state: "Create: unverified (MCP may only support assignment). Document result on first live test." Do not block the recipe on this; document the uncertainty inline.

3. **Classification GUID mapping in bulk-operations.md** — The spec says Klartext → GUID mapping is done via Discovery. The actual MCP tool for this is unknown pre-live. Reserve a "Klassifikations-Discovery" subsection in `bulk-operations.md` as a placeholder, populated during step 15 (Integration Test 2).

4. **SKILL.md token budget** — The spec targets < 1500 tokens. With safety rules, warm-up checklist, discovery pattern, and a link map to 12 files, hitting 1500 tokens will be tight. Mitigation: move the confirm-format example (currently in spec as a block) to `reference/mcp-conventions.md` and keep only the rule in SKILL.md.

---

## Sources

- `/Users/ap/.claude/skills/archicad/.planning/PROJECT.md` — requirements, constraints, key decisions
- `/Users/ap/docs/superpowers/specs/2026-05-19-archicad-skill-design.md` — full design spec, bootstrap plan, coverage matrix

---

*Architecture research for: Archicad-MCP skill for Claude Code*
*Researched: 2026-05-19*
