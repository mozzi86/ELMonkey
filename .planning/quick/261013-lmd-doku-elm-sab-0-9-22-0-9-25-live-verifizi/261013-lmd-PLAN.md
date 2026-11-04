---
phase: quick-261013-lmd
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - reference/mcp-extension.md
autonomous: true
requirements:
  - QUICK-261013-lmd

must_haves:
  truths:
    - "Die Abschnitte ELM_SAB 0.9.22, 0.9.23, 0.9.24 und 0.9.25 melden „Live verifiziert 2026-10-13 (WiSchu)“, und kein Abschnitt ab 0.9.22 enthält noch „noch NICHT live verifiziert“."
    - "Im 0.9.23-Abschnitt ist die Geschoss-Angabe korrigiert: vorn war das 1. OG (actStory 1). Die neue Falle nennt TapirCommand.GetStories.actStory statt GetUIState.currentWindow.index."
    - "Die Teil-C-Tabelle (22 Zeilen, je GUID-Menge gegen Referenz, 0 Abweichungen) steht im Unterabschnitt Live-Gegenprobe 0.9.23. In der Stand-Liste „nicht live“ bleibt nur die designOption-Namensauflösung."
    - "Die 0.9.24-Tabelle weist 3 Proben als „vom Eingabeschema abgefangen (4002)“ aus und 5/8 als exakten Tabellentext. Die Erfolgsform ist mit einer nicht existierenden GUID belegt."
    - "Der 0.9.25-Abschnitt enthält den Live-Befund (PNG, colorDepth MiC, „Layout geht ab 0.9.25“), widerlegt die These „verdeckt“, hakt den Testplan ab und nennt die drei Fallen aus dem Test."
    - "Außerhalb der Abschnitte 0.9.22–0.9.25 ist nur Zeile 703 (0.9.12/0.9.13) geändert, und zwar durch Anhängen des Vermerks „Live verifiziert erst mit 0.9.25 (WiSchu 13.10.)“."
  artifacts:
    - path: "reference/mcp-extension.md"
      provides: "ELM_SAB-Referenz mit Live-Befunden 0.9.22–0.9.25"
      contains: "Layout geht ab 0.9.25"
  key_links:
    - from: "0.9.23 CaptureView-Unterabschnitt (offener Punkt BADID)"
      to: "Abschnitt ELM_SAB 0.9.25"
      via: "Vermerk „erledigt in 0.9.25, live bestätigt 13.10.“"
      pattern: "erledigt in 0.9.25, live bestätigt"
    - from: "0.9.23 Fallen"
      to: "TapirCommand.GetStories"
      via: "Falle Geschoss lesen"
      pattern: "actStory"
---

<objective>
Die Live-Befunde vom 13.10. (WiSchu, AC29 Build 5101, Teamwork) für ELM_SAB 0.9.22–0.9.25 werden in reference/mcp-extension.md eingetragen. Außerdem werden die Stände auf „live verifiziert“ gesetzt, die Probe-Tabelle 0.9.24 korrigiert und die falsche Geschoss-Angabe (Geschoss-Index 0 → 1. OG) im 0.9.23-Abschnitt richtiggestellt.

Purpose: Die Referenz enthält seit heute Vormittag veraltete „nicht live“-Vermerke, einen eigenen Fehler (Geschoss) und eine widerlegte These (CaptureView scheitert an verdeckten Fenstern). Ohne Korrektur verteilen künftige Sessions unnötige Workarounds (`screencapture`) und lesen das aktuelle Geschoss falsch.
Output: EIN Commit auf main, der nur reference/mcp-extension.md ändert, gepusht.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@./CLAUDE.md
@reference/mcp-extension.md (nur Z. 691–723 und ab Z. 1417 bis Dateiende lesen; die Datei hat 1.730 Zeilen)

Repo: /Users/ap/.claude/repos/archicad-skill, main = 3b61109. Reiner Doku-Task: Archicad NICHT ansprechen, `.claude/` nicht anfassen, keine .planning-Dateien committen.

