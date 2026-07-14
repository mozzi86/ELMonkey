# Library-Objects — Platzierte Objects (Möbel, Sanitär, Beleuchtung, GDL)

Library-Objects sind die generische Klasse aller platzierten GDL-Library-Items im Archicad-Modell: Möbel, Sanitär, Leuchten, Spezial-Bauteile, Geräte. Sie werden über den Archicad-MCP-Server mit dem Element-Typ `Object` angesprochen.

**Wichtige Abgrenzungen:**
- **Türen und Fenster** sind technisch ebenfalls GDL-Library-Objects, werden aber in [`openings.md`](openings.md) behandelt — sie hosten auf Wänden und haben eine andere topologische Logik (gehostete Elemente). Dieses Recipe deckt sie nicht ab.
- **Curtain-Wall-Accessories** sind oft Library-Objects, werden aber in [`curtain-walls.md`](curtain-walls.md) behandelt, weil sie Sub-Elemente einer CurtainWall-Hierarchie sind.
- **Object-Create** ist via `elements_create_objects` im MCP v29 verifiziert, wird aber in diesem Recipe nur in der Discovery-Tabelle referenziert — Phase 4 ist read-focused. Create-Worked-Example in späterer Phase.

<!-- 2026-05-20 verifiziert AC29 -->

---

## Inhaltsverzeichnis

