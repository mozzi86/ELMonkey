# Pitfalls Research

**Domain:** Archicad-MCP Skill for Claude Code (MCP + BIM + Claude Code, live Archicad via mcp__archicad__*)
**Researched:** 2026-05-19
**Confidence:** See per-pitfall ratings

---

## Critical Pitfalls

### Pitfall 1: Discovery Ambiguity — Multiple Plausible Tool Matches

**Confidence:** HIGH

**What goes wrong:**
`archicad_discover_tools` returns 3–5 candidates for a query like "create wall". Claude picks the first or highest-scored match without reading the schema. The chosen tool is for reading wall types, not creating wall instances — elements are not created, or the wrong endpoint is called silently succeeding (returning a result that looks valid).

**Why it happens:**
The MCP discovery layer returns semantic matches by description similarity, not by operation type. "Wall" tools span create-instance, list-type, get-geometry, update-properties. Without schema inspection Claude cannot distinguish them from the name alone. The skill's spec says "semantically best match" but doesn't mandate schema validation before calling.

**How to avoid:**
- In `reference/mcp-conventions.md`: mandate that Claude always reads the **full schema** (parameters + return shape) of the top candidate before calling, not just the name and description.
- Require a tie-breaker rule: if two candidates share ≥70% name similarity, compare their required parameters — create tools will have geometry inputs (coordinates, length), read tools won't.
- Document that verbs in tool names are unreliable — "getWall" may in fact update; "wallInfo" may create a temporary selection.

**Warning signs:**
- Discovery returns more than 2 candidates for a single operation.
- Chosen tool has no geometry parameters for a spatial create.
- MCP call returns an ID but Archicad view shows nothing changed.

**Phase to address:**
FOUND-02 (`reference/mcp-conventions.md`) — must include a "disambiguation protocol" section before any Recipe work begins.

---

### Pitfall 2: Schema Drift Between Archicad / MCP Plugin Versions

**Confidence:** HIGH

**What goes wrong:**
A Recipe documents `"startPoint": {x, y}` as the wall parameter, but after the user upgrades the MCP plugin the parameter is renamed to `"beginPoint"` or the coordinate system changes from absolute to story-relative. Claude uses the cached Recipe schema, the call fails with `invalid argument`, and if Claude follows the current fallback ("retry with synonyms") it may hit an unrelated tool instead of re-discovering the correct schema.

**Why it happens:**
Archicad MCP is an actively developed plugin (the spec notes a known version mismatch: gsd-sdk v1.50.0-canary vs installer v1.42.3). Recipes document "typical parameters (hint character)" but the hint can become stale without a version bump marker. The skill has no mechanism to detect schema drift at session start.

**How to avoid:**
- Add a **version fingerprint** to Warm-up: call `discovery_list_active_archicads` and store the reported plugin version; if it differs from the version in `reference/workflow-context.md`, surface a "schema may have drifted — re-verify Discovery Anchors" warning.
- In each Recipe's Discovery-Anker table, add a `Verified with plugin vX.Y` column.
- The error-class table in FOUND-02 must distinguish `invalid argument` (schema drift → re-discover) from `tool not found` (name drift → retry synonyms).

**Warning signs:**
- `invalid argument` errors on a previously-working Recipe.
- MCP plugin version in `discovery_list_active_archicads` response differs from version in `workflow-context.md`.
- Multiple consecutive `invalid argument` errors across different tools (suggests coordinate system change, not a one-off).

**Phase to address:**
FOUND-02 (error-class handling) + FOUND-03 (version field in `workflow-context.md`) + each Recipe's Discovery-Anker table at authoring time.

---

### Pitfall 3: Stale Warm-Up Context — Cached Story After User Switched Stories

**Confidence:** HIGH

**What goes wrong:**
Claude runs Warm-up at task start, caches Story ID = "EG" (Erdgeschoss). Mid-conversation the user manually switches the active story in Archicad to "OG1". Claude continues creating elements with Story ID "EG". Elements land on the wrong floor. Because Create is free (no confirm), this is not caught before execution. The model is now geometrically wrong in a way that may require manual cleanup of dozens of elements.