Rohdaten (nur lesen, falls ein Detail fehlt): /Users/ap/.scratch/elmonkey/gegenprobe_0923/ mit teil_c_2026-10-13.json, teil_c_ifc.json, teil_c_0922_baustoff.json, gegenprobe_0925_2026-10-13.json und den PNGs cap_0925_*.png. Skripte: /Users/ap/.scratch/elmonkey/tools/gegenprobe_findelements_0923_teil_c.py und gegenprobe_elm_sab_0925.py.

Vom Planer geklärte Zeitfolge (Dateizeitstempel): Teil C und die 0.9.22-Baustoff-Probe liefen 15:12–15:14 noch mit dem **0.9.23-Bundle** (buildStamp „Oct 13 2026 10:09:39“ = CI `4e804b3`; es enthält 0.9.22). Der Tausch auf 0.9.25 erfolgte um 15:23 (Backup `~/.scratch/elm-bundle-0.9.25/backup_0.9.23`). D (0.9.24) und E (0.9.25) liefen um 15:33 mit dem **0.9.25-Bundle** (GetAddOnVersion 0.9.25 / Tapir 1.5.4 / buildStamp „Oct 13 2026 13:11:29“ = CI `86309a4`). Rücklese am Ende des 0.9.25-Laufs: Grundriss vorn, actStory 1, Auswahl 0. Bei designOption mit unbekanntem Namen lautet der Fehler -2130313112 „where.designOption: unbekannte Design-Option - vorhanden: “ (leer). `databases` lieferte 267 Treffer, perDatabase mit scopeCount 40.107. Die 0.9.22-Probe verglich 56 Wände, 56/56 gleich, 0 Abweichungen. Verteilung: 1.386 Wände, alle Basic (Bauteil 1.350, Stahlbeton 28, Aluminium 8).

<befund>
A) Eigener Fehler im 0.9.23-Abschnitt (Commit d84ca99): Vorn war das **1. OG** (Tapir GetStories → actStory 1), NICHT Geschoss 0. `ELM_SAB.GetUIState.currentWindow.index` ist KEIN Geschoss-Index (meldete 0). Beleg: Door + OnActualFloor = 84 = Türen im 1. OG (vgl. Punkt 1 der Gegenprobe: Geschoss 1: 84).

B) 0.9.23 FindElements Teil C, je GUID-Menge gegen eine unabhängige Referenz, 0 Abweichungen:
| Kriterium | Probe | Referenz | Ergebnis |
| or | Door ∪ Zone (WiSchu hat KEINE Window-Elemente) | native GetElementsByType | 464=464 |
| layer Name | „10 Wand innen A“ | eingebaute Property Ebenenname | 222=222 |
| layer pattern | „10 *“ | fnmatch auf Ebenenname | 267=267 |
| layer visible | visible:true | Tapir Door+IsVisibleByLayer | 267=267 |
| flags | OnActualFloor | Tapir Door+OnActualFloor | 84=84 |
| guids | 3 Tür-GUIDs + 1 Zonen-GUID mit type Door | – | 3 |
| inSelection | leer / 5 ausgewählt / not | Tapir ChangeSelection | 0 / 5 / 262 (Auswahl danach leer) |
| teamwork | frei/vonMir/vonAnderen | ELM_SAB.GetElementEditState lockUserId/isReservedByMe | 267/0/0 |
| classification | item / code / includeChildren (Eltern „Tür / Tor“) / unclassified | native GetClassificationsOfElements + Baum, System „Archicad Klassifizierung“ (Eintrag „Tür“) | 267/267/267/0 |
| libPart | Name „Blockrahmentür 1-Fl 27“ / pattern „Blockr*“ | Tapir-Details libPart.name | 230=230 / 239=239 |
| gdl | A > 1,0 | Tapir GetGDLParametersOfElements | 132=132 |
| ifc type | IfcDoor | Tapir GetIFCTypeOfElements | 267=267 |
| ifc attribute | GlobalId = ifcId (3 Stichproben, caseSensitive) + startsWith | Tapir GetIFCIdsOfElements | je 1=1 |
| ifc property | Pset_DoorCommon FireRating=„Mindestanforderung“ / IsExternal=„False“ / AcousticRating=„SSK 4“ | Tapir GetIFCPropertiesOfElements | 239 / 264 / 267, alle gleich |
| hostedBy | {type Wall} | Tapir-Details ownerElementType | 267=267 |
| hosts | Wall hosts {type Door} | Owner-Wände der Türen | 204=204 |
| inZone | 8 Zonen | native GetElementsRelatedToZones | 10=10 (752 ms) |
| bbox intersects / inside | Tür-0-Box ±3 m | native Get3DBoundingBoxes (x/y) | 9=9 / 9=9 |
| near | Punkt Türmitte, 5 m | dito | 10=10 |
| designOption | {mainModel:true} | WiSchu hat keine Design-Optionen | 267; unbekannter Name → Fehler „unbekannte Design-Option – vorhanden: (leer)“ |
| databases | Grundriss-DB explizit (GetDatabaseIdFromNavigatorItemId) | – | 267, perDatabase mit scopeCount 40.107 |
| limit | 10 | – | returnedCount 10, truncated true, matchCount 267 |
Nicht prüfbar am WiSchu: designOption mit echten Optionen (keine vorhanden).

