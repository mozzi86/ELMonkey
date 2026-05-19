# Self-Improvement — Archicad-Skill

Wie der Skill aus realen Sessions wächst. [SKILL.md](../SKILL.md) verweist hierher am Ende eines Auftrags und immer, wenn wir entscheiden, ob etwas Neues in den Skill gehört oder nicht.

## Inhaltsverzeichnis

1. [Warum überhaupt?](#warum-überhaupt)
2. [Lern-Klassen und ihre Speicherorte](#lern-klassen-und-ihre-speicherorte)
3. [Der Reflection-Trigger](#der-reflection-trigger)
4. [Format-Konventionen für neue Einträge](#format-konventionen-für-neue-einträge)
5. [Verification-Loop bei Wiederholfehlern](#verification-loop-bei-wiederholfehlern)
6. [Grenzen — was NICHT in den Skill gehört](#grenzen--was-nicht-in-den-skill-gehört)

## Warum überhaupt?

Statisch geschriebene Dokumentation veraltet. Ein Skill, der sich pro Session selbst erweitert, wird mit der Zeit präziser, robuster, individuell auf die tatsächlich auftretenden Archicad-Workflows zugeschnitten. Wir kapseln Lern-Gewinne sofort, bevor sie verloren gehen — und wir wissen aus Erfahrung, dass „später erfasse ich das schon" praktisch nie passiert.

Gleichzeitig wollen wir keine Skill-Bloat. Jeder neue Eintrag muss eine Hürde überschreiten: er muss live verifiziert sein, er muss generalisierbar sein (nicht ein einmaliger Vorfall), und er muss in eine der unten genannten Lern-Klassen passen.

## Lern-Klassen und ihre Speicherorte

| Lerntyp | Wohin |
|---|---|
| Neuer MCP-Tool-Name (Discovery hat ihn geliefert, Call hat funktioniert) | `recipes/<typ>.md` → Discovery-Anker — oder `reference/workflow-context.md` → das betroffene Warm-up-Feld |
| Neue Gotcha eines Elementtyps (z. B. „geschwungene Wände erwarten `radius` statt `endPoint`") | `recipes/<typ>.md` → Gotchas-Sektion |
| Allgemeines MCP-Verhalten (Paginierungs-Quirk, ungewöhnlicher Fehler-Code, neuer Idiom) | `reference/mcp-conventions.md` |
| Geometrie-Ableitungs-Trick für Bulk-Klassifizierung (z. B. „Innen-Erkennung via Zone-Membership funktioniert in Archicad 29") | `reference/bulk-operations.md` → entsprechende Sub-Sektion |
| Neuer Worked Example für ein Rezept (eine reale Aufgabe, die nicht offensichtlich aus den existierenden Beispielen abzuleiten war) | `recipes/<typ>.md` → Worked-Examples-Sektion |
| User-Präferenz („dieser User akzeptiert keine Caps in Confirm-Dialogen") | **Memory** — nicht Skill |
| Projekt-spezifischer Fakt („Layer ‚Wände-OG' enthält die OG-Bestandswände") | **Memory** — nicht Skill |

Die Trennlinie ist klar: **Skill = generisches, projektübergreifendes Wissen**. **Memory = du, dein aktuelles Projekt.** Wenn ein Lern-Gewinn nur in *einem* Projekt gilt, gehört er nicht in den Skill — sonst ist er für andere Projekte irreführend.

## Der Reflection-Trigger

Am Ende eines Archicad-Auftrags fragen wir den User **eine** Frage:

> Lern-Check: War da was Neues — ein Tool-Name, eine Geometrie-Stolperfalle, ein Klassifikations-Trick — das in den Skill sollte?

Antwort-Varianten:

- **`nein`** → Auftrag beendet, kein Skill-Update. Das ist die häufigste und korrekte Antwort. Nicht jeder Auftrag bringt neues Wissen.
- **`ja, <X>`** → Wir tragen X in die richtige Datei ein (siehe Lern-Klassen oben) mit Datums-Marker. Wenn der User nur eine Andeutung gibt, fragen wir kurz nach, um den Eintrag scharf zu formulieren.
- **`check selbst`** → Wir rekapitulieren den Auftrag mental, schlagen 0–3 Updates vor, der User bestätigt einzeln, was übernommen wird.

Wir verlangen NICHT, dass jeder Auftrag etwas Neues produziert. Wir verlangen, dass wir die Frage stellen.

## Format-Konventionen für neue Einträge

Jeder gelernte Eintrag bekommt einen **Inline-Datums-Marker** im HTML-Kommentar-Format:

```markdown
- Geschwungene Wände erwarten `radius` + `arcAngle` statt `endPoint`. <!-- 2026-06-12 -->
```

Der Marker erfüllt zwei Zwecke: er sagt, *wann* dieses Wissen aufgenommen wurde, und er macht den Eintrag auffindbar (per `grep -E '<!-- 20[0-9]{2}-'`).

### Status-Marker (optional)

Wenn ein Eintrag sich als problematisch herausstellt (z. B. der Tool-Name funktioniert nicht mehr nach einem Archicad-Update), markieren wir ihn:

```markdown
- Geschwungene Wände erwarten `radius` + `arcAngle` statt `endPoint`. <!-- 2026-06-12 --> <!-- ÜBERPRÜFEN -->
```

Drei Optionen:

- **Keine Markierung** = bewährt, vertrauenswürdig.
- **`<!-- ÜBERPRÜFEN -->`** = bei wiederholten Problemen, Eintrag braucht Re-Verifikation.
- **`<!-- VERIFY -->`** = vor Live-Tests in Folgephasen, frische Vermutung noch nicht geprüft.

So sehen wir auf einen Blick: was ist frisch und noch unter Beobachtung vs. was ist über Monate stabil.

## Verification-Loop bei Wiederholfehlern

Manche Lern-Einträge halten der Realität nicht stand — Archicad ändert sich, MCP-Server-Versionen drehen, oder die ursprüngliche Beobachtung war nicht generalisierbar.

**Regel:**

- **2 Fehlanwendungen in Folge** → Eintrag mit `<!-- ÜBERPRÜFEN -->` markieren, beim nächsten Reflection-Trigger einen Korrekturvorschlag erwähnen.
- **3 stabile Sessions ohne Probleme** → Marker `ÜBERPRÜFEN` entfernen, Eintrag bewährt.
- **Wenn nach Re-Verifikation der Eintrag falsch ist** → korrigieren oder ganz entfernen, NICHT „abschwächen" („normalerweise…" / „manchmal…").

Diese Schleife verhindert, dass falsche Lern-Einträge sich verewigen — eines der größten Risiken eines selbst-lernenden Skills (Hallucination-Feedback-Loop, Pitfall P9 aus der Research).

## Grenzen — was NICHT in den Skill gehört

Manche Versuchungen sehen wie Lern-Gewinne aus, sind aber Skill-Bloat oder direkte Schäden.

### Halluzinationen / Vermutungen ohne Live-Beleg

Jeder neue Tool-Name, jeder neue Parameter wird durch reale Ausführung im MCP validiert, bevor er in den Skill kommt. Niemals einfach „der User sagte, das Tool heißt so" eintragen, ohne dass ein erfolgreicher MCP-Call vorliegt.

### Projektspezifische Strings

Layer-Namen, GUID-Werte, Klassifikations-Bezeichnungen aus EINEM Projekt: das gehört in Memory, nicht in den Skill. Wenn wir „Layer ‚Wände-Bestand'" in `walls.md` schreiben würden, wäre das in anderen Projekten falsch oder irreführend.

### Einmal-Vorfälle ohne Wiederholungs-Erwartung

Ein seltsamer Bug, der nur einmal auftauchte und sich nicht reproduzieren lässt, gehört nicht in den Skill. Lieber gar nicht erfassen als Lärm produzieren.

### User-persönliche Präferenzen

„User mag keine Caps in Confirm-Dialogen", „User akzeptiert keine Batch-Obergrenze" — das sind Memory-Einträge, keine Skill-Einträge. Memory ist für *dich*, Skill ist für die Archicad-Domäne.

### Workflow-Schritte für den User

Wenn der User in einem Auftrag eine ungewöhnliche Vorgehensweise wählt („immer erst Selektion umschalten"), gehört das nicht als Anweisung in den Skill. Memory-Eintrag, nicht Skill-Eintrag.
