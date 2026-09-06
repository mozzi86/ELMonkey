# MCP-Conventions — Archicad-Skill

Diese Datei dokumentiert, wie wir mit dem Archicad-MCP-Server umgehen. [SKILL.md](../SKILL.md) lädt sie bei Bedarf — wenn ein neuer Elementtyp auftaucht, eine Discovery fehlschlägt oder das Confirm-Format gebraucht wird.

## Inhaltsverzeichnis

1. [Discovery-Pattern im Detail](#discovery-pattern-im-detail)
2. [Fehlerklassen und Reaktionen](#fehlerklassen-und-reaktionen)
3. [Confirm-Format für schreibende Aufrufe](#confirm-format-für-schreibende-aufrufe)
4. [Paginierung](#paginierung)
5. [Port-Handling bei mehreren Archicad-Instanzen](#port-handling-bei-mehreren-archicad-instanzen)
6. [Live-verifizierte Element-Create-Capabilities (AC29)](#live-verifizierte-element-create-capabilities-ac29)
7. [Modal-Dialoge in Archicad blockieren MCP](#modal-dialoge-in-archicad-blockieren-mcp)
8. [Direkter HTTP-Zugriff auf die JSON-API (MCP-Bypass)](#direkter-http-zugriff-auf-die-json-api-mcp-bypass)
9. [Verhalten bei „nein" oder Mid-Batch-Fehler](#verhalten-bei-nein-oder-mid-batch-fehler)

## Discovery-Pattern im Detail

[SKILL.md](../SKILL.md) skizziert das 5-Schritt-Pattern. Hier die Erweiterungen für schwierige Fälle.

**Synonym-Retry.** Wenn die erste Discovery-Query nichts Brauchbares liefert, probieren wir Synonyme. Beispiel-Familien:

- *create* — „create", „add", „insert", „new", „place"
- *delete* — „delete", „remove", „erase"
- *modify* — „modify", „update", „change", „edit", „set"
- *list* — „list", „get", „retrieve", „query", „find"

**Genauigkeit der Query.** Vage Queries liefern vage Ergebnisse. „Wand" allein ist zu wenig — besser „create wall element on story" oder „get all walls in current story". Wir nehmen Verb + Objekt + Kontext.

**Schema-Keyword-Anreicherung bei Fehltreffern.** Der Discovery-Index von tapir-archicad-MCP indexiert **nicht nur Namen und Beschreibungen, sondern auch Parameter-Schema-Keywords** (Feld: `f"{title}: {description} Keywords: {schema_keywords}"` — verifiziert im upstream `search_index.py`, Model `all-MiniLM-L6-v2`). Wenn eine Verb-Objekt-Query nicht trifft, konkrete Schema-Feldnamen anhängen — sie wirken als semantische Anker. <!-- 2026-07-09 -->

- „get walls" → nichts Brauchbares? Probier „get walls elementType filter" oder „get walls WallSettings".
- „set property" → probier „set property values elementPropertyValues".
- „classify elements" → probier „classify elements classificationSystemId".
- „get element details" → probier „get element details geometryType structureType".

Merke: der Trick wirkt am stärksten, wenn ein Schema-Feld einen ungewöhnlich spezifischen Namen hat (`elementPropertyValues`, `classificationSystemId`, `begCoordinate`, `endCoordinate`) — die haben quasi keinen Wettbewerb im Index.

**Mehrdeutige Treffer.** Wenn die Discovery mehrere Kandidaten gleichermaßen passend zurückgibt: nicht raten. Eine zweite, engere Query stellen oder den User fragen, welcher Tool-Name gemeint sein soll.

**Negativ-Schluss nach 2 Versuchen.** Wenn zwei Discovery-Queries mit Synonymen *keinen* plausiblen Treffer für die gewünschte Operation liefern, und auch ein direkter Aufruf mit dem vermuteten Namen `Tool not found in registry` zurückgibt, dann existiert das Tool **nicht**. Nicht endlos suchen — den User informieren und nach einer alternativen Vorgehensweise fragen. Konkretes Beispiel: `elements_create_walls` existiert nicht in Archicad MCP v29 (siehe Capability-Tabelle unten).

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
| **MCP-Call hängt oder timeoutet ohne Response** | **Modal-Dialog blockiert Archicad** — User bitten, offene Dialoge zu prüfen und sauber zu schließen (siehe Sektion „Modal-Dialoge"). | 0 |
| **`Invalid program status (there is an open modal dialog: <Dialog-Name>)` — Code 4001** | Spezifische Form des Modal-Block-Fehlers; Dialog-Name aus Fehlermeldung extrahieren + an User weiterleiten. | 0 |
| **Leere Liste von `discovery_list_active_archicads`, obwohl Archicad nachweislich läuft** | **Modaler Dialog, kein MCP-Defekt** — die Discovery verwandelt den 4001-Fehler still in eine leere Liste: der offene Dialog (Add-On-Manager, Element-Einstellungen …) blockiert die JSON-API, `connect.all()` bekommt keine Projekt-Infos, der unvollständige Header wird von `is_header_fully_initialized()` weggefiltert → leere Liste ohne jede Fehlermeldung („blinde Null"). **Diagnose-Einzeiler:** `curl -s -X POST http://127.0.0.1:19723 -d '{"command":"API.GetProductInfo"}'` — nennt den blockierenden Dialog im Klartext. Kein Prozess-Neustart nötig; User bitten, den Dialog zu schließen, dann Discovery wiederholen. <!-- 2026-08-05, live beim 0.9.11-Update --> | 1 nach Dialog-Schluss |
| **Pydantic-Validierungs-Fehler in Response** (z. B. `extra_forbidden` für `structureType` / `geometryType`) | **AC29-Schema-Drift-Bug** in `elements_get_details_of_elements` — Workarounds in der Capability-Tabelle unten + Memory-Eintrag `issue_archicad_mcp_get_details_bug.md`. Property-basiertes Lesen verwenden. | 0 |
| **`elements_get_gdl_parameters_of_elements` schlägt fehl mit ~tausenden Validierungs-Fehlern** | **AC29-Schema-Drift-Bug** — Pydantic-Validierung im MCP-Wrapper, nicht in Archicad. **Bester Workaround: direkter HTTP-Zugriff** auf die rohe JSON-API (siehe Sektion „Direkter HTTP-Zugriff") — die rohe Antwort ist parsebar. Alternativ Schedule-Export-Pipeline (`schedule-pipeline.md`). <!-- 2026-06-11 --> | 0 |
| **`elements_get_details_of_elements` für Zone wirft Schema-Mismatch** | Zone-Detail-Schema fehlt im MCP-Wrapper. Server liefert `name`, `numberStr`, `categoryAttributeId`, `stampPosition`, `isManual`, `zCoordinate`, `polygonOutline`, `holes`, aber kein `ZoneDetails`-Variant existiert. | 0 |
| **`project_get_stories` (GetStories) wirft Pydantic `extra_forbidden` für `height`** | **AC29-Schema-Drift-Bug** wie bei `get_details`: Server liefert pro Story zusätzlich `height`, der MCP-Wrapper verbietet das Feld. **Workaround: direkter HTTP-Zugriff** — Tapir-Command `GetStories` via `API.ExecuteAddOnCommand` liefert `stories` sauber inkl. `index`, `level`, `height`, `name`, `actStory` (siehe Sektion „Direkter HTTP-Zugriff"). Live verifiziert im THN-Teamwork-Projekt. <!-- 2026-07-17 --> | 0 |
| **Leere `properties: []`-Liste an `properties_get_property_values_of_elements`** | Server trennt die Verbindung hart (`RemoteDisconnected` / leere Response, Länge 0) und kann im Worst Case den JSON-Port der Instanz lahmlegen, bis sie reaktiviert wird. **Vor jedem Werte-Read prüfen, dass die Property-ID-Liste nicht leer ist** (z. B. wenn ein Elementtyp keine user-defined Properties hat). <!-- 2026-06-11 --> | 0 |
| **`TeamWork permission denied` — Code 6001** (im `executionResults`-Eintrag, nicht im Top-Level-Fehler) | **Teamwork-Projekt: Element ist nicht für dich reserviert.** Vor jedem schreibenden Aufruf (`SetPropertyValuesOfElements`, Klassifizierung, Geometrie-Update) die Ziel-Elemente reservieren: MCP-Tool **`teamwork_reserve_elements`** (`params.elements` = Liste `{elementId:{guid}}`), Erfolg = `executionResult.success:true`. Danach Write wiederholen. (`API.ReserveElements` existiert NICHT als JSON-Command — nur über das MCP-Tool bzw. Tapir.) <!-- 2026-06-18 --> | 1 nach Reserve |
| **`Port <n> is not an active Archicad connection`** beim `archicad_call_tool` (obwohl roher HTTP auf den Port antwortet) | Der MCP-Server hat seinen Instanz-Cache verloren. **`discovery_list_active_archicads` einmal aufrufen** — danach kennt der Server den Port wieder und der Call klappt. | 1 nach Discovery |
| **Reserve meldet `success:true`, der folgende Write scheitert trotzdem mit `6001`** | **Reserve ≠ Schreibrecht.** `ReserveElements` (Tapir/MCP) kann pauschal `success:true` melden, obwohl einzelne Ziel-Elemente faktisch nicht beschreibbar sind — typische Ursachen: **gesperrter Layer** (`ModelView_IsLayerLocked`), **Fremdvorbehalt** eines anderen Teamwork-Nutzers, oder Hotlink-Element. Nicht im Loop weiter-reservieren (bringt nichts). Stattdessen: betroffene GUIDs identifizieren, Layer-Lock / Vorbehalt prüfen, dem User melden (Layer entsperren / Element freigeben lassen). Live AC29: 2 Träger auf Layer „00 Hilfskonstruktion" blieben so blockiert. <!-- 2026-06-19 --> | 0 (User-Aktion nötig) |

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

**Bei sehr großen Datasets** (>1.000 Elemente) zeigen wir dem User während der Akkumulation einen Fortschritts-Hinweis, falls der Tool-Call länger braucht. **Achtung Context-Explosion:** Die MCP-Pagination schreibt jede 100er-Seite als JSON-Block in den Claude-Context — bei tausenden Elementen sprengt das den Context. Für solche Massen-Reads den **direkten HTTP-Zugriff** wählen (siehe Sektion „Direkter HTTP-Zugriff"): die rohe `API.*`-Antwort ist unpaginiert und wird in einem lokalen Script verarbeitet, sodass nur die Zusammenfassung zurückkommt. <!-- 2026-06-11 -->

**Paginierungs-Sessions verfallen.** <!-- 2026-07-06 --> Ein `page_token` ist an eine serverseitige Paginierungs-Session gebunden, die nach einigen Minuten bzw. nach zwischengeschobenen anderen Calls abläuft — dann kommt `Pagination session expired. Please start a new request.` und man muss **von Seite 1 neu** durchlaufen (Tokens sind nicht wiederaufsetzbar). Konsequenz: Seiten **direkt hintereinander** abrufen, keine anderen Operationen dazwischen. Live-Fall: 11-Seiten-Liste (1.091 Wände) verfiel, weil zwischen Seite 5 und 6 Property-Reads liefen. Bei Listen dieser Größe ohnehin besser gleich der HTTP-Bypass (unpaginiert, ein Aufruf).

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

### Ports sind volatil — Port ≠ Datei <!-- 2026-06-11 -->

Die JSON-/MCP-Ports (typisch ab 19723 aufwärts) sind **nicht stabil einer bestimmten Datei zugeordnet**. Wenn eine Instanz geschlossen und wieder geöffnet wird — oder eine andere Instanz dazwischen startet/stoppt — kann **dieselbe Datei beim nächsten Mal auf einem anderen Port liegen**. Live beobachtet: eine Datei wanderte nach einem Schließen/Öffnen von Port 19724 auf 19725; der vorher belegte Port war danach tot.

**Regel:** Niemals „Port X = Datei Y" über die Dauer einer Session hinaus annehmen oder aus einer früheren Session übernehmen. Vor jeder Operation, die den Port aus dem Warm-up wiederverwendet, **die offene Datei verifizieren** — via `GetProjectInfo` (Tapir) und Abgleich des `projectName`/`projectPath`. Wenn der erwartete Port keine Antwort gibt: nicht raten, alle aktiven Instanzen neu auflisten und den `projectName` matchen.

**Port → Prozess zuordnen, bevor man CPU oder Zustand misst.** <!-- 2026-09-03 --> Bei mehreren Instanzen zeigt `ps` mehrere gleichnamige `Archicad`-Prozesse; welcher zu welchem Port gehört, sagt nur `lsof -nP -iTCP:<port> -sTCP:LISTEN`. Live-Fehldiagnose: „7 % CPU, Zustand S → vermutlich modaler Dialog“ — gemessen war aber die THN-Instanz (19723, PID 9015); die tatsächlich betroffene Möbel-Instanz (19724, PID 14543) lief mit 95 % im Zustand R, also rechnend, nicht wartend. Es liefen **drei** Instanzen (19725 = PID 14850), obwohl nur zwei erwartet waren.

**Symptom für „Port tot / verschoben":** `IsAlive` liefert nichts oder die Verbindung ist refused, obwohl Archicad sichtbar läuft. Dann: aktive Instanzen neu auflisten, korrekten Port per Projektname bestimmen.

## Live-verifizierte Element-Create-Capabilities (AC29)

Diese Tabelle dokumentiert, welche Element-Typen via MCP **tatsächlich erstellt** werden können. Stand der Live-Probe vom 2026-05-19 gegen Archicad 29.

| Element-Typ | Create-Tool | Status |
|---|---|---|
| Slabs (Decken) | `mcp__archicad__elements_create_slabs` | ✓ verifiziert <!-- 2026-05-19 --> |
| Columns (Stützen) | `mcp__archicad__elements_create_columns` | ✓ Tool existiert (Schema gesehen) <!-- 2026-05-19 --> |
| Objects (Möbel, Sanitär, GDL) | `mcp__archicad__elements_create_objects` | ✓ Tool existiert (Schema gesehen) <!-- 2026-05-19 --> |
| Polylines (2D) | `mcp__archicad__elements_create_polylines` | ✓ verifiziert mit Aufruf <!-- 2026-05-19 --> |
| Meshes (Terrain) | `mcp__archicad__elements_create_meshes` | ✓ Tool existiert (Schema gesehen) <!-- 2026-05-19 --> |
| Zones (Räume) | `mcp__archicad__elements_create_zones` | ✓ verifiziert mit Aufruf <!-- 2026-05-19 --> |
| Walls | — | ✗ **kein Create-Tool im MCP v29** <!-- 2026-05-19 --> |
| Beams | — | ✗ kein Create-Tool im MCP v29 <!-- 2026-05-19 --> |
| Windows / Doors / Wandöffnungen | — | ✗ kein Create-Tool im MCP v29 <!-- 2026-05-19 --> |
| Curtain Walls | — | ✗ kein Create-Tool im MCP v29 <!-- 2026-05-19 --> |
| Fills / Hatches (eigenständig) | — | ✗ kein Create-Tool im MCP v29 <!-- 2026-05-19 --> |
| Standalone Lines | — | ✗ nur Polylines verfügbar <!-- 2026-05-19 --> |
| Stairs / Railings / Morphs / Shells / Skylights / Roofs | — | ✗ kein Create-Tool im MCP v29 <!-- 2026-05-19 --> |

**Was stattdessen geht für „nicht erstellbare" Typen:** Modifikation existierender Elemente via `mcp__archicad__elements_set_details_of_elements` mit dem typ-spezifischen Schema (z. B. `WallSettings` für Wände). Read + Update + Delete sind durchgängig verfügbar, nur Create fehlt selektiv.

### Zusätzliche AC29-Schema-Drift-Bugs (live verifiziert 2026-05-20)

Zwei MCP-Tools sind durch Pydantic-`extra='forbid'`-Validierung in AC29 unzuverlässig:

| Tool | Bug | Workaround |
|---|---|---|
| `mcp__archicad__elements_get_gdl_parameters_of_elements` | `index` als int statt string; extra Server-Felder `displayName`, `isLocked`, `flags`. Bei Zonen mit ~849 GDL-Params → ~4.000 Validierungs-Fehler. | **Schedule-Export-Pipeline** (siehe `schedule-pipeline.md`) — User exportiert Schedule mit GDL-Werten als XLSX/CSV, wir parsen lokal. |
| `mcp__archicad__elements_get_details_of_elements` für `elementType=Zone` | Wrapper kennt nur `WallDetails` / `BeamDetails` / `ColumnDetails` / `MeshDetails` / `NotYetSupportedElementType` — kein `ZoneDetails`-Variant. Server liefert Zone-Felder (name, numberStr, categoryAttributeId, stampPosition, isManual, zCoordinate, polygonOutline, holes), die kein Schema matchen. | Property-basiertes Lesen via `properties_get_property_values_of_elements` + Standard-Properties (Raumnummer, Name, Kategorie). |

**Attributes/Properties** sind ebenfalls erstellbar:

| Attribute-Typ | Create-Tool |
|---|---|
| Building Materials | `mcp__archicad__attributes_create_building_materials` |
| Composites | `mcp__archicad__attributes_create_composites` |
| Attribute Folders | `mcp__archicad__attributes_create_attribute_folders` |
| Property Groups | `mcp__archicad__properties_create_property_groups` |

**Wenn der User „erstelle eine Wand" sagt:** Erkläre die MCP-Limitation, schlage Alternativen vor:

- **Zone** für Raum-Semantik (Flächenberechnung, Stempel, Schedule-fähig).
- **Polylinien** für visuellen 2D-Rahmen (keine BIM-Wand, aber sichtbar).
- **Modifikation einer existierenden Wand**, falls eine vorhanden ist.

Nicht stillschweigend substituieren — der User soll wissen, dass es keine echte Wand wird.

## Modal-Dialoge in Archicad blockieren MCP

Wenn Archicad einen modalen Dialog offen hat (z. B. „Available Parameters", Property-Manager, Element-Settings-Dialog, Materialzuweisungs-Dialog), **frieren MCP-Calls ein oder timeouten**, weil Archicads UI-Thread blockiert ist. Discovery- und Lese-Calls verhalten sich genauso.

**Symptom:** Tool-Call hängt > 10 Sekunden ohne Response, oder Server gibt explizit `Invalid program status (there is an open modal dialog: <Dialog-Name>)` mit Code 4001 zurück.

**Reaktion:**
1. **NICHT mehrfach retry** — bringt nichts, blockiert nur weiter.
2. Dialog-Name aus der Fehlermeldung extrahieren (in den Klammern) und an User weiterleiten — er sieht so direkt, welcher Dialog gemeint ist.
3. User informieren: „Ein modaler Dialog ist in Archicad offen: `<Dialog-Name>`. Bitte schließen — OK wenn Änderungen behalten werden sollen, sonst Abbrechen."
4. **WICHTIG: Daten-Verlust-Risiko erinnern**, wenn der Dialog Property-Definitions, Enum-Werte, GDL-Parameter o. ä. editiert. Abbrechen verwirft die Bearbeitung.
5. **Selektion-Risiko erinnern:** Bei manchen Dialogen (z. B. „Einstellungen für die Raum-Auswahl") verschwindet die aktive Selektion beim Schließen via Abbrechen. Vor dem Schließen ggf. die Element-GUIDs sichern, oder „Neu-Selektieren" einplanen.
6. Nach dem User-Schließen: User mit „fertig" antworten lassen, dann den Call wiederholen.

**Bekannte Modal-Dialog-Auslöser (live verifiziert 2026-05-20):**
- „Available Parameters" Browser (Eigenschaft-Manager → Expression-Editor → GDL-Parameter referenzieren)
- „Einstellungen für die Raum-Auswahl" (Zonen-Multi-Edit-Dialog) — **Selektion geht beim Abbrechen verloren!**
- „Optionen-Setup" (Enum-Wert-Editor für Property-Definitions)
- Property-Definition-Edit-Dialog
- Element-Settings-Dialog (Wand, Stütze, Decke etc.)
- Material-/Composite-/Profile-Editor
- Klassifikations-System-Manager
- Layer-Manager

**Empfehlung im Recipe-Workflow:** Bei länger laufenden Bulk-Operationen den User vor Start kurz erinnern: „Bitte schließe offene Dialoge, sonst hängt der MCP-Server."

### Auch ein beschäftigtes Modell blockiert — `IsAlive=true`, aber Queries timeouten <!-- 2026-06-11 -->

Nicht nur modale Dialoge blockieren. Auch ein **aktiv bearbeitetes / beschäftigtes Modell** sperrt die JSON-API: aktives Klicken/Navigieren im Fenster, eine laufende Selektion, ein Hintergrund-Rebuild oder eine große Berechnung. Tückisch: `IsAlive` antwortet dabei **sofort mit `true`**, während echte Modell-Queries (selbst kleine wie `GetElementsByType` auf wenige Elemente) hängen, timeouten oder leere Responses liefern.

**Diagnose-Idiom:** Erst `IsAlive` (muss `true` sein), dann ein **kleiner** Modell-Call (z. B. `GetElementsByType` auf einen Typ mit wenigen Elementen). Schlägt der kleine Call fehl, obwohl `IsAlive=true` → Modell ist busy, **nicht** die Verbindung tot.

**Reaktion:** Nicht in einer Schleife retryen — das blockiert nur weiter. User bitten, das Modell kurz **idle** zu lassen (einmal auf leere Stelle klicken, damit nichts selektiert/aktiv ist, keine offene Dialogbox), dann den Call wiederholen. Große Payloads (Projekte mit vielen tausend Elementen) brauchen ohnehin längere Timeouts (≥120–180 s) und vertragen einen kleinen Retry mit Pause.

### Renderer-Hänger: ein Element blockiert die API minutenlang <!-- 2026-09-03 -->

Ein einzelner Render-Call kann die Instanz für Minuten sperren. Live: `GetElementPreviewImage` auf `Pflanze_Mittel_1800` (Alt-Pflanzen-Mesh) hielt die Möbel-Instanz **~7 min bei 95–120 % CPU** — in der Zeit blieb auch `GetProjectInfo` ohne Antwort. Weitere Verdachtsfälle derselben Art: `Pflanze_Mittel_1800_Bambus`, `Pflanze_klein_12001`, `Pflanzentopf_Klein_D21`, `INU012 (2)_HD`, `RunderTisch faces`. Normale Objekte brauchen 0,1–30 s.

**Regeln für Render-Batches:**
1. Erst ein bekannter Positivfall, dann der Batch — einzeln, mit kurzem Client-Timeout.
2. **Beim ersten Hänger abbrechen.** Weiterlaufen erzeugt nur eine Timeout-Kaskade: der Client gibt auf, Archicad rechnet serverseitig weiter, jeder Folge-Call reiht sich dahinter ein.
3. Rückkehr der API mit einem Wächter abwarten (`until curl … GetProjectInfo | grep -q succeeded; do sleep 5; done`), nicht mit Retry-Schleifen.
4. Bekannte Hänger in eine Skip-Liste, Rest weiterrendern.

### 4001 nennt den Dialog beim Namen <!-- 2026-09-03 -->

Seit AC29 steht der blockierende Dialog im Fehlertext: `Invalid program status (there is an open modal dialog: Teamwork-Projekt öffnen/beitreten)` — ebenso beim offenen Eigenschaften-Manager. Kein Retry, nicht raten: dem Nutzer sagen, welcher Dialog offen ist, und warten. Ein leerer Rückgabekörper ohne `executionResults` bei einem Set-Call ist dasselbe Symptom — immer die Rohantwort auf `succeeded`/`error` prüfen.

### API-Selektion ist ortsgebunden — der Nutzer sieht sie nur auf seinem Geschoss <!-- 2026-09-03 -->

`ChangeSelectionOfElements` + `GetSelectedElements` bestätigten 17/17 selektiert; der Nutzer meldete „nix ist selektiert“, weil die Elemente im 1.OG lagen (er stand auf einem anderen Geschoss) bzw. ihre Ebene ausgeblendet war. Seine manuelle Teamwork-Reservierung griff deshalb ins Leere. **Regel:** Soll der Nutzer auf eine API-Selektion reagieren, vorher `FitInWindow` auf die Elemente (zoomt und wechselt die Ansicht) und Ebene sichtbar machen — dann rückfragen, ob er die Markierung sieht. Meist besser: gleich per API reservieren (`ReserveElements`), das funktioniert, sobald die Ebene sichtbar ist (siehe [bulk-operations.md § 6001](bulk-operations.md#teamwork-bulk-set-scheitert-pro-element-mit-code-6001)).

## Direkter HTTP-Zugriff auf die JSON-API (MCP-Bypass) <!-- 2026-06-11 -->

Der Archicad-MCP-Server ist nur ein dünner Wrapper über Archicads **JSON-Command-API**, die direkt per HTTP auf dem Instanz-Port lauscht (`POST http://127.0.0.1:<port>`, Body `{"command": ..., "parameters": ...}`). Bei **großen Bulk-Aufträgen** (hunderte/tausende Elemente) ist der direkte HTTP-Zugriff oft die bessere Wahl als der MCP-Tool-Pfad — aus zwei Gründen, beide live verifiziert an einem 1.899-Möbel-Auftrag:

1. **Umgeht die MCP-Pagination-Context-Explosion.** Der MCP-Server paginiert Listen-Antworten in 100er-Seiten (`next_page_token` „MTAw"/„MjAw"/…), und jede Seite landet als riesiger JSON-Block im Claude-Context. Die **rohe API paginiert nicht** — `API.GetElementsByClassification` lieferte alle 1.899 Element-GUIDs in einem Aufruf. In einem lokalen Python-Script verarbeitet, bleibt die Massendaten komplett aus dem Context; nur die Zusammenfassung (Counts) kommt zurück.
2. **Umgeht die AC29-Pydantic-Schema-Drift-Bugs.** Die Bugs in `elements_get_gdl_parameters_of_elements` / `elements_get_details_of_elements` (siehe Fehlerklassen-Tabelle + Capability-Sektion) sind **Validierungs-Fehler im MCP-Wrapper, nicht in Archicad**. Die rohe JSON-Antwort ist völlig parsebar — nur Pydantic `extra='forbid'` lehnt sie ab. Per direktem HTTP liest man die GDL-Parameter problemlos selbst.

**Command-Mapping (wie der MCP-Tool-Name auf die rohe API abbildet):**

- **Offizielle Commands** gehen direkt: `{"command":"API.GetElementsByClassification","parameters":{...}}`. Dazu zählen u. a. `API.GetElementsByClassification`, `API.GetAllElements`, `API.GetSelectedElements`, `API.GetPropertyValuesOfElements`, `API.SetPropertyValuesOfElements`, `API.GetPropertyIds`, `API.GetClassificationsOfElements`, `API.GetProductInfo`, `API.IsAlive`.
- **Tapir-Add-On-Commands** werden gewrappt:
  ```json
  {"command":"API.ExecuteAddOnCommand",
   "parameters":{"addOnCommandId":{"commandNamespace":"TapirCommand","commandName":"GetGDLParametersOfElements"},
                 "addOnCommandParameters":{...}}}
  ```
  Tapir-only sind u. a. `GetGDLParametersOfElements`, `GetDetailsOfElements`, `ReserveElements`, `ReleaseElements`, `ChangeSelectionOfElements`, `HighlightElements`, `MoveElements`, `GetStories`, `GetProjectInfo`. Die Tapir-Antwort steckt in `result.addOnCommandResponse`.

**Welche Commands wo liegen** (offiziell vs. Tapir) steht in der installierten Server-Quelle:
`~/.local/share/uv/tools/tapir-archicad-mcp/lib/python3.13/site-packages/multiconn_archicad/core/literal_commands.py` — zwei Literal-Listen `AddonCommandType` (offiziell `API.*`) und `TapirCommandType`. Die exakten Parameter-Shapes liegen daneben in `models/official/` bzw. `models/tapir/`.

**Property-READS bei rohem HTTP: offizieller vs. Tapir-Command.** <!-- 2026-07-27 live verifiziert, Futurelab, 3.563 CW-Frames -->
Beide heißen `GetPropertyValuesOfElements`, verhalten sich aber verschieden:

- Der **offizielle** `API.GetPropertyValuesOfElements` scheitert an **CW-Subelementen** (Frames/Paneele) pro Element mit `7203 „Element not supported"`. Der **Tapir-Command gleichen Namens** kann Subelemente — für Bulk-Reads auf Frames/Paneelen also immer den Tapir-Weg nehmen (Namespace `TapirCommand`, Singular — `TapirCommands` gibt 4010).
- Der Tapir-Command liefert die Werte als **lokalisierte Display-Strings** (`"0,315"` mit Dezimalkomma, ohne Einheit, Länge in Metern) statt als typisierte Zahlen. Vor dem Rechnen parsen (`,` → `.`); ein `isinstance(v, float)`-Filter verwirft sonst stillschweigend alles.
- **Built-in-Property-GUIDs nicht über `API.GetPropertyIds` raten:** plausible `nonLocalizedName`-Kandidaten wie `General_Length` existieren nicht (Fehler 4005). Robust ist Tapir `GetAllProperties` (~2.000 Einträge, ein Call) und lokal nach `propertyGroupName`/`propertyName` filtern — z. B. Gruppe „Fassadenprofil", Property „Profil-Länge".
- Chunks à 500 Elemente laufen problemlos (3.563 Frames × 3 Properties in 8 Calls, je < 2 s).

**Set-Schema bei rohem HTTP weicht ab vom MCP-Schema.** Der MCP-Tool-Pfad akzeptiert die vereinfachte Form `{"value": "<string>"}` (siehe `bulk-operations.md` CAP-01). Die **rohe** `API.SetPropertyValuesOfElements` verlangt das **vollständige** Konstrukt — und die Einträge im Array `elementPropertyValues` sind **flach** `{elementId, propertyId, propertyValue}`, **nicht** verschachtelt mit einem `propertyValues`-Sub-Array pro Element: <!-- 2026-06-24 -->
```json
{"command":"API.SetPropertyValuesOfElements",
 "parameters":{"elementPropertyValues":[
   {"elementId":{"guid":"..."},
    "propertyId":{"guid":"..."},
    "propertyValue":{"type":"singleEnum","status":"normal",
                     "value":{"type":"displayValue","displayValue":"RV Standard"}}}
 ]}}
```
Falsch (verschachteltes `propertyValues`) → Top-Level-Fehler **4002** „`The JSON is invalid … Validation failed on schema rule 'additionalProperties' … on path '#/elementPropertyValues/0/propertyValues'`". Response bei korrektem Schema: `result.executionResults` — pro Element `{"success":true}` oder `{"success":false,"error":{...}}`.

**Klassifizierung setzen: offiziellen Command nehmen, nicht Tapir.** <!-- 2026-07-06 --> `API.SetClassificationsOfElements` existiert offiziell und verlangt **nested** `classificationId`:
```json
{"command":"API.SetClassificationsOfElements",
 "parameters":{"elementClassifications":[
   {"elementId":{"guid":"..."},
    "classificationId":{"classificationSystemId":{"guid":"..."},
                        "classificationItemId":{"guid":"..."}}}]}}
```
Flach (`classificationSystemId`/`classificationItemId` direkt neben `elementId`) → 4002. Es gibt auch einen Tapir-Command gleichen Namens (gleiches nested Schema), aber der **maskiert die echten Fehler**: er liefert nur generisch `Failed to set classification item for element` (Code -2130312310), wo der offizielle Command präzise `6001 TeamWork permission denied` (fehlende Reservierung) oder `7203 Element not supported` (Elementtyp nicht klassifizierbar, z. B. Polylinie/2D) meldet. Live verifiziert an 297 Wänden (Teamwork, AC29).

**Wann den HTTP-Bypass NICHT nehmen:** Für normale, kleine Operationen (< ~100 Elemente, keine kaputten Tools) bleibt der MCP-Tool-Pfad der Default — er ist Discovery-geführt und versionsrobust. Der Bypass ist das Werkzeug für **Massendaten** und **die bekannten Schema-Drift-Tools**. Port immer aus dem Warm-up/Discovery beziehen, nie hardcoden (Ports sind volatil, siehe oben).

## Verhalten bei „nein" oder Mid-Batch-Fehler

**User antwortet `nein` auf den Confirm-Dialog.** Wir stoppen den Auftrag sauber. Wir berichten, was wir *nicht* getan haben, und bieten an, die Operation zu verfeinern. Keine Teilausführung — wir machen entweder das volle Set oder gar nichts.

**MCP-Fehler mitten in einem Batch.** Wenn während eines APPLY-Schritts ein Einzel-Element scheitert:

- **Wir machen kein Auto-Rollback.** Archicads Undo ist und bleibt das Werkzeug des Users.
- Wenn der Fehler **systemisch** wirkt (z. B. „invalid GUID" für *alle* Elemente, nicht nur eines): sofort stoppen, vollständig berichten, User entscheiden lassen.
- Wenn der Fehler **einzeln** ist (z. B. ein Element wurde inzwischen vom User gelöscht): weitermachen mit den verbleibenden, am Ende vollständiger Report.
- **Format des Reports:** „127 verarbeitet, 124 erfolgreich, 3 Fehler bei IDs 0x1A2B, 0x1A2C, 0x1A30 — Gründe: A, B, C."

**Confirm war `ja`, aber das Ergebnis überrascht.** Element verschwindet, Eigenschaft sieht anders aus als erwartet: stoppen, lesen, berichten. Wir korrigieren nicht durch Folge-Operationen, weil wir nicht wissen, was der MCP-Server genau gemacht hat.

## Betriebs-Fallen (THN-Live-Sitzung) <!-- 2026-09-06 -->

- **Zeitgrenze auf 600 s setzen, auch bei „timed out" wiederholen.**
  Teamwork-Schreibungen und Senden können über 180 s dauern; die HTTP-Zeitgrenze
  im Hilfsmodul auf 600 s setzen und nicht nur bei 4001 (modaler Dialog, siehe
  oben) wiederholen, sondern auch beim reinen Timeout-Symptom „timed out" —
  beides ist vorübergehend, nicht fatal.
- **Aktionsbefehle nie mit leeren Parametern „zum Schema-Test" aufrufen — sie
  werden AUSGEFÜHRT.** `FitInWindow {}` setzt den Zoom auf „alles einpassen",
  `ChangeWindow` löscht die Selektion des Nutzers (kostete 2026-09-06 eine vom
  Nutzer markierte Objektgruppe). Schema stattdessen per Discovery lesen, nie
  live gegentesten.
- **Archicad nie per `kill` beenden.** Ein abgelehnter `osascript … to quit`
  mit `-128 „Von Benutzer:in abgebrochen"` heißt Abbruch durch den Nutzer,
  nicht Hänger. `kill -TERM` erzeugte 2026-09-06 einen Absturz mit
  Graphisoft-Bug-Reporter; folgenlos war das nur, weil kein Projekt offen war.
  Vor jedem Beenden `GetProjectInfo` je Port prüfen — bei offenem Projekt erst
  `TeamworkSend`, sonst den Nutzer selbst beenden lassen.
- Python puffert `print` in Pipes — `ps`-Laufzeit und Änderungszeit der
  Logdatei trennen „arbeitet" von „hängt", bevor man einen Lauf für hängend
  hält. Abhilfe: `sys.stdout.reconfigure(line_buffering=True)` oder direkt in
  die Datei schreiben; kein `| grep` hinter langen Läufen (grep puffert
  zusätzlich).
- Modell-Durchgang über alle Objekte einmal je Lauf cachen statt je Geschoss
  wiederholen (2045 Objekte ≈ 8 Minuten pro Durchgang).
- Parallele Lesezugriffe während eines Schreiblaufs erzeugen Zeitüberschreitungen
  — nur ein Prozess am Modell.
