# Phase 1: Skill-Gerüst + Safety - Context

**Gathered:** 2026-05-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 1 liefert das vollständige Skill-Verzeichnis `~/.claude/skills/archicad/` mit `SKILL.md` (alle Safety-Rules + Warm-up-Skelett + Discovery-Pattern + Index) und allen 4 Reference-Dateien als Skelette mit TOC. Ausführbar ohne laufendes Archicad. Description-Feld ist Trigger-only. Kein Live-MCP-Call in dieser Phase.

**In scope:** SKILL.md, reference/mcp-conventions.md, reference/workflow-context.md, reference/bulk-operations.md, reference/self-improvement.md (alle als Skelette mit Struktur + TODO-Markern für später live zu verifizierende Inhalte).

**Out of scope:** Recipes/*, Live-MCP-Verifikation (Phase 2), Klassifikations-System-Discovery (Phase 5).
</domain>

<decisions>
## Implementation Decisions

### Sprache und Ton der Skill-Inhalte
- **D-01:** SKILL.md und alle Reference-Files in **deutscher Prosa**. Ausnahmen: Skill-Description-Feld (englisch, für Anthropic-Matching-Konsistenz) und MCP-Discovery-Queries (englisch, MCP-Index ist vermutlich englisch).
- **D-02:** Ton ist **erklärend-freundlich, NICHT streng-direktiv**. Beispiel: „Bevor wir einen Update ausführen, fragen wir den User — weil falsche Element-IDs hier echte Schäden verursachen." Statt: „NEVER perform an Update without user confirmation." Reasoning eingebettet, damit Claude versteht *warum*, nicht nur *was*.
- **D-03:** Risikofaktor zu D-02: Research (PITFALLS.md, STACK.md) hat „strenge Regeln" empfohlen, weil Claude unter Stress Direktiven eher hält. User hat erklärenden Ton explizit gewählt — wir gehen das Risiko bewusst ein. Wenn in späteren Phasen Safety-Verletzungen beobachtet werden, kann der Ton der Safety-Sektion in Phase 8 (Gap-Close) nachgehärtet werden.

### Skill-Description (Auto-Trigger-Text)
- **D-04:** Description-Wording: **„Use for any Archicad modeling task via the MCP server (`mcp__archicad__*`)."** — maximal kompakt. Keine Element-Aufzählung, keine Workflow-Hints, keine Bulk-Klassifizierungs-Erwähnung.
- **D-05:** Risikofaktor zu D-04: Research warnte vor zu unspezifischer Description (Trigger könnten schwächer sein als bei einer mit Element-Liste). User akzeptiert das Risiko bewusst zugunsten von Minimalismus. Falls Phase 8 zeigt, dass der Trigger nicht zuverlässig feuert, ist eine Anreicherung im Folge-Update möglich.

### Confirm-Format-Beispielblock — Position
- **D-06:** Vollständiger Confirm-Format-Beispielblock (mit `Ich werde folgendes ändern: ...`-Layout + Antwortoptionen + Batch-Format) liegt in **`reference/mcp-conventions.md`** (lazy-geladen). SKILL.md enthält nur einen kurzen Hinweis-Satz mit Verweis auf die Reference-Datei.
- **D-07:** SKILL.md bekommt keinen Inline-Beispielblock. Begründung: 500-Zeilen-Token-Budget; Beispiele werden bei Bedarf via Reference-Read gezogen.

### Skelett-Platzhalter-Stil in Reference-Files
- **D-08:** **Reine TODO-Marker**, keine Vermutungs-Tool-Namen. Beispiel:
  ```markdown
  ### Warm-up-Feld 3: Längeneinheit
  - **Tool-Name:** TODO — in Phase 2 live verifizieren
  - **Discovery-Query (zum Probieren):** "project units" / "active preferences"
  - **Erwartete Parameter:** TODO
  ```
- **D-09:** Begründung: Vermutungs-Tool-Namen mit `<!-- VERIFY -->` schaffen einen Halluzinations-Anker, der Claude in Versuchung führt, „typische" Namen blind zu verwenden. Reine TODOs zwingen den Discovery-Aufruf in Phase 2.

### Claude's Discretion

Bei diesen Punkten hat Phase 1 Flexibilität:
- **Reihenfolge der Reference-Files**: Welche Reference-Datei zuerst geschrieben wird (innerhalb derselben Plan-Subtask).
- **Konkretes TOC-Format**: Verschachtelte Bullet-Liste vs. einzeilige Anker-Links — egal, solange `>100 Zeilen → TOC am Anfang` eingehalten wird.
- **Skelett-Inhalts-Tiefe**: Wie viele Skeleton-Sektionen vorgezeichnet werden (z. B. ob `mcp-conventions.md` schon alle Fehlerklassen-Sektionen mit Headers angelegt hat, oder nur einen Stub).
- **Genauer Wortlaut der Safety-Regeln**: Solange Inhalt (SAFE-01..05) korrekt ist; Tonalität gemäß D-02.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project-Level Specs
- `/Users/ap/docs/superpowers/specs/2026-05-19-archicad-skill-design.md` — Vollständiger Brainstorming-Spec mit allen ursprünglichen Design-Entscheidungen, Sicherheitsmodell, Self-Improvement-Pattern, Coverage-Matrix.
- `.planning/PROJECT.md` — Living project context, Core Value, Requirements-Übersicht, Key Decisions.
- `.planning/REQUIREMENTS.md` — REQ-IDs (FOUND-01..05, SAFE-01..05, SELF-01..03) inkl. Definition of Done.
- `.planning/ROADMAP.md` § Phase 1 — Goal, Success Criteria, Plans-Skizze (3 Plans).

### Research (Phase-übergreifend)
- `.planning/research/STACK.md` — Skill-Format-Konventionen (500-Zeilen-Limit, frontmatter, TOC-Regel, voll-qualifizierte MCP-Toolnamen, one-level-deep references).
- `.planning/research/ARCHITECTURE.md` — Hub-and-Spoke validiert; Element-ID-Threading-Regel-Gap (muss in SKILL.md rein); SKILL.md Token-Budget-Hinweis.
- `.planning/research/PITFALLS.md` — 12 Pitfalls, davon 5 nicht in Original-Spec: P4 (Invisible-Layer-Ausnahme), P6 (Hosted-Element-Orphaning), P9 (Self-Improvement-Verification-Gap), P10 (Create+Delete-Bypass), P12 (Composite/Surface-Disambiguierung). Phase 1 setzt P9, P10, P4 als Safety-Rules in SKILL.md um.
- `.planning/research/SUMMARY.md` — Synthese mit allen Spec-Anpassungen für Phase 1 (Description-Fix, ID-Threading, neue Safety-Rules, FOUND-05 ergänzt).
- `.planning/research/FEATURES.md` — Wirkt erst ab Phase 2 (Story-/Attribute-Listing) und Phase 3 (Zone). Phase 1 muss nur Platzhalter in Recipe-File-Index für ZONE bereit halten.

### Anthropic-externe Standards
- Claude-Code-Skill-Konventionen (siehe `.planning/research/STACK.md` für Quellen-Links) — werden in SKILL.md eingehalten.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

Greenfield-Projekt — keine existierenden Code-Assets im Skill-Verzeichnis. Allerdings:

- Andere Skills unter `~/.claude/skills/` (z. B. `brainstorming/`, `writing-plans/`, `using-superpowers/`) zeigen Format-Konventionen für SKILL.md — Stilreferenz, nicht Code-Reuse.
- `~/.claude/get-shit-done/templates/` enthält Templates die für Format-Inspiration nützlich sein können.

### Established Patterns

- **Hub-and-Spoke**: Bestätigt durch Research (STACK.md, ARCHITECTURE.md). SKILL.md = always-loaded Hub, alle anderen Files = on-demand.
- **Frontmatter-Konvention**: Pro STACK.md sollte SKILL.md ein `---`-Frontmatter mit `name`, `description`, evtl. `allowed-tools` haben (genaues Schema in Plan-Phase festlegen).
- **TOC bei >100 Zeilen Reference-Files**: STACK.md-Empfehlung. Phase 1 wendet diese Konvention auf alle 4 Reference-Skeletons an, sofern sie >100 Zeilen erreichen (mcp-conventions.md und bulk-operations.md vermutlich).
- **Datums-Marker `<!-- 2026-05-19 -->`**: Self-Improvement-Konvention für Live-verifizierte Einträge (Phase 2+). In Phase 1 noch nicht verwendet (keine Live-Verifikation).

### Integration Points

Keine externen Integration Points in Phase 1 (kein Live-MCP, keine bestehenden Skills die getriggert werden müssen). Erste echte Integration ist Phase 2 (MCP-Server-Verbindung) und Phase 3+ (Element-Operationen).

</code_context>

<deferred>
## Noted for Later

Nicht in dieser Phase, aber zur Erinnerung erfasst:

- **Description-Anreicherung bei schwachem Trigger**: Falls Phase 8 zeigt, dass die maximal-kompakte Description (D-04) nicht zuverlässig triggert, eine Erweiterung mit Element-Aufzählung in Folge-Update.
- **Strenger Ton-Wechsel bei Safety-Verletzungen**: Falls in Phasen 3-7 beobachtet wird, dass Claude die erklärend-freundliche Safety-Sektion abkürzt, in Phase 8 die Safety-Sektion alleine auf direktiv-streng umstellen (Mix-Modus).
- **Inline-Confirm-Beispiel in SKILL.md**: Falls sich in Phase 5 (bulk-operations) zeigt, dass Claude ohne Inline-Beispiel das Format nicht zuverlässig produziert, einen 2-Zeilen-Mini-Beispielblock in SKILL.md nachziehen.
</deferred>

<success_signals>
## Success Signals (for Plan-Phase to verify)

Phase 1 ist erfolgreich, wenn:

1. `~/.claude/skills/archicad/SKILL.md` existiert, ≤500 Zeilen, Description = D-04, Ton = erklärend (D-02), enthält alle 5 Safety-Rules (SAFE-01..05) und Element-ID-Threading-Regel.
2. `reference/mcp-conventions.md` existiert mit TOC, Confirm-Format-Beispielblock (D-06), Skelett-TODO-Markern (D-08), Fehlerklassen-Stubs.
3. `reference/workflow-context.md` existiert mit TOC, 7 Warm-up-Felder als TODO-Stubs (D-08), Story-Feld-Volatilitäts-Hinweis.
4. `reference/bulk-operations.md` existiert mit TOC, Read→Filter→Group→Confirm→Apply-Pattern als Prosa (TOML-frei), Klassifikations-Spezifika-Sektion als Stub.
5. `reference/self-improvement.md` existiert mit Reflection-Trigger, Datums-Marker-Format, Verification-Loop-Regeln, „nicht in Skill, in Memory"-Trennung.
6. Skill lädt in einer neuen Claude-Code-Session ohne Frontmatter-Fehler.
7. Smoke-Test: Claude wird mit „erkläre den Archicad-Skill" prompted und liest SKILL.md korrekt; gibt eine Zusammenfassung der Safety-Regeln zurück, ohne Skelett-TODOs als „bekannte Tool-Namen" zu halluzinieren.

</success_signals>
