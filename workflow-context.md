# Workflow-Context — Archicad-Skill

Diese Datei dokumentiert die 7 Kontext-Felder, die wir vor einem Archicad-Auftrag holen (Warm-up). [SKILL.md](../SKILL.md) verweist hierher, wenn ein Auftrag mehr Kontext braucht oder ein bestimmtes Feld unklar ist.

## Inhaltsverzeichnis

1. [Warm-up-Regeln](#warm-up-regeln)
2. [Feld 1: Port der aktiven Archicad-Instanz](#feld-1-port-der-aktiven-archicad-instanz)
3. [Feld 2: Projekt-Identität](#feld-2-projekt-identität)
4. [Feld 3: Längeneinheit](#feld-3-längeneinheit)
5. [Feld 4: Aktive Story](#feld-4-aktive-story)
6. [Feld 5: Sichtbare Layer](#feld-5-sichtbare-layer)
7. [Feld 6: Pen-Set / Aktive Stiftnummer](#feld-6-pen-set--aktive-stiftnummer)
8. [Feld 7: Aktives Klassifikations-System](#feld-7-aktives-klassifikations-system)
9. [Erweiterte Listings (STORY-01, ATTR-01)](#erweiterte-listings-story-01-attr-01)

## Warm-up-Regeln

- **Pro Auftrag einmal durchgehen**, dann für die Dauer des Auftrags cachen.
- Bei einem **neuen Auftrag** starten wir frisch — Cache wird verworfen.
- Das **Story-Feld ist volatil**. Wenn der User in seiner Aufgabe einen Floor erwähnt („im OG", „im 1. OG", „im EG"), ziehen wir es neu und nutzen nicht den Cache — der User könnte zwischenzeitlich in Archicad das Geschoss gewechselt haben.
- Die Felder **6 (Pen-Set) und 7 (Klassifikations-System)** ziehen wir nur, wenn der Auftrag sie tatsächlich braucht — also bei 2D-Arbeit bzw. Klassifizierung. Bei reinen Bauteil-Operationen entfallen sie.
- Wenn ein Feld nicht verfügbar ist (Tool fehlt, Discovery liefert nichts): **dokumentieren, weitermachen — nicht blockieren**. Operationen, die das Feld nicht brauchen, laufen weiter.

## Feld 1: Port der aktiven Archicad-Instanz

**Tool-Name:** `mcp__archicad__discovery_list_active_archicads` — bereits verifiziert (Top-Level-Tool des MCP-Servers, in Manifest dokumentiert).

**Erwartete Parameter:** keine.

**Erwartete Rückgabe:** Liste mit Objekten `{port, projectName, projectType, archicadVersion, projectPath}` — eine Zeile pro laufender Instanz.

**Verhalten:**
- Eine Instanz → Port übernehmen.
- Mehrere Instanzen → User fragen (siehe [mcp-conventions.md § Port-Handling](mcp-conventions.md#port-handling-bei-mehreren-archicad-instanzen)).
- Null Instanzen → User informieren, stoppen.

## Feld 2: Projekt-Identität

Wir bekommen Projektname, -typ, Archicad-Version, -Pfad bereits aus Feld 1. Das reicht meist.

**Falls ein separater Project-Info-Call gebraucht wird** (z. B. für interne Projekt-ID, Erstelldatum, Vorlage):

- **Discovery-Query (zum Probieren):** „project info" / „get project metadata" / „active project"
- **Tool-Name:** TODO — in Phase 2 live verifizieren.
- **Erwartete Parameter:** TODO (vermutlich `port`).
- **Erwartete Rückgabe:** TODO.

## Feld 3: Längeneinheit

Bestimmt, ob wir Wand-Längen in `m`, `mm`, `ft` etc. übergeben.

- **Discovery-Query (zum Probieren):** „project units" / „preferences" / „active preferences" / „working units"
- **Tool-Name:** TODO — in Phase 2 live verifizieren.
- **Erwartete Parameter:** `port`.
- **Erwartete Rückgabe:** Einheit als Code (`m` / `mm` / `ft` / `in` / ...) oder als strukturiertes Objekt mit Länge / Fläche / Volumen-Einheiten getrennt.

**Wichtig.** Wenn der User in seiner Aufgabe eine Einheit explizit erwähnt („mach das in cm"), nehmen wir seine Erwähnung, und Archicad-internes Setting kommt nur als Fallback zum Einsatz.

## Feld 4: Aktive Story

**Volatil — bei Floor-Erwähnung neu ziehen.** Siehe Warm-up-Regeln.

- **Discovery-Query (zum Probieren):** „current story" / „active story" / „get story by index" / „working story"
- **Tool-Name:** TODO — in Phase 2 live verifizieren.
- **Erwartete Parameter:** TODO.
- **Erwartete Rückgabe:** `{storyIndex, name, elevation, height}` — Index (oft 0-basiert oder 1-basiert je nach Implementation), sprechender Name (z. B. „EG", „1. OG"), Höhe-Z-Koordinate (Elevation), Geschoss-Höhe.

## Feld 5: Sichtbare Layer

Brauchen wir für SAFE-03 (Layer-not-visible-Ausnahme) und für Layer-Zuweisung bei Create-Operationen.

- **Discovery-Query (zum Probieren):** „visible layers" / „layer visibility" / „active layer combination"
- **Tool-Name:** TODO — in Phase 2 live verifizieren.
- **Erwartete Parameter:** `port`.
- **Erwartete Rückgabe:** Liste mit `{layerId, name, visible, locked}` für alle Layer; oder gefilterte Liste nur der sichtbaren.

**Hinweis.** Sichtbarkeit hängt oft an der aktiven *Layer-Kombination*. Wenn der MCP-Server eine Kombination zurückgibt, behandeln wir diese als Quelle der Wahrheit für „sichtbar".

## Feld 6: Pen-Set / Aktive Stiftnummer

**Nur bei 2D-Arbeit ziehen** (Linien, Polylinien, Schraffuren).

- **Discovery-Query (zum Probieren):** „active pen set" / „current pens" / „project pens"
- **Tool-Name:** TODO — in Phase 2 live verifizieren.
- **Erwartete Parameter:** TODO.
- **Erwartete Rückgabe:** TODO — vermutlich Liste mit `{penIndex, color, weight, name}` plus Markierung der aktiven Stiftnummer.

## Feld 7: Aktives Klassifikations-System

**Nur bei Klassifizierungs-Auftrag ziehen.** Kritisch — ein falsches System führt zu GUID-Cross-Pollution (Pitfall P5 aus der Research): wir setzen eine Klasse aus einem inaktiven System (z. B. Uniclass), aber das aktive ist ARCHICAD-eigen, und am Ende ist die BIM-Datenstruktur korrupt.

- **Discovery-Query (zum Probieren):** „classification system" / „active classification" / „available classifications" / „project classifications"
- **Tool-Name:** TODO — in Phase 5 live verifizieren.
- **Erwartete Parameter:** TODO.
- **Erwartete Rückgabe:** Liste mit `{systemId, name, version}` für alle im Projekt vorhandenen Systeme; aktives System klar markiert.

**Wichtige Folgeaufrufe für Phase 5:**

- Klartext → GUID-Mapping für eine bestimmte Klasse innerhalb des aktiven Systems. Discovery-Query (Versuch): „classification by name" / „class id by name". TODO.
- Hierarchie eines Systems abfragen (für sub-klassen). TODO.

## Erweiterte Listings (STORY-01, ATTR-01)

Phase 2 ergänzt zwei Sub-Recipes, die hier kurz skizziert sind und dort detailliert werden:

### STORY-01 — Alle Stories listen

Brauchen wir, wenn ein Auftrag über mehrere Geschosse läuft („zieh die Außenwand bis 3. OG durch") oder ein Listing pro Story angefragt ist.

- **Discovery-Query (zum Probieren):** „list stories" / „all stories" / „get stories"
- **Tool-Name:** TODO — in Phase 2 verifizieren.

### ATTR-01 — Attribute listen

Brauchen wir, wenn wir Properties zuweisen — Layer-Index aus Name, Surface-Index aus Name, Composite-Index aus Name. Ohne diese Sub-Recipes wäre jede Zuweisung ein Discovery-Detour.

- Layer-Listing: Discovery-Query (Versuch) „list layers" / „all layers" / „project layers" — TODO.
- Surfaces-Listing: Discovery-Query (Versuch) „list surfaces" / „project surfaces" — TODO.
- Composites-Listing: Discovery-Query (Versuch) „list composites" / „building materials" — TODO.
- Fills-Listing: Discovery-Query (Versuch) „list fills" / „fill types" — TODO.
- Line-Types-Listing: Discovery-Query (Versuch) „list line types" — TODO.
