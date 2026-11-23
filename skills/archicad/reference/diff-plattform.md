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
7. IFC-Kennung: externe vor eigener
8. Offen / beim ersten Live-Lauf prüfen

## 1. Bausteine

| Monkey | Was | Wo |
|---|---|---|
| Archicad-Monkey | `ELM_SAB.GetDiffDataELM` — liest je Element die Daten, vergleicht nicht | `archicad-addon/ELM_SAB_Tapir/Sources/GetDiffDataCommandELM.cpp` (ab 0.11.13) |
| Python-Monkey (Kern) | `elmonkey.diff_plattform` — Vergleich als reine Funktionen | `core/elmonkey/diff_plattform.py` (ab Kern 0.4.0) |
| Python-Monkey (MCP) | `elmonkey_diff_plattform` — Befehl lesen + vergleichen + `ergebnis.json` schreiben | `mcp/elmonkey-mcp/…/addons/archicad/diff.py` (ab MCP 0.3.0) |
| Python-Monkey (Skript) | `plattform_abgleich.py diff` — dieselbe Logik als dünne Hülle, für Nutzer ohne MCP | `skills/archicad/scripts/plattform_abgleich/` |
| Plattform-Monkey | `archicad_abgleich_lesen` (Stand mit `bbox`) / `archicad_abgleich_melden` (übernimmt `diff.geometrie`) | Plattform (privat), seit 16.11.2026 (Plattform-PR #338) |

`GetDiffDataELM` liefert je Element: `elementId`, `type`, `ifcType`, `ifcGlobalId` (Archicads eigene) und
`externalIfcGlobalId` (GlobalId eines per IFC gekommenen Bauteils), beide immer, leer wenn nicht vorhanden (AC28+:
22 Zeichen; AC27: GUID-Form, der Kern komprimiert; Abschnitt 7), `ifcAttributes` (Name, Description, ObjectType, Tag, PredefinedType …), `ifcProperties`
(Vorschau-Eigenschaften des **aktiven** IFC-Übersetzers, `pset`/`name`/`value`), `floorIndex`/`storyName`,
`renovationStatus` (Existing/New/ToBeDemolished), `elementIdText`, `layer`/`layerHidden`, `structure` (basic/composite/profile
+ Name), `surfaces`, `classifications` (System, ID, Name), `zone` (Name, Nummer), `hostElementId` (Tür/Fenster/Oberlicht/
Wandöffnung), `geometry` (`source` 3dModel|calcBounds|none, `box` [xmin,xmax,ymin,ymax,zmin,zmax], `bodies`).
Seitenweise: `offset`/`limit` (Standard 500), weiter mit `nextOffset`. `context` meldet ELMonkey-/Archicad-Version,
Datenbank-Umschaltung, Anzahl Elemente mit/ohne 3D-Modell samt Archicad-Code (`no3DModelCodes`), 3D-Schnittebenen,
`geometryHint`.

## 2. Ablauf

1. Konnektor `archicad_abgleich_lesen {projekt}` → `lauf`, `stand_link`, `ergebnis_ablage` (Links nur ans Werkzeug geben).
2. `elmonkey_diff_plattform {stand: "<stand_link>"}` (optional `basis`, `plattform_geometrie`, `gebaut`, `versatz`,
   `ordner`). Liest Archicad, vergleicht, schreibt `ergebnis.json` + `archicad.json` in den Arbeitsordner
   (Standard `~/ELMonkey/diff-plattform/<lauf>/`), antwortet mit Zahlen und Beispielen.
   Ohne MCP: `plattform_abgleich.py diff --ordner <Ordner> --stand-link "<stand_link>"` (gleiche Logik).
3. Zahlen dem Zeichner zeigen. Offene Fragen (`offen`, `diff.eigenschaften` mit `richtung: unklar`) klären — ggf. über
   `scripts/plattform_abgleich` (`entscheiden` mit `antworten.json`), siehe Rezept.
4. `plattform_abgleich.py hochladen --ordner <Ordner> --ablage "<ergebnis_ablage>"`, dann `archicad_abgleich_melden
   {projekt, lauf, gebaut}` → Vorschau → nach dem Ja `bestaetigt=true`. `gebaut` = `diff.gebaut_vorschlag` (Nachbauten
   mit Stempel „Plattform-GUID“).
5. Geometrie (`diff.geometrie`): `archicad_abgleich_melden` übersetzt sie seit 16.11.2026 selbst in Vorschläge
   (`verschieben`, `laenge`, `dicke`, `hoehe`, im selben Vorschlag, vorher Trockenlauf). Was nicht sicher geht (kein
   Rechteck, schräg zu den Archicad-Achsen, fast quadratisch, Gruppe, Öffnung, Zone, Plattform-Box seit dem Lesen
   geändert, nur gegen die Basis verglichen, auf beiden Seiten geändert), meldet sie mit Grund unter
   `geometrie.nicht_automatisch`; nur in der Plattform geändert → `plattform_geaendert` (Archicad von Hand nachziehen).
   Diese Listen dem Zeichner zeigen — von Hand: nicht-rechteckig `ersetzen` mit `guid_behalten` (Schichtaufbau geht
   verloren). Höhen in Projekthöhe = Archicad-z + (`hoeheNN` − `nullniveauNN`) aus dem Kopf `geometrie` des Stands —
   nie raten.

## 3. Ausgabe (`ergebnis.json`)

Format `sab-archicad-abgleich/1` wie `elmonkey.abgleich.entscheiden` (das liest `archicad_abgleich_melden`):
`archicad` {`neu_bauen`, `eigenschaften`, `loeschen_von_hand`}, `plattform` {`neu_importieren`, `eigenschaften`,
`ausblenden`, `einblenden`}, `basis` {`paare`, `werte`, **neu** `geometrie` = Archicad-Box je Paar}, `offen`, `zahlen`,
`lauf`. Die Plattform merkt in `basis.json` für übertragene Bauteile die Plattform-Box vor der Übertragung
(Basis-Regel wie bei den Eigenschaften) und rechnet damit beim nächsten Lauf die Richtung (Drei-Wege der Geometrie).
Zusätzlich `diff` (die Plattform ignoriert den Schlüssel):

| Schlüssel | Inhalt |
|---|---|
| `neu` | nur in Archicad (inkl. Wandöffnungen ohne Partner, `oeffnung: true`) |
| `weg` | stammte aus Archicad, fehlt jetzt dort (mit Umbau-Status der Plattform) |
| `nur_plattform` | Plattform-eigene Bauteile, noch nicht nachgebaut |
| `eigenschaften` | Feld, Werte, `richtung` archicad→plattform / plattform→archicad / unklar (+ Frage-ID) |
| `geometrie` | je Paar `seiten` {x-, x+, y-, y+, z-, z+: Delta in m, Archicad minus Plattform}, nur über `toleranz_m` (0,003), `vergleich_mit` plattform/basis, `richtung` archicad→plattform / plattform→archicad / beide / unklar (gegen die Basis-Box; nur archicad→plattform geht als Netz mit, <!-- 2026-11-20 -->), `archicad_box`, `plattform_box`, `bauteil` — genau die Felder, die `archicad_abgleich_melden` liest |
| `geometrie_ungeprueft` | Paare ohne belastbare Box (Archicad nur roh / keine; Plattform ohne Box) mit Grund |
| `zonen` | Anzahl je Seite, Zonen nur in Archicad — nie „neu“ |
| `oeffnungen` | Paare, `an_tuer_fenster` (IfcOpeningElement einer Tür/einem Fenster zugeordnet), `ohne_partner` (nie „weg“) |
| `bauteilteile` | IfcBuildingElementPart der Plattform: zugeordnet / ohne Bauteil |
| `gebaut_vorschlag` | `[{plattformGuid, archicadGuid}]` aus dem Stempel „Plattform-GUID“ |
| `ausgeblendet_in_plattform` | Paare, deren Plattform-Bauteil ausgeblendet ist (Frage, nichts geschrieben) |
| `plattform_geometrie_kopf` | Kopf `geometrie` des Stands (Rahmen, Einheit, Toleranz, `hoeheNN`, `nullniveauNN`) |
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
- Offen auch, ob `Get3DInfo` Körper liefert, wenn das 3D-Fenster in der Sitzung noch nie offen war (Hinweis aus dem
  parallelen Zweig `elm-0.11.13-diff-checker`). Kommt für kein Element ein Körper (`context.geometry3DModel` = 0), meldet
  der Kern: 3D-Fenster einmal öffnen (alle Ebenen, Umbau-Filter „alles zeigen“) und erneut lesen.
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
| Mehrschicht: Geometrie nur in `IfcBuildingElementPart` | C++: alle Körper = eine Box. Plattform: rechnet die Teile schon beim IFC-Import in ihr Bauteil ein, `bbox` im Stand ist die Hülle aller Schichten (seit 16.11.2026). Kern: nimmt diese `bbox` direkt; nur für Plattform-Gruppen (Kopf mit `bbox_fehlt`, Teile mit `teil_von`) und ältere Stände Vereinigung der Teile, IFC-Text (IfcRelAggregates) nur als Rückfall; Teile nie „weg“ | `test_falle2_*`, `test_gruppe_der_plattform_box_aus_teilen_mit_teil_von` |
| Umbau-Filter blendet Abbruch aus | C++: `GetElemList` ohne Filter; Kern: Hinweis, wenn Abbruch-Bauteile fehlen | `test_falle3_*` |
| `IfcOpeningElement`-GUIDs je Export neu | Kern: Paar über GUID/GlobalId, sonst Lage (±5 cm) bzw. Tür/Fenster; nie „weg“ | `test_falle4_*` |
| Klassenliste unvollständig | C++: alle Elemente mit IFC-Klasse; Kern: jede Klasse außer Nicht-Bauteilen | `test_falle5_*` |
| `IfcSpace` (Zonen) | Kern: eigene Liste `diff.zonen`, nie „neu“/Import | `test_falle6_*` |
| Schreibweisen der Werte | Kern: `wert_norm`/`gleich` (auch `$`/`*` = leer, `.TRUE.`, `['A','B']` = Liste) + Angleichen an die Plattform-Schreibweise | `test_falle7_*`, `test_schreibweisen_aus_dem_zweiten_zweig` |
| Mengen ändern sich ohne Geometrie | Kern: Mengen-Psets (Qto_*, BaseQuantities, …) nie verglichen | `test_falle8_*` |
| Plattform-Bauteile in Archicad nachgebaut | Kern: Stempel „Plattform-GUID“ / `gebaut` → Paar + `gebaut_vorschlag`; MCP liest den Stempel aus den Archicad-Eigenschaften | `test_falle9_*`, MCP `test_diff.py` |

## 6. Annahmen zu den Formaten

- `stand.json` (Plattform `core/plattform_monkey/abgleich.py` + `diff_plattform.py`, Stand PR #338 vom 16.11.2026,
  gelesen, nicht ausgeführt): `elemente[] {guid, name, klasse, geschoss, herkunft, ausgeblendet, eigenschaften}`, dazu
  **`bbox`** `[xmin, xmax, ymin, ymax, zmin, zmax]` in Archicad-Metern auf mm (aus dem Grundmodell-Netz, Snapshot =
  Archicad (x, z, −y)), ersatzweise **`bbox_fehlt`** mit Grund (nie geschätzt), bei Teilen von Plattform-Gruppen
  **`teil_von`**; Kopf **`geometrie`** (Rahmen archicad, Einheit m, `gerundet_m` 0,001, `toleranz_m` 0,003,
  `hoeheNN`, `nullniveauNN`, Projekthöhe-Formel); `geschosse[] {name, okff, archicadName}`, `basis {paare, werte,
  geometrie}`, `lauf`. Der Kern nimmt `bbox` direkt; `plattform_geometrie` (JSON/IFC) und `basis.geometrie` sind nur
  Rückfall für ältere Stände; `bbox_fehlt` erscheint als Grund in `geometrie_ungeprueft`.
- Eigenschaftsnamen: „Pset.Name“ wie im IFC, Kopffelder `IFC.Name`, `IFC.Beschreibung`, `IFC.Klasse`,
  `IFC.Vordefinierter Typ`, `IFC.GlobalId`, `Verortung.Geschoss`, `SAB_Planung.Umbau-Status` (Werte Bestand/Neubau/Abbruch
  angenommen, Synonyme werden gleich gewertet). Archicad-Seite: IFC-Vorschau-Eigenschaften des aktiven Übersetzers —
  gleich nur, wenn derselbe Übersetzer exportiert hat, aus dem die Plattform stammt.
- Koordinaten: beide Seiten in Archicad-Koordinaten. Laut Plattform-Doku gilt das nur, wenn der IFC-Übersetzer am
  Projektnullpunkt exportiert hat (nicht am Vermessungspunkt) — sonst `versatz`. Live nicht geprüft.

## 7. IFC-Kennung: externe vor eigener

Ein Bauteil, das per IFC ins Archicad-Projekt kam, kennt die Plattform unter seiner **externen** GlobalId; Archicads
eigene Kennung ist dann eine andere. Darum liefert `GetDiffDataELM` beide und der Kern paart in der Reihenfolge
externe GlobalId → eigene GlobalId → komprimierte Element-GUID (gegen Plattform-GUID und `IFC.GlobalId`).
Übernommen aus dem Zweig `elm-0.11.13-diff-checker`; dort ist die Antwortform von `TapirCommand.GetIFCIdsOfElements`
mit 0.11.12 / AC29 live geprüft (`elementIFCIds`, je Eintrag `ifcId` + `externalIFCId`) — dieselben API-Aufrufe
benutzt jetzt `GetDiffDataELM`. Das Skript `plattform_abgleich.py` las vorher `ifcIds` (falscher Schlüssel) und ist
korrigiert.

| Archicad | eigene Kennung | externe Kennung |
|---|---|---|
| 27 | `ACAPI_Element_GetIFCIdentifier` → `ifcId` (API_Guid, unkomprimiert) | dieselbe Funktion → `externalIfcId`; `APINULLGuid` = keine |
| 28–30 | `IFCAPI::ObjectAccessor::GetGlobalId` („der Wert, der beim Export benutzt wird“) | `GetExternalGlobalId` (DevKit: nur für IFC-Importe, sonst `APIERR_CANCEL`) |

Unsicher: Ob `GetGlobalId` bei einem IFC-Import schon die externe Kennung liefert (dann wären beide gleich), sagt das
DevKit nicht eindeutig — darum werden ab AC28 beide Funktionen gerufen, `ACAPI_Element_GetIFCIdentifier` braucht es
dort nicht (Tapir nutzt ab AC28 ebenfalls nur den Accessor). Nicht live geprüft mit `GetDiffDataELM`; im ersten Lauf an
einem importierten Bauteil beide Felder ansehen.

## 8. Offen / beim ersten Live-Lauf prüfen

- Live: Box `Get3DInfo` ↔ IFC-Box; Dauer bei großen Projekten (Eigenschaften + Oberflächen je Element);
  `no3DModelCodes`; Umbau-Filter/Teildarstellung; 3D-Fenster nie geöffnet; beide IFC-Kennungen an einem importierten
  Bauteil; Ursprung des Plattform-Netzes = Archicad-Projektnullpunkt.
- Plattform-Monkey: Box, `bbox_fehlt`, `teil_von`, `basis.geometrie` und Geometrie-Vorschläge sind seit PR #338 da.
  Nachzuziehen bleibt die Normalisierung, die der Kern seit dem Zusammenführen kann und die Plattform-Kopie noch nicht:
  `$`/`*` = leer, `.TRUE.`/`.FALSE.`, `['A','B']` = Liste (statt einer Zeichenkette).