**Why it happens:**
The spec says context is "cached for the duration of one order (Auftrag)". But a long multi-step order can span user actions in Archicad. There is no event subscription or polling mechanism in the current MCP architecture — the skill has no way to be notified of user-side state changes.

**How to avoid:**
- Define "Auftrag" strictly: any user message that introduces new geometry scope (different story name, "on floor X", "on level Y") must trigger a **partial re-warm-up** (re-fetch active story only).
- In SKILL.md Warm-up section: add rule "if the user mentions a story by name, re-verify current story via Discovery before proceeding — do not rely on cached value".
- In `reference/workflow-context.md`: flag Story as a **volatile field** (re-verify per geometry operation) vs. stable fields (units, project name) that only need session-level caching.

**Warning signs:**
- User uses story names explicitly in their request ("auf OG1", "in the first floor").
- The request comes more than ~5 minutes after initial Warm-up (heuristic for long sessions).
- Create operations immediately follow a story-name mention without a re-verify step.

**Phase to address:**
FOUND-03 (`reference/workflow-context.md` — volatile vs. stable field classification) + SKILL.md Warm-up section.

---

### Pitfall 4: Layer-Not-Visible But Still Created

**Confidence:** HIGH

**What goes wrong:**
Claude creates a wall on Layer "Wände-Bestand" which is currently invisible in the user's view. The MCP call succeeds (returns element ID), Archicad stores the element, but the user sees nothing. User assumes the creation failed and asks Claude to retry — creating a duplicate element on the same invisible layer. The duplicates are only discovered when the layer is turned back on.

**Why it happens:**
Create is defined as "free" (no confirm). The skill fetches visible layers at Warm-up but does not enforce that the target layer must be visible. The user may have hidden a layer for display purposes while still intending to add elements to it — but more commonly this is a silent mistake.

**How to avoid:**
- Add a **layer visibility pre-check** to all Create operations: before executing, verify that the target layer is in the visible set from Warm-up (or re-fetched if Warm-up was >5 min ago).
- If the layer is invisible, **pause and confirm** even though Create is nominally free: "Layer X ist derzeit ausgeblendet — trotzdem erstellen?" This is the one justified exception to the free-Create rule.
- Document this exception explicitly in SKILL.md's safety section: "Create is free *unless* the target layer is currently not visible."

**Warning signs:**
- Target layer not in the visible layers list from Warm-up.
- User says "Ich sehe nichts" immediately after a reported-successful Create.
- User asks to "do it again" after a successful MCP create response.

**Phase to address:**
FOUND-01 (SKILL.md safety section exception) + FOUND-03 (volatile layer visibility field) + WALL-01 and all other Recipe Create sections.

---

### Pitfall 5: Classification GUID Mapping Failure (Klartext → GUID)

**Confidence:** HIGH

