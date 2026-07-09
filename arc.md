---
description: Fast-Path für schnelle Read-only-Queries an ein laufendes Archicad — umgeht MCP-Discovery, benutzt direkten HTTP-Zugriff via `arc` CLI oder curl. Nutze bei Einzelfragen wie „welches Projekt läuft?", „wieviele Zonen?", „welche Instanzen sind aktiv?", „welche Layer sind sichtbar?".
allowed-tools: Bash
---

# /arc — Archicad Fast-Query-Modus

Direkter HTTP-Zugriff auf Archicads JSON-API, ohne MCP-Discovery-Roundtrip. Für Einzelfragen 3–5× schneller als der reguläre Skill-Path.

## Aufgabe

$ARGUMENTS

## Hard Rules

- **Nur Read-only.** Kein Update, kein Delete, kein Create. Wenn `$ARGUMENTS` schreibend klingt („ändere", „lösche", „erstelle", „klassifiziere", „setze"): Fast-Path abbrechen und User auf regulären Skill-Path verweisen („Bitte ohne `/arc`-Prefix fragen — der Skill übernimmt mit Confirm-Dialog.").
- **CLI-first, curl-fallback.** Wenn `~/.local/bin/arc` (bzw. `~/.claude/skills/archicad/scripts/arc`) existiert und ausführbar ist: nutze es. Sonst: raw `curl` gegen `http://localhost:19723` (Default) oder `19724` (falls Erst-Port refused).
- **Max 4 Bash-Calls.** Wenn nach 4 Calls keine Antwort da ist: stoppen und User informieren.
- **Konziser Reply.** Zahlen und Fakten, keine Prosa. Bullet-Liste statt Absätze. Keine Wiederholung der Frage. Kein „Ich habe X abgefragt…" — direkt das Ergebnis.
- **Port-Discovery.** Wenn nicht klar welche Instanz gemeint ist: `arc ports` (oder `curl -s -m 2 -X POST -H 'Content-Type: application/json' -d '{}' http://localhost:19723/` und dann `19724`, `19725` durchprobieren). Bei mehreren aktiven Instanzen: User fragen, welche.
- **Kein Auto-Retry auf Fehler.** Wenn Port nicht antwortet oder API einen Fehler zurückgibt: stoppen, User informieren.

## Typische Queries + Kommandos

| User fragt | Kommando |
|---|---|
| Welche Instanzen laufen? | `arc ports` |
| Welches Projekt ist offen? | `arc call GetProjectInfo` |
| Aktuelle Selection? | `arc selected` |
| Wieviele Zonen? | `arc zones \| wc -l` |
| Layer mit Namen … | `arc layers <regex>` |
| Elemente auf Story N? | `arc tapir GetElementsByType '{"elementType":"Wall"}'` |
| Rohdaten für Debugging | `arc call <CommandName> '<json>'` oder `arc tapir <TapirCmd> '<json>'` |

`arc call` = offizielle `API.*`-Commands. `arc tapir` = via `API.ExecuteAddOnCommand` an Tapir-Add-On.

## Escape-Hatch

Wenn der User etwas will, das nicht in der Verb-Tabelle steckt: **immer** über `arc call`/`arc tapir` pass-through. Nichts erfinden, keine Discovery, keine MCP-Aufrufe.

## Anti-Patterns

- ❌ `mcp__archicad__*` aufrufen (dafür ist der reguläre Skill-Path da; wir sind der Fast-Path)
- ❌ Vollständige Warm-up-Sequenz durchspielen (Port, Story, Layer …) — nur was für die Frage nötig ist
- ❌ Retries mit Synonym-Queries — hier sind wir konkret oder gar nicht
- ❌ Antworten in Prosa-Absätzen. Zahlen + Bullet.

## Installation

Der Command lädt automatisch, wenn er unter `~/.claude/commands/arc.md` liegt.
Aus dem Skill-Repo installieren:

```bash
ln -sf ~/.claude/skills/archicad/commands/arc.md ~/.claude/commands/arc.md
ln -sf ~/.claude/skills/archicad/scripts/arc ~/.local/bin/arc
```

**Historie:** früher `/archicad` + `ac`, umbenannt zu `/arc` + `arc` wegen Kollision mit macOS-System-Binary `/usr/sbin/ac` (BSD process accounting) und Namenskonflikt mit dem Skill „archicad".