C) 0.9.22 GetDetailsOfElements, Wand-Baustoff: WiSchu hat 1.386 Wände, ALLE Basic (Bauteil 1.350, Stahlbeton 28, Aluminium 8). `buildingMaterialId` → Name (native GetBuildingMaterialAttributes), verglichen mit der Bauteil-Property „Baustoffname“ (514C85CB-…). Diese ist NUR auf Komponentenebene verfügbar: native GetComponentsOfElements + GetPropertyValuesOfElementComponents; auf Elementebene notAvailable. Ergebnis: 56/56 gleich. compositeId/profileId sind am WiSchu nicht prüfbar (keine Composite- und keine Profilwände). Falle: Baustoffname ist eine Komponenten-Property.

D) 0.9.24 (sechs Befehle), Bundle 0.9.25:
- Fehlerpfade lieferten den exakten Tabellentext, schemakonform (kein 4009): SetLayerOfElements in beiden Fällen (das Umlaut in „Ebene nicht gefunden: ZZ_Prüfebene_gibt_es_nicht“ kam unverstümmelt an), SetPenOfElements, SetStoryVisibilityOfElements „visibility oder custom erforderlich.“, SetTextSizeOfElements → 5/8.
- 3 Tabellen-Proben erreicht man von außen NICHT, weil das Eingabeschema sie vorher abweist (4002 mit klarer Meldung): CreateWallOpenings `{"openings":[]}` → minItems '#/openings'; SetObjectParametersForce `{"elements":[]}` → minItems '#/elements'; SetStoryVisibilityOfElements `visibility:"Quatsch"` → enum '#/visibility'. Das ist kein Fehler des Fixes, die C++-Pfade sind Absicherung.
- Erfolgsform ohne Modelländerung: Alle sechs Befehle bekamen eine nicht existierende GUID. Jeder antwortete regulär mit einem Fehler je Element (executionResults bzw. results), KEIN 4009 → oneOf lässt die Erfolgsform durch. Hinweis: SetLayerOfElements, SetObjectParametersForce und CreateWallOpenings starten dabei ihre Undo-Klammer (leerer Undo-Schritt, keine Änderung).