**What goes wrong:**
The bulk classification workflow calls `classify wall as "Innenwand"`. The MCP expects a GUID, not a string label. Claude attempts a discovery query "classification Innenwand" and gets back a GUID — but the GUID belongs to the **wrong classification system** (e.g., OmniClass instead of the project's active Uniclass system). The classification is applied silently with the wrong system's GUID, corrupting the BIM data.

**Why it happens:**
Archicad supports multiple classification systems simultaneously. Discovery does not filter by active system unless explicitly queried with the system ID. The spec mentions "Klassifikations-IDs sind GUIDs, kein Klartext. Mapping Klartext→GUID per Discovery" but doesn't specify that the discovery query must be scoped to the **active system's GUID** obtained in Warm-up step 7.

**How to avoid:**
- In `reference/bulk-operations.md`: the GROUP step must **always** include the active classification system GUID (from Warm-up step 7) as a filter parameter when looking up item GUIDs by name.
- In FOUND-03: document the exact Discovery query pattern for system-scoped GUID lookup.
- If Discovery returns matches in multiple systems, always prefer the system matching the Warm-up-fetched active system GUID, and reject the others silently — never merge across systems.

**Warning signs:**
- Classification lookup returns multiple GUIDs for the same label string.
- Warm-up step 7 was skipped or returned "system not found."
- Post-apply verification shows classification system name doesn't match expected active system.

**Phase to address:**
FOUND-04 (`reference/bulk-operations.md` — GROUP step scoping) + CLASS-01 (per-Recipe bulk classification sections).

---

### Pitfall 6: Hosted-Element Orphaning on Host Delete

**Confidence:** HIGH

**What goes wrong:**
User asks to delete a wall. Claude confirms the delete with ID enumeration (correct). The wall is deleted. Windows and doors hosted in that wall are now unhosted orphans — they exist as floating elements in space with no wall geometry, invisible in normal views but present in the model. This corrupts IFC exports and room calculations even though it's "out of scope" in v1.

**Why it happens:**
The asymmetric safety model requires ID enumeration for delete, but only for the **directly targeted element**. Hosted elements (Fenster, Türen, Wandöffnungen) have a parent-host relationship that the current confirm format doesn't surface. The spec defines Delete confirm at the element level with no mention of cascading dependencies.

**How to avoid:**
- Add a **dependency pre-check** to all Delete operations: before the confirm dialog, query hosted elements for the target (e.g., "get openings in wall ID X").
- Extend the Delete confirm format: "Ich werde löschen: Wand 0x1A2B — enthält 2 Fenster (0xFF01, 0xFF02) und 1 Tür (0xFF03). Diese werden ebenfalls gelöscht/orphaned."
- In `reference/mcp-conventions.md`: document host-dependency queries for each element type that can be a host (walls → openings; slabs → ?).
- Add to `recipes/walls.md` Gotchas and `recipes/openings.md` Gotchas.

**Warning signs:**
- Delete target is a wall (most common host element).
- Warm-up or prior context reveals the project has openings on the targeted layer/story.
- User deletes "all walls on layer X" in a bulk delete — almost certainly affects hosted elements.

**Phase to address:**
FOUND-02 (host-dependency query pattern in `mcp-conventions.md`) + WALL-01 (walls Delete section) + OPEN-01 (openings context).

---

### Pitfall 7: Partial-Batch Failure Without Rollback — Silent Inconsistency

**Confidence:** HIGH

**What goes wrong:**
Bulk classification applies to 200 walls. After wall 147, the MCP returns an error (e.g., "element locked" or timeout). The skill's spec says: "Bei Einzel-Fehler: weitermachen, am Ende Report." So 146 walls get classified correctly, 54 don't. The user sees "54 Fehler" in the end report but is busy, acknowledges it, and moves on. The model now has a split classification state — some walls have correct classifications, some have old/no classifications — and there is no mechanism to re-run only the failed subset.

**Why it happens:**
The current spec handles this correctly in principle (continue + report), but doesn't specify the **resume format**: after a partial failure, Claude must preserve the failed-ID list in a format the user can re-submit. Without this, the user has to manually identify and re-run, which defeats the bulk workflow value.

**How to avoid:**
- In `reference/bulk-operations.md`: define the end-of-batch failure report format explicitly — it must include: (a) success count, (b) failure count, (c) **retry-ready command** ("Erneut versuchen mit IDs: [list]"), (d) failure reason per ID.
- Add a "retry failed elements" pattern as a named workflow variant in bulk-operations.md.
- The Confirm step summary should note that partial failure is possible so users aren't surprised.

**Warning signs:**
- Batch contains elements from multiple layers or stories (higher chance of locked/inaccessible elements).
- Element count > 50 without prior verification that all elements are editable.
- User is working in Archicad simultaneously (see Pitfall 8).

**Phase to address:**
FOUND-04 (`reference/bulk-operations.md` failure-report format + retry pattern).

---

### Pitfall 8: Race Condition — User Editing Live During Bulk Apply

**Confidence:** MEDIUM

**What goes wrong:**
Claude is mid-bulk classification (applying to element 80 of 200). User, also working in Archicad, selects and moves wall 0x1A9F (which Claude hasn't processed yet). Claude's subsequent `update classification` call for 0x1A9F either conflicts with the user's active selection lock, or — worse — succeeds but the user's move then overwrites geometry while Claude's classification is applied, creating an element with classification from Claude and geometry from user's edit but in an inconsistent combined state.

**Why it happens:**
Archicad's MCP plugin does not expose a "lock element" or "transaction" API. Claude cannot claim exclusive access. The spec acknowledges no Auto-Rollback (Undo is user's tool) but doesn't address concurrent editing at all.

**How to avoid:**
- In SKILL.md: add a "Concurrent Editing Warning" to the Bulk Operations section — Claude should explicitly state at Confirm time: "Diese Operation verändert N Elemente. Bitte Archicad während der Ausführung nicht manuell bearbeiten."
- In `reference/bulk-operations.md`: document the "single-user assumption" explicitly — this skill is designed for single-user Archicad sessions (Teamwork is out of scope).
- After any bulk apply, recommend an "Ansicht aktualisieren" (view refresh) in Archicad to surface any conflicts.

**Warning signs:**
- User says "ich mache gerade noch schnell X" during a multi-step bulk operation.
- `archicad_call_tool` returns a "element in use" or "locked" error mid-batch.

**Phase to address:**
SKILL.md (concurrent editing warning) + FOUND-04 (`reference/bulk-operations.md` single-user assumption section).

---

### Pitfall 9: Self-Improvement Hallucination Feedback Loop

**Confidence:** MEDIUM

**What goes wrong:**
Reflection-Trigger at task end: user says "ja, der Tool-Name für Wände ist `archicad_create_wall_element`." Claude writes this to `recipes/walls.md` Discovery-Anker without re-verifying via live Discovery. If the user mis-remembered or mis-typed the name, the Skill now permanently contains a wrong tool name. Next session, Claude uses the "learned" wrong name, gets a `tool not found` error, retries with synonyms, discovers the correct tool, but the Skill file still has the wrong entry — which gets used again next session.

**Why it happens:**
The Self-Improvement spec says "Keine Halluzinationen ohne Live-Beleg" but doesn't define what constitutes a live-beleg specifically for user-provided information (vs. Claude's own discovery). User-provided tool names feel like facts but can be mis-recalled, especially after a long session.

