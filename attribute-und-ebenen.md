# Attribute, Ebenen und Ebenenkombinationen

Attribute — Ebenen, Baustoffe, Verbünde, Profile, Linientypen, Schraffuren, Oberflächen —
sind der Datenunterbau eines Archicad-Projekts. Sie sind per API weitgehend lesbar,
teilweise schreibbar, und beim Löschen deutlich heikler als Elemente: ein gelöschter
Baustoff wird in jedem Verbund und jedem Profil, das ihn benutzt, still ersetzt.

Stand 2026-07-30, live an einer Büro-Vorlage verifiziert (AC29, 87 Ebenen, 122 Baustoffe,
41 Verbünde, 53 Profile).

## Inhaltsverzeichnis

1. [Das Zwei-Schritt-Lesemuster](#das-zwei-schritt-lesemuster)
2. [Tool-Inventar](#tool-inventar)
3. [Ebenen](#ebenen)
4. [Ebenenkombinationen](#ebenenkombinationen)
5. [Attribute löschen — was sicher ist](#attribute-löschen--was-sicher-ist)
6. [Baustoffe, Verbünde, Profile](#baustoffe-verbünde-profile)
7. [Dubletten finden](#dubletten-finden)
8. [Umbenennen ist gefährlicher als Löschen](#umbenennen-ist-gefährlicher-als-löschen)

## Das Zwei-Schritt-Lesemuster

`GetAttributesByType` liefert **nur GUIDs**, keine Details. Immer zwei Schritte:

```
1. API.GetAttributesByType   {attributeType: "Layer"}        → Liste von GUIDs
2. attributes_get_layer_attributes {attributeIds: [...]}     → Namen, Flags, Index
```

Das gilt analog für jeden Typ: `…_get_building_material_attributes`,
`…_get_composite_attributes`, `…_get_profile_attributes`,
`…_get_layer_combination_attributes`.

Bei größeren Listen ist der **HTTP-Bypass** zuverlässiger als MCP (siehe
[`mcp-conventions.md`](mcp-conventions.md)) — MCP lief bei Reads über ~100 Attribute
mehrfach in Timeouts.

## Tool-Inventar

<!-- 2026-07-30 live per Discovery verifiziert, AC29 -->

| Tool | Richtung | Anmerkung |
|---|---|---|
| `attributes_get_layer_attributes` | R | Details je GUID |
| `attributes_get_layer_combinations` | R | erwartet `attributes` = GUID-Liste |
| `attributes_get_layer_combination_attributes` | R | Details inkl. Layer-Zustände |
| `attributes_create_layer_combinations` | **W** | `overwriteExisting`; pro Layer `isHidden`, `isLocked`, `isWireframe`, `intersectionGroupNr` |
| `attributes_create_building_materials` | **W** | `name`, `connPriority`, `cutFillIndex`, `cutFillPen`, `cutFillBackgroundPen`, `cutSurfaceIndex`, `thermalConductivity`, `density`, `heatCapacity`, `embodiedEnergy`, `embodiedCarbon`; `overwriteExisting` |
| `attributes_delete_attributes` | **W** | Liste von `attributeId` — **keine Verwendungsprüfung**, siehe unten |
| `attributes_delete_attribute_folders` | **W** | löscht Ordner **samt Inhalt** |

Alle `W`-Operationen fallen unter SAFE-01: Confirm-Schleife mit Namensliste vor dem Call.
`delete_attribute_folders` bekommt eine besonders deutliche Ansage — es nimmt alles mit,
was im Ordner steckt.

## Ebenen

**Namensschema ist projektspezifisch** und gehört nach Memory, nicht in den Skill. Was
generisch gilt: Ebenennamen können von *nachgelagerten Systemen* gelesen werden — AVA-
und Kalkulationsfilter (NOVA-AVA & Co.) matchen häufig auf `ifc_layer` mit Operator
*equals*. Ein Umbenennen bricht solche Filter sofort und geräuschlos. **Vor jedem
Ebenen-Rename klären, ob eine AVA-Filterbibliothek darauf zeigt.**

Zwei Dinge, die in der Praxis überraschen:

- **Die Systemebene „Archicad-Ebene" tauchte in Tapir-Listen nie auf.** Wer sie als
  „fehlt / gelöscht" meldet, erzeugt falschen Alarm — sie ist da, sie wird nur nicht
  gelistet.
- **Leere Ebenen sind per API gefahrlos löschbar** (verifiziert an 6 Dubletten). Bei
  belegten Ebenen dagegen entscheidet die API die Ersatzfrage still — dann UI.

## Ebenenkombinationen

Vor jedem Ebenen-Löschen prüfen, ob die Ebene in Kombinationen sichtbar geschaltet ist.
`attributes_get_layer_combination_attributes` liefert pro Kombination die Layer-Liste mit
`isHidden` / `isLocked` / `isWireframe` / `intersectionGroupNr`.

`intersectionGroupNr` ist mehr als Kosmetik: **Elemente auf Ebenen mit gleicher
Schnittgruppe verschneiden sich.** Wer eine Ebene neu anlegt und die Gruppennummer
vergisst, bekommt Wände, die sich nicht mit dem Rest verbinden — ein Fehler, der erst im
Schnitt oder im IFC auffällt. Beim Anlegen einer neuen Ebene die Gruppennummer von einer
fachlich benachbarten Ebene übernehmen.

`attributes_create_layer_combinations` mit `overwriteExisting: true` ersetzt eine
Kombination **vollständig** — die Layer-Liste ist kein Delta. Wer eine einzelne Ebene
umschalten will, muss vorher die komplette Liste lesen, ändern und zurückschreiben.

## Attribute löschen — was sicher ist

`attributes_delete_attributes` prüft **nicht**, wer das Attribut benutzt. Die Lage je Typ:

| Typ | Verwendung auslesbar? | Empfehlung |
|---|---|---|
| Ebene (leer) | ja, über Elementfilter | ✅ API |
| Ebene (belegt) | ja | Elemente erst umhängen, dann API |
| Baustoff in **Verbünden** | ✅ `GetCompositeAttributes` → `compositeSkins[].buildingMaterialId` | prüfbar |
| Baustoff in **Profilen** | ❌ **nicht auslesbar** — `GetProfileAttributes` liefert nur Name, `useWith`, Maße, `profileModifiers` | ⚠️ **UI: Attributmanager** |
| Baustoff direkt in Elementen | nur über Elementabfrage je Typ | in Vorlagen meist irrelevant, in Projekten prüfen |

**Deshalb Baustoffe im Attributmanager löschen, nicht per Skript.** Der Attributmanager
fragt nach, womit ersetzt werden soll; die API trifft diese Entscheidung still — und weil
Profile nicht auslesbar sind, ist ein „unbenutzt"-Befund per API grundsätzlich
unvollständig.

## Baustoffe, Verbünde, Profile

**Ein `_NNN`-Suffix im Baustoffnamen ist üblicherweise die `connectionPriority`** —
`Stahlbeton_670` hat 670, `Luftschicht vertikal_205` hat 205. Der Name trägt die
Verschneidungspriorität sichtbar mit. Vor dem Umbenennen die Priorität lesen und den
Vorschlag daraus ableiten, nicht raten.

**„Unbenutzt" ist kein Mangel.** In einer Bürovorlage stecken viele Baustoffe in keinem
Verbund (real: 89 von 122) — Marmor, Reet, Sandstein, Profilitglas sind Vorratsmaterial
für den Projektbedarf. Ein Aufräumen nach „unbenutzt" zerstört den Katalog. Weg gehören
nur echte **Dubletten**.

**Neue Baustoffe** legt `attributes_create_building_materials` sauber an — inkl.
`connPriority` und Schraffur/Stift. Das ist der bequeme Weg für ganze Gruppen (z. B. eine
Schadstoff-Familie). Danach rücklesen und die Prioritäten gegen die Nachbarn prüfen.

## Dubletten finden

Rezept, das sich bewährt hat:

1. Alle Attribute des Typs lesen (2-Schritt-Muster).
2. **Normalisieren** für den Vergleich: Kleinschreibung, `_NNN` abtrennen, `ß→ss`,
   `ae/oe/ue ↔ ä/ö/ü`, Komma und doppelte Leerzeichen weg. So findet man
   `Beton unbewehrt_330` vs. `Beton, unbewehrt_330` und `Wand Aussen` vs. `10 Wand außen`.
3. Pro Kandidatenpaar die **tatsächliche Verwendung** ermitteln (Verbünde zählen,
   Elemente zählen) — und daraus ableiten, welche Variante bleibt: **die benutzte**, nicht
   die schöner benannte.
4. Ergebnis als Tabelle „löschen / behalten / benutzt in / Grund" vorlegen und bestätigen
   lassen.

Namensähnlichkeit allein ist **kein** Dublettenbeweis. `Stahlbeton_800` und
`Stahlbeton_WU_800` sehen verwandt aus und sind verschiedene Baustoffe (WU = wasserundurchlässig).

## Umbenennen ist gefährlicher als Löschen

Ein gelöschtes Attribut fällt auf: Archicad fragt nach Ersatz, Elemente ändern ihr
Aussehen. Ein **umbenanntes** Attribut funktioniert im Modell unverändert weiter und
bricht nur die Systeme *außerhalb* von Archicad — AVA-Filter, Schedule-Vorlagen,
Auswertungs-Skripte, IFC-Property-Mappings.

Praxisregel: **Vor jedem Rename die Filter-/Auswertungsbibliothek durchsuchen.** Zeigt
irgendwas mit *equals* auf den alten Namen, ist Schema-Konformität das schwächere
Argument — dann bleibt der Name, wie er ist, und die Ausnahme wird dokumentiert.