E) 0.9.25 CaptureView — Ursache BESTÄTIGT. Jede Probe lieferte ein PNG mit `colorDepth:"MiC"`, ein 256C-Rückfall war nie nötig:
| Fenster | crop | Ergebnis |
| Grundriss 1. OG, Archicad verdeckt (Claude vorn, per System Events unix id belegt) | true | PNG 832×690 RGBA |
| Grundriss, Archicad vorn | true / false | 832×690 / 4000×3317 |
| Layout FR_00-06.1 | true / false | 832×690 / 4000×3317 → Layout geht ab 0.9.25 |
| 3D | true | PNG, view2D:false |
| leerer outputPath | – | -2130313112 „CaptureView: outputPath fehlt oder ist leer“ |
Bildinhalt gesichtet: im Grundriss der Türstempel T.1.029, das Layout „Flucht- und Rettungswege“ farbig. Damit ist die These „CaptureView scheitert an verdeckten Grundriss-Fenstern“ (aus einer anderen Session) WIDERLEGT. Die Ursache war allein die Farbtiefe FromSourceImage. Der Workaround `screencapture` ist nicht mehr nötig.
Fallen aus dem Test:
(1) Es liefen ZWEI Archicad-Instanzen (WiSchu Port 19723, zweites Projekt 7732_LSG_I Port 19724). Für „Archicad vorn“ muss bei mehreren Instanzen der richtige Prozess per `System Events … unix id` nach vorn geholt werden; `tell application "Archicad 29" to activate` ist mehrdeutig.
(2) Zwischen Versions-Rücklese und Test öffnete der Nutzer „Teamwork-Projekt öffnen/beitreten“ → alle Aufrufe 4001, nichts wurde ausgeführt. Vor Testläufen deshalb GetUIState.modalDialogOpen unmittelbar davor prüfen.
(3) Archicad beenden: Ohne offenes Projekt lehnte TapirCommand.QuitArchicad einmal mit 4001 „no open project“ ab (vormittags ging es). osascript-quit meldete -128, Archicad beendete sich trotzdem nach ~6 s → nach -128 erst ein paar Sekunden mit pgrep pollen, dann entscheiden.
</befund>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Live-Befunde 0.9.22–0.9.25 in reference/mcp-extension.md eintragen, committen, pushen</name>
  <files>reference/mcp-extension.md</files>
  <action>
Vorab im Repo /Users/ap/.claude/repos/archicad-skill `git pull --ff-only origin main` ausführen und `BASE=$(git rev-parse HEAD)` notieren. Ist HEAD nicht mehr 3b61109, die Zeilenangaben unten per Grep neu verorten (Anker sind die Überschriften). Alle Änderungen mit dem Edit-Tool, Stil wie in der Datei (deutsch, „live 13.10.“, ✓/✗-Vermerke, Backticks für Befehle und Felder). Die Wörter „Geschoss 0“ und „noch NICHT live verifiziert“ dürfen in neuen Texten nicht vorkommen. Für die Korrektur heißt es „Geschoss-Index 0“.

(1) 0.9.12/0.9.13, Z. 703: An das ENDE der vorhandenen Korrekturzeile (gleiche Zeile, keine neue Zeile, damit sich keine Zeilennummern verschieben) wird „ **Live verifiziert erst mit 0.9.25 (WiSchu 13.10.).**“ angehängt. Sonst bleibt der Abschnitt unverändert.

(2) 0.9.22 (Befund C): Die Zeile „Stand: gebaut (CI), noch NICHT live verifiziert.“ wird ersetzt durch „Stand: **Live verifiziert 2026-10-13 (WiSchu, mit dem 0.9.23-Bundle, das 0.9.22 enthält)** für Basic-Wände; `compositeId`/`profileId` sind am WiSchu nicht prüfbar (keine Composite- oder Profilwände).“ Nach dem Testplan folgt ein Unterabschnitt „### Live-Befund 2026-10-13 (WiSchu)“ mit dieser Verteilung: 1.386 Wände, alle Basic (Bauteil 1.350 / Stahlbeton 28 / Aluminium 8). Dazu die Methode: `buildingMaterialId` → Name über native `GetBuildingMaterialAttributes`, verglichen mit der Bauteil-Property „Baustoffname“ (514C85CB-…) über native `GetComponentsOfElements` + `GetPropertyValuesOfElementComponents`. Ergebnis 56/56 gleich, Rohdaten `~/.scratch/elmonkey/gegenprobe_0923/teil_c_0922_baustoff.json`. Danach folgt ein Fallen-Punkt: „Baustoffname“ ist eine Komponenten-Property, auf Elementebene (`GetPropertyValuesOfElements`) kommt `notAvailable`. Die Testplan-Punkte werden abgehakt: 1 ✓ (über das 0.9.23-Bundle), 2 ✓ (56/56), 3 „nicht prüfbar am WiSchu (keine Composite-Wände)“.