**How to avoid:**
- In `reference/self-improvement.md`: add a rule — **any tool name or parameter name added via Reflection-Trigger must be immediately verified** by running a discovery query confirming it exists before writing to the Skill file.
- Format: `<!-- user-provided, unverified 2026-05-19 -->` vs. `<!-- live-verified 2026-05-19 -->` markers.
- The Verification-Loop (2 failures → ÜBERPRÜFEN) partially catches this but only after repeated failures; the pre-write verification prevents the bad entry from ever persisting.

**Warning signs:**
- User provides a very specific tool name with confidence ("it's definitely called X").
- The provided name doesn't appear in a discovery query for the described operation.
- Verification-Loop marker `ÜBERPRÜFEN` appears on an entry that was user-provided.

**Phase to address:**
FOUND-05 (`reference/self-improvement.md` — verification-before-write rule).

---

### Pitfall 10: Asymmetric Safety Bypass via Create-Then-Delete Chain

**Confidence:** MEDIUM

**What goes wrong:**
The asymmetric model makes Delete require explicit confirm. But Claude could be prompted to: (1) create a "replacement" element with updated properties [free], then (2) delete the "old" element [nominally requires confirm]. If the user's request is phrased as "replace this wall with a new one that has X", Claude may collapse both steps into a Create+Delete sequence where the Delete confirm is presented as part of a create-replace flow, making it feel lower-stakes than a standalone delete.

More subtly: "Update the height of wall X" — Claude could implement this as Delete + Create-at-same-location rather than a true Update, bypassing the Update confirm entirely since Create is free.

**Why it happens:**
The safety model is defined at the operation level but not at the **semantic intent** level. The spec doesn't prohibit Create+Delete as an implementation of Update. Without this rule, Claude may naturally choose Create+Delete when Update's MCP interface is more complex.

**How to avoid:**
- In SKILL.md asymmetric safety section: add rule — "Never implement an Update or Delete as a Create+[Delete|overwrite] sequence. Use the Update MCP endpoint directly. If no Update endpoint exists for a property, surface this to the user rather than destroying and recreating."
- In `reference/mcp-conventions.md`: document that Replace = Delete + Create is **always** a confirm-level operation regardless of how it's decomposed.

**Warning signs:**
- Claude describes a plan as "ich erstelle eine neue Wand und lösche die alte."
- A "create" call immediately precedes a "delete" call on the same element type in the same location.