1. [Umfang](#umfang)
2. [Erforderlicher Warm-up-Kontext](#erforderlicher-warm-up-kontext)
3. [Library-Type-Tabelle](#library-type-tabelle)
4. [Discovery-Anker](#discovery-anker)
5. [Typische Parameter](#typische-parameter)
6. [Worked Example 1 — Libraries auflisten](#worked-example-1--libraries-auflisten)
7. [Worked Example 2 — Subtype-Discovery + Gruppierung (Kern-Workflow)](#worked-example-2--subtype-discovery--gruppierung-kern-workflow)
8. [Worked Example 3 — Single-Object lesen + Eigenschaften](#worked-example-3--single-object-lesen--eigenschaften)
9. [Worked Example 4 — GDL-Parameter abfragen (Bug-Workaround)](#worked-example-4--gdl-parameter-abfragen-bug-workaround)
10. [Worked Example 5 — Object verschieben / Größe ändern](#worked-example-5--object-verschieben--größe-ändern)
11. [Worked Example 6 — Object löschen](#worked-example-6--object-löschen)
12. [Worked Example 7 — Property-Bulk-Update für Inventar (6-Schritt-Pattern)](#worked-example-7--property-bulk-update-für-inventar-6-schritt-pattern)
13. [Worked Example 8 — Object klassifizieren](#worked-example-8--object-klassifizieren)
14. [Bulk-Klassifizierung (Stub)](#bulk-klassifizierung-stub)
15. [Gotchas](#gotchas)
16. [Verwandte Recipes](#verwandte-recipes)

---

## Umfang

**Abgedeckt:**

- Aktive Libraries des Projekts auflisten (Library-Typ, Verfügbarkeit, Server-URL).
- Alle platzierten Objects eines Projekts / einer Story ermitteln.
- Subtype-Discovery: Objects nach Library-Part-Name gruppieren (Möbel vs. Sanitär vs. Leuchten etc.).
- Eigenschaften einzelner Objects via Property-System lesen (Workaround für `get_details`-Bug).
- GDL-Parameter lesen — mit Bug-Caveat und Workaround-Verweisen.
- Position, Rotation, Größe über `elements_set_details_of_elements` ändern.
- Objects löschen.
- Property-Bulk-Update für Inventar-Felder (Hersteller, Modell, Anschaffungsjahr).
- Object im aktiven Klassifikations-System klassifizieren.

**Nicht abgedeckt:**

- Object-Create (verifiziert, aber hier nicht als Worked Example — defer zu Phase 5).
- Türen + Fenster (GDL-Objects, aber im [`openings.md`](openings.md) Recipe behandelt).
- Curtain-Wall-Accessories (im [`curtain-walls.md`](curtain-walls.md) Recipe behandelt).
- Hotlinks / XREFs mit Library-Objects (v2 geplant).

---

## Erforderlicher Warm-up-Kontext

| Feld | Woher | Wozu |
|---|---|---|
| `port` | `mcp__archicad__discovery_list_active_archicads` | Jeder MCP-Call |
| aktive Story (`floorIndex`) | `mcp__archicad__archicad_call_tool` → „get story info" | Filter `OnActualFloor` beim Listing |
| sichtbare Layer | Warm-up | SAFE-03: Layer prüfen vor Create/Edit |
| Klassifikations-System-GUID | `mcp__archicad__archicad_call_tool` → „get classification systems" | Klassifikation lesen/setzen |
| Längeneinheit | Warm-up | Koordinaten + Dimensionen korrekt übergeben |

Vollständiges Warm-up-Protokoll: [`../reference/workflow-context.md`](../reference/workflow-context.md).

---

## Library-Type-Tabelle

Archicad kennt vier Library-Typen, die `library_get_libraries` zurückliefert:

| Typ | Beschreibung | Beispiel | Read/Write |
|---|---|---|---|
| `BuiltInLibrary` | In Archicad integriert, nicht editierbar | `BuiltInLibraryParts.libpack`, Add-On-Bundles | Read-Only |
| `EmbeddedLibrary` | Im Projekt-Paket eingebettet; wandert mit dem Projekt | `Emb_3305904328` (AutoSave-Projekt-Library) | Read/Write |
| `LocalLibrary` | Lokales Verzeichnis; typisch für Migrationsbibliotheken | `Archicad Migrationsbibliotheken (AC28)` | Read/Write (wenn vorhanden) |
| `ServerLibrary` | BIMcloud / TeamWork-Server-Library; zentral gepflegt | `BIBLIOTHEKEN 28` auf `schwarz.bimcloud.com` | R/W je nach User-Rechten |

**Hinweis Server vs. Local:** Eine ServerLibrary ist nur verfügbar, wenn die BIMcloud-Verbindung aktiv ist. `available: false` bedeutet, dass Library-Parts aus dieser Library im Projekt referenziert sind, aber nicht geladen werden können — Worked Example 1 zeigt, wie man das erkennt.

---

## Discovery-Anker

| Operation | Query-String | Typischer Tool-Name |
|---|---|---|
| Libraries auflisten | `"get libraries list"` | `library_get_libraries` |
| Objects listen | `"get elements by type object"` | `elements_get_elements_by_type` |
| Object-Details lesen | `"get details of elements"` | `elements_get_details_of_elements` ¹ |
| Bounding Box | `"get 3d bounding box elements"` | `elements_get_3_d_bounding_boxes` |
| GDL-Parameter lesen | `"get gdl parameters of elements"` | `elements_get_gdl_parameters_of_elements` ¹ |
| GDL-Parameter schreiben | `"set gdl parameters of elements"` | `elements_set_gdl_parameters_of_elements` |
| Object bewegen/Größe | `"set details of elements object"` | `elements_set_details_of_elements` |
| Object löschen | `"delete elements by id"` | `elements_delete_elements` |
| Object erstellen | `"create objects library part"` | `elements_create_objects` |
| Klassifikation lesen | `"get classifications of elements"` | `elements_get_classifications_of_elements` |
| Klassifikation setzen | `"set classifications of elements"` | `elements_set_classifications_of_elements` |
| Property-IDs holen | `"get all property ids of elements"` | `properties_get_all_property_ids_of_elements` |
| Property-Definition | `"get details of properties"` | `properties_get_details_of_properties` |
| Property-Werte lesen | `"get property values of elements"` | `properties_get_property_values_of_elements` |
| Property-Werte setzen | `"set property values of elements"` | `properties_set_property_values_of_elements` |

¹ Hat Bug-Caveats in AC29 — siehe [Gotchas](#gotchas) und [Worked Example 4](#worked-example-4--gdl-parameter-abfragen-bug-workaround).

---

## Typische Parameter

### ObjectsDatum (für Create-Reference)

| Parameter | Typ | Pflicht | Beschreibung |
|---|---|---|---|
| `libraryPartName` | String | ja | Eindeutiger Library-Part-Pfad, z. B. `"Szék 29"` oder `"Sessel"` — muss in einer geladenen Library existieren |
| `coordinates` | `{x, y, z}` | ja | 3D-Position in Metern |
| `dimensions` | `{x, y, z}` | nein | Überschreibt Größe; nicht alle Library-Parts unterstützen das — siehe [Gotcha 9](#gotcha-9-dimensions-override-caveat) |
| `floorIndex` | Integer | nein | Story-Index; wenn weggelassen → aktive Story |

---

> **User sagt:** „Welche Libraries sind im Projekt gerade geladen?"

## Worked Example 1 — Libraries auflisten

<!-- 2026-05-20 verifiziert AC29 -->

Alle aktiven Libraries des Projekts mit Typ, Verfügbarkeit und Server-URL ermitteln.

```python
mcp__archicad__archicad_call_tool(
  name="library_get_libraries",
  arguments={
    "port": <port>
  }
)
```

Response pro Library: `{name, path, type, available, readOnly, twServerUrl, urlWebLibrary}`. Im Sandkasten 19723 live verifiziert: 19 Libraries inkl. BuiltInLibraryParts, EmbeddedLibrary (Emb_3305904328), und ServerLibrary `BIBLIOTHEKEN 28` auf `schwarz.bimcloud.com`.

**Was interessiert uns:**

- `available: false` → Library-Part referenziert, aber nicht geladen. Betroffene Objects zeigen Platzhalter-Geometrie; Subtype-Discovery über Library-Part-Name unzuverlässig. User auffordern, BIMcloud-Verbindung herzustellen.
- `readOnly: true` → BuiltIn-Libraries nicht erweiterbar. Neue Büro-spezifische Parts: EmbeddedLibrary oder ServerLibrary.
- `twServerUrl` → Nur bei ServerLibrary gesetzt; zeigt ob BIMcloud-Verbindung nötig ist.

---

> **User sagt:** „Welche Möbel hat das Projekt, gruppiert nach Typ?"

## Worked Example 2 — Subtype-Discovery + Gruppierung (Kern-Workflow)

Alle platzierten Objects des Projekts ermitteln, dann nach Library-Part-Name (= Subtype) gruppieren. Das ist der Kern-Workflow, wenn der User fragt: „Welche Möbel hat das Projekt?" oder „Zeig mir alle Sanitär-Objekte."

### Schritt A — Alle Objects listen

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": <port>,
    "params": {
      "elementType": "Object"
    }
  }
)
```

Paginierung bis vollständig (alle `next_page_token` ausschöpfen). Resultat: Liste aller Object-GUIDs im Projekt.

Für Story-spezifisches Listing:

```python
"params": {
  "elementType": "Object",
  "filters": ["OnActualFloor", "IsVisibleByLayer"]
}
```

### Schritt B — Library-Part-Name pro Object ermitteln

Der Library-Part-Name ist in den Built-in Properties des Objects enthalten. Er ist die zuverlässigste Subtype-Quelle — er codiert den Library-Pfad des platzierten GDL-Items.

Property-IDs für ein Beispiel-Object holen:

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<object-guid>"}}],
      "propertyType": "BuiltIn"
    }
  }
)
```

**Hinweis:** Objects haben ~230 Built-in Properties (live verifiziert AC29). Uns interessiert die Property, deren `nonLocalizedValue` auf `"Library Part Name"` oder ähnlich lautet. Mit `properties_get_details_of_properties` prüfen:

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_details_of_properties",
  arguments={
    "port": <port>,
    "params": {
      "properties": [
        {"propertyId": {"guid": "<builtin-property-guid-1>"}},
        {"propertyId": {"guid": "<builtin-property-guid-2>"}}
        // Für alle ~230 IDs oder gezielte Selektion nach Name
      ]
    }
  }
)
```

Sobald die Library-Part-Name-Property-ID bekannt ist: Wert für alle Objects in einem Batch-Call lesen:

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [
        {"elementId": {"guid": "<object-guid-1>"}},
        {"elementId": {"guid": "<object-guid-2>"}},
        {"elementId": {"guid": "<object-guid-3>"}},
        {"elementId": {"guid": "<object-guid-4>"}}
      ],
      "properties": [
        {"propertyId": {"guid": "<library-part-name-property-id>"}}
      ]
    }
  }
)
```

### Schritt C — Nach Subtype gruppieren

Die Response liefert pro Object einen `displayValue`. Daraus bilden wir eine Subtype-Map:

```
"Sessel 29"           → 2 Objects  (Gruppe: Sitzmöbel)
"Waschbecken 29"      → 1 Object   (Gruppe: Sanitär)
"Strahler 29"         → 1 Object   (Gruppe: Beleuchtung)
```

Diese Gruppierung ist die Grundlage für alle weiteren Bulk-Operationen (Bulk-Update, Bulk-Klassifizierung). Pro Gruppe einen repräsentativen Vertreter lesen, um das Property-Set der Gruppe zu verstehen — dann erst auf alle Objects der Gruppe anwenden.

**Ergebnis im Sandkasten (4 Test-Objects, 2026-05-20):** Detaillierte Library-Part-Namen wurden in Worked Example 3 weiterverfolgt.

---

> **User sagt:** „Lies mir die Eigenschaften von diesem Sessel-Objekt."

## Worked Example 3 — Single-Object lesen + Eigenschaften

Detail-Read eines einzelnen Objects via Property-Workaround (da `get_details_of_elements` in AC29 Schema-Drift-Bug hat — siehe [Gotcha 1](#gotcha-1-get_details_of_elements-bug)).

**Bounding Box (3D-Geometrie):**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_3_d_bounding_boxes",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<object-guid>"}}]
    }
  }
)
```

Response: `{xMin, yMin, zMin, xMax, yMax, zMax}` in Metern. Daraus Grundfläche und Höhe ableitbar.

**Built-in Properties** (Position, Layer, Library-Part-Name) — `propertyType: "BuiltIn"`. Achtung: ~230 Built-in IDs pro Object (live verifiziert). Selektiv vorgehen: erst `properties_get_details_of_properties` auf Stichprobe, dann nur die gewünschten IDs für den Wert-Read.

**UserDefined Properties** (Inventar, Hersteller, Anschaffungsjahr) — `propertyType: "UserDefined"`. Werte via `properties_get_property_values_of_elements`. Typisches Real-Projekt: Gruppen wie „Produktinformationen", „Hersteller-Daten".

---

> **User sagt:** „Gib mir die GDL-Parameter dieses Möbelstücks — Breite, Höhe, Typ."

## Worked Example 4 — GDL-Parameter abfragen (Bug-Workaround)

Das Tool `elements_get_gdl_parameters_of_elements` hat in AC29 einen Pydantic-Validierungs-Bug: Der Server liefert `index` als Integer und extra Felder (`displayName`, `isLocked`, `flags`), die der MCP-Wrapper mit `extra='forbid'` ablehnt. Bei Library-Objects mit vielen GDL-Parametern können hunderte Validierungs-Fehler entstehen.

**Versuch (kann funktionieren bei einfachen Library-Parts mit wenigen Params):**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_gdl_parameters_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<object-guid>"}}]
    }
  }
)
```

**Bei Fehler (Validierungs-Fehler-Flood oder leere Response):**

Drei Workaround-Pfade, in empfohlener Reihenfolge:

1. **Property-System statt GDL** — Der häufigste GDL-Parameter (Breite, Höhe, Typ-Bezeichnung) ist oft über Built-in- oder UserDefined-Properties zugänglich. Schritt: Built-in-Property-IDs mit `properties_get_all_property_ids_of_elements` holen, relevante lesen. Einfachster Pfad, kein extra Setup.

2. **Property-Expression-Linking** — Wenn GDL-Werte dauerhaft ins Property-System gespiegelt werden sollen (z. B. Türbreite → Property „Lichte Breite"): native Archicad-Synchronisation über den Expression-Editor einrichten. Einmalige manuelle Aktion in Archicad-UI, dann via Properties lesbar. Details: [`../reference/property-expression-linking.md`](../reference/property-expression-linking.md).

3. **Schedule-Export-Pipeline** — Wenn GDL-Werte nicht anders erreichbar sind: User exportiert einen Schedule aus Archicad als XLSX/CSV (inkl. GDL-Felder), wir parsen lokal und matchen via GUID. Details: [`../reference/schedule-pipeline.md`](../reference/schedule-pipeline.md). Aufwändigster Pfad, aber vollständig.

**Empfehlung:** Für ad-hoc-Lesezwecke Pfad 1. Für dauerhaften Sync Pfad 2. Für einmalige Bulk-Datenmigration Pfad 3.

---

> **User sagt:** „Verschieb den Sessel um 50cm nach rechts."

## Worked Example 5 — Object verschieben / Größe ändern

Position oder Dimensionen eines platzierten Objects ändern. Immer mit Confirm-Dialog (SAFE-01).

**Confirm-Dialog vor dem Aufruf:**

```
Ich werde folgendes ändern:
- Object <object-guid>  „Sessel 29"  Position: {x: 5.0, y: 3.0, z: 0.0} → {x: 5.5, y: 3.0, z: 0.0}

Ausführen? (ja / nein / details / abbrechen)
```

Nach `ja`:

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_details_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementsWithDetails": [
        {
          "elementId": {"guid": "<object-guid>"},
          "details": {
            "typeSpecificDetails": {
              "coordinates": {"x": 5.5, "y": 3.0, "z": 0.0}
            }
          }
        }
      ]
    }
  }
)
```

**Größe ändern** (wenn Library-Part es unterstützt):

```python
"typeSpecificDetails": {
  "dimensions": {"x": 0.9, "y": 0.6, "z": 0.85}
}
```

**Rotation** (um Z-Achse, im Bogenmaß): `"rotation": 1.5708` (= 90°) im `typeSpecificDetails`-Objekt.

<!-- VERIFY: Object-typeSpecificDetails-UPDATE weiterhin ungetestet; LESE-Pfad (libPart/origin/dimensions/angle) verifiziert 2026-07-14 -->

**Nach dem Update:** Bounding-Box via `elements_get_3_d_bounding_boxes` zur Verifikation.

---

> **User sagt:** „Lösch diesen Sessel aus dem Grundriss."

## Worked Example 6 — Object löschen

Library-Objects hosten typischerweise keine anderen Elemente — kein SAFE-04-Pre-Check nötig (außer bei komplexen GDL-Items mit eingebetteten Sub-Library-Objects, was selten ist).

**Confirm-Dialog:**

```
Ich werde folgendes löschen:
- Object <object-guid>  „Sessel 29"  Layer: Z_Möbel

Dieses Object hostet keine weiteren Elemente. Die Operation kann via Archicad-Undo rückgängig gemacht werden.

Ausführen? (ja / nein / abbrechen)
```

Nach `ja`:

```python
mcp__archicad__archicad_call_tool(
  name="elements_delete_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [
        {"elementId": {"guid": "<object-guid>"}}
      ]
    }
  }
)
```

**Bulk-Delete:** GUIDs via Worked Example 2 sammeln, nach Layer/Subtype filtern, Summary-Confirm (> 10 Elemente), dann mit vollständiger Liste aufrufen. Confirm-Format: [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Confirm-Format.

---

> **User sagt:** „Setz Hersteller aller Stühle auf Vitra."

## Worked Example 7 — Property-Bulk-Update für Inventar (6-Schritt-Pattern)

Wenn der User sagt: „Setze Hersteller aller Möbel-Objects auf ‚Vitra'" oder „Aktualisiere Anschaffungsjahr für alle Stühle auf 2025" — das ist der Inventar-Bulk-Workflow. Das Muster ist analog zu [`zones.md`](zones.md) § Property-Bulk-Update (Bodenbelag-Pattern) und gilt als Standard für Property-Bulk-Operationen auf Objects.

**Voraussetzungen:**
- Alle modalen Dialoge in Archicad geschlossen ([`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Modal-Dialoge).
- Property „Hersteller" existiert in der Property-Definition mit dem gewünschten Enum-Wert oder als String-Property. Bei fehlendem Enum-Wert: Pre-Flight-Check in Schritt 4 fängt das ab.
- Ziel-Subtype bekannt (aus Worked Example 2: Library-Part-Name-Gruppierung).

### Schritt 1 — Alle Objects (oder gefilterte Untermenge) listen

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_elements_by_type",
  arguments={
    "port": <port>,
    "params": {"elementType": "Object"}
  }
)
```

Paginierung vollständig durchziehen. Dann nach Subtype filtern: nur die GUIDs behalten, deren Library-Part-Name zum Ziel-Subtype passt (z. B. alle Objects mit `displayValue` enthält „Sessel" oder „Stuhl").

### Schritt 2 — Aktuelle Selektion abfragen (optional, wenn User auf Selektion zielt)

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_selected_elements",
  arguments={"port": <port>}
)
```

Wenn Selektion nach Modal-Dialog-Abbrechen leer ist: User bitten, neu zu selektieren — Selektion-Recovery ist nicht via MCP möglich.

### Schritt 3 — Property-IDs der Ziel-Objects holen <!-- 2026-05-20 verifiziert AC29 -->

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_all_property_ids_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<example-object-guid>"}}],
      "propertyType": "UserDefined"
    }
  }
)
```

Liefert Custom-Property-IDs des Objects. Unter den IDs die „Hersteller"-Property identifizieren (via `properties_get_details_of_properties` → `name` prüfen).

### Schritt 4 — Property-Definition holen (Enum-Werte + Type) <!-- 2026-05-20 verifiziert AC29 -->

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_details_of_properties",
  arguments={
    "port": <port>,
    "params": {
      "properties": [{"propertyId": {"guid": "<hersteller-property-guid>"}}]
    }
  }
)
```

