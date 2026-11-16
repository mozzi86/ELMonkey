# Diff-Checker Archicad ↔ Plattform (ohne IFC-Export) <!-- 2026-11-16, noch NICHT live gelaufen -->

Vergleicht das offene Archicad-Modell direkt mit dem Stand der Schwarz Plattform und liefert nur die Unterschiede —
ohne Komplett-IFC auf der Platte. Teil des Plattform-Abgleichs ([`recipes/plattform-abgleich.md`](../recipes/plattform-abgleich.md)).

## Inhalt

1. Bausteine
2. Ablauf
3. Ausgabe (`ergebnis.json`)
4. Wie die Geometrie gewonnen wird — und wie sicher das ist
5. Fallen aus dem ersten Live-Lauf und wo sie abgefangen werden
6. Annahmen zu den Formaten
7. Offen / beim ersten Live-Lauf prüfen

## 1. Bausteine

| Monkey | Was | Wo |
|---|---|---|
| Archicad-Monkey | `ELM_SAB.GetDiffDataELM` — liest je Element die Daten, vergleicht nicht | `archicad-addon/ELM_SAB_Tapir/Sources/GetDiffDataCommandELM.cpp` (ab 0.11.13) |
| Python-Monkey (Kern) | `elmonkey.diff_plattform` — Vergleich als reine Funktionen | `core/elmonkey/diff_plattform.py` (ab Kern 0.4.0) |
| Python-Monkey (MCP) | `elmonkey_diff_plattform` — Befehl lesen + vergleichen + `ergebnis.json` schreiben | `mcp/elmonkey-mcp/…/addons/archicad/diff.py` (ab MCP 0.3.0) |
| Plattform-Monkey | `archicad_abgleich_lesen` / `archicad_abgleich_melden` | Plattform (privat), siehe Abschnitt 7 |

`GetDiffDataELM` liefert je Element: `elementId`, `type`, `ifcType`, `ifcGlobalId` (AC28+: 22 Zeichen; AC27: GUID-Form,
der Kern komprimiert), `ifcAttributes` (Name, Description, ObjectType, Tag, PredefinedType …), `ifcProperties`
(Vorschau-Eigenschaften des **aktiven** IFC-Übersetzers, `pset`/`name`/`value`), `floorIndex`/`storyName`,
`renovationStatus` (Existing/New/ToBeDemolished), `elementIdText`, `layer`/`layerHidden`, `structure` (basic/composite/profile
+ Name), `surfaces`, `classifications` (System, ID, Name), `zone` (Name, Nummer), `hostElementId` (Tür/Fenster/Oberlicht/
Wandöffnung), `geometry` (`source` 3dModel|calcBounds|none, `box` [xmin,xmax,ymin,ymax,zmin,zmax], `bodies`).
Seitenweise: `offset`/`limit` (Standard 500), weiter mit `nextOffset`. `context` meldet ELMonkey-/Archicad-Version,
Datenbank-Umschaltung, Anzahl Elemente mit/ohne 3D-Modell samt Archicad-Code (`no3DModelCodes`), 3D-Schnittebenen.

## 2. Ablauf

1. Konnektor `archicad_abgleich_lesen {projekt}` → `lauf`, `stand_link`, `ergebnis_ablage` (Links nur ans Werkzeug geben).
2. `elmonkey_diff_plattform {stand: "<stand_link>"}` (optional `basis`, `plattform_geometrie`, `gebaut`, `versatz`,
   `ordner`). Liest Archicad, vergleicht, schreibt `ergebnis.json` + `archicad.json` in den Arbeitsordner
   (Standard `~/ELMonkey/diff-plattform/<lauf>/`), antwortet mit Zahlen und Beispielen.
3. Zahlen dem Zeichner zeigen. Offene Fragen (`offen`, `diff.eigenschaften` mit `richtung: unklar`) klären — ggf. über
   `scripts/plattform_abgleich` (`entscheiden` mit `antworten.json`), siehe Rezept.
4. `plattform_abgleich.py hochladen --ordner <Ordner> --ablage "<ergebnis_ablage>"`, dann `archicad_abgleich_melden
   {projekt, lauf, gebaut}` → Vorschau → nach dem Ja `bestaetigt=true`. `gebaut` = `diff.gebaut_vorschlag` (Nachbauten
   mit Stempel „Plattform-GUID“).
5. Geometrie (`diff.geometrie`): die Plattform-Meldung übernimmt sie heute nicht — als Vorschläge mit
   `modell_bauteile_aendern` (`geometrie laenge|dicke|hoehe`, nicht-rechteckig `ersetzen` mit `guid_behalten`) schreiben.
   `laenge` streckt entlang der Längsachse, Querseiten über `dicke` mit `fest` = Gegenseite; Höhen in Projekthöhe =
   Archicad-z + (Höhe ü. NN des Projektnullpunkts − Nullniveau ü. NN) — Werte je Projekt, nie raten.

