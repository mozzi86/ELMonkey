# Feature Research

**Domain:** Archicad-MCP skill for Claude Code — feature gap analysis
**Researched:** 2026-05-19
**Confidence:** Mixed (noted per entry)

---

## Scope of This Document

The spec covers 8 recipe files (walls, openings, slabs/columns/beams, curtain walls, library objects,
surfaces/materials, fills, lines/polylines) with full CRUD + bulk classification + self-improvement.
This document identifies what a **practicing architect** would expect that the spec does NOT address.
Items the spec already handles are NOT repeated.

---

## Feature Landscape

### Table Stakes (Architect-Essential — Should Be Added to v1)

Features a daily Archicad user would reach for in the first week of using the skill.
Missing these makes the skill feel half-done.

| Feature | Why Expected | Complexity | Confidence | Notes |
|---------|--------------|------------|------------|-------|
| **Zone / Room creation & properties** | Architects place zones daily for area schedules, GFA calculation, room naming on plans. No zones = no area data from Claude. | MEDIUM | HIGH | Zones are a first-class Archicad element type with their own tool. Properties: name, number, category, story assignment, area (read-only from geometry). MCP likely has a `createZone` or `createElement(type=zone)` path. |
| **Story management: read all stories, set story height** | Nearly every task that involves elevation is story-relative. The warm-up reads the *active* story but there is no recipe for listing all stories, reading their elevations, or changing a story's height. Multi-story projects need this constantly. | LOW | HIGH | Archicad stories have index, name, elevation, height. Read is trivial. Update story height is a structural change — needs confirm. Create/delete story is rare but should be documented as out-of-scope for v1. |
| **Element story / layer re-assignment (bulk)** | "Move all elements on Layer X to Layer Y" or "push these walls from EG to OG" are everyday ops when reorganising a project. The spec's bulk-operations covers classification but not layer migration or story-shift. | MEDIUM | HIGH | This is the "layer migration" and "level shift" asked about in the research question. Layer reassignment = property update → confirm. Story-shift = geometry/host change → confirm. Both fit the existing bulk pattern. |
| **Read element properties / quantities** | Before an architect asks Claude to *change* anything, they ask "what do I have?" — counts by type per story, total wall area, slab area, window schedule. No read-only analytics recipe exists. | LOW | HIGH | Pure read = free (no confirm). Minimal MCP overhead. High utility for pre-bulk-op scouting. |
| **Attribute listing: layers, surfaces, composites, fills, line types, pen sets** | Architects constantly need to know attribute IDs before assigning them to elements. The recipes reference attributes by name but there is no recipe for listing/querying the attribute catalog. | LOW | HIGH | Without this, every assignment op has a discovery detour. A `reference/attributes.md` or inline section in relevant recipes would suffice. |
| **Opening sub-operation: sill height & head height** | Placing a window with correct sill height (e.g., 0.90 m) is the single most-requested window parameter after width and height. The spec says "Fenster/Türen/Wandöffnungen, CRUD vollständig" but does not call out sill height, head height, or the relationship between them. | LOW | HIGH | In Archicad these are parameters of the opening/window element. Sill height is separate from the wall's bottom elevation. Must be explicit in openings.md. |
| **Wall sub-operation: composite / building material assignment** | Walls in real projects have composites (Mehrschichtaufbau), not just a single surface. Assigning or changing the composite is a daily operation. The spec mentions surfaces/materials but does not explicitly cover the wall's *composite structure* property. | MEDIUM | HIGH | `composite` is a wall property, distinct from a surface. MCP likely exposes it as a parameter on wall update. Should be explicit in walls.md. |
| **Wall sub-operation: wall priority / intersection control** | "Why is my wall junction not cleaning up?" — wall priority (Wandvorrang) is the parameter architects adjust to fix intersection rendering. Spec does not mention it. | LOW | MEDIUM | Archicad assigns numeric priority per building material; Claude can read and set it. Confidence MEDIUM because MCP exposure is unverified. |

---

### Differentiators (V2 Candidates)

Features that would make the skill significantly more powerful than basic MCP scripting,
but are not required to deliver daily value in v1.

