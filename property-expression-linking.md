# Property-Expression-Linking

Pattern für die **automatische Synchronisation** zwischen einem Library-Object-Parameter (z. B. GDL-Stempel-Wert) und einer Element-Property. Eleganter als jeder Bulk-Update via MCP, weil der Property-Wert dem Quell-Parameter **dauerhaft** folgt — egal wer ihn ändert.

[SKILL.md](../SKILL.md) verweist hierher bei Anfragen wie „synchronisiere mir Property X mit Stempel-Wert Y" oder „warum sind die Property-Werte und die Stempel-Werte unterschiedlich?".

## Inhaltsverzeichnis

1. [Wann verwenden](#wann-verwenden)
2. [Wie es funktioniert (Archicad-Native)](#wie-es-funktioniert-archicad-native)
3. [User-Workflow im Property-Manager](#user-workflow-im-property-manager)
4. [Was Claude via MCP tun kann](#was-claude-via-mcp-tun-kann)
5. [Wann Linking NICHT geht](#wann-linking-nicht-geht)
6. [Gotchas](#gotchas)

## Wann verwenden

- **Symptom:** Property-Werte und GDL-Parameter-Werte einer Library-Object-Klasse (z. B. Zone-Stempel-Belag, Tür-Beschriftungs-Text) sind dauerhaft auseinandergelaufen. User editiert mal im Stempel, mal im Property-Inspector — beide bleiben out-of-sync.
- **Ziel:** Eine Single-Source-of-Truth herstellen. Üblicherweise: GDL-Parameter ist der „echte" Wert (vom Stempel-Library-Object verwaltet), Property ist die abgeleitete Größe für Schedules/IFC/Klassifizierung.
- **Vorteil gegenüber Bulk-Copy:** Linking ist **persistent** — neue Räume, geänderte Stempel — alles bleibt automatisch synchron, ohne wiederholten Bulk-Update.

## Wie es funktioniert (Archicad-Native)

Archicad bietet im Property-Manager einen **Expression-Editor** für jede Custom-Property. Statt eines festen Default-Werts kann eine **Expression** hinterlegt werden, die zur Laufzeit ausgewertet wird — z. B. auf den GDL-Parameter eines verbundenen Library-Objects zugreift.

Konzept:

```
Property „Bodenbelag" (Single-Enum oder String)
     ↑
     Expression: PARAMVALUE("flooring_type", REF(<stamp-object>))
     ↑
     GDL-Parameter „flooring_type" des Zone-Stempel-Objects
```

Bei jeder Eigenschafts-Abfrage (Schedule, IFC-Export, Property-Inspector) wird die Expression evaluiert. Property-Wert == GDL-Parameter-Wert. Immer.

## User-Workflow im Property-Manager

**Wir können den Property-Manager nicht via MCP automatisieren.** Das ist ein User-Workflow:

1. Archicad: Optionen → Property Manager öffnen.
2. Property „Bodenbelag" (oder welche auch immer) wählen.
3. Default Value Type von „Constant" / „User Defined" auf **„Expression"** umstellen.
4. Expression-Editor öffnet sich. GDL-Parameter referenzieren — typische Syntax:
   ```
   PARAMVALUE("<param-name>", <element-ref>)
   ```
   Wobei `<param-name>` der interne Name des GDL-Parameters (NICHT der Display-Name) und `<element-ref>` ein Reference-Identifier des Library-Objects ist.
5. **Test:** Validate-Button im Expression-Editor zeigt Syntax-Fehler.
6. **Apply Scope:** Die Expression kann für die ganze Property gelten, oder per Klassifikations-Item gescoped werden (z. B. nur für Klassifikation „Innenraum").
7. OK schließt den Dialog und persistiert die Expression.

**Wichtig:** Während der Property-Manager geöffnet ist, blockiert er den MCP. Claude muss warten, bis der User „fertig" meldet (siehe [mcp-conventions.md § Modal-Dialoge](mcp-conventions.md#modal-dialoge-in-archicad-blockieren-mcp)).

## Was Claude via MCP tun kann

**Vorher (Vorbereitung):**
- GDL-Parameter-Namen ermitteln: `mcp__archicad__elements_get_gdl_parameters_of_elements` an einem Beispiel-Library-Object. Rückgabe enthält die internen `name`, `index`, `type` der Parameter.
- Property-Definition lesen (über Element-Property-Listing) um zu prüfen, ob die Property bereits eine Expression hat oder ein fester Default-Wert.
- Klassifikations-Scope ermitteln, falls relevant.

**Nachher (Verifikation):**
- Property-Werte an Stichproben-Elementen abfragen: `mcp__archicad__properties_get_property_values_of_elements`.
- Vergleich mit GDL-Parameter-Werten der gleichen Elemente: stimmen sie überein? Wenn ja, Linking funktioniert.
- Bei Mismatch: nicht selbst korrigieren — User informieren, dass Expression vermutlich nicht greift (Scope-Problem, Syntax-Fehler, Element nicht im Geltungsbereich).

**Was Claude NICHT tun kann:**
- Expression im Property-Manager via MCP setzen — kein MCP-Endpoint dafür (Stand AC29).
- GDL-Parameter-Namen aus dem Library-Object „erraten" — immer per Discovery-Call lesen.

## Wann Linking NICHT geht

- **Property ist Built-in** (z. B. ARCHICAD-Standard-Properties wie „Volumen") — Expression-Editor nicht verfügbar.
- **Library-Object ist ein Custom-GDL ohne ordentliche Parameter-Deklaration** — kein PARAMVALUE-Zugriff möglich.
- **Source und Target sind auf unterschiedlichen Element-Typen** (z. B. Wand-Property soll von Zone-GDL kommen) — die Expression-Engine erlaubt typischerweise nur Self-Reference oder Verwandte-Elemente.
- **User braucht historische Werte** (Property soll *eingefroren* bleiben, GDL aber editierbar) — dann kein Linking, sondern bewusster Bulk-Copy.

In diesen Fällen: Fallback auf [`schedule-pipeline.md`](schedule-pipeline.md) oder einmaligen Property-Bulk-Update.

## Gotchas

- **Expression-Auswertung ist lazy** — Property-Werte werden erst bei nächster Eigenschaft-Abfrage neu berechnet. Schedule sofort regenerieren nach Linking-Einrichtung.
- **GDL-Parameter-Namen** sind nicht die Display-Namen. PARAMVALUE braucht den internen Namen, nicht „Bodenbelag" sondern z. B. „flooring_type".
- **Klassifikations-Scope kann Linking auf wenige Elemente einschränken** — wenn die Property nur für Klassifikation „Wohnen" eine Expression hat, bleiben Räume mit anderer Klassifikation auf dem Default-Wert.
- **Beim Library-Object-Wechsel** (User tauscht Stempel-Typ): Expression kann brechen, weil neuer Library-Object andere Parameter hat. Empfehlung: nach jedem Library-Object-Replace die Property-Werte stichproben-prüfen.
- **Modal-Dialog (Property-Manager) blockiert MCP** — siehe [mcp-conventions.md](mcp-conventions.md).