## 3. Ausgabe (`ergebnis.json`)

Format `sab-archicad-abgleich/1` wie `elmonkey.abgleich.entscheiden` (das liest `archicad_abgleich_melden`):
`archicad` {`neu_bauen`, `eigenschaften`, `loeschen_von_hand`}, `plattform` {`neu_importieren`, `eigenschaften`,
`ausblenden`, `einblenden`}, `basis` {`paare`, `werte`, **neu** `geometrie`}, `offen`, `zahlen`, `lauf`.
Zusätzlich `diff` (die Plattform ignoriert den Schlüssel):

| Schlüssel | Inhalt |
|---|---|
| `neu` | nur in Archicad (inkl. Wandöffnungen ohne Partner, `oeffnung: true`) |
| `weg` | stammte aus Archicad, fehlt jetzt dort (mit Umbau-Status der Plattform) |
| `nur_plattform` | Plattform-eigene Bauteile, noch nicht nachgebaut |
| `eigenschaften` | Feld, Werte, `richtung` archicad→plattform / plattform→archicad / unklar (+ Frage-ID) |
| `geometrie` | je Paar `seiten` {x-, x+, y-, y+, z-, z+: Delta in m, Archicad minus Plattform}, nur über `toleranz_m` (0,003), `vergleich_mit` plattform/basis, beide Boxen |
| `geometrie_ungeprueft` | Paare ohne belastbare Box (Archicad nur roh / keine; Plattform ohne Box) mit Grund |
| `zonen` | Anzahl je Seite, Zonen nur in Archicad — nie „neu“ |
| `oeffnungen` | Paare, `an_tuer_fenster` (IfcOpeningElement einer Tür/einem Fenster zugeordnet), `ohne_partner` (nie „weg“) |
| `bauteilteile` | IfcBuildingElementPart der Plattform: zugeordnet / ohne Bauteil |
| `gebaut_vorschlag` | `[{plattformGuid, archicadGuid}]` aus dem Stempel „Plattform-GUID“ |
| `kontext`, `hinweise`, `zahlen` | Archicad-Kontext, Warnungen, Zählung |

## 4. Wie die Geometrie gewonnen wird — und wie sicher das ist

**Weg:** `ACAPI_ModelAccess_Get3DInfo` je Element, dann die Körper (`API_BodyID`) und deren Box `body.xmin…zmax`
(Weltkoordinaten; derselbe Weg wie Tapirs `Get3DGeometryOfElements`). Alle Körper eines Elements → **eine** Box.
Treppe/Geländer/Fassade: Box über die Körper der Teilelemente.

**Warum dieser Weg:** Laut DevKit rechnet Archicad das Element für `Get3DInfo` unabhängig vom 3D-Fenster in 3D um —
mit den Verschneidungen (Anschlüsse von Wänden sind als Polygone im Körper sichtbar), **ohne** 3D-Schnittebenen und
Perspektivschnitte. Das ist dieselbe 3D-Umrechnung, aus der auch der IFC-Export seine Körper bildet. Die rohen
Parameter (Referenzlinie + Dicke + Höhe) bzw. `ACAPI_Element_CalcBounds` kennen die Anschlüsse nicht — daher die
20–35 cm im ersten Lauf. Eine IFC-API für Geometrie ohne Export gibt es im DevKit nicht (IFCAPI liefert GlobalId,
Klasse, Attribute, Eigenschaften — keine Körper).

**Unsicherheit (ehrlich):**

- **Nicht live belegt**, dass die Box aus `Get3DInfo` der Box des exportierten IFC-Körpers entspricht. Der IFC-Export
  kann eigene Einstellungen anwenden (Übersetzer: Teile zerlegen, Schichten, Geometrie vereinfachen, Umbau-Filter des
  Übersetzers). Erster Live-Lauf: für einige Wände/Decken die Box aus `GetDiffDataELM` gegen die Box aus einem
  Einzel-IFC derselben Elemente legen (Referenz-Skript `ac_geom.py`-Logik).