| Feature | Value Proposition | Complexity | Confidence | Notes |
|---------|-------------------|------------|------------|-------|
| **Zone analytics: area by story / use type** | Generate a room-by-room area schedule from scratch — GFA, NFA, net area per zone category. Architects produce these for clients and authorities. | MEDIUM | HIGH | Builds on Zone creation (table stakes). Read-only once zones exist. Could produce a markdown table the user pastes into a report. |
| **Mesh / terrain element** | Site modelling (Geländemodell) uses the Mesh tool. Architects do site plans. | HIGH | MEDIUM | Mesh geometry (triangulated surface with control points) is structurally different from parametric elements. MCP support is unverified. Defer. |
| **Morph element** | Morph is Archicad's free-form solid. Used for complex massing, furniture, or bespoke geometry. Rare in daily production work but powerful. | HIGH | MEDIUM | Morph creation/editing via MCP is unverified. Likely requires point-array geometry input. High complexity, low daily frequency. |
| **Stair & railing** | Stairs (Treppe) are a dedicated Archicad element type with their own rule-based geometry engine. Railings attach to stairs or slabs. | HIGH | HIGH | The stair tool in AC29 uses a multi-step rule-based dialog even in the GUI. MCP exposure of the full parameter set is uncertain. Significant complexity; defer. |
| **Dimension strings (Bemaßung)** | Placing linear/angular/radial dimensions. The spec explicitly lists this as out-of-scope, but it's a daily 2D workflow. | MEDIUM | HIGH | Already out-of-scope in PROJECT.md. Keep out. |
| **Label / annotation placement** | Placing text labels and element labels (Beschriftungen) on plan. | MEDIUM | HIGH | Also already out-of-scope. Keep out. |
| **Hotlink / XREF management** | Hotlinks embed one AC project in another (like XREFs). Architects use them for repeated units (Wohnungstypen), site context, or consultant models. | MEDIUM | MEDIUM | Read (list hotlinks, check update status) is low-risk and useful. Write operations (update, relink) touch file I/O and need careful safety model. V2. |
| **Sweep-replace (element type conversion)** | Replace all instances of one library object (e.g., a chair type) with another. This is a mass-property-update that goes beyond classification — it changes the element's GDL object reference. | HIGH | LOW | MCP support for object-reference replacement unverified. Potentially achievable via delete+recreate pattern, but that loses IDs and element links. Flag as aspirational. |
| **Story duplication** | Copy a complete story (all elements) and offset it by one story height — useful when floors repeat. | HIGH | LOW | No standard MCP operation for this; would require read-all-elements + bulk-create with shifted coordinates. Complex. Low confidence on MCP support. |
| **Attribute remapping (bulk surface / composite change)** | "Replace all uses of Surface A with Surface B across the project." Bulk property update filtered by current attribute value. | MEDIUM | MEDIUM | Technically feasible with the existing bulk pattern (read→filter by current value→update). Not in spec, but architecturally fits. V1.x candidate if bulk-operations.md is extended. |
| **Clash detection lite** | Report elements that geometrically overlap — e.g., a beam penetrating a slab. Read-only. | HIGH | LOW | Requires spatial intersection logic. MCP probably does not expose this natively; would need bounding-box heuristics. Confidence LOW. Defer. |
| **IFC property sets (read/write)** | Reading and writing IFC properties (Psets) on elements is increasingly required for BIM Level 2 delivery. | HIGH | HIGH | Archicad stores IFC data alongside classifications. Already listed as out-of-scope (IFC import/export), but Pset read/write is distinct from file I/O. Clarify boundary in v2 planning. |

---

### Anti-Features (Explicitly Out of Scope)

