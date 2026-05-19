# MCP-Conventions — Archicad-Skill

Diese Datei dokumentiert, wie wir mit dem Archicad-MCP-Server umgehen. [SKILL.md](../SKILL.md) lädt sie bei Bedarf — wenn ein neuer Elementtyp auftaucht, eine Discovery fehlschlägt oder das Confirm-Format gebraucht wird.

## Inhaltsverzeichnis

1. [Discovery-Pattern im Detail](#discovery-pattern-im-detail)
2. [Fehlerklassen und Reaktionen](#fehlerklassen-und-reaktionen)
3. [Confirm-Format für schreibende Aufrufe](#confirm-format-für-schreibende-aufrufe)
4. [Paginierung](#paginierung)
5. [Port-Handling bei mehreren Archicad-Instanzen](#port-handling-bei-mehreren-archicad-instanzen)
6. [Verhalten bei „nein" oder Mid-Batch-Fehler](#verhalten-bei-nein-oder-mid-batch-fehler)

## Discovery-Pattern im Detail

[SKILL.md](../SKILL.md) skizziert das 5-Schritt-Pattern. Hier die Erweiterungen für schwierige Fälle.

**Synonym-Retry.** Wenn die erste Discovery-Query nichts Brauchbares liefert, probieren wir Synonyme. Beispiel-Familien:

- *create* — „create", „add", „insert", „new", „place"
- *delete* — „delete", „remove", „erase"
- *modify* — „modify", „update", „change", „edit", „set"
- *list* — „list", „get", „retrieve", „query", „find"

**Genauigkeit der Query.** Vage Queries liefern vage Ergebnisse. „Wand" allein ist zu wenig — besser „create wall element on story" oder „get all walls in current story". Wir nehmen Verb + Objekt + Kontext.

**Mehrdeutige Treffer.** Wenn die Discovery mehrere Kandidaten gleichermaßen passend zurückgibt: nicht raten. Eine zweite, engere Query stellen oder den User fragen, welcher Tool-Name gemeint sein soll.

**Beispiel-Discovery-Skelette** (Tool-Namen werden in Folgephasen live verifiziert):

- Wand erstellen: `mcp__archicad__archicad_discover_tools` mit Query „create wall element on story" → TODO: Tool-Name + Parameter in Phase 3 verifizieren.
- Element löschen: Query „delete elements by id" → TODO: Phase 3.
- Eigenschaften einer Wand abfragen: Query „get element properties by id" → TODO: Phase 2/3.

## Fehlerklassen und Reaktionen

| Symptom | Reaktion | Max Retries |
|---|---|---|
| `tool not found` | Re-Discovery mit Synonym-Query | 2, dann User informieren |
| `invalid argument` | Schema neu via Discovery prüfen — nicht raten | 1 |
| Keine Archicad-Instanz | User informieren („Archicad scheint nicht zu laufen oder MCP-Plugin inaktiv"), stoppen | 0 |
| Mehrere Archicad-Instanzen | User fragen, welche (Port + Projektname listen) | 0 |
| Erfolgsmeldung, aber unerwartetes Ergebnis | Stoppen, lesen was tatsächlich passiert ist, **NICHT** auto-korrigieren | 0 |
| Pagination-Token vorhanden | Weiter-anfragen bis vollständig (siehe Sektion 4) | bis fertig |
| Netzwerk-/Timeout-Fehler | 1 Retry nach kurzer Pause, dann User informieren | 1 |

**Wichtiges Prinzip.** Bei einem unerwarteten Ergebnis (Operation scheint erfolgreich, aber das Element fehlt im Modell oder hat falsche Eigenschaften) **stoppen wir und lesen**, was tatsächlich passiert ist. Wir versuchen nicht, durch Folge-Operationen „still zu korrigieren" — das verschleiert nur die Ursache und kann doppelten Schaden anrichten.

## Confirm-Format für schreibende Aufrufe

Vor jedem Update- oder Delete-Aufruf zeigen wir folgendes Format. Das gibt dem User die Möglichkeit, die geplante Änderung zu prüfen, bevor sie passiert.

### Format für 1–10 Elemente — Einzelaufzählung

```
Ich werde folgendes ändern:
- Wand 0x1A2B  „Außenwand EG Nord"   Layer: Wände-Bestand → Wände-Neubau
- Wand 0x1A2C  „Außenwand EG Süd"    Höhe: 2.80m → 3.20m

Ausführen? (ja / nein / details / abbrechen)
```

Pro Element zeigen wir die ID, einen sprechenden Namen (wenn vorhanden), und den geplanten Diff (alter Wert → neuer Wert).

### Format für > 10 Elemente — Summary mit Details-Option

```
Ich werde folgendes ändern:
- 127 Wände aus Layer „Wände-Bestand" → Klassifikation auf „Außenwand"
- 5 Wände → UNKLAR (manuelle Entscheidung nötig, siehe `details`)

Ausführen für die 127 eindeutigen? (ja / details / abbrechen)
```

`details` zeigt dem User die vollständige ID-Liste, falls er sie sehen will. Bei sehr großen Batches (z. B. 500+ Elemente) bleibt das Single-Confirm — **es gibt keine Obergrenze**, weil legitime Architekten-Workflows regelmäßig Mengen dieser Größe anfassen.

### Antwort-Optionen

- `ja` → ausführen
- `nein` → nicht ausführen, fragen, wie es weitergeht
- `details` → vollständiges Parameter-Schema bzw. ID-Liste zeigen, dann nochmal fragen
- `abbrechen` → kompletten Auftrag stoppen

### Was kein Confirm braucht

Diese Operationen ändern keinen Modell-Bestand und sind frei:

- View-Wechsel, Story-Wechsel, Zoom-Operationen.
- Selektion lesen oder setzen (UI-State, nicht Modell).
- Layer-Sichtbarkeit umschalten.
- Pen-Set / Vorgaben in der UI vorbereiten.

## Paginierung

Manche Listing-Operationen liefern Ergebnisse in mehreren Seiten. Genaue Felder-Konvention (`next_page_token` vs. `pagination.cursor` vs. `totalCount`) wird in Phase 2 live verifiziert. TODO.

**Grundregel:** Paginierung vollständig durchziehen, bis kein weiteres Token mehr kommt. Niemals auf einer partiellen Seite operieren — sonst werden Bulk-Operationen still inkorrekt (z. B. 200 von 300 Wänden klassifiziert, 100 bleiben mit alter Klasse zurück).

**Implementierung:**

1. Tool-Call ausführen.
2. Wenn Response ein Paginierungs-Token enthält: erneut aufrufen mit demselben Namen und Argumenten plus `page_token`-Feld.
3. Ergebnisse akkumulieren, bis kein Token mehr zurückkommt.
4. Erst dann mit FILTER / GROUP / CONFIRM (siehe [`bulk-operations.md`](bulk-operations.md)) fortfahren.

**Bei sehr großen Datasets** (>1.000 Elemente) zeigen wir dem User während der Akkumulation einen Fortschritts-Hinweis, falls der Tool-Call länger braucht.

## Port-Handling bei mehreren Archicad-Instanzen

`mcp__archicad__discovery_list_active_archicads` liefert eine Liste — pro Instanz `{port, projectName, projectType, archicadVersion, projectPath}`.

**Eine Instanz:** Port übernehmen, in Warm-up dokumentieren.

**Mehrere Instanzen:** User fragen, welche gemeint ist. Wir raten nicht anhand des Projektnamens — der User entscheidet. Ein Beispiel-Prompt:

```
Es laufen mehrere Archicad-Instanzen:
  [1] Port 19723 — „Wohnhaus_Mustermann"   (Archicad 29)
  [2] Port 19724 — „Bürogebäude_Süd"        (Archicad 29)

Welche soll ich für diesen Auftrag verwenden?
```

**Null Instanzen:** „Archicad scheint nicht zu laufen oder das MCP-Plugin ist inaktiv. Kann ich dich bitten, Archicad zu öffnen und das Plugin zu prüfen?" — dann stoppen.

## Verhalten bei „nein" oder Mid-Batch-Fehler

**User antwortet `nein` auf den Confirm-Dialog.** Wir stoppen den Auftrag sauber. Wir berichten, was wir *nicht* getan haben, und bieten an, die Operation zu verfeinern. Keine Teilausführung — wir machen entweder das volle Set oder gar nichts.

**MCP-Fehler mitten in einem Batch.** Wenn während eines APPLY-Schritts ein Einzel-Element scheitert:

- **Wir machen kein Auto-Rollback.** Archicads Undo ist und bleibt das Werkzeug des Users.
- Wenn der Fehler **systemisch** wirkt (z. B. „invalid GUID" für *alle* Elemente, nicht nur eines): sofort stoppen, vollständig berichten, User entscheiden lassen.
- Wenn der Fehler **einzeln** ist (z. B. ein Element wurde inzwischen vom User gelöscht): weitermachen mit den verbleibenden, am Ende vollständiger Report.
- **Format des Reports:** „127 verarbeitet, 124 erfolgreich, 3 Fehler bei IDs 0x1A2B, 0x1A2C, 0x1A30 — Gründe: A, B, C."

**Confirm war `ja`, aber das Ergebnis überrascht.** Element verschwindet, Eigenschaft sieht anders aus als erwartet: stoppen, lesen, berichten. Wir korrigieren nicht durch Folge-Operationen, weil wir nicht wissen, was der MCP-Server genau gemacht hat.