(3) 0.9.23, Korrektur A:
- scope-Tabelle Z. 1535: „24.731 (Geschoss 0)“ wird zu „24.731 (1. OG)“.
- Z. 1538: „(Grundriss Geschoss 0 vorn)“ wird zu „(Grundriss 1. OG vorn)“.
- Gegenprobe-Zeile 2: „(Grundriss Geschoss 0 vorn)“ wird zu „(Grundriss 1. OG vorn)“, und „137 ms, Geschoss 0“ wird zu „137 ms, 1. OG“.
- Zeile 9: beide Vorkommen von „Grundriss Geschoss 0“ werden zu „Grundriss 1. OG“.
- Direkt unter der 9-Zeilen-Tabelle steht künftig eine Zeile **Korrektur 13.10. nachmittags:** Vorn war das 1. OG (`TapirCommand.GetStories` → `actStory` 1), nicht Geschoss-Index 0. Der frühere Wert stammte aus `ELM_SAB.GetUIState.currentWindow.index`, und das ist KEIN Geschoss-Index (meldete 0). Beleg: Door + OnActualFloor = 84 = Türen im 1. OG (Punkt 1).
- Unter „### Fallen“ kommt ein neuer Punkt: **Aktuelles Geschoss** über `TapirCommand.GetStories` → `actStory` lesen, nie über `GetUIState.currentWindow.index` (das ist kein Geschoss-Index, live 13.10.: meldete 0 bei 1. OG vorn).
- In der 0.9.25-Anlass-Zeile Z. 1685 wird „für Grundriss (Geschoss 0)“ zu „für Grundriss (1. OG)“.

(4) 0.9.23, Teil C (Befund B): Direkt nach der Korrekturzeile aus (3), noch vor „### CaptureView: …“, kommt ein Block „**Teil C (13.10. nachmittags, 0.9.23-Bundle): alle bisher nur gebauten Kriterien**“. Er nennt die Methode (je GUID-Menge gegen eine unabhängige Referenz, 0 Abweichungen), die Rohdaten `teil_c_2026-10-13.json` + `teil_c_ifc.json` und das Skript `~/.scratch/elmonkey/tools/gegenprobe_findelements_0923_teil_c.py`. Es folgt die Tabelle aus Befund B mit allen 22 Zeilen und den Spalten Kriterium | Probe | Referenz | Ergebnis, Werte wörtlich übernehmen. Der designOption-Fehlertext ist dabei „unbekannte Design-Option - vorhanden: “ (leer). Schlusssatz: Am WiSchu nicht prüfbar ist nur die Auflösung von `designOption` per Name/id/guid, weil das Modell keine Design-Optionen hat.

(5) 0.9.23 CaptureView-Unterabschnitt:
- Die Bullet „Offener Punkt: CaptureView-Funktion – Ursache APIERR_BADID untersuchen:“ beginnt künftig mit „**Offener Punkt → erledigt in 0.9.25, live bestätigt 13.10.:** CaptureView-Funktion – Ursache APIERR_BADID“, analog zur 0.9.24-Bullet darunter.
- In derselben Bullet wird „Fix in 0.9.25 (nicht live verifiziert):“ zu „Fix in 0.9.25 (live bestätigt 13.10.: PNG mit `colorDepth` "MiC" für Grundriss, Layout und 3D):“.
- „**Workaround bis dahin:** `screencapture` mit Archicad-Fenster vorn.“ wird zu „Workaround `screencapture` war nur bis 0.9.25 nötig.“
- An die Live-Bullet Z. 1580 (Layout-Fehler) wird angehängt: „→ ab 0.9.25 liefert Layout ein PNG (live 13.10.).“

(6) 0.9.23 Stand + Testplan:
- „Getesteter Umfang“ bekommt zusätzlich: Teil C mit `or`, `layer` (Name/pattern/visible), `flags`, `guids`, `inSelection`, `teamwork`, `classification` (item/code/includeChildren/unclassified), `libPart`, `gdl`, `ifc` (type/attribute/property), `hostedBy`/`hosts`, `inZone`, `bbox`/`near`, `designOption` `{mainModel:true}` + Fehler bei unbekanntem Namen, die Parameter `databases` und `limit`/`truncated`, außerdem Beispiel (c) über die scope-Probe mit `countOnly`.
- Die CaptureView-Bullet wird zu: Fehlerform verifiziert. Der Bildexport scheiterte in 0.9.23 mit APIERR_BADID → behoben in 0.9.25, live bestätigt 13.10. (Grundriss, Layout, 3D).
- Die Bullet „Nur gebaut (CI), live noch nicht getestet: …“ wird ersetzt durch „**Am WiSchu nicht prüfbar:** `designOption` per Name/id/guid (keine Design-Optionen im Modell). Die Beispiele (d) und (e) wurden nicht wörtlich ausgeführt; ihre Bausteine (`or`, `classification` mit `includeChildren`/`unclassified`, `hostedBy`, `layer.pattern`) sind einzeln live belegt.“
- Testplan Punkt 9: Anhang „; ab 0.9.25 liefert Layout ein PNG (Abschnitt 0.9.25)“.
- Testplan Punkt 10: Anhang „→ ✓ ab 0.9.25 (live 13.10.: Grundriss PNG 832×690 bzw. 4000×3317, 3D PNG `view2D:false`, `colorDepth` "MiC")“.