Operations the skill should deliberately NOT do despite seeming useful.
These extend the PROJECT.md out-of-scope list with the reasoning why.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **Auto-save / project save** | Architect wants changes committed to disk after bulk ops. | Saving mid-session can corrupt Teamwork reservations and bypasses the user's own save workflow. Archicad's undo stack is also cleared selectively on save. | Tell user "changes are in memory — save manually with Cmd+S when ready." |
| **Undo execution** | After a bad batch, user wants Claude to undo. | Archicad's undo stack is sequential and shared with manual edits. Claude cannot know what's on the stack. Executing undo programmatically could silently reverse unrelated user edits. | The asymmetric confirm model prevents the need. For recovery: tell user to use Cmd+Z or File → Revert. |
| **Layer create / delete** | Seems like a natural attribute operation. | Layer creation changes the project template structure; layer deletion can orphan elements silently. Both require careful project-level coordination. | Cover layer *listing* and element *reassignment* between existing layers. Treat create/delete as out-of-scope. |
| **Story create / delete** | "Add a new floor" sounds simple. | Story creation requires coordinating slab placement, stair extension, and roof reassignment — far beyond a single MCP call. Story deletion with elements present is destructive without a clear safety model. | Cover story *reading* and *height update* only. |
| **Batch delete without explicit ID list** | "Delete all walls on Layer X" feels efficient. | Bulk delete based on filter criteria (not explicit IDs) is a category-level wipe. One wrong filter destroys work silently. | The confirm+ID-enumeration pattern already mitigates this for explicit IDs. Never skip the ID list step, even for large batches. |
| **Clipboard operations (copy/paste)** | Architects copy/paste elements between stories or files. | Clipboard is a GUI-level concept not exposed via MCP. Emulating it via read+create loses all element relationships (wall-window host links, zone memberships). | Use bulk-create from coordinates for story duplication; document the limitation. |
| **View / layer combination switching** | "Switch to the structural layer combination" sounds like context setup. | Layer combination changes affect *what the user sees* immediately and can hide elements during an ongoing operation, causing confusion. | Warm-up reads visible layers passively; do not set them. Teach user to set layer combinations manually before invoking the skill. |
| **GDL parameter scripting** | Power users want Claude to write GDL for custom objects. | GDL is a programming language with its own toolchain and debugging workflow. Mixing modelling and development scopes creates an unmaintainable skill boundary. | Already in PROJECT.md out-of-scope. Reinforce: library *usage* (placement) yes; library *development* (authoring) no. |

---

## Sub-Operation Gaps Within the 8 Covered Element Types

Specific parameter-level gaps the spec does not address per recipe.

### walls.md gaps
- **Composite assignment** (which Mehrschicht-Verbund the wall uses) — HIGH confidence it's a core wall property
- **Wall priority** (numeric value governing which material wins at intersections) — MEDIUM confidence MCP exposes it
- **Offset from home story** (wall bottom elevation relative to story) — HIGH confidence, needed for sloped sites
- **Reference line position** (which face of the wall the reference line sits on: inside/outside/center) — HIGH confidence, affects placement accuracy
- **Wall end conditions / caps** (open end, square cap, angled) — MEDIUM confidence on MCP support

### openings.md gaps
- **Sill height** (explicit parameter, not just total height) — HIGH confidence, architect-essential
- **Head height** (derived or independent) — HIGH confidence
- **Reveal depth / wall offset** — MEDIUM confidence on MCP support
- **Door swing direction** (left/right, inward/outward) — HIGH confidence, required for accessibility and scheduling
- **Window type / subtype** (casement, fixed, tilt-and-turn) controls GDL parameters — MEDIUM

### slabs-columns-beams.md gaps
- **Slab edge profile** (the edge detail seen in section/3D) — MEDIUM confidence on MCP support; architect expects it
- **Slab offset from home story** (top or bottom of slab relative to story) — HIGH confidence
- **Beam cross-section profile** (rectangular vs. I-beam vs. custom) — HIGH confidence, structural drawings depend on this
- **Column profile / shape** (circular vs. rectangular vs. custom) — HIGH confidence
- **Beam/column connection to slab** (trim/extend to slab face) — LOW confidence on MCP support; may require geometry logic

### curtain-walls.md gaps
- **Panel type per cell** (glass, opaque, door panel) — HIGH confidence, facades are heterogeneous
- **Mullion profile assignment** — MEDIUM confidence; likely accessible as a property
- **Grid pattern definition** (which division rule — fixed distance, fixed count) — MEDIUM confidence

### library-objects.md gaps
- **GDL parameter access beyond position/rotation** (custom parameters like `ZZYZX` variables) — MEDIUM confidence; MCP may expose a generic parameters dictionary
- **Object replacement** (swap placed object A for object B while keeping position/rotation) — LOW confidence on MCP support