- `Get3DInfo` scheitert bei Elementen auf **ausgeblendeten Ebenen** (`APIERR_HIDDENLAY`). Dann liefert der Befehl die
  rohe Box (`calcBounds`) bzw. bei Decke/Dach/Zone keine (`CalcBounds` stürzt dort ab, #686); der Kern wertet sie
  **nicht** als Änderung, sondern listet sie unter `geometrie_ungeprueft`. Ebenen umzuschalten wäre eine Änderung am
  Projekt (Teamwork-Attribut) — macht der Befehl bewusst nicht. Wenn viele betroffen sind: der Zeichner blendet die
  Ebenen selbst ein und startet neu.
- Ob der **Umbau-Filter** oder die **Teildarstellung** (z. B. „nur Kern“) die 3D-Umrechnung von `Get3DInfo`
  beeinflusst, ist nicht belegt. `context.no3DModelCodes` zeigt, welche Codes vorkamen.
- Oberflächen kommen aus der Materialtabelle der Körperpolygone (wie `Get3DGeometryOfElements`); GDL-eigene Materialien
  ohne Attribut fehlen.

## 5. Fallen aus dem ersten Live-Lauf und wo sie abgefangen werden

| Falle | Abgefangen | Test (`core/tests/test_diff_plattform.py`) |
|---|---|---|
| Rohe 3D-Box statt getrimmter Geometrie | C++: Box aus dem 3D-Modell (`source` 3dModel); Kern: nur 3dModel gilt, rohe Box → ungeprüft | `test_falle1_*` |
| Mehrschicht: Geometrie nur in `IfcBuildingElementPart` | C++: alle Körper = eine Box; Kern: Plattform-Teile ins Bauteil (`teil_von`), nie „weg“; IFC-Rückfall über IfcRelAggregates | `test_falle2_*` |
| Umbau-Filter blendet Abbruch aus | C++: `GetElemList` ohne Filter; Kern: Hinweis, wenn Abbruch-Bauteile fehlen | `test_falle3_*` |
| `IfcOpeningElement`-GUIDs je Export neu | Kern: Paar über GUID/GlobalId, sonst Lage (±5 cm) bzw. Tür/Fenster; nie „weg“ | `test_falle4_*` |
| Klassenliste unvollständig | C++: alle Elemente mit IFC-Klasse; Kern: jede Klasse außer Nicht-Bauteilen | `test_falle5_*` |
| `IfcSpace` (Zonen) | Kern: eigene Liste `diff.zonen`, nie „neu“/Import | `test_falle6_*` |
| Schreibweisen der Werte | Kern: `wert_norm`/`gleich` + Angleichen an die Plattform-Schreibweise | `test_falle7_*` |
| Mengen ändern sich ohne Geometrie | Kern: Mengen-Psets (Qto_*, BaseQuantities, …) nie verglichen | `test_falle8_*` |
| Plattform-Bauteile in Archicad nachgebaut | Kern: Stempel „Plattform-GUID“ / `gebaut` → Paar + `gebaut_vorschlag`; MCP liest den Stempel aus den Archicad-Eigenschaften | `test_falle9_*`, MCP `test_diff.py` |

## 6. Annahmen zu den Formaten

- `stand.json`: Format, Felder und `basis` aus `archicad_abgleich_lesen` (Plattform `core/plattform_monkey/abgleich.py`,
  Stand 10.11.2026): `elemente[] {guid, name, klasse, geschoss, herkunft, ausgeblendet, eigenschaften}`,
  `geschosse[] {name, okff, archicadName}`, `basis {paare, werte}`, `lauf`. **Keine Geometrie** im Stand — der Kern nimmt
  eine Box, wenn ein Element `bbox` (6 Zahlen oder `{xMin…}`) trägt, sonst `plattform_geometrie` (JSON oder IFC der
  Plattform), sonst `basis.geometrie` des letzten Laufs.
- Eigenschaftsnamen: „Pset.Name“ wie im IFC, Kopf­felder `IFC.Name`, `IFC.Beschreibung`, `IFC.Klasse`,
  `IFC.Vordefinierter Typ`, `IFC.GlobalId`, `Verortung.Geschoss`, `SAB_Planung.Umbau-Status` (Werte Bestand/Neubau/Abbruch
  angenommen, Synonyme werden gleich gewertet). Archicad-Seite: IFC-Vorschau-Eigenschaften des aktiven Übersetzers —
  gleich nur, wenn derselbe Übersetzer exportiert hat, aus dem die Plattform stammt.
- Plattform-Teile: Verweis auf das Bauteil in `teil_von` (oder `parent`, `IFC.Teil von`) — heute liefert die Plattform
  das nicht (Abschnitt 7).
- Koordinaten: beide Seiten in Archicad-Koordinaten (Plattform-Export „modell_exportieren“ in Archicad-Koordinaten);
  sonst `versatz`.

## 7. Offen / beim ersten Live-Lauf prüfen

- Live: Box `Get3DInfo` ↔ IFC-Box; Dauer bei großen Projekten (Eigenschaften + Oberflächen je Element);
  `no3DModelCodes`; Umbau-Filter/Teildarstellung.
- **Plattform-Monkey nachziehen:** `bauteil()` soll eine Box (`bbox`) und bei Teilen `teil_von` mitgeben;
  `basis_neu` soll `basis.geometrie` behalten; `archicad_abgleich_melden` soll `diff.geometrie` in Geometrie-Ops
  übersetzen (heute nur Eigenschaften/Aus-/Einblenden); `diff.zonen` nicht als Fehler werten.