Response enthält `type` (z. B. `"singleEnum"` oder `"string"`) und bei Enum: `possibleEnumValues` mit den erlaubten Werten.

**Pre-Flight-Check (Enum-Normalisierung):**
- Ziel-Wert (z. B. `"Vitra"`) gegen `possibleEnumValues` matchen.
- Bei No-Match: STOPPEN. Dem User berichten, welche Werte existieren, und Vorschläge machen (Substring-Match). Property-Enum in Archicad-UI ergänzen lassen, dann Re-Run.
- Details: [`../reference/bulk-operations.md`](../reference/bulk-operations.md) § Pre-Flight.

### Schritt 5 — Aktuelle Werte lesen (Diff-Vorbereitung)

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [
        {"elementId": {"guid": "<object-guid-1>"}},
        {"elementId": {"guid": "<object-guid-2>"}}
        // alle Ziel-Objects
      ],
      "properties": [{"propertyId": {"guid": "<hersteller-property-guid>"}}]
    }
  }
)
```

Diff aufbauen: welche Objects haben bereits den Ziel-Wert (kein Update nötig), welche brauchen Update. Nur die Delta-Menge in den Confirm-Dialog aufnehmen.

### Schritt 6 — Werte setzen (SAFE-01 Confirm) <!-- verifiziert 2026-07-14 -->

**Confirm-Dialog vor dem Aufruf:**

```
Bulk-Update Hersteller für Möbel-Objects:
- 8 Objects bekommen neuen Wert: (leer) → „Vitra"
- 2 Objects bereits korrekt (kein Update nötig)

