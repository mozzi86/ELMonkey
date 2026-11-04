# Stack Research

**Domain:** Claude Code skill wrapping a discovery-driven MCP server (Archicad modeling, many CRUD element types)
**Researched:** 2026-05-19
**Confidence:** HIGH — sourced from official `code.claude.com/docs/en/skills` and `platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices`

---

## 1. Skill Content Format

### Frontmatter Fields (YAML between `---` markers)

| Field | Required | Constraint | Recommendation for this skill |
|---|---|---|---|
| `name` | No (uses dir name) | ≤64 chars, `[a-z0-9-]` only | Omit — dir name `archicad` is fine |
| `description` | Recommended | ≤1024 chars, no XML tags | English; triggers + capabilities only; no workflow summary (see below) |
| `when_to_use` | No | Appended to description; shares 1536-char cap | Add if description needs trigger phrases |
| `allowed-tools` | No | Space-separated or YAML list | List all `mcp__archicad__*` tools + `Read` to avoid prompts |
| `model` | No | Same values as `/model` | Omit (inherit session model) |
| `disable-model-invocation` | No | `true`/`false` | Leave `false` — Claude should auto-trigger |
| `user-invocable` | No | `true`/`false` | Leave default `true` |

**Critical — description anti-pattern:** Anthropic testing shows that if the description summarizes the skill's *workflow*, Claude follows the description shortcut and skips reading SKILL.md body. Description must describe only *when to trigger*, not *what to do*.

```yaml
# WRONG — Claude skips body, uses description as workflow
description: Archicad skill — runs warm-up, discovers tools, applies asymmetric safety

# CORRECT — triggering conditions only
description: Use when creating, querying, modifying, or organizing elements in Archicad — walls, openings, slabs, columns, beams, curtain walls, library objects, fills/hatches, lines, polylines, surfaces/materials. Triggers on any task involving the Archicad MCP server (mcp__archicad__*).
```

### Size Limits (official)

- **SKILL.md body: keep under 500 lines.** Official hard recommendation (both claude.com and platform.claude.com docs).
- **Description + when_to_use: 1536-char combined cap** in skill listing context. Key use case first; truncation drops the tail.
- **Post-compaction budget:** Top-5 most-recently-invoked skills get ≤5000 tokens each; shared cap of 25 000 tokens. Older skills can be evicted. → Keep SKILL.md lean; on-demand reference files are free until loaded.

### On-Demand Reference Loading

Files in the skill directory consume **zero context tokens until Claude reads them**. Claude reads them via Read tool when following a link from SKILL.md. Pattern:

```markdown
## Reference files (load only when needed)
- Discovery strategies & error classes → [reference/mcp-conventions.md](reference/mcp-conventions.md)
- Warm-up context fields → [reference/workflow-context.md](reference/workflow-context.md)
- Bulk-classification pattern → [reference/bulk-operations.md](reference/bulk-operations.md)
- Self-improvement rules → [reference/self-improvement.md](reference/self-improvement.md)
```

**One level deep only.** If `mcp-conventions.md` references further files, Claude may partial-read those (HEAD preview, not full). All reference files must link directly from SKILL.md. [CONFIDENCE: HIGH — documented anti-pattern in official best practices.]

---

## 2. Multi-File Skill Organization

**Official guidance (2026):** Hub-and-spoke is the documented best practice for skills with multiple domains. Monolithic SKILL.md is appropriate only for simple, single-domain skills. Official name for the pattern: "Domain-specific organization" (pattern 2 in best-practices).

```
archicad/
├── SKILL.md                    # ≤500 lines; hub with links; always in context when triggered
├── reference/
│   ├── mcp-conventions.md      # Discovery, errors, pagination, port handling
│   ├── workflow-context.md     # Live-verified tool names for 7 warm-up fields
│   ├── bulk-operations.md      # Read→Filter→Group→Confirm→Apply + classification
│   └── self-improvement.md     # Reflection trigger, learn classes, format rules
└── recipes/
    ├── walls.md
    ├── openings.md
    ├── slabs-columns-beams.md
    ├── curtain-walls.md
    ├── library-objects.md
    ├── surfaces-materials.md
    ├── fills-hatches.md
    └── lines-polylines.md
```

This matches the spec exactly. **No change needed.**