(7) 0.9.24 (Befund D):
- Die Tabelle „Betroffene Befehle“ bekommt rechts eine Spalte „Live 13.10. (Bundle 0.9.25)“. Werte je Zeile in Tabellenreihenfolge: „vom Eingabeschema abgefangen (4002, minItems '#/openings')“ · „unter AC29 nicht erreichbar, nicht geprüft“ · „✓ exakter Text“ · „✓ exakter Text, „ü“ unverstümmelt“ · „vom Eingabeschema abgefangen (4002, minItems '#/elements')“ · „✓ exakter Text“ · „vom Eingabeschema abgefangen (4002, enum '#/visibility')“ · „✓ exakter Text“ · „✓ exakter Text“.
- Unter der Tabelle ein Satz: Drei Proben erreichen den C++-Pfad von außen nicht, weil das Eingabeschema sie vorher mit 4002 und klarer Meldung abweist. Die C++-Pfade bleiben Absicherung, das ist kein Fehler des Fixes. Ergebnis: 5/8 erreichbare Proben mit exaktem Tabellentext, kein 4009.
- „### Stand“ wird ersetzt durch „**Live verifiziert 2026-10-13 (WiSchu, AC29 Build 5101, Teamwork; Bundle 0.9.25, buildStamp „Oct 13 2026 13:11:29“ = CI `86309a4`)**“. Der Satz zum Push-Zeitpunkt und zum SUMMARY 261013-jom bleibt.
- Neuer Unterabschnitt „### Live-Gegenprobe 2026-10-13 (WiSchu)“ mit drei Teilen: (a) die Fehlerpfade wie oben. (b) Die Erfolgsform ohne Modelländerung: alle sechs Befehle mit nicht existierender GUID → reguläre Antwort mit Fehler je Element (`executionResults` bzw. `results`), kein 4009 → `oneOf` lässt die Erfolgsform durch. (c) Rohdaten `~/.scratch/elmonkey/gegenprobe_0923/gegenprobe_0925_2026-10-13.json` (Schlüssel `D_0924`) und Skript `~/.scratch/elmonkey/tools/gegenprobe_elm_sab_0925.py`.
- Darunter zwei Fallen: (i) Leere Pflichtlisten und ungültige Enum-Werte fängt bereits das Eingabeschema mit 4002 ab, die Meldung nennt den JSON-Pfad. (ii) Die Probe mit nicht existierender GUID startet bei `SetLayerOfElements`, `SetObjectParametersForce` und `CreateWallOpenings` die Undo-Klammer → leerer Undo-Schritt im Verlauf, keine Änderung.
- Testplan abhaken:
  - 1 ✓ (GetAddOnVersion 0.9.25; das Bundle enthält 0.9.24)
  - 2 ✓ (5/8 exakter Text, 3 vom Eingabeschema abgefangen, s. Tabelle)
  - 3: Vermerk „ersetzt durch die Probe mit nicht existierender GUID (Erfolgsform belegt, ohne Modelländerung). Ein Schreibvorgang an einem eigenen Testelement wurde nicht ausgeführt; das Erfolgsschema ist byte-gleich.“
  - 4 ✓ (Auswahl am Ende 0 laut Rücklese; nichts geändert)