**Phase to address:**
FOUND-01 (SKILL.md safety rules) + SAFE-01 (asymmetric model enforcement throughout recipes).

---

### Pitfall 11: Pagination Cut-off in Bulk Read — Silent Incomplete Dataset

**Confidence:** HIGH

**What goes wrong:**
Claude runs "get all walls in project" for a bulk classification. The MCP returns page 1 of 3 (say, 100 of 300 walls). Claude processes only the 100 walls without noticing pagination tokens. The bulk classification appears to succeed ("100 Wände klassifiziert") but 200 walls remain unclassified. Because there's no count validation against "total elements" metadata, the silent truncation is never surfaced.

**Why it happens:**
The spec's error-class table mentions "Paginierung: `next_page_token` durchziehen bis fertig" but this is easy to skip in practice — the first page's result is a valid-looking list, and without a "total count" field in the response, Claude has no signal that it's incomplete.

**How to avoid:**
- In `reference/mcp-conventions.md` and `reference/bulk-operations.md`: mandate a **pagination exhaustion loop** as required (not optional) for all bulk-read operations — never process partial pages.
- After pagination: always compare the collected count against any `totalCount` field in the response; if such a field exists and collected < total, treat as an error.
- In the Confirm step: always state the total collected count ("N Wände gelesen, X Seiten") — the user can catch obvious truncations.

**Warning signs:**
- First bulk-read response contains a `next_page_token` or `hasMore` field.
- Collected element count is a suspiciously round number (100, 50, 200) — often a default page size.
- User says "das sind nicht alle Wände, wir haben mehr."

**Phase to address:**
FOUND-02 (`reference/mcp-conventions.md`) + FOUND-04 (`reference/bulk-operations.md` READ step spec) — must be addressed before any bulk Recipe work (pre-CLASS-01).

---

### Pitfall 12: Composite vs. Single Material Confusion in Surface Assignment

**Confidence:** MEDIUM

**What goes wrong:**
User asks to assign "Mauerwerk Klinker" to a wall. The wall uses a Composite (multi-layer building material structure, e.g., Klinker + Dämmung + Beton) not a single Surface. Claude discovers "Mauerwerk Klinker" as a Surface and assigns it directly to the wall's surface override, visually changing the appearance — but the underlying Building Material Composite (which drives area calculations, thermal properties, and IFC Layer semantics) is unchanged. The model looks correct but BIM data is wrong.

**Why it happens:**
Archicad has three overlapping concepts: Surface (visual/render), Building Material (physical/structural, with fill, pen, classification), and Composite (ordered stack of Building Materials). The MCP exposes tools for each but the user's language ("Mauerwerk Klinker zuweisen") doesn't disambiguate. The spec flags this as "live verifizieren" but doesn't document a disambiguation protocol.

**How to avoid:**
- In `recipes/surfaces-materials.md`: document the three-way disambiguation as a required pre-step: "Ist es (a) ein visueller Surface-Override, (b) ein Building Material Wechsel am Composite-Layer, oder (c) ein Composite-Wechsel?" — and ask the user if ambiguous.
- In SKILL.md: Surface/material changes require this disambiguation question before any Discovery step.
- Never assign a Surface where a Building Material or Composite was expected — they are not interchangeable via MCP.

**Warning signs:**
- User says "weise zu" or "setze Material" without specifying Surface vs. Building Material.
- Target element has a Composite structure (discoverable via element property query).
- User's follow-up is "aber die Berechnungen stimmen nicht."