### surfaces-materials.md gaps
- **Building material composite structure editing** (adding/removing layers, changing thicknesses) — LOW confidence MCP supports this; spec already flags create as uncertain
- **Surface texture parameter read** (which bitmap is mapped, at what scale) — LOW confidence

### fills-hatches.md gaps
- **Fill orientation / angle** — HIGH confidence, architects rotate hatching to match material grain or geometry
- **Fill background pen** (separate from foreground pen) — MEDIUM confidence

### lines-polylines.md gaps
- **Arc center/radius parametric creation** (beyond start/end/midpoint) — MEDIUM confidence
- **Spline control point editing** — LOW confidence on MCP support

---

## Bulk Operations Beyond Classification

Operations the spec's bulk pattern could support but doesn't document.

| Bulk Operation | Frequency | Fits Existing Pattern | Confidence |
|----------------|-----------|----------------------|------------|
| **Layer migration** (all elements Layer A → Layer B) | HIGH | Yes — filter by layer, update layer property | HIGH |
| **Level shift** (move elements up/down by N mm across a story) | MEDIUM | Yes — filter by story, update offset/elevation | HIGH |
| **Attribute remapping** (Surface A → Surface B project-wide) | MEDIUM | Yes — filter by current surface value, update | MEDIUM |
| **Story duplication** (copy all elements, shift by story height) | LOW | Partial — read all, bulk create with offset; loses host links | LOW |
| **Sweep-replace** (Object type A → Object type B) | LOW | Partial — delete+create loses element IDs and links | LOW |
| **Property set bulk write** (write IFC Pset values across all elements of a type) | MEDIUM | Yes — extend classify step to write properties | MEDIUM |

---

## Read-Only Analytics

Pure read operations with no modification risk. All "free" (no confirm needed).

| Analytics Feature | Architect Use Case | Complexity | Confidence |
|-------------------|--------------------|------------|------------|
| **Element count by type and story** | "How many walls / windows / doors are on EG?" Pre-bulk scouting. | LOW | HIGH |
| **Wall area tabulation** | Gross wall area per layer or story for cost estimation. | LOW | HIGH |
| **Slab area tabulation** | Floor area per story; cross-check against zone areas. | LOW | HIGH |
| **Zone area / volume by category** | Room schedule for planning submission. Requires zones to exist. | LOW | HIGH |
| **Opening schedule** (count by type, size grouping) | Door/window schedule for joinery contractor. | MEDIUM | HIGH |
| **Layer usage audit** (which layers have elements, which are empty) | Before layer cleanup or migration. | LOW | HIGH |
| **Attribute usage audit** (which surfaces/composites are actually used) | Before attribute remapping. | MEDIUM | HIGH |
| **Clash detection lite** (bounding-box overlap report) | Quick sanity check, not a substitute for dedicated clash tools. | HIGH | LOW — MCP likely does not expose spatial queries natively |

---

## Feature Dependencies

```
Zone analytics
    └──requires──> Zone creation (table stakes)

Attribute remapping (bulk)
    └──requires──> Attribute listing (table stakes)

Layer migration (bulk)
    └──requires──> Layer listing (table stakes)

Story-relative operations (walls, slabs, openings)
    └──requires──> Story management read (table stakes)

Sweep-replace
    └──requires──> Library object replacement sub-op
    └──requires──> Bulk delete (with IDs)

Clash detection lite
    └──requires──> Read element bounding boxes (unverified MCP capability)
```

### Dependency Notes

- **Zone analytics requires Zone creation:** There is no point analysing zones if the skill cannot create them. Both should land together in v1 or v2.
- **Attribute remapping requires Attribute listing:** Claude cannot map Surface A → Surface B without first knowing the ID of Surface B. Attribute listing is therefore a prerequisite for any bulk property update.
- **Story management enables half the sub-operation gaps:** Slab offset from story, wall bottom elevation, and correct story assignment for new elements all depend on Claude knowing story IDs and elevations.

---

## MVP Definition

### Launch With (v1)

The items below are not in the current spec and should be added to v1 because
their absence will be felt on day one.