**Reference file target size:** Official guidance says add a table of contents at the top of any reference file over 100 lines, so Claude can navigate even partial reads.

**Sub-skill "spoke" triggering note:** Recipe files are *not* sub-skills in the Claude Code sense (they have no SKILL.md). They are reference documents Claude reads on demand. This is the correct pattern for this use case — recipes do not need their own frontmatter or invocation mechanism.

---

## 3. MCP Server Interaction Patterns

### Tool References in Skills

Official guidance (best-practices doc, "MCP tool references" section):

> Always use fully qualified tool names: `ServerName:tool_name`

```markdown
# CORRECT
Use `mcp__archicad__archicad_discover_tools` to discover available tools.
Use `mcp__archicad__archicad_call_tool` with `{port, name, arguments}`.

# WRONG — ambiguous when multiple MCP servers present
Use archicad_call_tool...
```

The `mcp__archicad__` prefix is the server name qualifier. **Always include it.** [CONFIDENCE: HIGH]

### Port Discovery Idiom

No official SDK pattern for port management exists — this is Archicad-MCP-specific. The spec's approach (`discovery_list_active_archicads` → extract port → cache for session) is the correct idiom. Document it in `reference/mcp-conventions.md` as the canonical warm-up step 1.

```markdown
## Port Discovery (Warm-up Step 1)
1. Call `mcp__archicad__discovery_list_active_archicads` (no arguments).
2. If 0 results → inform user, stop.
3. If >1 result → ask user to choose, never guess.
4. Extract `port` from result. Cache for this task session.
```

### Error Retry Pattern

No official SDK retry idiom in the skill docs. The spec's error-class table is well-formed. Recommended canonical form for `mcp-conventions.md`:

| Error symptom | Action | Max retries |
|---|---|---|
| `tool not found` | Re-run `archicad_discover_tools` with synonym query | 2, then inform user |
| `invalid argument` | Re-read schema via Discovery, do NOT guess | 1 |
| No active Archicad | Inform user, stop | 0 |
| Multiple Archicad instances | Ask user to choose | 0 |
| Unexpected success result | Stop, read what happened, do NOT auto-correct | 0 |

### Pagination Idiom

Document in `mcp-conventions.md`:

```markdown
## Pagination
If response contains `next_page_token`:
- Re-call same tool with `next_page_token` argument until absent.
- Accumulate results before filtering/classifying.
- Never operate on a partial page set.
```

### Schema Validation

Official pattern: execute the tool, inspect the live schema from Discovery output, use that schema — never hardcode parameters. This aligns exactly with the spec's "Typische Parameter (Hinweis-Charakter)" recipe section. **No change needed.**

---

## 4. Versioning / MCP Server Version Drift

**No official Claude Code skill guidance on MCP versioning exists as of 2026-05-19.** [CONFIDENCE: LOW for this specific topic]

The spec's chosen approach — document "typical tool name (hint)" but validate via Discovery at runtime — is the correct mitigation. The following additions will make it robust:

**Adopt: Inline date markers on all live-verified tool names**

```markdown
| Operation | Discovery Query | Verified Tool Name |
|---|---|---|
| List elements | "get walls" | `GetWallsOfStory` <!-- verified 2026-05-19, AC29 --> |
```

**Adopt: `<!-- VERIFY -->` marker when a tool name fails in session**

After two failures → mark as `<!-- VERIFY -->` in the recipe, update next session.

**Do NOT: hardcode tool names as invariants.** Even if Discovery returns the same name 10 times, treat it as a hint. Always call `archicad_discover_tools` before a new operation type in a session.

**Gap vs. spec:** The spec says "Validierung via Discovery zur Laufzeit" but does not specify whether to re-discover on every call or once per operation type per session. **Recommendation: once per operation type per session** (balance reliability vs. token cost). Document this rule in `mcp-conventions.md`.

---

## 5. Sub-Skill Composition

### Official Pattern: Skill Invoking Another Skill

Official docs show two approaches:

**A. In-body reference (recommended for this project):** SKILL.md mentions another skill by name; Claude invokes it via the Skill tool autonomously.

```markdown
## Bulk Classification
For bulk operations, use the `dispatching-parallel-agents` skill to parallelize
element reads across stories, then apply the pattern in [reference/bulk-operations.md](reference/bulk-operations.md).
```