**Phase to address:**
SURF-01 (`recipes/surfaces-materials.md` disambiguation section) — must precede any surface/material CRUD work.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Hardcoding tool names in Recipes instead of documenting as hints | Fewer Discovery calls per session | Names drift with plugin updates, skill breaks silently | Never — all names must be discovery-verified hints only |
| Skipping Warm-up steps 5–7 for "simple" read requests | Faster response | Wrong layer context applied to inadvertent side-effect creates | Never — Warm-up is atomic |
| Writing user-provided tool names directly to Skill without verification | Captures user knowledge quickly | Hallucinated/mis-recalled names pollute Discovery Anchors | Never — always verify before write (Pitfall 9) |
| Using page 1 only for reads (assuming small projects) | Simpler code path | Silent dataset truncation in larger projects (Pitfall 11) | Never — pagination must always be exhausted |
| Treating Classification as a one-shot operation without system-scoping | Faster bulk apply | Cross-system GUID corruption (Pitfall 5) | Never once a project has multiple classification systems |
| Storing project-specific classification strings in Skill files | Convenient reference | Skill becomes project-coupled, not reusable | Never — project-specifics go to Memory only |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| `archicad_discover_tools` | Treating the top match as correct without schema inspection | Always read full schema of top candidate; apply tie-breaker on geometry parameters |
| `archicad_call_tool` with `port` parameter | Using a hardcoded or previously-seen port | Always fetch live port from `discovery_list_active_archicads` at session start |
| Classification GUID lookup | Querying by label string without system scoping | Always scope to active classification system GUID from Warm-up step 7 |
| Hosted element delete (wall with openings) | Deleting host without checking for hosted children | Pre-query hosted elements; include in confirm dialog |
| Composite structure surface assignment | Assigning Surface where Building Material is expected | Disambiguate three-way Surface/Building Material/Composite before any assign call |
| Multi-page MCP responses | Processing only first page | Exhaustive pagination loop — never assume single page is complete |
| Story-scoped element creation | Using cached story ID from session start | Re-verify active story on any user mention of a floor/level or after >5 min elapsed |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Fetching all elements of all types at session start for "context" | 10–30 second warm-up, token bloat | Per-operation lazy Discovery — only fetch what the current operation needs | Projects >500 elements |
| Full re-discovery of tool names every call instead of caching per session | Redundant MCP calls, slow multi-step workflows | Cache verified tool names within a session; re-verify only on `invalid argument` error | Not a hard limit but degrades UX immediately |
| Pagination without batch-size awareness | Very slow bulk reads on large projects | Pass explicit page-size parameter if MCP supports it; process pages as they arrive rather than collecting all before acting | Projects >1000 elements per type |
| Requesting full geometry of all elements during classification | Unnecessary data transfer for classification use case | For classification, request only ID + layer + bounding box + story — not full geometry | Projects >200 walls |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Storing project-specific element IDs or layer names in Skill files | Skill becomes a project-specific artefact exposing project data structure; breaks on different projects | Project facts → Memory only; Skill files contain only patterns and hint names |
| Applying bulk classification without scoping to active system | Cross-system GUID pollution corrupts BIM data irreversibly (no undo for classification data in some MCP versions) | Mandatory system-scoping in bulk-operations.md GROUP step |
| Implementing Update as Delete+Create without confirm | Permanent element loss disguised as an update | Explicit rule in SKILL.md: never decompose Update into Delete+Create |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Reporting "N Elemente klassifiziert" without failure details | User doesn't know 54 walls have wrong/no classification | Always report success + failure counts + retry-ready command |
| Confirm dialog shows element IDs without readable names | User cannot verify which walls are being changed | Always include human-readable identifier (name, layer, story) alongside ID in confirm |
| "Are you sure?" prompt buried in a long explanation | User may miss it or click yes reflexively | Confirm dialog must be the last thing before action, unambiguous, not embedded in a paragraph |
| Asking Lern-Check question even on trivial read-only sessions | Annoying friction | Lern-Check only if the session included a Create, Update, Delete, or Classification operation |

## "Looks Done But Isn't" Checklist

