# SCRATCH: Zone-Attribut-Klassifizierung & Enum-Verwaltung

**Datum:** 2026-05-20
**Session-Kontext:** BSZ Gunzenhausen V29 (Teamwork, Port 19724), 66 Räume EG+OG, Bulk-Aktualisierung „Bodenbelag"-Property aus Raumstempel-GDL-Werten.
**Status:** Real-World-Befunde aus Live-Session — gehört in Skill, aber Form noch offen (siehe „Skill-Anker" unten).

---

## Use-Case in einer Zeile

Stempel-GDL-Parameter „Belag" (z. B. `Linoleum 2,5 (R9)`) soll konsistent in die Zone-Property „Bodenbelag" (Enum) wandern, damit die Schedule-Ausgabe denselben Wert zeigt wie der Plan-Stempel.

## MCP-Bugs entdeckt (Archicad 29, Live verifiziert 2026-05-20)

| Tool | Bug | Auswirkung |
|------|-----|------------|
| `mcp__archicad__elements_get_gdl_parameters_of_elements` | Pydantic-Validierungsfehler: `index` als int zurückgegeben, Schema erwartet string; zusätzliche Felder (`displayName`, `isLocked`, `flags`) werden vom Server geliefert, im Schema nicht erlaubt. Bei einer Zone mit ~849 Parametern wirft das ~3993 Validierungsfehler. | **GDL-Parameter via MCP komplett nicht nutzbar.** Workaround: Schedule-Export (XLSX/CSV) und lokales Parsing. |
| `mcp__archicad__elements_get_details_of_elements` | Für `elementType=Zone`: das Schema kennt keine `ZoneDetails`-Variante (nur `WallDetails`/`BeamDetails`/`ColumnDetails`/`MeshDetails`/`NotYetSupportedElementType`). Server liefert Zone-Felder (name, numberStr, categoryAttributeId, stampPosition, isManual, zCoordinate, polygonOutline, holes), aber sie matchen kein Schema. | **Details von Zonen nicht via MCP lesbar.** Workaround: nicht offensichtlich; Property-Werte separat lesen. |

→ Diese Bugs gehören in `reference/mcp-conventions.md` Capability-Tabelle mit Datums-Marker `<!-- verified 2026-05-20, AC29 -->`.

## Modal-Dialog-Fehlerklasse (häufig, übersehen)

**Fehlermeldung:** `Invalid program status (there is an open modal dialog: <Dialog-Name>)`
**Code:** 4001

Während dieser Session blockierten:
- „Einstellungen für die Raum-Auswahl"
- „Available Parameters" (GDL-Parameter-Browser)
- „Optionen-Setup" (Enum-Wert-Editor)

**Aktion:** User darum bitten, den Dialog zu schließen (Abbrechen oder OK, je nach Save-Wunsch) **bevor** weitere MCP-Calls. Sollte als Fehlerklasse in `reference/mcp-conventions.md` Fehlerklassen-Tabelle:

| Error symptom | Action | Max retries |
|---|---|---|
| `Invalid program status (there is an open modal dialog: …)` | User informieren, Dialog-Namen weiterleiten, auf Schließen warten | 0 |

## Zone-Property-Workflow (was via MCP funktioniert)

Verifizierte Sequenz für Bulk-Klassifizierung von Zonen:

1. `elements_get_elements_by_type` mit `elementType=Zone` → alle Zone-GUIDs (paginiert).
2. `elements_get_selected_elements` → aktuelle Selektion. **Achtung:** Selektion kann beim Schließen modaler Dialoge verschwinden (in dieser Session: „Abbrechen" hat Selektion gelöscht; „Neu selektieren" war nötig).
3. `properties_get_all_property_ids_of_elements` mit `propertyType=UserDefined` → Property-IDs der Zone (in dieser Session: 26 Custom-Properties auf einer Zone, organisiert in Gruppen „Allgemeine Werte", „Produktinformationen", „Räume").
4. `properties_get_details_of_properties` → Name, Type, Group, `possibleEnumValues` bei Enum-Typen.
5. `properties_get_property_values_of_elements` → aktuelle Werte.
6. `properties_set_property_values_of_elements` → Update (asymmetrisch sicher → Confirm-Dialog vorab erforderlich).

## Enum-Property-Erweiterung (Archicad UI, nicht via MCP)

Beobachtung: Enum-Properties erscheinen erst limitiert (8 vordefinierte Werte für „Bodenbelag"), können aber vom User in Archicad selbst erweitert werden:

- Zonen-Einstellungen-Dialog → Bereich „Klassifizierung und Eigenschaften" → Property aufklappen → Enum-Feld → ⊞-Button → „Bearbeiten" → **Optionen-Setup** öffnet sich
- Button „Hinzufügen" → neuer Enum-Wert
- Checkbox „Mehrfachauswahl erlauben" → wandelt singleEnum in multiEnum

**MCP-Capability-Check (offen):**
- ❓ Kann MCP Enum-Werte zu einer existierenden Property-Definition hinzufügen? `properties_create_property_definitions` legt **neue** Property an, nicht Modifikation. Discovery-Ergebnis: kein offensichtlicher `add_enum_value`-Endpoint. **→ Skill-Phase 5 sollte dies klären.**

## Datenqualitäts-Lehre (sehr relevant)

Beim User-getriebenen Erweitern der Enum-Liste sind in dieser Session 21 Werte entstanden, davon 13 neu hinzugefügt — mit Inkonsistenzen:

| Problem | Beispiel |
|---------|----------|
| Trailing-Space | `Linoleum ` (Original-Wert) |
| Spacing inkonsistent | `Linoleum 2,5 (R9)` vs. `Linoleum 2,5(R10)` |
| Tippfehler | `Linolium (R10)` statt `Linoleum (R10)` |
| Trennzeichen inkonsistent | `Fliesen (R11/R12)` vs. Stempel-Wert `Fliesen (R11+R12)` |
| Plural/Singular | `Fliesen (R10)` vs. Stempel-Wert `Fliese R10` |

**Implikation für Bulk-Update:** Vor einem GDL→Property-Sync **muss** der Enum normalisiert sein, sonst landen GDL-Werte als „kein Match" (oder schlimmer: in falschem Enum-Wert). Empfehlung für Skill: vor `properties_set_property_values_of_elements` immer einen **Match-Check** zwischen Quell-Wert und `possibleEnumValues` durchführen + Mismatches dem User vorab melden.

## Workaround-Pfade für GDL → Property-Sync

Da MCP keine GDL-Werte liefert (Bug oben), bleiben zwei Pfade:

### A) Schedule-Export-Pipeline (manueller Export, automatisches Update)

1. User exportiert Schedule (z. B. „00-02 Räume Nordflügel+Neubau") als XLSX oder CSV — mit Spalten Raumnummer, Raumname, GDL-Belag, Property-Bodenbelag.
2. Lokales Parsing (z. B. `openpyxl`).
3. ID-Mapping: Raumnummer → Zone-GUID via `properties_get_property_values_of_elements` (für die Raumnummer-Property aller Zonen).
4. Match-Check: GDL-Wert ↔ vorhandene Enum-Werte. Mismatches melden, User entscheidet (Enum erweitern oder Wert anpassen).
5. Confirm-Summary nach Skill-Konvention (Pro-Klasse-Counts).
6. `properties_set_property_values_of_elements` für alle matchenden Räume.

### B) Property-Expression in Archicad (vollautomatisch, einmal eingerichtet)

Property-Manager → Property „Bodenbelag" → Default-Value als **Expression** → referenziert den GDL-Parameter des Raumstempels direkt. Vorteil: keine Bulk-Updates nötig, Property folgt GDL automatisch.

**Setup-Aufwand:** User muss in Archicad-UI die Expression einrichten. MCP kann nur prüfen, ob die Verknüpfung funktioniert (Werte vergleichen).

**Limitation:** Type-Mismatch — Property ist Enum, Expression liefert String. Kann erfordern, dass die Property auf Type=string umgestellt wird, was die Auswertbarkeit ändert.

## Skill-Anker (wo das hingehört)

Vorschläge für Einbau bei nächster Skill-Ausbau-Session:

1. **`reference/mcp-conventions.md`** — Fehlerklassen-Tabelle erweitern um „modal dialog open"; Capability-Tabelle erweitern um zwei Bug-Einträge mit Datums-Marker.
2. **`recipes/zones.md`** (Phase 3 Plan 03-04) — Worked-Example „Property `Bodenbelag` setzen" inkl. Enum-Match-Check und der Modal-Dialog-Gotcha.
3. **Neues Recipe `recipes/property-bulk-classification.md`** (Phase 5) — universelles Read-Filter-Group-Confirm-Apply für Property-Updates, mit Enum-Spezifika und Datenqualitäts-Pre-Check.
4. **`reference/bulk-operations.md`** — Sektion „Enum-Normalisierung" mit den Failure-Modes aus dieser Session.

## Pending-Capability-Tests für Skill-Phase 3+5

- [ ] Verhält sich `properties_set_property_values_of_elements` mit Enum-Type? Wird `value` als String/displayValue/nonLocalizedValue erwartet?
- [ ] Kann MCP eine Property-Definition modifizieren (Enum-Werte hinzufügen ohne Property neu anzulegen)?
- [ ] Verifizieren: Selektion bleibt erhalten oder verschwindet beim Schließen modaler Dialoge? Welche Dialoge welches Verhalten?
- [ ] `properties_get_property_values_of_elements`: Funktioniert für Built-in „Element-ID"-ähnliche Properties (Raumnummer)? Für ID-Mapping.

---

**Session-Endpunkt:** User klickt die Property-Updates manuell durch (singleEnum-Dropdown pro Raum). Dieser Scratch sammelt, was wir gelernt haben — beim nächsten Skill-Ausbau-Run einarbeiten und gegen die Anker oben prüfen.
