# Bulk-Operations — Archicad-Skill

Diese Datei dokumentiert, wie wir Massen-Updates und Klassifizierungen durchführen. [SKILL.md](../SKILL.md) verweist hierher bei Operationen, die mehr als ein paar Elemente betreffen — also bei den meisten echten Architekten-Aufträgen.

## Inhaltsverzeichnis

1. [Das universelle Read → Filter → Group → Confirm → Apply-Pattern](#das-universelle-read--filter--group--confirm--apply-pattern)
2. [Klassifikations-Spezifika](#klassifikations-spezifika)
3. [Pro-Element-Typ-Ableitungslogik](#pro-element-typ-ableitungslogik)
4. [Mehrdeutigkeits-Handling](#mehrdeutigkeits-handling)
5. [Mid-Batch-Fehlerverhalten](#mid-batch-fehlerverhalten)
6. [TODO — Phase 5 Live-Verifikation](#todo--phase-5-live-verifikation)

## Das universelle Read → Filter → Group → Confirm → Apply-Pattern

Bei jeder Operation, die mehr als eine Handvoll Elemente anfasst, folgen wir diesem Ablauf. Jeder Schritt hat seinen Zweck — und das Auslassen eines Schrittes ist meist ein Fehler.

### 1. READ — Alle Elemente vom Zieltyp laden

Wir holen die vollständige Liste der relevanten Elemente. Wenn der MCP-Server paginiert, ziehen wir alle Seiten — siehe [mcp-conventions.md § Paginierung](mcp-conventions.md#paginierung). Niemals auf einer partiellen Seite weitermachen, sonst wird die Bulk-Operation still inkorrekt.

### 2. FILTER — Auf relevante Teilmenge eingrenzen

Aus der Gesamtmenge nehmen wir nur die, die der Auftrag wirklich meint. Filter-Kriterien sind typischerweise:

- **Story** — „nur Elemente im 1. OG".
- **Layer** — „nur Layer ‚Wände-Bestand'".
- **Geometrie** — „nur Wände länger als 3 m" oder „nur Stützen mit quadratischem Querschnitt".
- **Property / Klassifikation** — „nur Wände ohne zugewiesene Klassifikation".

Wir filtern lokal nach dem READ; nicht über mehrere Discovery-Calls verteilt, weil das das Problem in Partial-Page-Fallen verlagert.

### 3. GROUP — Pro Element die Ziel-Klasse oder Property ableiten

Das ist der kreative Schritt. Pro Element entscheiden wir, was die richtige Klassifikation oder das richtige Property-Set ist. Quellen-Kombinationen sind oben in Sektion 3 (Pro-Element-Typ-Ableitungslogik) aufgeführt.

Das Resultat ist ein In-Memory-Mapping: `{elementId → zielKlasse}` oder `{elementId → zielProperty}`.

### 4. CONFIRM — Asymmetrische Sicherheit greift

Updates und Property-Changes sind nicht frei (siehe SAFE-01 in SKILL.md). Wir zeigen dem User vor APPLY eine Summary:

- Anzahl pro Ziel-Klasse / Ziel-Property.
- Mehrdeutige Fälle separat aufgeführt — der User entscheidet pro Fall (siehe Mehrdeutigkeits-Handling).
- `details`-Option für die vollständige ID-Liste, falls der User sie sehen will.

Erst nach `ja` geht es weiter. Format-Beispiele siehe [mcp-conventions.md § Confirm-Format](mcp-conventions.md#confirm-format-für-schreibende-aufrufe).

### 5. APPLY — Pro Element via MCP setzen

Wir iterieren durch das Mapping und setzen pro Element die Klasse / das Property. Bei Einzel-Fehler weitermachen (kein Auto-Rollback), Report am Ende. Bei systemischem Fehler stoppen.

## Klassifikations-Spezifika

Klassifizierung ist ein häufiger Bulk-Auftrag und hat eigene Eigenheiten, die wir explizit kennen müssen.

### Klassifikations-System ist Projekt-spezifisch

Ein Archicad-Projekt kann mehrere Klassifikations-Systeme parallel führen: das ARCHICAD-eigene, Uniclass 2015, OmniClass, Custom. **Aktiv** ist immer nur eines. Wenn wir eine GUID aus dem inaktiven Uniclass-System holen und sie an einer Wand setzen, die das ARCHICAD-System nutzen soll, ist die BIM-Datenstruktur korrupt und IFC-Export geht schief — ohne dass der User es merkt.

**Regel: System-scoped GUID-Lookup.** Vor jedem Klartext → GUID-Mapping holen wir das aktive System (Warm-up Feld 7) und scope'n unsere Discovery-Query darauf. Niemals eine globale „suche eine Klasse namens ‚Innenwand'"-Query absetzen.

### Klassifikations-IDs sind GUIDs, kein Klartext

Wenn der MCP-Server uns sagt, eine Wand ist klassifiziert als Klasse mit GUID `xxxxxxxx-yyyy-...`, ist das die Wahrheit. Wir mappen erst beim Anzeigen für den User auf den sprechenden Namen.

Beim Setzen einer neuen Klassifizierung machen wir das umgekehrt: User nennt „Außenwand", wir holen die GUID aus dem aktiven System, dann setzen.

### Hierarchische Klassen

Manche Systeme haben Hierarchien („Wand" > „Außenwand" > „Sichtmauerwerk"). Beim Mapping nach „Außenwand" zielen wir auf das passende Level, nicht zwingend auf den Root. Wenn der User „klassifiziere als Außenwand" sagt und das aktive System hat „Außenwand" als Sub-Klasse von „Wand", nehmen wir die Sub-Klasse.

## Pro-Element-Typ-Ableitungslogik

Wie man pro Element entscheidet, welche Klasse er bekommt. Diese Algorithmen werden in Phase 5 live verifiziert und pro betroffenem Recipe als Worked Example konkretisiert.

### Wand — Innen vs. Außen

Quellen, kombinierbar:

- **Layer-Name.** „Wände-Außen" / „Wände-AW" → Außenwand. „Wände-Innen" / „Wände-IW" → Innenwand. Wenn ein Layer-Schema vorhanden ist und konsistent, ist das die schnellste und sicherste Quelle.
- **Geometrische Lage.** Wand-Position relativ zu Raum-Polygonen. Wenn beide Seiten an Innenräumen anliegen → Innenwand. Wenn eine Seite an einem Innenraum und die andere Seite frei oder an einem Außenraum / Außenkontur → Außenwand.
- **Verbundene Räume.** Wenn der MCP-Server Zone-Membership pro Wand-Seite zurückgibt: Wand zwischen Innen-Zone und Außen-Zone → Außenwand.

**Reihenfolge der Quellen:** Layer-Name zuerst (schnellste Heuristik), Geometrie/Zone fallback bei unklarem oder nicht-vorhandenem Layer-Schema. TODO Phase 5: exakte Algorithmik festlegen.

### Öffnung — Tür vs. Fenster vs. Wandöffnung

Quellen:

- **Subtype** des Library-Objects, das die Öffnung repräsentiert (Fenster-Bibliothekstyp → Fenster-Klasse, Tür-Bibliothekstyp → Tür-Klasse).
- **Höhenposition.** Tür meist mit Sill-Höhe nahe 0, Fenster mit Sill > 0. Heuristik, kein Beweis.

TODO Phase 5: ob Subtype reliable genug ist als alleinige Quelle.

### Stütze — tragend vs. nicht-tragend

Quellen:

- **Property „tragend" / „structural function"** falls vorhanden im Projekt.
- **Composite-Material.** Stützen aus Stahlbeton mit Bewehrungs-Composite → tragend; rein dekorative Holz-Stützen → nicht-tragend.

### Library-Object — Möbel / Sanitär / Beleuchtung / sonstiges

Quellen:

- **Subtype-Pfad** im Library-System (z. B. „Möbel/Sitzmöbel/Stuhl") gibt direkt die Kategorie.
- **Library-Kategorie** falls separater Property.

## Mehrdeutigkeits-Handling

Manche Elemente passen in keine eindeutige Klasse. Beispiele:

- Eine Wand grenzt sowohl an einen Innen- als auch an einen Außenraum (Garage zwischen Haus und Carport).
- Eine Stütze sitzt auf der Geschoss-Grenze — gehört sie zum EG oder OG?
- Ein Object hat keinen klaren Subtype-Pfad.

**Regel: NICHT raten.** Wir schließen solche Elemente vom automatischen Mapping aus und listen sie SEPARAT im Confirm-Dialog auf, mit Markierung „unklar". Der User entscheidet pro Fall, oder schließt sie für diesen Auftrag vom Bulk aus.

Beispiel-Confirm-Format:

```
Klassifizierung Wände — Vorschlag:
- 84 Wände → Innenwand
- 43 Wände → Außenwand
- 5 Wände → UNKLAR (manuelle Entscheidung nötig)

Soll ich für die 127 eindeutigen ausführen?
Die 5 unklaren möchtest du dann separat behandeln? (ja / details-eindeutig / details-unklar / abbrechen)
```

`details-unklar` zeigt die 5 Wände mit ID, Lage, und warum sie unklar sind („grenzt an Innenraum links und Außenraum rechts").

## Mid-Batch-Fehlerverhalten

Während APPLY scheitert ein Einzel-Element. Was tun?

- **Kein Auto-Rollback.** Archicads Undo bleibt das Werkzeug des Users — wir rollen nicht „heimlich" zurück.
- **Systemisch oder einzeln?** Wenn der Fehler systemisch wirkt (gleicher Fehler bei *allen* Elementen, etwa „invalid GUID format"): sofort stoppen, vollständig berichten.
- **Weiterlaufen bei Einzel-Fehlern.** Wenn der Fehler nur ein Element betrifft (etwa „element not found" — der User hat es zwischenzeitlich gelöscht): weitermachen mit dem Rest.
- **Report am Ende.** Format: „127 verarbeitet, 124 erfolgreich, 3 Fehler bei IDs A, B, C — Gründe: X, Y, Z." Damit kann der User entscheiden, ob er die 3 manuell oder mit einem erneuten Lauf nachreicht.

## TODO — Phase 5 Live-Verifikation

Diese Punkte werden in Phase 5 am laufenden Archicad geklärt:

- **Exakte Discovery-Query für System-scoped GUID-Lookup.** Was ist der Tool-Name? Welche Parameter? Wie unterscheidet sich die Antwort zwischen aktiven und inaktiven Systemen?
- **Geometrische „Innen vs. Außen"-Ableitung.** Welche MCP-Endpoints liefern Zone-Membership oder Polygon-Containment? Falls keine: müssen wir Geometrie selbst berechnen?
- **Innere-Raum-Erkennung.** Via Zone-Listing oder via Geometrie? Was ist effizienter und zuverlässiger im realen Projekt?
- **Subtype-Reliability bei Öffnungen.** Reicht der Subtype-Pfad alleine, oder brauchen wir Höhen-Fallback?
- **Pagination-Felder im Klassifikations-Response.** Liste der Klassen oft groß — wie ist die Pagination strukturiert?

Wenn Phase 5 abgeschlossen ist, ersetzen wir die TODO-Marker hier durch live-verifizierte Werte mit Datums-Marker (siehe [`self-improvement.md`](self-improvement.md)).
