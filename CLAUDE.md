<!-- GSD:project-start source:PROJECT.md -->
## Project

**ELMonkey**

ELMonkey besteht aus zwei Affen: dem **Python-Monkey** (Kern `elmonkey` in `core/` plus MCP-Server `elmonkey-mcp` in `mcp/`) und dem **Archicad-Monkey** (C++-Add-On ELM_SAB_Tapir in `archicad-addon/`). Dazu der Archicad-Skill unter `skills/archicad` (wird nach `mozzi86/-archicad` gespiegelt). Der Kern wird auch von der Schwarz Plattform als festes Release genutzt (`elmonkey.plattform`). Zielgruppe: ein Architekturbüro, das täglich in Archicad und der Plattform arbeitet und KIs als Assistenten einsetzt.

**Core Value:** **Jede KI arbeitet sicher in Archicad und der Plattform: finden, vorprüfen, schreiben, zurücklesen** — ohne dass Tool-Namen oder Parameter zur Laufzeit erfunden werden müssen. Discovery-zentriert, asymmetrisch sicher, selbst-verbessernd.

### Constraints

- **Tech Stack**: Kern Python, nur Standardbibliothek; MCP-Server Python; Add-On C++17, gebaut nur von der CI (AC27–30, Mac/Win), kein lokaler Build; Skill Markdown (Hub-and-Spoke: 1 SKILL.md + reference/* + recipes/*).
- **MCP-Abhängigkeit**: Skill ist nur funktional, wenn der Archicad-MCP-Server (`mcp__archicad__*`) konfiguriert und Archicad mit MCP-Plugin geöffnet ist.
- **Discovery-First**: Konkrete MCP-Tool-Namen werden nicht hartkodiert, sondern als „typischer Name (Hinweis)" dokumentiert; Validierung via Discovery zur Laufzeit. Begründung: Robustheit gegen Archicad-Versionswechsel.
- **Live-Verifikation Pflicht**: Jede Recipe-Datei wird gegen das laufende Archicad geprüft, bevor sie als „fertig" gilt. Worked Example dient als Smoke-Test.
- **Sicherheit asymmetrisch**: Create + Read frei; Update + Delete mit User-Confirm + Element-ID-Aufzählung. Keine Batch-Obergrenze (User-Präferenz).
- **Self-Improvement on**: Skill wächst pro Session durch Reflection-Trigger; Einträge mit Datums-Marker, Verifikation-Loop bei Wiederholfehlern.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:research/STACK.md -->
## Technology Stack

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
# WRONG — Claude skips body, uses description as workflow
# CORRECT — triggering conditions only
### Size Limits (official)
- **SKILL.md body: keep under 500 lines.** Official hard recommendation (both claude.com and platform.claude.com docs).
- **Description + when_to_use: 1536-char combined cap** in skill listing context. Key use case first; truncation drops the tail.
- **Post-compaction budget:** Top-5 most-recently-invoked skills get ≤5000 tokens each; shared cap of 25 000 tokens. Older skills can be evicted. → Keep SKILL.md lean; on-demand reference files are free until loaded.
### On-Demand Reference Loading
## Reference files (load only when needed)
- Discovery strategies & error classes → [reference/mcp-conventions.md](skills/archicad/reference/mcp-conventions.md)
- Warm-up context fields → [reference/workflow-context.md](skills/archicad/reference/workflow-context.md)
- Bulk-classification pattern → [reference/bulk-operations.md](skills/archicad/reference/bulk-operations.md)
- Self-improvement rules → [reference/self-improvement.md](skills/archicad/reference/self-improvement.md)
## 2. Multi-File Skill Organization
## 3. MCP Server Interaction Patterns
### Tool References in Skills
# CORRECT
# WRONG — ambiguous when multiple MCP servers present
### Port Discovery Idiom
## Port Discovery (Warm-up Step 1)
### Error Retry Pattern
| Error symptom | Action | Max retries |
|---|---|---|
| `tool not found` | Re-run `archicad_discover_tools` with synonym query | 2, then inform user |
| `invalid argument` | Re-read schema via Discovery, do NOT guess | 1 |
| No active Archicad | Inform user, stop | 0 |
| Multiple Archicad instances | Ask user to choose | 0 |
| Unexpected success result | Stop, read what happened, do NOT auto-correct | 0 |
### Pagination Idiom
## Pagination
- Re-call same tool with `next_page_token` argument until absent.
- Accumulate results before filtering/classifying.
- Never operate on a partial page set.
### Schema Validation
## 4. Versioning / MCP Server Version Drift
| Operation | Discovery Query | Verified Tool Name |
|---|---|---|
| List elements | "get walls" | `GetWallsOfStory` <!-- verified 2026-05-19, AC29 --> |
## 5. Sub-Skill Composition
### Official Pattern: Skill Invoking Another Skill
## Bulk Classification
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
## Gaps vs. Current Spec
## Sources
- [code.claude.com/docs/en/skills](https://code.claude.com/docs/en/skills) — SKILL.md structure, frontmatter fields, size limits, context lifecycle, dynamic injection, sub-skill composition, allowed-tools, MCP tool reference guidance (HIGH)
- [platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices](https://platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices) — Multi-file organization patterns, description anti-patterns, progressive disclosure, TOC guidance, MCP tool qualification (HIGH)
- [code.claude.com/docs/en/agent-sdk/skills](https://code.claude.com/docs/en/agent-sdk/skills) — SDK skill loading, `skills` option, allowed-tools SDK caveat (HIGH)
- `/Users/ap/.claude/skills/writing-skills/SKILL.md` — Local skill authoring conventions; CSO anti-pattern for description (HIGH, corroborates official docs)
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

Conventions not yet established. Will populate as patterns emerge during development.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

Architecture not yet mapped. Follow existing patterns found in the codebase.
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->
## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