(8) 0.9.25 (Befund E):
- In „Ursache (DevKit 29.3100, nicht live belegt)“ wird der Klammerinhalt zu „(DevKit 29.3100; live bestätigt 13.10.)“.
- An „Restunsicherheit“ wird angehängt: „→ ausgeräumt am 13.10.: MiC lieferte bei jeder Probe ein PNG, kein Rückfall.“
- In der Auswertungstabelle bekommt die erste Zeile den Zusatz „← Ergebnis 13.10. (WiSchu)“.
- „### Stand“ wird ersetzt durch „**Live verifiziert 2026-10-13 (WiSchu, AC29 Build 5101, Teamwork; GetAddOnVersion 0.9.25 / Tapir 1.5.4, buildStamp „Oct 13 2026 13:11:29“ = CI `86309a4`; Backup 0.9.23 in `~/.scratch/elm-bundle-0.9.25/backup_0.9.23`)**“. Dazu ein Satz: Beide Änderungen des Bundles wurden getrennt abgehakt (0.9.24 im Abschnitt oben).
- Der Satz „Workaround bis zur Live-Bestätigung: `screencapture` …“ wird ersetzt durch „Workaround `screencapture` ist nicht mehr nötig.“
- Neuer Unterabschnitt „### Live-Gegenprobe 2026-10-13 (WiSchu)“:
  - Tabelle aus Befund E mit 5 Zeilen, Spalten Fenster | crop | Ergebnis, inklusive „**Layout geht ab 0.9.25**“.
  - Bildinhalt gesichtet: im Grundriss der Türstempel T.1.029, das Layout „Flucht- und Rettungswege“ farbig.
  - These widerlegt: Die These „CaptureView scheitert an verdeckten Grundriss-Fenstern“ ist **widerlegt** (Grundriss bei verdecktem Archicad → PNG). Ursache war allein die Farbtiefe `FromSourceImage`.
  - Rohdaten: `gegenprobe_0925_2026-10-13.json` (Schlüssel `E_0925`) und `cap_0925_*.png` in `~/.scratch/elmonkey/gegenprobe_0923/`.
- Neuer Unterabschnitt „### Fallen beim Live-Test“ mit den drei Fallen (1)–(3) aus Befund E, knapp formuliert. Die Ports 19723/19724 stehen als Beispiel drin, keine PID. Genannt werden müssen `System Events … unix id`, `GetUIState.modalDialogOpen`, `TapirCommand.QuitArchicad` 4001 „no open project“ sowie osascript -128 → pgrep-Polling.
- Testplan abhaken:
  - 1 ✓, mit Vermerk „Abweichung: zwei Instanzen offen, s. Fallen“
  - 2 ✓
  - 3 ✓ (832×690 RGBA, "MiC"; Inhalt gesichtet, Türstempel T.1.029). Steht in `gegenprobe_0925_2026-10-13.json` kein screencapture-Vergleich, wird „kein screencapture-Vergleich“ ergänzt; ob der Vergleich vorkommt, per `grep -c screencapture` prüfen.
  - 4 ✓ (4000×3317)
  - 5 ✓ (`view2D:false`)
  - 6 ✓ „Layout geht ab 0.9.25“
  - 7 ✓ (-2130313112)
  - 8 ✓ (→ Abschnitt 0.9.24)
  - 9 ✓ (Auswertung Zeile 1; Auswahl am Ende 0)