Ausführen für die 8 Objects? (ja / details / abbrechen)
```

Nach `ja`:

```python
mcp__archicad__archicad_call_tool(
  name="properties_set_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementPropertyValues": [
        {
          "elementId": {"guid": "<object-guid-1>"},
          "propertyId": {"guid": "<hersteller-property-guid>"},
          "propertyValue": {
            "type": "singleEnum",
            "status": "normal",
            "value": {"type": "displayValue", "displayValue": "Vitra"}
          }
        },
        {
          "elementId": {"guid": "<object-guid-2>"},
          "propertyId": {"guid": "<hersteller-property-guid>"},
          "propertyValue": {
            "type": "singleEnum",
            "status": "normal",
            "value": {"type": "displayValue", "displayValue": "Vitra"}
          }
        }
        // alle Delta-Objects
      ]
    }
  }
)
```

<!-- verifiziert 2026-07-14 --> Geklärt wie zones.md: displayValue, singleEnum einzelnes Objekt / multiEnum Array mit enumValueId, status:"normal" Pflicht.

**Mid-Batch-Fehlerverhalten:** Systemischer Fehler (alle scheitern) → sofort stoppen. Einzelner Fehler → weitermachen, Report am Ende. Details: [`../reference/bulk-operations.md`](../reference/bulk-operations.md) § Mid-Batch-Fehlerverhalten.

**Stichproben-Verifikation nach Update:**

```python
mcp__archicad__archicad_call_tool(
  name="properties_get_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<spot-check-object-guid>"}}],
      "properties": [{"propertyId": {"guid": "<hersteller-property-guid>"}}]
    }
  }
)
```

3–5 zufällige aus den geupdate'ten Objects prüfen: ist `displayValue` == „Vitra"?

---

> **User sagt:** „Klassifizier den Sessel als Möbelstück nach SAB."

## Worked Example 8 — Object klassifizieren

Klassifikation eines Objects im aktiven System `SAB_Klassifizierung_29` setzen. Voraussetzung: System-GUID + Item-GUID aus dem Warm-up bekannt.

**Aktuelle Klassifikation lesen:**

```python
mcp__archicad__archicad_call_tool(
  name="elements_get_classifications_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elements": [{"elementId": {"guid": "<object-guid>"}}],
      "classificationSystemIds": [
        {"classificationSystemId": {"guid": "<SAB_Klassifizierung_29-system-guid>"}}
      ]
    }
  }
)
```

**Klassifikations-Item-GUIDs holen** (falls nicht aus Warm-up bekannt): Discovery mit `"get classification items in system"`, dann gewünschten Item-GUID aus Response entnehmen.

**Confirm-Dialog:**

```
Ich werde folgendes ändern:
- Object <object-guid>  „Sessel 29"  Klassifikation: (keine) → „Möbel / Sitzmöbel"