- [ ] **Story management read** (list all stories, elevations, heights) — needed for every multi-story operation
- [ ] **Zone creation + basic properties** — needed for area schedules, the most common non-geometry deliverable
- [ ] **Attribute listing** (layers, surfaces, composites, fills, line types) — prerequisite for correct property assignment
- [ ] **openings.md: sill height + door swing direction** — architect-essential window/door parameters
- [ ] **walls.md: composite assignment + reference line position** — core wall parameters missing from spec
- [ ] **slabs-columns-beams.md: slab offset + beam/column cross-section** — structural drawings depend on these
- [ ] **Bulk layer migration** — add to reference/bulk-operations.md; fits existing pattern
- [ ] **Read-only analytics: element count + area tabulation** — free, low cost, high utility for pre-bulk scouting

### Add After Validation (v1.x)

- [ ] **fills-hatches.md: fill orientation/angle** — once base recipe is verified working
- [ ] **Attribute remapping bulk op** — extend bulk-operations.md after layer migration is proven
- [ ] **Zone analytics (area schedule)** — after zone creation is validated
- [ ] **Opening schedule read** — after openings.md is stable
- [ ] **curtain-walls.md: panel type per cell** — add once base curtain wall recipe is live-verified

### Future Consideration (v2+)

- [ ] **Hotlink / XREF read + update** — useful but complex safety model
- [ ] **Stair & railing** — high MCP complexity, unverified
- [ ] **Morph element** — geometry complexity, unverified MCP support
- [ ] **Mesh / terrain** — structural difference from parametric elements
- [ ] **IFC Pset read/write** — distinct from IFC file I/O but still complex
- [ ] **Clash detection lite** — needs spatial query support not confirmed in MCP
- [ ] **Story duplication** — partial support only, loses host links

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Story management read | HIGH | LOW | P1 |
| Attribute listing | HIGH | LOW | P1 |
| openings.md: sill height + door swing | HIGH | LOW | P1 |
| walls.md: composite + reference line | HIGH | LOW | P1 |
| slabs-columns-beams.md: slab offset + cross-section | HIGH | LOW | P1 |
| Zone creation + properties | HIGH | MEDIUM | P1 |
| Bulk layer migration | HIGH | LOW | P1 |
| Read-only analytics (counts + areas) | HIGH | LOW | P1 |
| fills-hatches.md: fill orientation | MEDIUM | LOW | P2 |
| Attribute remapping bulk op | MEDIUM | MEDIUM | P2 |
| Zone analytics (area schedule) | HIGH | MEDIUM | P2 |
| Opening schedule read | MEDIUM | LOW | P2 |
| curtain-walls.md: panel type per cell | MEDIUM | MEDIUM | P2 |
| Hotlink read | LOW | MEDIUM | P3 |
| Stair & railing | HIGH | HIGH | P3 |
| Morph element | LOW | HIGH | P3 |
| Mesh / terrain | LOW | HIGH | P3 |
| Clash detection lite | MEDIUM | HIGH | P3 |

**Priority key:**
- P1: Must have for launch (add to v1 spec)
- P2: Should have — add after v1 validation
- P3: Defer to v2+

---

## Anti-Feature Summary

| Anti-Feature | Category |
|---|---|
| Auto-save / project save | Out of scope — corrupts Teamwork, bypasses user workflow |
| Undo execution | Out of scope — MCP cannot safely target specific undo steps |
| Layer create / delete | Out of scope — template-level structural change |
| Story create / delete | Out of scope — cascading geometry and structure changes |
| Batch delete without explicit ID list | Prohibited pattern — never skip ID enumeration |
| Clipboard operations (copy/paste) | Out of scope — loses element host relationships |
| View / layer combination switching | Out of scope — visual disruption during operation |
| GDL parameter scripting / authoring | Out of scope — already in PROJECT.md, reinforced |

---

## Sources

- Archicad 29 documentation (element types, zone tool, story structure, wall parameters) — HIGH confidence on element model
- Archicad MCP plugin general patterns (discovery-centric, same server as used in spec) — MEDIUM confidence on specific tool names
- Architect workflow knowledge (daily production workflows, area schedules, joinery schedules) — HIGH confidence on user needs
- MCP spatial query support (clash detection, bounding boxes) — LOW confidence; unverified against live server

---

*Feature research for: Archicad-MCP skill for Claude Code*
*Researched: 2026-05-19*