(9) Commit und Push:
- Erneut `git pull --ff-only origin main` ausführen.
- `git add reference/mcp-extension.md`, dann prüfen, dass `git diff --cached --name-only` nur diese Datei zeigt.
- Commit mit der Nachricht „docs(ELM_SAB): 0.9.22–0.9.25 live verifiziert (WiSchu 13.10.) – FindElements komplett, CaptureView geht, 0.9.24-Proben“, Leerzeile, Abschlusszeile `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- `git push origin main`.
- Keine .planning-Dateien committen, `.claude/` nicht anfassen.
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && F=reference/mcp-extension.md && for t in "actStory" "1. OG" "Layout geht ab 0.9.25" "MiC" "minItems" "Baustoffname" "GetPropertyValuesOfElementComponents" "464" "FireRating" "unix id" "modalDialogOpen" "erledigt in 0.9.25, live bestätigt" "widerlegt" "Live verifiziert erst mit 0.9.25"; do grep -q -- "$t" $F || echo "FEHLT: $t"; done; S23=$(awk '/^## ELM_SAB 0.9.23/{f=1} /^## ELM_SAB 0.9.24/{f=0} f' $F); SEC=$(awk '/^## ELM_SAB 0.9.22/{f=1} f' $F); echo "geschoss0_in_0923=$(printf '%s\n' "$S23" | grep -c '(Geschoss 0)')"; echo "grundriss_geschoss0_ab_0922=$(printf '%s\n' "$SEC" | grep -c 'Grundriss Geschoss 0\|Grundriss (Geschoss 0)')"; echo "noch_NICHT_ab_0922=$(printf '%s\n' "$SEC" | grep -c 'noch NICHT live verifiziert')"; echo "fix_nicht_live=$(printf '%s\n' "$SEC" | grep -c '0.9.25 (nicht live verifiziert)')"; echo "diff_vor_0922_zeilen=$(diff <(git show HEAD~1:$F | sed -n '1,1416p') <(sed -n '1,1416p' $F) | grep -c '^[<>]')"; echo "files=$(git diff HEAD~1 --name-only | tr '\n' ' ')"; git fetch -q origin && [ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] && echo PUSH_OK || echo PUSH_FEHLT</automated>
  </verify>
  <done>
Diese Ausgaben müssen kommen:
- keine Zeile „FEHLT“
- geschoss0_in_0923=0, grundriss_geschoss0_ab_0922=0, noch_NICHT_ab_0922=0, fix_nicht_live=0
- diff_vor_0922_zeilen=2 (nur Z. 703 alt/neu; liegt BASE nicht auf 3b61109 und ist die 0.9.22-Überschrift verrutscht, die Grenze per `grep -n '^## ELM_SAB 0.9.22'` in HEAD~1 anpassen)
- files=„reference/mcp-extension.md “ (nur diese Datei)
- PUSH_OK

Inhaltlich muss die Referenz danach Folgendes zeigen: alle vier Abschnitte „Live verifiziert 2026-10-13“, die Teil-C-Tabelle mit 22 Zeilen, die 0.9.24-Tabelle mit Live-Spalte und die 0.9.25-Gegenprobe mit 5 Zeilen.
  </done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| lokales Repo → origin/main (GitHub mozzi86/-archicad) | Direkter Push auf main ohne Review |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-261013-lmd-01 | Tampering | git push origin main | mitigate | Vor dem Edit und vor dem Commit `git pull --ff-only` ausführen (kein Merge, kein Force). Vor dem Commit `git diff --cached --name-only` = nur reference/mcp-extension.md. Nach dem Push prüfen, dass HEAD == origin/main. |
| T-261013-lmd-02 | Information Disclosure | Pfade/Ports/Projektnamen im Doku-Text | accept | Nur lokale ~/.scratch-Pfade, Ports und Projektkürzel, keine Zugangsdaten. Die Datei enthält bereits gleichartige WiSchu-Belege, PIDs werden bewusst weggelassen. |
| T-261013-lmd-03 | Tampering | Archicad-Modell | mitigate | Reiner Doku-Task, Archicad wird NICHT angesprochen, keine MCP-Aufrufe. |
</threat_model>

<verification>
Siehe den automatisierten Verify-Block in Task 1. Zusätzlich bestätigt eine Sichtprobe von `git show --stat HEAD`: 1 Datei, nur Einfügungen und Ersetzungen in den Abschnitten 0.9.22–0.9.25 plus Z. 703.
</verification>

<success_criteria>
- reference/mcp-extension.md enthält die Befunde A–E vollständig in den zugehörigen Abschnitten.
- Die Stände 0.9.22–0.9.25 lauten „Live verifiziert 2026-10-13“. Offen bleibt nur die designOption-Namensauflösung (am WiSchu nicht prüfbar).
- Die Geschoss-Angabe ist korrigiert, und die Falle „actStory statt currentWindow.index“ steht drin.
- Ein Commit mit der vorgegebenen Nachricht ist auf origin/main gepusht und ändert nur reference/mcp-extension.md.
</success_criteria>

<output>
Create `.planning/quick/261013-lmd-doku-elm-sab-0-9-22-0-9-25-live-verifizi/261013-lmd-SUMMARY.md` when done (NICHT committen; das macht der Orchestrator).
</output>