Ausführen? (ja / nein / details / abbrechen)
```

**Klassifikation setzen** (nach `ja`) <!-- 2026-05-21 live verifiziert AC29 — Schema-Form korrigiert -->:

```python
mcp__archicad__archicad_call_tool(
  name="elements_set_classifications_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementClassifications": [
        {
          "elementId": {"guid": "<object-guid>"},
          "classificationId": {
            "classificationSystemId": {"guid": "<SAB_Klassifizierung_29-system-guid>"},
            "classificationItemId": {"guid": "<moebel-sitzmoebel-item-guid>"}
          }
        }
      ]
    }
  }
)
```

---

## Bulk-Klassifizierung (Stub)

Für den Workflow „alle Möbel-Objects klassifizieren" kombinieren wir Subtype-Discovery (Worked Example 2) mit dem Standard-Bulk-Klassifizierungs-Pattern aus [`../reference/bulk-operations.md`](../reference/bulk-operations.md).

Schematisch:

1. Alle Objects listen → Library-Part-Name lesen → nach Subtype gruppieren.
2. Für jede Subtype-Gruppe das passende Klassifikations-Item ermitteln (z. B. „Sessel → Möbel/Sitzmöbel", „Waschbecken → Sanitär/WC-Anlagen").
3. Mapping zeigen: Subtype-Name → Klassifikations-Item, Count pro Gruppe.
4. Confirm mit Gesamt-Count — dann `elements_set_classifications_of_elements` mit der vollständigen Liste.

**Hinweis:** Die Subtype-basierte Klassifikation ist die natürliche Ableitung für Library-Objects. Im Gegensatz zu Wänden (deren Klassifikation Zone-Membership erfordert) reicht für Objects der Library-Part-Name als Klassifikations-Schlüssel.

Für das vollständige Read → Filter → Group → Confirm → Apply-Muster: [`../reference/bulk-operations.md`](../reference/bulk-operations.md).

---

## Gotchas

### Gotcha 1 — `get_details_of_elements`-Bug in AC29

`elements_get_details_of_elements` wirft Pydantic-Validierungs-Fehler für viele Element-Typen in AC29 (Schema-Drift durch neue Server-Felder). Für Objects bedeutet das: direkte Geometrie, Position und Layer-Info sind via diesem Tool unzuverlässig. **Workaround:** Bounding Box via `elements_get_3_d_bounding_boxes`, Properties via `properties_get_property_values_of_elements`. Memory-Eintrag: `issue_archicad_mcp_get_details_bug.md`. Gilt bis MCP-Plugin-Update.

### Gotcha 2 — `gdl_parameters_of_elements`-Bug in AC29

`elements_get_gdl_parameters_of_elements` schlägt bei Library-Objects mit komplexen GDL-Parametern fehl — `index` wird als Integer geliefert statt String, plus extra Felder die der Wrapper ablehnt. Bei einfachen Library-Parts kann es funktionieren; bei komplexen (z. B. Türen mit vielen Params, Spezialteilen) entstehen hunderte Validierungs-Fehler. Drei Workaround-Pfade: Property-System, Expression-Linking, Schedule-Pipeline. Ausführlich in [Worked Example 4](#worked-example-4--gdl-parameter-abfragen-bug-workaround).

### Gotcha 3 — Library-Part-Name ist die zuverlässigste Subtype-Quelle

Es gibt keine generische „Subtype"- oder „Kategorie"-Property, die einheitlich alle Library-Objects klassifiziert. Der Library-Part-Name (aus Built-in Properties) ist de facto die einzige zuverlässige Quelle für den Subtype. Subtype-Properties (falls in UserDefined-Properties vorhanden) sind Backup, aber nicht garantiert. Immer den Library-Part-Name-Pfad nehmen.

### Gotcha 4 — Modal-Dialog blockiert MCP (Code 4001)

Wenn in Archicad ein modaler Dialog offen ist (Element-Settings, Property-Manager, Optionen-Setup, Material-Editor), frieren MCP-Calls ein oder liefern Code 4001 mit dem Dialog-Namen. **Reaktion:** Nicht mehrfach retry. Dialog-Namen aus Fehler extrahieren, User bitten ihn zu schließen. Details: [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) § Modal-Dialoge. Besonders relevant bei Object-Edit: der Object-Settings-Dialog ist modal.

### Gotcha 5 — Türen + Fenster sind GDL-Objects, aber NICHT in diesem Recipe

Türen und Fenster sind intern Library-Parts mit dem Element-Typ `Window` bzw. `Door` — nicht `Object`. Sie erscheinen nicht in `elements_get_elements_by_type` mit `elementType: "Object"`. Sie werden über ihren Host-Wand-Bezug verwaltet und sind ausschließlich in [`openings.md`](openings.md) dokumentiert. Verwechslung führt zu leeren Listings oder falschen Bulk-Operationen.

### Gotcha 6 — Curtain-Wall-Accessories sind Library-Objects, aber im falschen Recipe wenn hier behandelt

CurtainWallAccessory-Elemente sind Library-Part-basiert, aber sie erscheinen im Element-Typ `CurtainWallAccessory`, nicht in `Object`. Sie sind Sub-Elemente einer CurtainWall-Hierarchie und werden über `elements_get_subelements_of_hierarchical_elements` zugänglich — nicht über `elements_get_elements_by_type` mit `Object`. Behandlung: [`curtain-walls.md`](curtain-walls.md).

### Gotcha 7 — Library-Server-Verbindung muss aktiv sein

Wenn `library_get_libraries` für eine ServerLibrary `available: false` liefert, sind die Library-Parts nicht geladen. Objects, die auf diese Library-Parts verweisen, zeigen in Archicad Platzhalter-Geometrie. Der Library-Part-Name aus Properties kann dann leer oder inkonsistent sein. Subtype-Discovery für betroffene Objects ist in diesem Zustand unzuverlässig. User informieren: BIMcloud-Verbindung herstellen, Library in Archicad-Library-Manager neu laden.

### Gotcha 8 — EmbeddedLibrary ist Read/Write, aber Pfad-los

EmbeddedLibraries haben `path: null` — sie sind in das Projektpaket eingebettet. Create via `elements_create_objects` mit einem `libraryPartName` aus einer EmbeddedLibrary funktioniert, aber der Name muss exakt mit dem Teil in der EmbeddedLibrary übereinstimmen. Tippfehler → Fehler; kein Fuzzy-Matching. Immer Namen via `library_get_libraries` + Object-Listing verifizieren.

### Gotcha 9 — `dimensions`-Override-Caveat

`dimensions` beim Create (oder Update) überschreibt die Default-Größe des Library-Parts — aber nicht immer vollständig. Manche Library-Parts ignorieren `dimensions`, wenn ihre GDL-Logik die Größe intern via GDL-Parameter steuert (z. B. ein Tisch-Object, das Breite und Tiefe über `A`/`B`-GDL-Parameter hat). In diesen Fällen müssen die GDL-Parameter gesetzt werden, nicht `dimensions`. Nach Create/Update: Bounding-Box-Read zur Verifikation der tatsächlichen Größe.

### Gotcha 10 — Collada-Diffuse-*Texturen* statt Lambert-*Farben* für getrennte Surfaces

<!-- 2026-06-25 live verifiziert AC29 — Collada-Import (3D-Modell zusammenführen) -->

Archicads Collada-Importer liest **plain Lambert-Farben** (`<lambert><diffuse><color>`) **nicht** zuverlässig als getrennte Oberflächen — sie kollabieren beim „Auswahl als Objekt sichern" auf 1–2 generische Surfaces. Dagegen liest er **Diffuse-Texturen** zuverlässig als je eine eigene benannte Surface ein. **Lösung:** pro Material/Bauteil eine winzige (8×8 px) Volltonfarben-JPG als `<diffuse><texture>` einbinden (mit konstanter UV `0.5 0.5`, vollständigem `bind_vertex_input semantic="UVSET0"`). Ergebnis: N Bauteile → N distinkte farbige Archicad-Surfaces. Verifiziert mit INU012-Schirm (5 Surfaces) und 5012-Möbeln (3 Surfaces je Objekt).

### Gotcha 11 — Z-up muss in die Geometrie *gebacken* werden

<!-- 2026-06-25 live verifiziert AC29 -->

Archicad **ignoriert** das Collada-`<up_axis>`-Tag beim Import — ein als `Z_UP` deklariertes, aber Y-up-koordiniertes Mesh kommt liegend/umgefallen rein. **Lösung:** die Rotation direkt in die Vertex-Koordinaten backen: `(x, y, z) → (x, -z, y)`. Zusätzlich im DAE-Header `<up_axis>Z_UP</up_axis>` **und** `<unit name="meter" meter="1"/>` setzen (sonst Skalierungsfehler). Das war die Grundursache des „fällt um beim HD-Umschalten"-Symptoms bei den Außenmöbeln.

### Gotcha 12 — Collada-Merge erzeugt benannte Embedded-Library-Teile → Sammel-Export, nicht 18× Einzelspeichern

<!-- 2026-06-25 live verifiziert AC29 — Teamwork-Projekt -->

„3D-Modell zusammenführen" (Collada-Import) legt jedes importierte Objekt **automatisch als benanntes Teil in der Eingebetteten Bibliothek** an (Typ `Object`, `id: "Catalog object"`, GDL-Params `ColladaMod_sp0/sp1`). Daher: zum Erzeugen vieler GSMs **nicht** N× „Auswahl als Objekt sichern" — denn „Auswahl als Objekt" **verschmilzt die gesamte Selektion zu EINEM** GSM. Stattdessen **Bibliotheksmanager → Eingebettete Bibliothek → alle Teile markieren → Sammel-Export** als einzelne .gsm. Für Teamwork müssen die GSMs danach in eine **geteilte ServerLibrary** (BIMcloud, z. B. `SAB-Furniture_xx.lcf`), sonst löst der Host-`CALL "<name>_HD"` nur lokal auf. Einzeln-Speichern via „Auswahl als Objekt" bleibt nur nötig für ein Objekt, das *nicht* aus dem Merge stammt (z. B. eine separat gebaute Primitive-DAE).

### Gotcha 13 — Scan-Mesh-Dezimierung: textur-erhaltend blockiert an UV-Nähten → pro-Material weld + plain Quadric

<!-- 2026-06-25 Tooling-Lesson (pymeshlab/trimesh, außerhalb MCP) -->

Bei Scan-Meshes (pCon/Möbelkonverter-Exporte) mit stark fragmentierten UV-Inseln **blockiert** die textur-erhaltende Quadric-Dezimierung (`meshing_decimation_quadric_edge_collapse_with_texture`) an den Textur-Nähten — sie erreicht das Face-Target nicht (z. B. 1,25 M → stagniert bei ~239 k statt 40 k). **Lösung:** Mesh nach Material in Untergruppen splitten (trimesh-Scene), pro Gruppe `meshing_merge_close_vertices` (weld) + **plain** `meshing_decimation_quadric_edge_collapse` mit proportionalem Target, dann als getrennte farbige Surfaces wieder zusammenbauen (siehe Gotcha 10). **Verlustfrei farblich**, weil die Original-Texturen ohnehin Volltonfarben sind (Varianz ≈ 0; Foto-Check vorab mit PIL `ImageStat`). Erreicht zuverlässig ~40 k bei erhaltener Farbtrennung.

---

## Verwandte Recipes

- [`openings.md`](openings.md) — Türen + Fenster (technisch GDL-Objects, aber mit Host-Wand-Topologie; NICHT über `elementType: "Object"` abrufbar).
- [`curtain-walls.md`](curtain-walls.md) — Curtain-Wall-Accessories (Library-Object-basiert, aber als CurtainWall-Sub-Elemente verwaltet).
- [`zones.md`](zones.md) — Property-Bulk-Pattern (Vorlage für Worked Example 7 in diesem Recipe).
- [`../reference/bulk-operations.md`](../reference/bulk-operations.md) — Universelles Read → Filter → Group → Confirm → Apply-Muster; Pre-Flight-Check, Identifier-Mapping, Mid-Batch-Fehlerverhalten.
- [`../reference/schedule-pipeline.md`](../reference/schedule-pipeline.md) — Wenn GDL-Werte nicht via MCP lesbar sind (Export-Parse-Match-Update-Pipeline).
- [`../reference/property-expression-linking.md`](../reference/property-expression-linking.md) — Dauerhafter GDL-Parameter ↔ Property-Sync über Archicad-Expression-Editor.
- [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md) — Confirm-Format, Modal-Dialog-Handling, AC29-Bug-Tabelle.

## Worked-Example: Lesen + Klassifikation live (2026-07-14, THN-Referenzmodell)

596 Objekte: `GetDetailsOfElements` liefert `details.libPart{name, parentUnID, ownUnID}`
(z. B. „Notdusche", „Labor-Arbeitsplatz") + `origin{x,y,z}`, `dimensions{x,y,z}`, `angle`.
Klassifikation (System-Filter MUSS verschachtelt sein:
`classificationSystemIds=[{classificationSystemId:{guid}}]` — flache Form gibt 4002):
L_BRANDSCHUTZ-Objekte → Item „Brandschott", A_25_MOEBEL → „Möbel".
Querschnitts-/Maßfragen: 2D-Symbol verfälscht die 2D-Box → `API.Get3DBoundingBoxes`.
