# Bulk-Operations — Archicad-Skill

Diese Datei dokumentiert, wie wir Massen-Updates und Klassifizierungen durchführen. [SKILL.md](../SKILL.md) verweist hierher bei Operationen, die mehr als ein paar Elemente betreffen — also bei den meisten echten Architekten-Aufträgen.

## Inhaltsverzeichnis

1. [Das universelle Read → Filter → Group → Confirm → Apply-Pattern](#das-universelle-read--filter--group--confirm--apply-pattern)
2. [Klassifikations-Spezifika](#klassifikations-spezifika)
3. [Pro-Element-Typ-Ableitungslogik](#pro-element-typ-ableitungslogik)
4. [Mehrdeutigkeits-Handling](#mehrdeutigkeits-handling)
5. [Mid-Batch-Fehlerverhalten](#mid-batch-fehlerverhalten)
6. [Property-Set live verifiziert (CAP-01)](#property-set-live-verifiziert-cap-01)
7. [Property-Definitions sind Create-Only (CAP-02 Negativ-Befund)](#property-definitions-sind-create-only-cap-02-negativ-befund)
8. [Schema-Bug: `defaultValue` ist required (nicht optional)](#schema-bug-defaultvalue-ist-required-nicht-optional)
9. [Klassifikations-Set: success ist nicht success](#klassifikations-set-success-ist-nicht-success-live-verifiziert-2026-05-21)
10. [Klassifikations-System löschen = stiller Datenverlust](#klassifikations-system-löschen--stiller-datenverlust-erst-migrieren-dann-löschen)
11. [Enum-Migration-Pattern (Modify-Endpoint-Umgehung)](#enum-migration-pattern-modify-endpoint-umgehung)
12. [Pre-Flight: Property-Enum-Normalisierung](#pre-flight-property-enum-normalisierung)
13. [Identifier-Mapping bei externen Daten-Quellen](#identifier-mapping-bei-externen-daten-quellen)
14. [GDL-Parameter vs. expression-verlinkte Property als Quelle](#gdl-parameter-vs-expression-verlinkte-property-als-quelle-live-verifiziert-2026-06-11)
15. [Phase-5-Abschluss: Live-Antworten](#phase-5-abschluss-live-antworten-2026-07-14-thn-4500-wände-maßstab)

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

**Klassifikations-System-GUIDs driften — nie sessionübergreifend hardcoden.** <!-- 2026-06-11 --> Dieselbe (Teamwork-)Datei kann nach einem Schließen/Wiederöffnen ein Klassifikations-System mit **anderer GUID** präsentieren als vorher (live beobachtet: SAB-System-GUID änderte sich nach Reopen, die alte GUID war danach nicht mehr auffindbar → Fehler `6201 Classification system not found`). Ursache: Teamwork-Sync / Versions-Neuableitung beim Re-Open. **Regel:** System-GUIDs immer **live neu auflösen** (`GetAllClassificationSystems` → über den `name` matchen, z. B. „SAB_Klassifizierung"), nie eine GUID aus einer früheren Session oder einem Notiz-/Memory-Eintrag übernehmen. Vorsicht auch vor **Doppel-Systemen gleichen Namens** (Teamwork-Merge-Artefakt): zwei Systeme „Archicad Klassifizierung" mit verschiedenen GUIDs/Versionen können parallel existieren — beide einzeln behandeln.

**Folge-Risiko:** Wird gegen eine veraltete System-GUID gemessen, liefert die Analyse stillschweigend Unsinn (0 Items / 0 klassifiziert), obwohl Daten da sind. Bei widersprüchlichen Zahlen zwischen zwei Läufen: **stoppen** und die System-GUID live gegen den Namen verifizieren, bevor irgendeine Entscheidung (erst recht eine Löschung) getroffen wird.

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

## Property-Set live verifiziert (CAP-01)

Live verifiziert am 2026-05-21 gegen AC29: das verwendete Schema für Property-Updates ist viel einfacher als zunächst befürchtet.

**Set-Schema:**
```python
mcp__archicad__archicad_call_tool(
  name="properties_set_property_values_of_elements",
  arguments={
    "port": <port>,
    "params": {
      "elementPropertyValues": [{
        "elementId": {"guid": "<element-guid>"},
        "propertyId": {"guid": "<property-guid>"},
        "propertyValue": {"value": "<display-string>"}  # einfach String!
      }]
    }
  }
)
```

`propertyValue.value` ist immer ein String — auch für Enum-Properties wird der Display-Wert als String übergeben (z. B. `"Linoleum 2,5 (R9)"`). Nicht das komplexe `{type, status, value: {type: "displayValue", displayValue: ...}}`-Konstrukt.

**Read-Schema** symmetrisch via `properties_get_property_values_of_elements` — Response liefert `propertyValue.value` als String zurück.

**Voraussetzung (kritisch!):** Das Element muss **klassifiziert** sein im Klassifikations-Item, das in der Property-`availability`-Liste steht. Sonst Fehler: `Not available or not evaluated property` (Code `-2130312908`).

**Workaround bei nicht klassifizierten Elementen:** Vor dem Set einmalig `mcp__archicad__elements_set_classifications_of_elements` aufrufen mit der passenden Klassifikation. Live verifiziert: nach Klassifizierung funktioniert Property-Set sofort.

**Achtung — Raw-HTTP-Schema ≠ MCP-Schema (multiEnum).** <!-- 2026-06-18 --> Beim **direkten HTTP-Bypass** (`API.SetPropertyValuesOfElements`, für Massendaten) ist `propertyValue` NICHT der einfache `{"value": "<string>"}`. Der Wrapper validiert ein `oneOf` und lehnt den String mit `4002` ab. Verifiziert für eine **multiEnum**-Property:
```json
{"elementPropertyValues": [{
  "elementId": {"guid": "..."},
  "propertyId": {"guid": "..."},
  "propertyValue": {"type": "multiEnum", "status": "normal",
                    "value": [{"enumValueId": {"type": "displayValue", "displayValue": "fb"}}]}
}]}
```
Also: `type` + `status` zwingend, und `value` ist eine **Liste** von `{enumValueId:{type:"displayValue", displayValue:...}}` (mehrere Einträge = mehrere gesetzte Werte; Co-Werte beim Migrieren mitführen). Das ist exakt das Format, das der Read (`API.GetPropertyValuesOfElements`) zurückliefert — Read-Schema spiegeln. Erfolg pro Element in `result.executionResults[i].success`.

## Property-Definitions sind Create-Only (CAP-02 Negativ-Befund)

Live verifiziert am 2026-05-21: **es gibt keinen Modify-Endpoint für Property-Definitions.** `properties_create_property_definitions` mit gleichem Namen wie eine existierende Property schlägt fehl (Code `-2130312988`).

**Auswirkung für Enum-Erweiterung:**
- Neue Enum-Werte zu einer existierenden Property hinzufügen → **nicht via MCP möglich**
- User muss in Archicad-UI Property-Manager → „Optionen-Setup" verwenden
- Alternative (zerstörerisch): Property löschen + neu erstellen mit erweiterter Enum-Liste — verliert alle zugewiesenen Werte

**Workflow-Konsequenz für Bulk-Klassifizierung:**
1. Vor jedem Bulk-Property-Set die Property-Definition lesen via `properties_get_details_of_properties`
2. Source-Werte gegen `possibleEnumValues` matchen
3. Bei Mismatch: STOPPEN und User auffordern, in Archicad-UI die fehlenden Werte zu ergänzen
4. NIEMALS „einfach" delete + recreate — Daten-Verlust

## Schema-Bug: `defaultValue` ist required (nicht optional)

Live verifiziert am 2026-05-21: das `properties_create_property_definitions`-Schema markiert `defaultValue` als optional, **aber tatsächlich ist es Pflicht**. Aufrufe ohne `defaultValue` schlagen fehl (Code `-2130313104`).

**Korrektes Schema** für Enum-Property-Create:
```python
{
  "name": "<name>",
  "description": "<desc>",
  "type": "singleEnum",
  "isEditable": true,
  "defaultValue": {  # <-- Pflicht trotz Schema-Optional
    "basicDefaultValue": {
      "type": "singleEnum",
      "status": "normal",
      "value": {"type": "displayValue", "displayValue": "<default-enum-value>"}
    }
  },
  "possibleEnumValues": [{"enumValue": {"displayValue": "..."}}, ...],
  "availability": [{"classificationItemId": {"guid": "<class-item-guid>"}}],  # min 1
  "group": {"propertyGroupId": {"guid": "<group-guid>"}}
}
```

Für Property-Groups: erst `properties_create_property_groups` aufrufen, dann die Group-GUID in `propertyDefinition.group` verwenden. Built-in-Group-GUIDs scheinen nicht für UserDefined-Properties verwendbar zu sein.

## Klassifikations-Set: success ist nicht success (live verifiziert 2026-05-21)

**Wichtige Warnung:** `elements_set_classifications_of_elements` meldet `{success: true}` für JEDES Element im Bulk-Call — auch wenn die Klassifikation in Wirklichkeit nicht greift. Live verifiziert: 56-Element-Bulk-Test, 56× „success", aber nur 9 Elemente tatsächlich klassifiziert.

**Welche Element-Typen lassen sich direkt klassifizieren (in AC29):**

| Element-Typ | Direkt klassifizierbar? |
|---|---|
| Wall, Window, Door, Slab, Column, Zone, Beam | ✅ |
| CurtainWall (Top-Level) | ✅ |
| **CurtainWallSegment / Frame / Panel / Junction / Accessory** | ❌ |
| Object (user-platziert: Möbel, Sanitär, ...) | ✅ |
| Object (System-Stamps: Apartment-Stamp, Camera, ...) | ❌ |
| Andere hierarchische Sub-Elemente (Stair-Riser, Railing-Post, ...) | wahrscheinlich ❌ (analog CW-Subs) |

**Pflicht-Pattern:** Nach jedem Bulk-Set IMMER `elements_get_elements_by_classification` als Reverse-Lookup aufrufen und Cardinality vergleichen (erwartet vs. real). Bei Diskrepanz: User informieren, welche Elemente nicht klassifiziert sind.

**Umgehung für CurtainWall-Sub-Klassifikationen:**
- Sub-Elemente **erben vermutlich** die Klassifikation vom Top-Level — Top-Level klassifizieren reicht für die meisten Workflows.
- Falls Sub-Element-spezifische Klassen nötig: nur UI-Workflow möglich (Archicad-Element-Settings).

**Umgehung für System-Stamps:**
- Vor Bulk-Klassifikation: `elements_get_elements_by_type` + Property-Filter um nur user-platzierte Objects zu finden (siehe `recipes/library-objects.md` § Subtype-Discovery).

## Klassifikations-System löschen = stiller Datenverlust (erst migrieren, dann löschen) <!-- 2026-06-11 -->

Wenn der User ein ganzes Klassifikations-System loswerden will (z. B. ein redundantes „Archicad Klassifizierung"-System nach einem Teamwork-Merge), ist die Löschung **irreversibel** und kann zwei Arten von Daten still vernichten. Wir geben **nie** grünes Licht zum Löschen, bevor eine Migration + Verifikation gelaufen ist.

**Was bei der Löschung verloren geht:**

1. **Die Klassifikation selbst** — jedes Element, das **nur** im gelöschten System klassifiziert ist, wird danach unklassifiziert. (Elemente, die zusätzlich in einem anderen System klassifiziert sind, behalten dort ihr Etikett.)
2. **Property-Werte über Verfügbarkeits-Bindung** — die gefährliche, unsichtbare Variante. Property-`availability` ist an Klassifikations-**Items** gebunden (siehe CAP-01). Hängt die Verfügbarkeit eines befüllten Properties **nur** an einem Item des gelöschten Systems, wird das Property nach der Löschung `notAvailable` und der **Wert verschwindet**. Dasselbe Muster wie bei einer Umklassifizierung, die Properties wegfallen lässt.

**Audit vor dem Löschen (read-only):**

1. Alle drei Mengen live bestimmen: Elemente klassifiziert im **Lösch-System**, Elemente klassifiziert im **Ziel-System**, Schnittmenge. → `nur-im-Lösch-System` ist die Risikomenge.
2. Pro Risiko-Element den Item-**Namen** ermitteln und prüfen, ob im Ziel-System ein **gleichnamiges Item** existiert (Migrierbarkeit). Fehlende Gegenstücke separat melden — die kann man nicht automatisch migrieren.
3. Auf den Risiko-Elementen die befüllten Properties zählen (Status `normal`, nicht leer). Das ist der potenzielle Property-Wert-Verlust.

**Sichere Reihenfolge — erst spiegeln, dann löschen:**

```
1. MIGRATE (additiv, sicher): Risiko-Elemente zusätzlich im Ziel-System
   mit dem gleichnamigen Item klassifizieren. Es wird nichts überschrieben —
   das Element trägt danach BEIDE Etiketten. Confirm-Schleife wie bei jedem
   schreibenden Bulk (SAFE-01); bei Teamwork vorher ReserveElements.
2. VERIFY Klassifikation: Reverse-Lookup, dass alle Risiko-Elemente jetzt
   auch im Ziel-System klassifiziert sind.
3. VERIFY Property-Verfügbarkeit (1-Element-Kontrolltest): an EINEM Beispiel
   die Lösch-System-Klassifikation entfernen, prüfen ob die Property-Werte
   erhalten bleiben (weil das Ziel-Item dieselbe availability liefert).
   Bleiben sie → Löschung sicher. Verschwinden sie → erst die Property-
   availability auf die Ziel-Items erweitern (Archicad-UI, Property-Manager),
   sonst gehen die Werte trotz Migration verloren.
4. DELETE: erst jetzt löscht der USER das System in der Archicad-UI
   (Klassifikations-Manager). Die Löschung selbst ist kein MCP-Schritt.
```

**Merksatz:** Eine bestandene Klassifikations-Migration heißt noch nicht, dass die *Property-Werte* die Löschung überleben — die hängen an der `availability`, nicht am Etikett. Immer beides verifizieren.

### Basis-vs-Unterklasse-Entscheidung: Verfügbarkeits-Diff <!-- 2026-06-19 -->

Vor dem Klassifizieren auf eine **Unterklasse** (statt Basisklasse) prüfen, ob dabei Property-Verfügbarkeit verloren geht. Wenn ja → Basisklasse wählen; wenn nein → Unterklasse ist sicher (feiner = besser).

Vorgehen — `API.GetClassificationItemAvailability` pro Item, dann Set-Diff `basis_props − unterklasse_props`:

```python
# params: {"classificationItemIds":[{"classificationItemId":{"guid": <item>}}]}
def get_props(guid):
    r = call("API.GetClassificationItemAvailability",
             {"classificationItemIds":[{"classificationItemId":{"guid":guid}}]})
    lst = r["result"]["classificationItemAvailabilityList"][0] \
           ["classificationItemAvailability"]["availableProperties"]
    return set(p["propertyId"]["guid"] for p in lst)

lost = get_props(BASIS) - get_props(UNTERKLASSE)   # leer → kein Verlust → Unterklasse OK
```

**Parse-Pfad merken** (leicht zu verfehlen): `result.classificationItemAvailabilityList[0].classificationItemAvailability.availableProperties[].propertyId.guid`. Ein falscher Schlüssel liefert still `0`/leere Mengen → man würde fälschlich „alles verloren" schließen. Live verifiziert AC29: alle 8 Wand-Unterklassen (Beton-, Trockenbau-, MW-, Trennwand, Brüstung, Vor-, Element-, Bewegliche Wand) haben **identische 82 Properties** wie Basis „Wand" → kein Verlust.

## Enum-Migration-Pattern (Modify-Endpoint-Umgehung)

Da `properties_create_property_definitions` Create-Only ist (CAP-02), gibt es keinen direkten Modify-Endpoint. Wenn eine bestehende Enum-Property um neue Werte erweitert werden muss, lautet der programmatische Workaround:

```
1. BACKUP: Alle Element-Werte der Property lesen
2. DELETE: Property-Definition löschen (verliert alle Werte global)
3. RECREATE: Neue Property mit gleichem Namen + erweiterter Enum-Liste anlegen
4. RESTORE: Alle Element-Werte aus Backup zurück-setzen
5. REPORT: Alte Property-GUID war X, neue ist Y — User informieren falls externe Refs (Schedules, IFC, Saved-Views) neu verknüpft werden müssen
```

**Detail-Pseudocode:**
```python
# 1. BACKUP
old_property_guid = "<existing-property-guid>"
all_elements = mcp__archicad__elements_get_elements_by_type(elementType="<typ>")
backup = {}
for e in all_elements:
    response = mcp__archicad__properties_get_property_values_of_elements(
        elements=[{"elementId": e["elementId"]}],
        properties=[{"propertyId": {"guid": old_property_guid}}]
    )
    value = response["propertyValuesForElements"][0]["propertyValues"][0]
    if "propertyValue" in value:  # skip errors
        backup[e["elementId"]["guid"]] = value["propertyValue"]["value"]

# 2. CONFIRM mit User
print(f"Bin gleich Property {old_property_guid} löschen + neu anlegen mit erweiterter Enum-Liste.")
print(f"  Backup: {len(backup)} Element-Werte gesichert in Memory")
print(f"  ACHTUNG: Property-GUID ändert sich. Externe Refs (Schedules, IFC) müssen ggf. neu verknüpft werden.")
print(f"  Fortfahren? (ja/abbrechen)")

# 3. DELETE alte Property
mcp__archicad__properties_delete_property_definitions(propertyIds=[{"propertyId": {"guid": old_property_guid}}])

# 4. RECREATE mit erweiterter Enum-Liste
new_response = mcp__archicad__properties_create_property_definitions(
    propertyDefinitions=[{
        "propertyDefinition": {
            "name": "<same-name>",
            "description": "<same-or-updated>",
            "type": "singleEnum",
            "isEditable": True,
            "defaultValue": {"basicDefaultValue": {"type": "singleEnum", "status": "normal",
                "value": {"type": "displayValue", "displayValue": "<some-existing-or-new-value>"}}},
            "possibleEnumValues": [{"enumValue": {"displayValue": v}} for v in extended_enum_list],
            "availability": [{"classificationItemId": {"guid": "<class-guid>"}}],
            "group": {"propertyGroupId": {"guid": "<group-guid>"}}
        }
    }]
)
new_property_guid = new_response["propertyIds"][0]["propertyId"]["guid"]

# 5. RESTORE — Werte aus Backup zurück-setzen
restore_data = []
for element_guid, value in backup.items():
    restore_data.append({
        "elementId": {"guid": element_guid},
        "propertyId": {"guid": new_property_guid},
        "propertyValue": {"value": value}
    })
result = mcp__archicad__properties_set_property_values_of_elements(elementPropertyValues=restore_data)

# 6. VERIFY: pro restauriertem Element den Wert nochmal lesen + mit Backup vergleichen
# (mind. 3-5 Stichproben)

# 7. REPORT
print(f"Migration komplett:")
print(f"  Alte Property-GUID: {old_property_guid} (gelöscht)")
print(f"  Neue Property-GUID: {new_property_guid}")
print(f"  Restored: {len(restore_data)} Element-Werte")
print(f"  Externe Refs ggf. neu verknüpfen: Schedules, IFC-Pset-Mappings, Saved-Views")
```

**Caveats:**
- **GUID-Cascade:** Alle externen Referenzen auf die alte Property-GUID (Schedule-Spalten-Konfigs, IFC-Pset-Mappings, Saved-Views mit Property-Filter) brechen und müssen manuell neu verknüpft werden.
- **Atomar nicht garantiert:** Wenn `delete` durchläuft aber `recreate` scheitert, sind alle Werte verloren. Möglichst zuerst in einem Test-Projekt verifizieren.
- **Werte mit nicht-mehr-existenten Enum-Strings:** Wenn alte Werte sich in der neuen Enum-Liste nicht finden, werden sie beim restore fehlschlagen. Pre-Flight: alle Backup-Werte gegen `extended_enum_list` matchen, mismatches dem User vorher melden.

**Alternative-Pattern (siehe `property-expression-linking.md`):** Expression-Property statt fester Enum — wächst automatisch via GDL-Parameter, kein Migration nötig. Setup-Aufwand einmal in Archicad-UI.

## Pre-Flight: Property-Enum-Normalisierung

Bei Bulk-Updates von Single-Enum- oder Multi-Enum-Properties prüfen wir **vor APPLY**, ob die Ziel-Werte exakt in der Property-Enum-Definition vorkommen. Sonst:

- Archicad lehnt den Set-Call ab (Fehler), oder
- Archicad ignoriert den Wert still (Daten-Inkonsistenz, schwer zu debuggen).

**Typische Inkonsistenzen** zwischen Source-Daten und Enum-Definition:

| Inkonsistenz-Typ | Beispiel | Auswirkung |
|---|---|---|
| Trailing Spaces | „Linoleum " vs. „Linoleum" | zwei verschiedene Enum-IDs intern |
| Tippfehler | „Linolium" statt „Linoleum" | kein Match, Update schlägt fehl |
| Trennzeichen | „R11+R12" vs. „R11/R12" | kein Match |
| Singular/Plural | „Fliese R10" vs. „Fliesen (R10)" | kein Match |
| Klammer-Stil | „Fliesen R10" vs. „Fliesen (R10)" | kein Match |
| Fehlende Enum-Werte | Source hat 21 Werte, Enum-Definition hat nur 8 | 13 Werte landen nirgendwo |

**Pre-Flight-Workflow:**

1. **Property-Enum-Definition lesen** — entweder via `mcp__archicad__properties_get_property_values_of_elements` an einem Beispiel-Element (zeigt aktuelle möglichen Werte), oder via dediziertem Property-Definition-Read falls verfügbar.
2. **Source-Werte gegen Enum diffen:**
   ```
   not_in_enum = [v for v in source_values if v not in enum_values]
   close_matches = {v: difflib.get_close_matches(v, enum_values) for v in not_in_enum}
   ```
3. **Wenn `not_in_enum` nicht leer:** STOPPEN. User-Report mit:
   - Werte, die fehlen.
   - Ähnliche existierende Werte (Substring-Match oder Levenshtein).
   - Vorschlag: User ergänzt die Property-Definition in Archicad (Property-Manager → Property-Edit → Enum-Werte hinzufügen), dann Re-Run.

**NIEMALS** einen Bulk-Update mit unbekannten Enum-Werten starten. Lieber Pre-Flight-Pause als Daten-Korruption.

## Identifier-Mapping bei externen Daten-Quellen

Bei Bulk-Updates aus externen Datenquellen (Schedule-Export, Capmo, Excel) ist der Identifier meistens **nicht** die MCP-GUID, sondern eine fachliche ID — Raumnummer, Element-Bezeichnung, Schedule-Index.

**Mapping-Workflow:**

1. Alle relevanten Elemente via `mcp__archicad__elements_get_elements_by_type` listen.
2. Pro Element die Identifier-Property auslesen via `mcp__archicad__properties_get_all_property_ids_of_elements` + `properties_get_property_values_of_elements`. Typische Identifier-Properties:
   - „Raumnummer" / „RoomNumber" — bei Zonen
   - „Element-ID" — Built-in
   - „Bezeichnung" / „Name" — bei Library-Objects
3. Dict bauen: `{identifier_string: element_guid}`.
4. **Eindeutigkeits-Check:** Jede Source-Zeile sollte genau einen GUID-Match haben. Bei mehreren oder keinem → User informieren, **nicht raten**.

Falls die externen Daten als XLSX/CSV vorliegen: siehe [`schedule-pipeline.md`](schedule-pipeline.md) für die End-to-End-Pipeline.

## GDL-Parameter vs. expression-verlinkte Property als Quelle (live verifiziert 2026-06-11)

Beim GROUP-Schritt einer Bulk-Klassifizierung ist die **Wahl der Quell-Daten** entscheidend — und nicht jede Quelle, die plausibel aussieht, trägt die echten per-Objekt-Werte. Live-Fall: 1.899 Möbel sollten nach „Vertragsart" (Architektur / RV Standard / RV Innovativ / Funktional) klassifiziert werden.

**Der Reinfall:** Es gab eine expression-verlinkte Property „Preisklasse" (siehe `property-expression-linking.md`), die nach dem Library-Objekt befüllt sein *sollte*. Der Property-Read lieferte aber **uniform 1.664× „RV Standard" + 235× notAvailable** — null Ästhetik, null Innovativ, null Funktional. Das war der **Definitions-Default**, nicht die echten Objektwerte. Hätte man darauf vertraut, wären 1.664 Möbel falsch als „RV Standard" klassifiziert worden.

**Die echte Quelle:** Der **GDL-Parameter** des Bibliothekselements (`priceClass`-Code: `aesthetik` / `rv_standard` / `innovativ` / `rv_innovativ` / `funktional`, plus Anzeige-Param `preisklasse`). <!-- 2026-06-24: `innovativ` ist ein eigener Code zusätzlich zu `rv_innovativ` — beide → Vertragsart „RV Innovativ". `innovativ` beginnt NICHT mit `rv`, fällt also durch eine naive `startswith("rv")`-Logik. Codes explizit matchen, nicht per Präfix raten. -->. Per GDL-Read ergab sich die wahre Verteilung: 568 Ästhetik, 136 RV Standard, 45 Funktional, 5 RV Innovativ — und **1.145 Objekte ganz ohne `priceClass`** (Bibliothekselemente, die die Metadaten nicht eingebettet haben). Der saubere Maschinen-Code im GDL-Param war zugleich eindeutiger als die separate boolean-Property „Innovativ" (die `rv_standard`-vs-`rv_innovativ`-Unterscheidung steckte direkt im Code).

**Regel: Quelle gegenchecken, bevor man tausende Werte schreibt.** Eine verdächtig **uniforme** Verteilung (alle identisch oder fast alles ein Wert) ist ein Warnsignal, dass man einen Default statt echter Daten liest. Bei Bulk-Klassifizierung aus Library-Objekten ist der **GDL-Parameter die verlässlichere Ground Truth** als eine expression-verlinkte Property — letztere kann projektweit unbefüllt sein und nur den Default zeigen. Mindestens eine Handvoll Objekte mit bekannten, unterschiedlichen Werten als Stichprobe gegen beide Quellen prüfen. (GDL-Read bei großen Mengen via direktem HTTP-Zugriff, siehe `mcp-conventions.md` § Direkter HTTP-Zugriff — umgeht den AC29-Pydantic-Bug.)

**Objekte ohne Quelldaten:** Die 1.145 ohne `priceClass` ehrlich als „nicht zuordenbar" ausweisen und **leer lassen** — nicht raten, nicht auf einen Default zwingen. Der User entscheidet, ob diese aus einer anderen Quelle nachklassifiziert werden.

**Additiv statt voller Soll/Ist-Recompute, wenn der User schon manuell gearbeitet hat.** <!-- 2026-06-24 --> In einer Folge-Session („ich habe jetzt 89 Möbel auf Inno gesetzt") verlockt ein vollständiger Recompute (Soll aus Quelle berechnen, alles Abweichende überschreiben). Gefahr: Quellen widersprechen sich (GDL `priceClass=aesthetik`, aber Property `Preisklasse=RV Innovativ` + `Innovativ=Wahr`), und der Recompute würde die **manuell gesetzten** Werte zurückdrehen (live: 8× `RV Innovativ → Architektur`, 2× `Schreiner → RV Standard` — echter Datenverlust an Handarbeit). **Regel:** Bei einer „mach die fehlenden noch"-Aufgabe rein **additiv** vorgehen — nur Elemente setzen, deren Quelle das Ziel klar sagt **und** die den Zielwert noch *nicht* tragen; bestehende echte Werte (inkl. Spezial-Kategorien wie „Schreiner") nie überschreiben. Vor dem Write die Transitionen nach „echter Wert → anderer echter Wert" filtern und dem User separat zeigen; diese sind verdächtig und gehören nicht in einen Blind-Write. Wenn die erwartete Änderungsmenge (vom User genannt, z. B. ~89) stark von der berechneten abweicht (live: nur 4 echt offen, 162 schon korrekt), ist die Prämisse überholt → stoppen und berichten statt den alten Plan ausführen.

### TeamWork: Bulk-Set scheitert pro-Element mit Code 6001

Bei einem Teamwork-Projekt liefert `SetPropertyValuesOfElements` **pro Element** `success` oder `{"code":6001,"message":"TeamWork permission denied"}` — nicht reservierte Elemente schlagen einzeln fehl, der reservierte Rest geht durch. Im Live-Fall: 754 zu setzen → 723 erfolgreich, 31 mit 6001 (vom User schlicht nicht mitreserviert).

**Claude kann selbst reservieren — kein manueller User-Schritt nötig.** <!-- 2026-06-18 --> Das MCP-Tool **`teamwork_reserve_elements`** reserviert eine Liste Elemente direkt: `arguments={"port":<port>, "params":{"elements":[{"elementId":{"guid":...}}, ...]}}`. Erfolg = `{"executionResult":{"success":true}}`. Beim **rohen HTTP-Bypass** geht dasselbe über die **Tapir**-Command `ReserveElements` (`API.ExecuteAddOnCommand`, `commandNamespace:"TapirCommand"`, `addOnCommandParameters={"elements":[{"elementId":{"guid":...}}]}`); Erfolg = `result.addOnCommandResponse.executionResult.success:true`. <!-- 2026-06-24: live verifiziert, 3 unreservierte GUIDs in einem Rutsch reserviert, danach Set erfolgreich. `API.ReserveElements` als offizieller Command existiert NICHT — nur via Tapir bzw. MCP-Tool. -->. Damit ist der Idiom-Ablauf für einen Teamwork-Write: **(1) Ziel-GUIDs reservieren → (2) `SetPropertyValuesOfElements` → (3) Rücklesen/Verify.** 31 GUIDs auf einmal live verifiziert. (Hinweis: schlägt `teamwork_reserve_elements` mit „port not active" fehl → erst `discovery_list_active_archicads`, siehe `mcp-conventions.md` § Fehlerklassen.)

**Robustes Nachfass-Pattern (idempotent), falls trotzdem 6001 übrig bleibt:**
1. Nach dem Bulk-Set per **Verify** (Soll/Ist-Vergleich: aktuellen Property-Wert gegen Ziel lesen) ermitteln, welche Elemente noch *nicht* den Zielwert tragen — das deckt die 6001-Fehlschläge unabhängig vom Batch-Log auf.
2. Diese Rest-GUIDs reservieren (`teamwork_reserve_elements`). Falls Reservierung scheitert (z. B. anderer User hält das Element), dem User geben — hilfreich: per `ChangeSelectionOfElements` (Tapir, `addElementsToSelection`) **in Archicad selektieren**, damit er sie in einem Rutsch übernehmen kann (Selektion ist frei, kein Confirm nötig).
3. Nach Reservierung das Set **nur für die Rest-GUIDs** wiederholen. Re-Set bereits korrekter Elemente wäre harmlos, aber das Verify-gefilterte Retry ist sauberer.

**`ReserveElements`-„success" ist KEIN Beweis für eine Reservierung.** <!-- 2026-07-06 --> Tapir `ReserveElements` (1.4.0) meldet `executionResult.success:true` auch dann, wenn die Reservierung real **nicht** greift — Konflikte (Elemente von anderem User/anderer Session gehalten) werden verschluckt, der anschließende Set scheitert weiter mit 6001. Live-Fall: 297 Wände, Reserve „success", Set 297× 6001; erst nachdem der User **manuell in der Archicad-UI reserviert** hat (Suchen & Auswählen → Elementtyp Wand → Rechtsklick → Teamwork → Elemente reservieren), lief der Set 297/297 durch. Der 2026-06-24-Befund oben (Reserve via API funktioniert) gilt also nur für den konfliktfreien Fall. **Idiom bei hartnäckigem 6001:** User manuell reservieren lassen — die UI zeigt dabei auch, *wer* die Elemente hält.

**Diagnose-Trick: Rolle vs. Reservierung trennen.** <!-- 2026-07-06 --> 6001 kann „Rolle darf nicht" oder „Element nicht reserviert" heißen. Entscheidungstest: ein **eigenes Wegwerf-Element erstellen und klassifizieren** (Create ist reservierungsfrei) — z. B. Mini-Slab via Tapir `CreateSlabs`, dann `API.SetClassificationsOfElements` darauf, dann `DeleteElements`. Klappt das (live: `success:true`), sind Rolle und Klassifizierungs-API in Ordnung → das Problem ist rein die Reservierung der Bestandselemente. Achtung: **Polylinien sind nicht klassifizierbar** (7203 „Element not supported") — für den Test ein 3D-Element (Slab) nehmen.

**Doppelt geöffnetes Teamwork-Projekt = Zombie-Reservierungen.** <!-- 2026-07-06 --> Ist dasselbe Teamwork-Projekt in **zwei Archicad-Instanzen** offen (Discovery zeigt zweimal denselben `projectName` auf verschiedenen Ports), behandelt BIMcloud die Sessions getrennt — auch beim selben Benutzer. Reservierungen der einen Session blockieren die andere; die ältere Instanz degeneriert oft zum Zombie (`GetProductInfo` liefert `{}`, `GetProjectInfo` „no open project"). Vor Teamwork-Writes per Discovery auf Doppel-Öffnung prüfen und die Zombie-Instanz schließen lassen (ohne Senden).

**Achtung Selektions-Falle:** `ChangeSelectionOfElements` mit `addElementsToSelection` *addiert* zur bestehenden Auswahl. Wenn der User „mach alle Selektierten" sagt, kann seine Auswahl andere/mehr Elemente enthalten als die Rest-Menge — die echten Problemkinder sind evtl. gar nicht dabei (z. B. weil unreserviert/nicht selektierbar). Nach so einem Lauf **immer per Verify gegen die Soll-Menge** prüfen, ob die ursprünglichen Fehlschläge wirklich erledigt sind, statt sich auf „0 Fehler im letzten Batch" zu verlassen.

## Phase-5-Abschluss: Live-Antworten (2026-07-14, THN, 4.500-Wände-Maßstab)

Alle offenen TODO-Punkte wurden im Produktivbetrieb geklärt:

- **System-scoped GUID-Lookup**: `API.GetAllClassificationSystems` → Systeme mit
  GUID; `API.GetAllClassificationsInSystem {classificationSystemId}` → Item-Baum
  (rekursiv `children` laufen, `classificationItemId.guid` + `id`/`name` sammeln;
  THN: 299 Items in EINER Antwort, keine Pagination aufgetreten).
  `GetClassificationsOfElements` braucht die System-ID **explizit** — leere
  Systemliste ⇒ leeres Ergebnis. Leer-Klassifizierung = Item-GUID `00000000-…`.
- **Innen/Außen bzw. semantische Ableitung**: Nicht geometrisch gelöst, sondern
  über **Ebenen-Semantik + Referenz-Kreuztabelle** (Ebene × Klasse ist oft
  deterministisch) und **räumliche kNN-Zuordnung** für Regionen-Kategorien —
  siehe [`referenzmodell-abgleich.md`](referenzmodell-abgleich.md). Robuster als
  Polygon-Containment bei verzerrten Beständen.
- **Innere-Raum-Erkennung**: Zonen sind per direktem Tapir-HTTP VOLL lesbar
  (polygonOutline + Bögen + Löcher) → Punkt-in-Zone offline mit shapely.
- **Pagination**: Klassifikations-Endpunkte lieferten ungeteilt; nur der
  MCP-Wrapper lagert Riesen-Antworten (>500 KB) in Dateien aus — direkter
  HTTP-Zugriff umgeht das.
- **Skalen-Beweis für das Read→Filter→Group→Confirm→Apply-Muster**: 4.500
  Klassifizierungen + 4.500 Bauteilname + 2.171 Brandschutz in 300er-Batches,
  0 Fehler. Reihenfolge zwingend: **erst Klassifizierung, dann Properties**
  (notAvailable-Falle) — Formate/Fallen in
  [`referenzmodell-abgleich.md § Property-API`](referenzmodell-abgleich.md).

### CAP-03/CAP-04 aufgelöst <!-- 2026-07-14 -->

- **CAP-03 (Selektion)**: `GetSelectedElements` produktiv als Markierungsrahmen-
  Workflow (Rahmen + Cmd+A → bis 251.798 Elemente gelesen). Selektion überlebt
  Fensterwechsel; per API setzbar via `ChangeSelectionOfElements`
  (add/remove) — Basis für „Zoom auf Auswahl"-Übergaben an den User.
- **CAP-04 (Raumnummer)**: Zonen-Nummer = Feld `numberStr` in den Zone-Details
  (direkter Tapir-Abruf, MCP-Wrapper-Bug greift dort nicht). **Create-only** —
  bei CreateZones mitgeben; kein Modify-Pfad. Element-IDs (z. B. Türnummern)
  dagegen über BuiltIn-Property `General_ElementID` schreibbar.