**B. `context: fork` with `agent` field:** Skill becomes a subagent task. Only useful if you want full isolation (no conversation history). Not appropriate for interactive Archicad sessions where warm-up context must persist.

**For this skill:** Sub-skill composition is not needed between recipe files (they are reference docs, not skills). The only inter-skill reference that may be useful is invoking `dispatching-parallel-agents` for parallel bulk reads. Document as an optional pointer in `reference/bulk-operations.md`.

**Do NOT:** Create a separate SKILL.md per recipe. Recipes are reference files, not skills. Adding SKILL.md to each recipe creates 8 additional skills in the listing, burning description budget and causing unintended auto-triggers on unrelated Archicad questions.

---

## Adopt / Do NOT Do

### Adopt

| What | Why |
|---|---|
| Hub-and-spoke: one SKILL.md + reference/* + recipes/* | Official best practice for multi-domain skills; zero token cost for unloaded files |
| SKILL.md ≤500 lines | Official hard recommendation; compaction budget constraint |
| Description: triggering conditions only, no workflow summary | Documented Anthropic finding: workflow summary in description causes Claude to skip body |
| Fully qualified MCP tool names: `mcp__archicad__*` | Official: prevents "tool not found" when multiple MCP servers present |
| Date markers on all live-verified tool names | Enables version drift detection without runtime coupling |
| TOC in any reference file >100 lines | Official guidance; ensures Claude navigates partial reads correctly |
| One-level-deep references only (all links from SKILL.md) | Official anti-pattern: nested references cause partial reads |
| Re-discover tool once per operation type per session | Balance reliability vs. token cost; not per-call, not per-session |

### Do NOT Do

| What | Why |
|---|---|
| Hardcode MCP tool names as invariants in skill text | Server version drift; always treat as hints verified by Discovery |
| Summarize workflow in description frontmatter | Documented cause of Claude skipping SKILL.md body content |
| Create SKILL.md per recipe file | Floods skill listing (8 extra skills); unintended auto-triggers |
| Nest references (SKILL.md → A.md → B.md) | Official anti-pattern; causes partial reads of deep files |
| Re-discover tools on every individual MCP call | Unnecessary token overhead once tool name confirmed in session |
| Use `context: fork` for Archicad skill | Forks lose warm-up context (port, story, layers) that must persist across calls |
| Put project-specific data (layer names, GUIDs) in skill files | Per spec: project-specific facts belong in Memory, not skill |

---

## Gaps vs. Current Spec

1. **Description anti-pattern not in spec:** The spec's proposed skill description includes workflow context ("Includes per-job warm-up, asymmetric safety..."). This violates the documented Anthropic finding. **Fix:** Trim description to triggering conditions only; move workflow detail to SKILL.md body.

2. **"One level deep" rule not in spec:** The spec's reference structure is one level deep already (`reference/*.md`, `recipes/*.md` all linked from SKILL.md) — compliant. No change needed, but explicitly acknowledge this constraint in SKILL.md authoring notes.

3. **Table of contents in long reference files:** Spec does not mention this. All reference files and recipes will likely exceed 100 lines. **Add TOC** at top of each.

4. **Fully qualified MCP names in skill text:** Spec uses shorthand like "archicad_discover_tools" in code blocks. These should be `mcp__archicad__archicad_discover_tools` throughout. Low risk (Claude can infer server from context when only one MCP server present), but the official guidance says always qualify.

---

## Sources

- [code.claude.com/docs/en/skills](https://code.claude.com/docs/en/skills) — SKILL.md structure, frontmatter fields, size limits, context lifecycle, dynamic injection, sub-skill composition, allowed-tools, MCP tool reference guidance (HIGH)
- [platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices](https://platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices) — Multi-file organization patterns, description anti-patterns, progressive disclosure, TOC guidance, MCP tool qualification (HIGH)
- [code.claude.com/docs/en/agent-sdk/skills](https://code.claude.com/docs/en/agent-sdk/skills) — SDK skill loading, `skills` option, allowed-tools SDK caveat (HIGH)
- `/Users/ap/.claude/skills/writing-skills/SKILL.md` — Local skill authoring conventions; CSO anti-pattern for description (HIGH, corroborates official docs)

---
*Stack research for: Claude Code skill wrapping Archicad MCP server*
*Researched: 2026-05-19*