- [ ] **Discovery-Anker table:** Has a `Verified with plugin vX.Y` column — without this, schema drift is invisible.
- [ ] **Bulk-operations READ step:** Explicitly loops `next_page_token` to exhaustion — not just "call the read tool."
- [ ] **Classification GUID lookup:** Scoped to active system GUID from Warm-up step 7 — not a global string search.
- [ ] **Delete confirm:** Includes hosted-element dependency pre-check — not just the direct target IDs.
- [ ] **Create operation:** Includes layer visibility pre-check — layer being invisible is an exception to free-Create.
- [ ] **Self-improvement entries:** Have `<!-- live-verified YYYY-MM-DD -->` or `<!-- user-provided, unverified -->` markers — not bare entries.
- [ ] **Batch failure report:** Includes retry-ready command with failed IDs — not just a count.
- [ ] **Story field:** Marked as volatile in `workflow-context.md` with re-verify trigger conditions — not treated as session-stable.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Wrong story assignment (Pitfall 3) | MEDIUM | Query elements by ID list; move to correct story via bulk Update; if IDs unknown, filter by creation-time proximity |
| Duplicate elements from invisible layer (Pitfall 4) | LOW | Turn layer on; select duplicates by ID from prior session context; Delete with confirm |
| Wrong classification system GUID (Pitfall 5) | HIGH | Re-run full classification workflow scoped to correct system; may require manual verification of each element if system metadata was overwritten |
| Orphaned hosted elements from host delete (Pitfall 6) | MEDIUM | Query "unhosted" or floating openings; delete orphans with confirm; or manually re-host in Archicad |
| Partial batch inconsistency (Pitfall 7) | LOW | Use failure report's retry-ready command; confirm retry scope; re-apply to failed IDs only |
| Bad tool name in Skill from unverified self-improvement (Pitfall 9) | LOW | Remove entry, run Discovery to get correct name, re-write with live-verified marker |
| Schema drift causing systematic failures (Pitfall 2) | MEDIUM | Version bump detected → full re-verification pass of affected Recipe's Discovery-Anker table |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| P1 — Discovery ambiguity | FOUND-02 (disambiguation protocol) | Discovery for wall create returns ≥3 matches; schema tie-breaker rule applied correctly |
| P2 — Schema drift | FOUND-02 + FOUND-03 (version fingerprint) | Warm-up logs plugin version; error class table distinguishes drift from name-not-found |
| P3 — Stale story context | FOUND-03 (volatile field classification) | Story re-verify triggers on any story-name mention in user request |
| P4 — Invisible layer create | FOUND-01 (safety exception) + FOUND-03 | Create on invisible layer produces pause-and-confirm, not silent proceed |
| P5 — Classification GUID cross-system | FOUND-04 (GROUP step scoping) + CLASS-01 | Bulk classification query explicitly includes active system GUID as filter |
| P6 — Hosted element orphaning | FOUND-02 (host-dependency queries) + WALL-01 | Wall delete pre-check returns hosted elements; confirm dialog lists them |
| P7 — Partial batch silent inconsistency | FOUND-04 (failure report format) | End-of-batch report includes retry-ready command with failed IDs |
| P8 — Race condition concurrent edit | SKILL.md + FOUND-04 (single-user warning) | Confirm dialog includes concurrent-edit warning; INT-01 test verifies message |
| P9 — Self-improvement hallucination | FOUND-05 (verify-before-write) | Any tool name written via Reflection-Trigger has live-verified marker |
| P10 — Create+Delete safety bypass | FOUND-01 (SKILL.md rule) + SAFE-01 | Recipe Update sections use Update endpoint; no Create+Delete pattern present |
| P11 — Pagination cut-off | FOUND-02 + FOUND-04 (pagination mandate) | Bulk read in INT-02 test processes all 3 pages of a large dataset |
| P12 — Composite vs Surface confusion | SURF-01 (disambiguation pre-step) | Surface assign always asks three-way disambiguation; INT-01 test covers material assignment |

## Sources

- PROJECT.md (`/Users/ap/.claude/skills/archicad/.planning/PROJECT.md`) — Known Issues section, Constraints, Out of Scope
- Design spec (`/Users/ap/docs/superpowers/specs/2026-05-19-archicad-skill-design.md`) — Bulk-Operations spec, Error Classes, Self-Improvement spec, Asymmetric Safety spec
- Archicad API / MCP domain knowledge: Archicad's tripartite material model (Surface / Building Material / Composite), hosted element host-child relationships, classification system GUID architecture, story-based coordinate system, layer visibility vs. layer lock semantics
- General MCP reliability patterns: discovery ambiguity, schema drift in versioned plugins, pagination contracts
- Claude Code skill architecture patterns: self-improvement feedback loops, cache invalidation in long sessions

---
*Pitfalls research for: Archicad-MCP Skill (MCP + BIM + Claude Code)*
*Researched: 2026-05-19*
