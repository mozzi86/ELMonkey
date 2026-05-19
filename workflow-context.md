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

**Tool-Name:** `mcp__archicad__discovery_list_active_archicads` <!-- 2026-05-19 verifiziert AC29 -->

**Erwartete Parameter:** keine.

**Erwartete Rückgabe:** Liste mit Objekten `{port, projectName, projectType, archicadVersion, projectPath}` — eine Zeile pro laufender Instanz.

**Verhalten:**
- Eine Instanz → Port übernehmen.
- Mehrere Instanzen → User fragen (siehe [mcp-conventions.md § Port-Handling](mcp-conventions.md#port-handling-bei-mehreren-archicad-instanzen)).
- Null Instanzen → User informieren, stoppen.

## Feld 2: Projekt-Identität

Wir bekommen Projektname, -typ, Archicad-Version, -Pfad bereits aus Feld 1. Das reicht meist.

**Falls die erweiterten Projekt-Felder gebraucht werden** (Projektname, -beschreibung, -ID, -Code, Adresse, Auftraggeber, Planer:in, etc.):

- **Tool-Name:** `mcp__archicad__project_get_project_info_fields` <!-- 2026-05-19 verifiziert AC29 -->
- **Erwartete Parameter:** `port`.
- **Erwartete Rückgabe:** `{fields: [{projectInfoId, projectInfoName, projectInfoValue}]}` — ca. 85 Standard-Felder (PROJECTNAME, PROJECT_DESCRIPTION, PROJECT_ID, PROJECT_CODE, PROJECTNUMBER, PROJECTSTATUS, SITE_NAME, SITEADDRESS1..3, SITECITY, SITESTATE, SITEPOSTCODE, SITECOUNTRY, BUILDING_NAME, BUILDING_DESCRIPTION, CONTACT_FULLNAME, CONTACTCOMPANY, CONTACTEMAIL, CONTACTPHONE, CLIENT_FULLNAME, CLIENTCOMPANY, CLIENTEMAIL, …) plus projekt-spezifische `autotext-*`-Felder.
- **Setzen:** `mcp__archicad__project_set_project_info_field` mit `{projectInfoId, projectInfoValue}` (zählt als Update — asymmetrische Sicherheit greift).

## Feld 3: Längeneinheit

Bestimmt, ob wir Wand-Längen in `m`, `mm`, `ft` etc. übergeben.

- **Status:** ⚠ **MCP-Gap** — kein direktes Tool für „project units / working units" gefunden in Discovery (2026-05-19, AC29). Auch `project_get_geo_location` enthält keine Einheit.
- **Workaround:** MCP-Aufrufe verwenden **immer Meter** als Default (Coordinate-Werte in m, Höhe in m — verifiziert durch erfolgreiche Aufrufe wie `elements_create_zones` mit `{x:5, y:3}` ergab 5×3m-Zone).
- **Falls der User eine andere Einheit erwähnt** („mach das in cm"): selbst umrechnen, bevor die MCP-Call-Argumente formuliert werden. Aus 300 cm wird `3` (Meter).
- **Bei Unsicherheit:** User explizit fragen, in welcher Einheit die Aufgabe gemeint ist.

## Feld 4: Aktive Story

**Volatil — bei Floor-Erwähnung neu ziehen.** Siehe Warm-up-Regeln.

- **Tool-Name:** `mcp__archicad__project_get_stories` <!-- 2026-05-19 verifiziert AC29 -->
- **Erwartete Parameter:** `port`.
- **Erwartete Rückgabe:** `{firstStory, lastStory, actStory, skipNullFloor, stories: [{index, floorId, dispOnSections, level, name}]}`.
- **Aktive Story:** in `actStory` (Index-Wert). Story-Details aus `stories[i]` wo `i.index === actStory`.
- **Hinweis:** Story-`name` ist oft leer (`""`). Verlasse dich nicht auf den Namen — der `index` plus `level` ist die zuverlässige Identifikation. EG = `index: 0`, `level: 0.0`.
- **STORY-01 (alle Stories listen):** **derselbe Call** — `stories[]` enthält bereits die vollständige Liste. Kein separater Call nötig.

## Feld 5: Sichtbare Layer

Brauchen wir für SAFE-03 (Layer-not-visible-Ausnahme) und für Layer-Zuweisung bei Create-Operationen.

- **Discovery-Query (zum Probieren):** „visible layers" / „layer visibility" / „active layer combination"
- **Tool-Name:** TODO — in Phase 2 live verifizieren.
- **Erwartete Parameter:** `port`.
- **Erwartete Rückgabe:** Liste mit `{layerId, name, visible, locked}` für alle Layer; oder gefilterte Liste nur der sichtbaren.

**Hinweis.** Sichtbarkeit hängt oft an der aktiven *Layer-Kombination*. Wenn der MCP-Server eine Kombination zurückgibt, behandeln wir diese als Quelle der Wahrheit für „sichtbar".

## Feld 6: Pen-Set / Aktive Stiftnummer

**Nur bei 2D-Arbeit ziehen** (Linien, Polylinien, Schraffuren).

### Pen-Sets / Pen-Tables listen

- **Tool-Name:** `mcp__archicad__attributes_get_attributes_by_type` mit `attributeType: "PenTable"` <!-- 2026-05-19 verifiziert AC29 -->
- **Erwartete Rückgabe:** `{attributes: [{attributeId: {guid}, index, name}]}` — typische Templates haben 4–10 Pen-Tables (z. B. „ArchiCAD 1:100/200", „S/W 1:20/50", „AutoCAD Stifte").
- **Detail-Abfrage:** `mcp__archicad__attributes_get_pen_table_attributes` mit `{attributeIds: [{guid}]}` — gibt die einzelnen Stifte mit Farbe/Gewicht zurück.

### Aktives Pen-Set

Das aktive Pen-Set ist Teil der View-Settings.

- **Tool-Name:** `mcp__archicad__navigator_get_view_settings` für den aktuellen View → `penSetName` (kann auch leer sein, dann ist „custom" gesetzt).
- **Bei Custom-Pen-Set:** kein Standardname zurück; ggf. User fragen oder Pen-Index direkt verwenden.

## Feld 7: Aktives Klassifikations-System

**Nur bei Klassifizierungs-Auftrag ziehen.** Kritisch — ein falsches System führt zu GUID-Cross-Pollution (Pitfall P5).

### Alle Klassifikations-Systeme listen

- **Tool-Name:** `mcp__archicad__classifications_get_all_classification_systems` <!-- 2026-05-19 verifiziert AC29 -->
- **Erwartete Parameter:** `port`.
- **Erwartete Rückgabe:** `{classificationSystems: [{classificationSystemId: {guid}, name, description, source, version, date}]}` — alle im Projekt verfügbaren Systeme.
- **Hinweis:** Es gibt **keinen Endpoint, der das aktive System direkt markiert**. Wenn mehrere Systeme vorhanden sind und der User keine Angabe macht: User fragen welches gemeint ist. Wenn nur eines vorhanden, dieses verwenden.

### Alle Klassifikationen eines Systems holen (Hierarchie + Klartext→GUID)

- **Tool-Name:** `mcp__archicad__classifications_get_all_classifications_in_system` mit `{classificationSystemId: {guid}}`.
- **Erwartete Rückgabe:** Baum-Struktur mit Klassen + Sub-Klassen. Daraus wird Klartext→GUID-Mapping abgeleitet (für SAFE-konformes Setzen).

### Klassifikationen eines Elements lesen

- **Tool-Name:** `mcp__archicad__elements_get_classifications_of_elements` mit `{elements: [{elementId: {guid}}], classificationSystemIds: [{classificationSystemId: {guid}}]}`.
- **Erwartete Rückgabe:** Pro Element die zugeordnete Klasse in jedem angegebenen System.

## Erweiterte Listings (STORY-01, ATTR-01)

### STORY-01 — Alle Stories listen

**Bereits erledigt durch Feld 4** (siehe oben). `project_get_stories` gibt die vollständige Story-Liste in `stories[]` zurück. Kein separater Call nötig.

### ATTR-01 — Attribute listen (universal)

Brauchen wir, wenn wir Properties zuweisen — Layer-Index aus Name, Surface-Index aus Name, Composite-Index aus Name, etc.

- **Universal-Tool:** `mcp__archicad__attributes_get_attributes_by_type` <!-- 2026-05-19 verifiziert AC29 -->
- **Erwartete Parameter:** `{port, params: {attributeType: <type>}}` — **paginiert** via `next_page_token`.
- **Verfügbare Attribute-Typen** (Enum):
  - `Layer` — Plan- und Modell-Layer (oft viele, Pagination Pflicht)
  - `Line` — Linientypen (durchgezogen, gestrichelt, gepunktet, …)
  - `Fill` — Schraffuren-Muster
  - `Composite` — Mehrschicht-Aufbauten (Wand-Composites)
  - `Surface` — Oberflächen / Materialien (visuell)
  - `LayerCombination` — Layer-Kombinationen (Layer-Sichtbarkeits-Sets)
  - `ZoneCategory` — Zonen-Kategorien (z. B. „Wohnen", „Verkehr")
  - `Profile` — Komplexe Profile (für Wände, Stützen, Träger)
  - `PenTable` — Stift-Sets (siehe Feld 6)
  - `MEPSystem` — MEP (Mechanical/Electrical/Plumbing) Systeme
  - `OperationProfile` — Energie-Berechnungs-Profile
  - `BuildingMaterial` — Bau-Materialien (mit physikalischen Eigenschaften)
- **Detail-Abfragen pro Typ:**
  - Layer-Details: `attributes_get_layer_attributes`
  - Building-Material-Details: `attributes_get_building_material_attributes`
  - Pen-Table-Details: `attributes_get_pen_table_attributes`
  - Layer-Combination-Details: `attributes_get_layer_combination_attributes`
  - Building-Material physikalische Eigenschaften: `attributes_get_building_material_physical_properties`
- **Listing-Rückgabe-Format:** `{attributes: [{attributeId: {guid}, index, name}], next_page_token?}` — einheitlich für alle Typen.
