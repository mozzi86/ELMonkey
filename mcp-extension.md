# MCP-Stack: Architektur, Grenzen, Erweiterung (ELM_SAB_Add-On)

*Stand: 2026-07-13, live verifiziert am THN-Projekt (AC29, Teamwork).*

## Der Stack — wer spricht mit wem

```
Claude ──► tapir-archicad-mcp (Python, uv tool "archicad-server")
              │  nutzt: multiconn_archicad
              ▼
         Archicad JSON-API  +  Tapir Add-On (C++, TapirAddOn_AC29_Mac.bundle)
                            +  ELM_SAB_Add-On (C++, eigenes Bundle, Namespace ELM_SAB)
```

- MCP-Server: `uv tool list` → `tapir-archicad-mcp`; Update: `uv tool upgrade tapir-archicad-mcp`
  (danach Claude Desktop neu starten — laufende `archicad-server`-Prozesse bleiben sonst alt).
- Tapir-Bundle: `/Applications/Graphisoft/Archicad 29/Add-Ons/TapirAddOn_AC29_Mac.bundle`;
  Releases: github.com/ENZYME-APD/tapir-archicad-automation/releases (Archicad vorher beenden,
  nach Download `xattr -rd com.apple.quarantine` auf das Bundle).
- Eigene Befehle: `ELM_SAB_Add-On/` in diesem Repo (CMake, Vorbild tlorantfy/archicad-additional-json-commands).

## Was der MCP NICHT kann (verifiziert 2026-07-13, Tapir ≤1.5.3)

- **Stift/Farbe von Elementen ändern** — `elements_set_details_of_elements` kann nur
  Ebene/Geschoss/drawIndex (+ Wand-Geometrie). Kein Pen-Befehl in Tapir, keiner in der
  offiziellen JSON-API. → Genau dafür gibt es `SetPenOfElements` im ELM_SAB_Add-On.
- **2D-Elemente außer Polylinien anlegen** — Tapir kann nur `CreatePolylines`; Linien, Bögen,
  Kreise, Splines und Schraffuren fehlen. → Seit ELM_SAB 0.9.26 `ELM_SAB.Create2DElements`
  (siehe Abschnitt 0.9.26 am Dateiende; noch nicht live verifiziert).
- **Grafische Überschreibungen anlegen** — nur per Ansicht zuweisen
  (`navigator_set_view_settings`, Feld `graphicOverrideCombination`).
- `elements_get_details_of_elements` hat in AC29 einen Schema-Bug (pydantic
  `additionalProperties`-Fehler) → property-basiert lesen (siehe Memory/Workaround).

## Was der MCP KANN (oft übersehen)

- **Layout-Buch komplett lesen**: `navigator_get_navigator_item_tree` mit
  `{"navigatorTreeId": {"type": "LayoutBook"}}` — liefert Layouts, Subsets, Master
  und **platzierte Zeichnungen (DrawingItem) pro Layout mit Namen**. THN: 264 Layouts,
  357 Zeichnungen. Achtung: Antwort >500 KB → landet als Datei, mit Python parsen.
- Ansicht-Settings lesen/schreiben: `navigator_get_view_settings` / `navigator_set_view_settings`
  (LayerCombination, PenSet, MVO, GraphicOverride, DimStyle — pro View).
- 2D-Elemente inkl. Line/PolyLine/Arc/Circle/Spline/Hatch/Text/Label/Dimension per
  `elements_get_elements_by_type`, Filter `OnActualFloor`/`IsVisibleByLayer`;
  Positionen via `elements_get2_d_bounding_boxes`.
- Ebenenkombinationen: `attributes_get_attributes_by_type {"attributeType": "LayerCombination"}`
  (das parameterlose `attributes_get_layer_combinations` mit leerer Liste gibt LEER zurück — Falle!).

## Tapir ≥1.5.3 bringt neu (ggü. altem Stand „Wände nicht erstellbar")

*Live verifiziert 2026-07-13 (`app_get_add_on_version` → 1.5.3, MCP 0.4.3):*
`CreateWalls`, `CreateBeams`, `CreateWindows/Doors/Openings`, `CreateRoofs/Stairs/Morphs/
Lamps/Texts/Labels`, `ModifyWalls/Slabs/Columns/Beams/Windows/Doors/Roofs/Morphs/Meshes`,
`RotateElements`, `LockElements/UnlockElements`, `GetElementPreviewImage` (2D/Section/3D-PNG),
`GetRoomImage`, `CreateSections`, `CreateAssociativeDimensions`, `GetDimensionData`;
neue Modul-Gruppen `design_options`, `element_grouping`, `ifc`.
`recipes/wall-operations.md` ist entsprechend korrigiert.

## Update-Stolperfalle: Zweit-Instanzen (live erlebt 2026-07-13)

Nach Bundle-Tausch meldete `app_get_add_on_version` weiter die alte Version. Ursache:
**zwei Archicad-Prozesse** — eine Instanz lief seit Tagen im Hintergrund weiter (nur das
Projektfenster war zu). Vor dem Update mit `pgrep -fl Archicad` prüfen, dass ALLE
Archicad-Prozesse beendet sind (Cmd+Q, nicht nur Fenster schließen). Außerdem: keine
Backup-Kopien im `Add-Ons/`-Ordner lassen — Archicad versucht sie zu laden
(„Einige Add-Ons konnten nicht geladen werden"). <!-- 2026-08-05: real verletzt — ein
.bak-Bundle lag seit 23. Juni im Add-Ons-Ordner und war die ganze Zeit die Ursache der
Meldung. --> Deshalb als **Pflichtschritt jeder Update-Prozedur**: vor dem Archicad-Start
den `Add-Ons/`-Ordner auf `*.bak`/Backup-Bundles prüfen (`ls`) und Sicherungen NUR
außerhalb ablegen (z. B. `~/ELM_SAB_Backups/`).

## Erweiterungs-Entscheidungsbaum

1. **Kombi aus vorhandenen Befehlen?** → Python, `tools/custom/functions.py` im
   tapir-archicad-mcp-Paket (oder Skill-Script).
2. **Neues Primitiv (C++-API nötig)?** → Befehl ins **ELM_SAB_Add-On** (nicht Tapir forken).
   Muster: Command-Klasse von `ELMCommandBase` ableiten, in `AddOnMain.cpp` registrieren.
3. **Allgemein nützlich?** → Zusätzlich als PR an Tapir (Maintainer-Kontakt: tlorantfy,
   sehr aktiv — Forks tagesaktuell).

## Referenz-Repos

- github.com/ENZYME-APD/tapir-archicad-automation — Tapir (Add-On + Befehlsquellen unter
  `archicad-addon/Sources/*Commands.cpp`)
- github.com/tlorantfy/archicad-additional-json-commands — Blaupause „eigenes Zusatz-Add-On"
- github.com/tlorantfy/archicad-python-scripts — fertige Script-Patterns (recurring_publish u.a.)
- github.com/GRAPHISOFT/archicad-addon-cmake — Build-Template (Basis von ELM_SAB_Add-On)

## ELM_SAB_Add-On v0.1 — Live-Erkenntnisse (2026-07-13, THN-Farbabgleich)

**Status: FUNKTIONIERT.** Erster Produktiveinsatz: 32.806 Elemente gelesen (0 Fehler),
~9.200 Stift-Zuweisungen geändert (13→90, 240/19→91, 21→226, 93→253, 7→6).

**MDID-Pflicht:** Add-Ons laufen nur mit offiziell registrierter MDID (Developer-ID +
Local-ID, beide aus archicadapi.graphisoft.com/profile/add-ons — SAB: Dev-ID 944131939,
ELM_SAB Local-ID 1033975726). Selbstgewählte IDs ⇒ „kann nicht freigeschaltet werden /
Distributor" und stilles Nicht-Laden. Signatur (ad-hoc) ist dagegen KEIN Blocker.

**Schreib-Limits von ACAPI_Element_Change (v0.1):**
1. Nur Elemente der **aktiven Datenbank** änderbar (Lesen geht überall). Workaround:
   User klickt Arbeitsblätter durch, Auto-Retry-Schleife stiftet um. → v0.2: DB-Wechsel einbauen.
2. Elemente auf **ausgeblendeten Ebenen** sind nicht änderbar (err -2130312912-Familie).
   Vorher Ebenen sichtbar schalten (Ansicht mit passender Ebenenkombination öffnen).
3. **Teamwork:** Elemente müssen reserviert sein.
4. Fehler-Response ist verschachtelt: `executionResults[i].error.error.message`.

**Farbabgleich-Rezept (wiederverwendbar):** Inventur via GetPenOfElements (Batch 500)
→ RGB-Lookup in Stifttabelle (API.GetPenTableAttributes) → Mapping-Tabelle → Confirm
→ SetPenOfElements pro (Rolle, Ziel)-Gruppe → Nachher-Inventur als Verifikation.

## v0.2 (2026-07-13, gleicher Tag) — RGB-Overrides + Auto-DB-Wechsel

DWG-Importe brennen Füllungsfarben oft als **RGB-Override direkt ins Element**
(API_HatchType.hatchFlags: APIHatch_HasFgRGBColor/HasBkgRGBColor + foregroundRGB/backgroundRGB)
— unsichtbar für jede Stift-Logik. v0.2 kann beides:
- `GetPenOfElements` liefert `hasForeground/BackgroundRGB` + Farbwerte
- `SetPenOfElements` löscht Overrides (`clearForegroundRGB`/`clearBackgroundRGB`) und setzt
  zugleich den Ziel-Stift; optionaler `databases`-Param wechselt selbst durch die
  Arbeitsblätter (Tapir-Muster: ACAPI_Window_GetDatabaseInfo → ACAPI_Database_ChangeCurrentDatabase,
  Ausgangs-DB wird wiederhergestellt) — v0.1-Klickerei entfällt.
Produktiv: 1.878 Override-Schraffuren (27 Farben) in EINEM Lauf auf nächstliegende
SAB-Stifte gemappt, 0 Fehler. Anschließend Projekt-Vollscan (48 DBs: Grundriss, 41
Arbeitsblätter, Schnitte/Ansichten/Details): weitere 49.046 Overrides in 10 Import-
Arbeitsblättern (PDF Übersichtspläne 40k!, Möblierung, Sammel-Blätter, Plankopf) —
alle in einem Batch normalisiert, 49.046/49.046 ok. Voraussetzung: Ebenenkombination
„alles an" (Sichtbarkeit) + Teamwork-Reservierung. Nearest-Pen-Matching = RGB-Distanz gegen Stifttabelle;
Treffer meist exakt, da DWG-Farben ≈ AutoCAD-Palette ≈ Stift-Slots.
**DevKit-Header lokal:** ~/Developer/APIDevKit29/Support/Inc — Feldnamen IMMER dort
verifizieren statt raten.

## ELM_SAB v0.3 + v0.4 (2026-07-13 nachmittags) — Wände aus 2D

- **v0.3 `CreatePolygonWalls`**: Polygon-Wände (APIWtyp_Poly + Memo coords/pends,
  Muster: Tapir CreateZones-Allokation). Tapir 1.5.3 kann nur gerade Wände.
  Params: polygonCoordinates, floorIndex, height, bottomOffset, layerIndex,
  compositeIndex/buildingMaterialIndex. Braucht aktives Grundriss-Fenster.
- **v0.4 `Get2DGeometryOfElements`**: Line (beg/end), Arc/Circle (origin, r, ratio,
  angle, beg/endAngle, reflected, whole), PolyLine (coords + arcs mit arcAngle,
  1-basierte Indizes) + layerIndex/floorIndex. Liest cross-DB.
- Workflow-Doku: `recipes/konturen-zu-waende.md`. Erster Produktivlauf: 2.032 Wände.
- DevKit-Feld-Verifikation vor jedem neuen Befehl: ~/Developer/APIDevKit29/Support/Inc.

## ELM_SAB v0.5 (2026-07-14) — GetTextsOfElements

- **`GetTextsOfElements`**: Inhalt (memo.textContent via APIMemoMask_TextContentUni),
  Position (text.loc), Ebene, Geschoss. Schließt Tapirs Text-Lücke
  („Not yet supported"). Produktiv: 40.479 Texte gelesen, 1.193 Durchbruch-
  Beschriftungen extrahiert → 301 WDs mit echten DWG-Maßen/-Höhen gesetzt.
- Nützliche Tapir-Befehle rund um Öffnungen: `GetFavoritesByType` /
  `ApplyFavoritesToElementDefaults` (SAB-Favoriten aufs Tool anwenden, dann
  `CreateOpenings`), `GetDetailsOfElements` liefert für Wände **floorIndex +
  `floorPlanPolygons`** (Host-Matching! — das Feld heißt NICHT `polygonOutline`,
  siehe Korrektur 2026-08-31 weiter unten), `API.Get2DBoundingBoxes` (offizieller
  Befehl — Tapir hat keinen eigenen).
- **Tapir-Bug (blacklist)**: `ModifySlabs` mit `polygonOutline` crasht Archicad
  fatal; `holes: []` wird als „kein Feld" ignoriert. Details + Workaround:
  `recipes/oeffnungen-aus-konturen.md`.

## Robuste Detail-Reads + Ein-Schreiber-Regel <!-- 2026-07-14 -->

`GetDetailsOfElements` scheitert bei einzelnen Elementen am AC29-Schema-Bug
(Code 4009, Tapir-eigene Response-Validierung). Standard-Muster: **Bisektion** —
Batch bei Fehler halbieren, defekte Einzelfälle überspringen statt den ganzen
Batch zu verlieren.

Bei Agent-Delegation gilt: **nur EIN Archicad-API-Schreiber gleichzeitig**
(seriell); Parallelität nur für Offline-Analysen. Und die Fenster-Regel:
Erzeugen von Modellelementen (Wände, Türen, Decken) braucht ein offenes
GRUNDRISS-Fenster — im 3D-Fenster kommen irreführende Fehler.
Details: `reference/referenzmodell-abgleich.md`, `recipes/tueren-aus-boegen.md`.

## Kombi-Add-On ELM_SAB_Tapir + Label-Pipeline <!-- 2026-07-16 -->

**Sichtbarer Name seit 0.9.10 (2026-08-04): „ELMonkey SAB"** — Menüreiter,
Add-On-Manager und About-Dialog (zeigt `ELM_SAB x.x.x · Tapir x.x.x · Port n`).
Seit 0.9.11 mit eigenem Logo (King Kong am Hochhaus; Palette: Affenkopf —
`Sources/RFIX/Images/ELMonkeyLogo*.svg`, Ressourcen-IDs heißen weiter
`ID_TAPIR_LOGO*`) und Dankeszeile an die Tapir-Macher im About. Vom Nutzer
abgenommen 2026-08-04 („der Affe bleibt").
Die Namespaces `TapirCommand`/`ELM_SAB` sind unverändert. **Upstream-Auto-Update
ist stillgelegt** (Menüpunkt entfernt, `TapirPalette::UpdateAddOn` returnt false):
Tapirs am 31.07. repariertes macOS-Auto-Update hätte sonst das Vanilla-Tapir über
unser Bundle installiert — alle ELM_SAB-Befehle und SAB-Fixes weg. Updates kommen
ausschließlich aus der eigenen CI (`ci-status`, `bundles/`). Upstream steht auf
**1.5.7** (neu ggü. 1.5.4: `GetRelationsOfElements`, `GetMEPPreferenceTables`,
Object/Lamp-Vollsupport) — Nachzug ist ein eigenes, offenes Arbeitspaket.

Seit 2026-07-16 ist Tapir 1.5.4 KOMPLETT in unser Add-On integriert
(`ELM_SAB_Tapir/`, Bundle `ELM_SAB_AC29_Mac.bundle`, eigene CI-Release
`elm-sab-tapir-latest`). Ein Bundle, beide Namespaces (`TapirCommand` +
`ELM_SAB`), SAB-MDID. Originale Tapir-/ELM-Bundles liegen in `~/ELM_SAB_Backups/`.

Neue ELM_SAB-Befehle: `SetTextSizeOfElements` (Text+Label, mm/Faktor),
`SetTextsOfElements` (Inhalt Text/Label), `SetAddParsOfElements`
(GDL-Parameter via AddPars-Memo — Object/Lamp/Label). Alle mit
**eingebauter Rücklese-Verifikation** (NoError beweist nichts, s.u.).

- **Tapir-Bug (Crash, live reproduziert)**: `SetGDLParametersOfElements` auf
  einem LABEL → SIGSEGV in `VBElem::LibPartConnections::GetLibPartId`
  (paramOwner.type ist hart API_ObjectID). Im Kombi-Add-On per Typ-Guard
  entschärft; für Labels IMMER `ELM_SAB.SetAddParsOfElements` nehmen.
- **Label-Größe ändern**: Nur `u.text.size` maskieren wird still ignoriert.
  DevKit-Muster Do_Label_Edit: `textSize` (top-level) + `u.text.size` setzen
  UND das Memo (textContent) an ACAPI_Element_Change übergeben.
- **Etikett-Subtypen**: Eigene Label-GSMs brauchen Ancestry
  `F938E33A…` (General GDL Object) → `B176ABF1…` → `4FD10D67…` →
  `BDB8C3EE…` (Label). Reihenfolge Wurzel→Elternteil; falsche GUIDs =
  Objekt existiert, ist aber im Werkzeug UNSICHTBAR.
- **GSM-Erzeugung ohne Handarbeit**: LP_XMLConverter steckt in
  `Archicad 29.app/Contents/MacOS/LP_XMLConverter.app` (xml2libpart);
  Subtyp-GUIDs aus `BuiltInLibraryParts.libpack` (extractpackage →
  extractcontainer → libpart2xml der Subtypes/*.gsm). Upload per Tapir
  `AddFilesToEmbeddedLibrary` + `ReloadLibraries` — braucht in Teamwork die
  **Reservierung der eingebetteten Bibliothek**; Überschreiben geht nicht
  (erst im Bibliothekenmanager löschen).
- **GDL-Fallen**: `mod` und `off` sind Operator-/Keyword-Namen — als Variablen
  bricht das 2D-Skript still ab („Ungültiges 2D-Symbol" in den
  Grundeinstellungen ist der einzige Hinweis). GDL-Parameterstrings kappen
  API-seitig bei 255/512 Zeichen → lange Daten (QR-Bits: 2401) auf mehrere
  Parameter à ≤250 splitten.
- **Capmo-QR-Pipeline (Pilot ABGENOMMEN 2026-07-16, QR scannt vom Bildschirm)**:
  SAB_QR_Etikett (Label-GSM, zeichnet QR aus qrbits1..10 + Ticket-Text +
  Mikro-URL) hängt per Etikett-Werkzeug am Klappen-Quader; Befüllung via
  SetAddParsOfElements; QR-Matrix aus python `qrcode` (ERROR_CORRECT_M,
  border=0, v8=49×49). Capmo-API: api.capmo.de (Key in Bridge-config.json),
  kein Kurzlink-Feld — URL bleibt lang. Textgrößen: Ticket 0.4 mm, URL 0.1 mm.
- **GDL-Falle Nr. 1 dieser Session — `poly2_b` füllt NUR geschlossene
  Polygone**: frame_fill ist ein Bitfeld: 1=Rahmen, 2=Füllung, **4=Polygon
  geschlossen**. Mit j=3 gilt das Polygon als OFFENER Linienzug → Archicad
  zeichnet still nur die Kontur („umgedrehte C's", letzte Kante fehlt).
  Richtig: **j=7** (Graphisofts eigene Marker-Macros nutzen 5/7, nie 3).
  Kostete 3 Bibliothekstausch-Zyklen — Referenz: `Section-Elevation Marker
  Macro` in BuiltInLibraryParts (libpart2xml + grep poly2_b).
  **Nachtrag 2026-07-28 (PDF-Plot-Test):** Für WINZIGE Füllflächen (QR-Module
  ~0,25 mm) gilt **j=6** (Füllung+geschlossen, OHNE Rahmen-Bit 1) — am
  Bildschirm sieht j=7 gut aus, aber beim PDF-Plot bekommt die Kontur die
  echte Stiftstärke und schmiert benachbarte Module zu einem schwarzen
  Klotz zusammen. j=7 nur für Flächen, deren Kantenlänge ≫ Stiftstärke.
  Fix-Zyklus: XML editieren → LP_XMLConverter xml2libpart → User löscht
  Alt-Libpart im Bibliothekenmanager → AddFilesToEmbeddedLibrary +
  ReloadLibraries; gleiche GUID in der XML ⇒ platzierte Etiketten
  reconnecten automatisch (340/340 verifiziert).
  **CRASH-FALLE `UpdateDrawings` (2026-07-28), Ursache gefunden + behoben in
  0.9.9 (2026-08-03):** Tapir `UpdateDrawings` ({elements:[Drawing-GUIDs]})
  → Archicad stürzt komplett ab (Connection closed, Prozess weg). **Ursache:**
  `UpdateDrawingsCommand::Execute` rief `ACAPI_Drawing_Update_Drawings` ohne
  umgebenden Command-Scope; das Zeichnungs-Update öffnet aber einen
  ODB-Modification-Scope, und `ODB::Database::OpenModificationScope` schlägt
  dann als Assert fehl → BugRep → FATAL. Belegt im Crashreport
  (`BugReporting-29/…-FATAL.rpt`, oberster eigener Frame
  `UpdateDrawingsCommand::Execute`). Ab ELM_SAB 0.9.9 in
  `ACAPI_CallUndoableCommand` gekapselt wie jeder andere ändernde Befehl.
  **Live verifiziert 2026-08-03 am THN** (leere Liste, echte Zeichnung
  unreserviert, echte Zeichnung reserviert — dreimal saubere Antwort statt
  Absturz). Upstream-Tapir hat den ungekapselten Aufruf Stand 2026-08-03
  noch — PR-Kandidat. **Noch offen:** der Befehl meldet `success:false`
  (`-2130312306`) statt zu aktualisieren; liegt NICHT an der Reservierung
  (mit ReserveElements gegengeprüft). Der DevKit-Aufruf braucht vermutlich
  einen bestimmten DB-/Fenster-Kontext — eigenes Arbeitspaket. Bis dahin:
  Zeichnungs-Update weiterhin MANUELL (Layout öffnen → Zeichnungen
  aktualisieren) oder Neuaufbau ⌘⇧R durch den User — aber ohne
  Absturzrisiko bei versehentlichem Aufruf.
  **THN-Praxiswarnung (User, 2026-08-03):** die THN-Zeichnungen sind extrem
  voll — Drawing-Reads und -Updates blockieren Archicad minutenlang und
  sehen wie ein Hänger aus. Vorher ankündigen, Einzelstücke statt
  Massenläufe, „hängt" nie am Antwortverhalten diagnostizieren, sondern an
  Prozess/CPU. Drawing-Elemente sind übrigens auch bei AKTIVEM Layout nicht
  per GetElementsByType lesbar (0 Treffer, offizielle wie Tapir-Variante);
  der verlässliche Weg ist Selektion durch den User + `GetSelectedElements`.
  Und `API.GetTypesOfElements` kennt Drawings nicht (7203) — der Typ-Check
  läuft dort ins Leere. **Merksatz:** Ein Upstream-Befehl, der etwas ändert und
  NICHT in `ACAPI_CallUndoableCommand` läuft, ist ein Crash-Kandidat — beim
  Übernehmen fremder Befehle darauf prüfen (Crashreports liegen unter
  `~/Library/Application Support/Graphisoft/BugReporting-29/`, NICHT in
  `~/Library/Logs/DiagnosticReports` — dort steht zu Archicad nichts). WICHTIG danach: API-gesetzte Änderungen (z. B.
  Zonenstempel via SetDetailsOfElements) erscheinen im Publisher-PDF erst
  nach Neuaufbau/Drawing-Update — Modell-Rücklese allein beweist nicht,
  dass der Plot sie zeigt!
  **Text-Entzerrungs-Pipeline (2026-07-28, THN Sanierungsübersicht):**
  PDF-getriebene Überlagerungs-Korrektur über MEHRERE DBs: (1) Layout-PDF =
  Modell-Viewport + Nagel-Arbeitsblatt-Viewport übereinander; Transform je
  Drawing per String-Matching kalibrieren (Fixskala 1pt=0,0353m bei 1:100,
  Translation per Dichte-Cluster; Referenzmodell-Geist = Δy 350 aussortieren).
  Modell-Viewport am robustesten über ZONENNUMMERN kalibrieren (eindeutig!).
  (2) `GetTextsOfElements` (ELM_SAB) liefert content/location/sizeMm/widthMm/
  angleRad/anchor — Zeilentrenner ist `\r`, NICHT `\n`! (3) Arbeitsblätter sind
  eigene DBs: Inventur+Moves nur bei AKTIVEM Fenster; User klickt Blätter durch,
  Skript erkennt aktive DB an GUID-Schnittmenge und wendet idempotent an
  (verify-first: erst Ist-Lage vs. Snapshot klassifizieren, nie blind
  re-moven!). (4) MoveElements auf AUSGEBLENDETEM Layer = Silent-No-Op mit
  success:true — Layer sichtbar schalten geht NUR manuell (kein API-Befehl).
  (5) **Zonenstempel verschieben**: `SetDetailsOfElements` mit
  `details.typeSpecificDetails.stampPosition` {x,y} (Nesting beachten —
  direkt unter details → 4002!); vorher ReserveElements, Ziel per
  Punkt-in-Polygon in der Zone halten; Referenz-Zonen (y<350) tabu.
  **AddFilesToEmbeddedLibrary-Schema (2026-07-28):** `files` ist ein Array
  von OBJEKTEN `{inputPath, outputPath}` (outputPath = Dateiname in der
  eingebetteten Bibliothek). Eine flache Pfadliste `files:["/pfad.gsm"]`
  ist ein SILENT-NO-OP (`executionResults: []` — leer heißt „nichts
  verarbeitet", nicht „Erfolg"). Verifikation immer per
  GetGDLParametersOfElements auf einer platzierten Instanz (fehlendes
  `parameters`-Feld = Libpart fehlt). Achtung außerdem: Tapir-Reads laufen
  gegen das AKTIVE Fenster — im Layout/Arbeitsblatt liefert
  GetElementsByType fast nichts; vor Inventuren Grundriss aktivieren.
- **Objekt-Vorschau lügt**: Die 2D-Vorschau im Einstellungsdialog rendert
  OHNE Projekt-Attribute — Füllungen erscheinen leer, obwohl sie im Grundriss
  korrekt sind. Füll-/Attribut-Debugging NUR im Grundriss, nie in der Vorschau.
- **Füllung parametrisch halten**: `fill qrfill` (FillPattern-Parameter) statt
  im Skript hartkodiert/ind()-gesucht — Muster ist dann per
  SetAddParsOfElements in Sekunden wechselbar, ohne Bibliothekstausch.
  `ind (FILL, "Name")` funktioniert in GDL zur Laufzeit (Indizes sind
  projektspezifisch!); Attribut-Check per `API.GetFillAttributes`
  (pattern=2^64-1 ⇒ echtes Vollton-Bitmuster). Stifttabellen sind per
  `API.GetPenTableAttributes` lesbar (alle 6 SAB-Tabellen: Stift 1 = schwarz).
- **Eine Datenquelle — assoziatives Etikett**: Label-2D-Skript liest die
  Capmo-Properties des etikettierten Elements live via
  `REQUEST ("ASSOCEL_PROPVALUE", "<Property-GUID>", var)` (Ticket Kurztext /
  Status / Stichwoerter=Capmo-URL); Etikett-Parameter nur als Fallback.
  Nur die QR-Bits bleiben als berechneter Cache am Etikett (aus derselben
  Quader-URL erzeugt). Property-GUIDs via Tapir `GetAllProperties` (Gruppe
  „Capmo Klassifikationen", 14 Properties; Quader-ID-Feld = Ticketnummer).
- **Debug-Zyklus ohne Bibliothekstausch**: Testobjekte unter NEUEM Namen
  hochladen (kein Löschen nötig), platzieren per Tapir `CreateObjects`
  (Feld heißt `objectsData`!), Geschoss nachträglich per `SetDetailsOfElements`
  (`floorIndex` — CreateObjects landet sonst im EG), sichtbar machen per
  `ChangeSelectionOfElements`. Teamwork-Grün im Plan = eigene Reservierung,
  keine Stiftfarbe. Modale Dialoge (Einstellungen, Favorites-Popup) blockieren
  die KOMPLETTE JSON-API (Fehler 4001) — Retry-Schleife einbauen.
- **GDL: Properties des etikettierten Elements lesen** — der Request heißt
  `Property_Value_Of_Parent` (aus Graphisofts „Property Label" abgeschaut;
  „ASSOCEL_PROPVALUE" existiert NICHT): `r = request
  ("Property_Value_Of_Parent", pgid, ty_, d1_, d2_, werte_[])`, Erfolg =
  Rückgabe > 3, Werte im dim-Array. **PROPVAL_*-Konstanten sind
  SKRIPT-definiert** (PROPVAL_STRING=4 usw., Definition im GS-Label) — keine
  Builtins! Undefinierte GDL-Bezeichner sind still 0 → Typvergleich scheitert
  ohne Fehlermeldung. Fundort der Referenz: Annotation Elements.libpack →
  extractpackage → extractcontainer → „Property Label.gsm" → libpart2xml.
- **Etiketten-Design API-tunbar halten**: Property-GUIDs (pgid1..3), Füllmuster
  (qrfill), Stifte (bgpen/whitepen), Textgröße (txtfac) als PARAMETER — dann
  gehen Fixes per SetAddParsOfElements statt per Bibliothekstausch.
  Versions-Iteration über NEUE Libpart-Namen (v2…v7) umgeht das
  Nicht-Überschreiben der eingebetteten Bibliothek. Textgröße
  modell-proportional (`th_ = A * txtfac`, Stilgröße = th_/GLOB_SCALE*1000 mm)
  passt bei jedem Maßstab. AC_*-Markerparameter (z. B. AC_TextPen_1) sind via
  AddPars NICHT änderbar (Rücklese-Verifikation schlägt ehrlich fehl).
- **Teamwork-Falle „Verwerfen"**: Lokale ungesendete Änderungen (Elemente UND
  eingebettete Bibliothek!) verschwinden komplett, wenn der User beim
  Beitreten „Änderungen verwerfen" wählt. Nach jedem Meilenstein SENDEN
  erinnern. Zweite Erkenntnis: Nicht löschbare Elemente (silent no-op trotz
  Reservierungs-Aufforderung) können Reservierungs-Leichen einer
  ABGESTÜRZTEN eigenen Session sein — Verwerfen+Neu-Beitreten räumt sie auf.
  Rollout ist dank Manifest (Quader-GUIDs, URLs, QR-Chunks) + Skripten in
  ~3 min reproduzierbar — Wiederholbarkeit ist die beste Versicherung.
- **Properties schreiben via Tapir `SetPropertyValuesOfElements`**: Items im
  Array sind FLACH `{elementId, propertyId, propertyValue:{value}}` — KEIN
  `elementPropertyValue`-Wrapper (Fehler „elementId is missing" trotz Wrapper
  ist das Symptom; Schema in RFIX/Images/CommonSchemaDefinitions.json
  nachschlagen). Capmo-Status-Abgleich: Tickets via curl (Cursor-Pagination
  `data.after`, `data.items`, total in `data.total`); Matching über
  Ticket-GUID aus der URL (`drawerTicketId=`), Fallback ticket_number =
  Quader-ID-Feld. Bridge-Mapping: Ticket ID←ticket_number, Kurztext←name,
  Stichwoerter←Deep-Link-URL.
- **Klassifizieren: GUIDs IMMER frisch aus DIESEM Projekt** (live-Falle
  2026-07-20): Klassifikations-System-/Item-GUIDs sind projektspezifisch —
  gemerkte GUIDs aus anderem Projekt ⇒ „Failed to set classification item".
  Frisch holen via OFFIZIELLER API (`API.GetAllClassificationSystems` +
  `API.GetAllClassificationsInSystem` — in Tapir NICHT vorhanden). Set via
  Tapir `SetClassificationsOfElements` mit `classificationId:
  {classificationSystemId, classificationItemId}`; die RÜCKLESE-Antwort
  (`GetClassificationsOfElements`) liefert classificationIds-Items dagegen
  FLACH ohne classificationId-Wrapper — Parser entsprechend bauen, sonst
  falsches „0 klassifiziert". Schraffur→Wand-Massenlauf: Dedup gegen Bestand
  GESCHOSS-GETRENNT rechnen (Wand-BBoxen aller Geschosse ⇒ massiv
  überschätzte Abdeckung).
- **Layer-Index → Name auflösen**: Tapir `GetAttributesByType {attributeType:
  "Layer"}` liefert `index` + `name` pro Attribut — die OFFIZIELLE
  API.GetLayerAttributes hat KEIN Index-Feld (Mapping unmöglich). Gleiches
  Muster für andere Attributtypen. KI-Stempel-Praxis: „Hast du X schon
  gezeichnet?" = Property „KI generiert" im Bereich abfragen (Bereich via
  Get2DGeometryOfElements-Stichprobe der Selektion) — live beantwortet in
  Sekunden (1.766 gestempelte Wände gefunden).
- **Bestand-Beschriftung ↔ Elemente abgleichen (Rezept, live: EG-Unterzüge)**:
  Maßtexte der Beschriftungs-Ebene (z. B. „UNTERZUG 60/75", „/40UKD/") via
  GetTextsOfElements + Layer-Filter einsammeln, Regex-parsen (B/H in cm),
  per Distanz zum Element-Mittelpunkt matchen (<6 m), Ist-Querschnitt über
  Properties „Trägerbreite/-höhe (Archicad 20)" lesen (Werte kommen als
  Strings mit DEUTSCHEM Komma!). Tapir ModifyBeams kann KEINEN Querschnitt
  (nur Achse/Level) — bei wenigen Treffern: Abweichler selektieren, User
  ändert im Dialog, danach Property-Rücklesen als Verifikation.
  THN-Befund: 6/23 EG-Träger hatten Default 60/110 statt beschrifteter 60/75.
- **Dopplungs-Sweep 2D (Rezept + Fallen)**: Tapir GetDetailsOfElements liefert
  für Hatch UND Text nur `{"error": "Not yet supported element type"}` und
  Get2DBoundingBoxes lehnt Hatch/Label ab (7203) — ein „0 Duplikate" aus so
  einer Pipeline ist ein BLINDES Null! Funktionierende Quellen: PolyLine-Koordinaten
  via Tapir-Details; Text-Inhalt+Position via ELM_SAB `GetTextsOfElements`;
  Hatch-Polygone via ELM_SAB **`Get2DGeometryOfElements`** (so heißt er —
  nicht „Get2DGeometry"; bei 4010 immer GetName() in den Quellen nachschlagen
  statt einen Neubau anzuwerfen). Duplikat-Schlüssel: Geschoss + gerundete
  (0,1 mm), richtungs-/reihenfolge-normalisierte Koordinaten (+Inhalt bei Text).
  THN-Befund: 395 PolyLine- + 25 Text-Doppler, ALLE im EG (doppelter
  DWG-Import), Schraffuren sauber. Verifikation IMMER als Voll-Neuinventur.
- **Etiketten positionieren: Symbol-Anker = ENDPUNKT, nicht Anfangspunkt!**
  begCoordinate ist nur der Zeigerlinien-Start am Element — wer Positionen auf
  beg verifiziert, bekommt „verifiziert und trotzdem verstreut". Das Symbol
  hängt am endCoordinate mit festem Offset (SAB_QR_Etikett_7: Symbol sitzt
  bullseye-mittig, wenn end = Kubusmitte − (0.125, 0.125); Offset per
  User-Kalibrier-Schleife ermittelt: bekannte Deltas verschieben, User schaut).
  `Get2DBoundingBoxes` unterstützt Labels NICHT (Fehler 7203) — Renderposition
  ist per API nicht messbar, Kalibrierung am lebenden Objekt ist der Weg.
  Umplatzieren IMMER via Tapir `MoveElements` (Element/Assoziation/Params
  bleiben) statt Delete+Create — SAFE-02 gilt auch für Etiketten. Klumpen:
  Greedy-Layout mit Schild-Footprint-Kollision (Ring-Kandidaten 0.7-m-Raster)
  gegen fremde Kuben + platzierte Schilder, User-Handbeispiele als Fixpunkte.
- **Massenersetzung assoziativer Etiketten** (Capmo-Rollout, 338 Stück):
  Tapir `CreateLabels` übernimmt den Etikett-WERKZEUG-Default (Symbol-Etikett
  inkl. Zeigerlinien-Einstellung!); mit `parentElementId` + beg/mid/end des
  ALTEN Etiketts entsteht der Ersatz an exakt gleicher Position, Assoziation
  bleibt. Pipeline: Inventur (Label→Owner-Map) → Batch-Delete alt →
  Batch-Create neu → Batch-Fill → **unabhängige Voll-Inventur als einzige
  gültige Verifikation** (fand 71 stille Delete-Überlebende auf abweichendem
  Layer, obwohl Delete + Stichproben-Check „Erfolg" meldeten).
- **Die Verifikation verifizieren — „blinde Nullen"** *(2026-07-21)*: Auch
  Prüf-Skripte können still fehlschlagen und fälschlich „0 Probleme" melden.
  Erlebte Fälle: (a) `Get2DBoundingBoxes` als Tapir-Befehl aufgerufen — gibt
  4010 (unbekannter Befehl), ein try/except drumherum machte daraus „Element
  gelöscht"; (b) `GetDetailsOfElements` auf Hatch/Text → „Not yet supported",
  als „0 Dopplungen" interpretiert. Regel: VOR dem Zählen die Antwortstruktur
  an EINEM bekannten Positiv-Beispiel prüfen (Befehl existiert? Elementtyp
  unterstützt? Feld heißt wirklich so?). Eine Null ist erst glaubwürdig, wenn
  derselbe Code an einem Beispiel nachweislich eine Eins liefern kann.
- **Bestand-Aufräum-Endspiel (Rezepte)** *(2026-07-23)*: (1) Zeigerlinien:
  Text ohne Symbol daneben → Linienkette ab Textanker verfolgen (Hops ≤0,35 m),
  Ende = wahre Durchbruch-Position; Kettenverfolgung ohne Hop-Limit driftet
  aber durch fremde Linienzüge — max. Hops UND Ziel-Radius begrenzen, jede
  Umsetzung als „bitte Stichprobe" in die Inventur. (2) Layer-Waisen per
  NACHBARSCHAFTS-VOTING einsortieren: Mehrheits-Layer der Elemente im
  0,5-m-Raster ringsum erbt; ohne Nachbarn → A_04_BESCHRIFT + Inventur.
  (3) Durchbruch-Symbol-Objekte zeigen sich GESCHOSSABHÄNGIG auch im
  Geschoss darunter (symb_text_2!) — kein Bug, sondern BD/DD-Zwilling;
  User dazusagen, bevor er sich wundert. (4) Embedded-Library-Items sind
  per Tapir NICHT löschbar (nur AddFiles/Reload) — Aufräumen bleibt
  Bibliothekenmanager-Handarbeit.
- **Symbol↔Text-Matching: nur global 1:1 und layergleich** *(2026-07-23)*:
  Naives „nächstes Symbol pro Text" erzeugt Mehrfachbelegung (256 Symbole
  mit 2–3 Objekten) und Fehlzuordnungen über Gewerke hinweg. Richtig:
  ALLE Symbolkandidaten (Hatch/PolyRect/Kreis/Linien-Rechteck) einsammeln,
  vorher POSITIONSWEISE DEDUPLIZIEREN (dasselbe gezeichnete Symbol taucht
  als Hatch UND Linienrechteck auf → sonst doppelte Objekte), dann
  Paare (Text, Symbol) nach Distanz sortieren mit starkem Bonus für
  Layer-Gleichheit und GREEDY 1:1 vergeben. Textlose Zwillings-Symbole
  erben ID/Attribute vom layergleichen Nachbartext (User-Prinzip: „Objekt
  kopieren, nicht ID erfinden"). Objekte ohne Zeichnungstiefe: WD-Plantiefe
  Standard 0,25 m — NIE die Lochhöhe als Tiefe stehen lassen (User sieht
  3-m-Rechtecke quer überm Flur). Höhen-Annotationen (OK=UKD, UK=… ü.FFB)
  gehören NUR an WD/WS — an BD/DD sind sie physikalisch sinnlos.
  User-Hand-Kopien ins eigene Objektregister übernehmen (Dedupe-Schutz).
- **SetGDLParametersOfElements: Paket-Totalausfall + Options-Parameter** *(2026-07-23)*:
  Ein einziger ungültiger Parameter im gdlParameters-Array lässt ALLE
  Parameter des Elements still scheitern (1.140 von 1.602 Objekten defekt,
  weil `symb_text_1='Eigene'` als String mitgeschickt wurde — auch A/B
  fielen mit um). Regeln: (1) Options-/Dropdown-Strings über ihren
  **Integer-Zwilling** setzen (`iSymbText1=0` = „Eigene", dann den
  _short-Freitext); (2) Booleans als echtes `true/false`, NIEMALS `1/0`
  (wird still ignoriert); (3) Numerik und Beschriftung in GETRENNTEN Calls;
  (4) Parameternamen sind PRO LIBPART verschieden (Deckendurchbruch:
  `symb_text_1_*`, Bodenschlitz: `symb_text_*` ohne _1) — vor Batch je
  Objekttyp einen Kalibrier-Einzeltest mit Read-back fahren, erst dann
  skalieren. Verifikation nur elementweise über GetGDLParameters.
- **WD im Grundriss: Text B/H ≠ Zeichnung B/T** *(2026-07-23)*: Beim
  Wanddurchbruch beschreibt der Text Breite × HÖHE (Z-Achse, im Plan
  unsichtbar), die Zeichnung zeigt Breite × WANDTIEFE. Fürs Symbol-Objekt:
  A = Breite (Text), B = gezeichnete Tiefe, ZZYZX = Höhe (Text), Höhenlage
  aus Annotationen wie „OK=UKD" (als symb_cust_text übernehmen). Maß-
  Vergleich Text vs. Zeichnung ist bei WD/WS nur für die BREITE aussagekräftig.
- **Durchbruch-Objekt-Massenlauf (1.038 Stück, Rezept)** *(2026-07-23)*:
  Texte (Grammatik ^-verankert) liefern ID+Maß, die gezeichnete Symbol-
  Schraffur (nächste kleine Hatch ≤1,5 m, geschossgleich) liefert die
  Position; Text-Maß gewinnt bei Konflikt, Abweichungen >5 cm in
  Inventur-CSV. Kette pro Objekt: CreateObjects (`objectsData`,
  `libraryPartName` reicht!) → **SetGDLParametersOfElements** für A/B
  (ELM_SAB SetAddPars scheitert ehrlich — A/B sind KEINE AddPars) →
  SetDetailsOfElements (layerIndex; `id` wird dort NICHT akzeptiert) →
  Element-ID als BuiltIn-Property `General_ElementID` via
  SetPropertyValuesOfElements → Klassifikation → Stempel.
  **Reihenfolge zwingend: erst klassifizieren, DANN Property setzen** —
  klassifikationsgebundene Properties (KI-Stempel!) verweigern sonst mit
  „Failed to set property value" (erklärt rückwirkend ungestempelte
  Openings). Objekt-Anker = linke UNTERE Ecke: für Zentrierung Ursprung =
  Symbolmitte − (A/2, B/2), sonst sitzt alles versetzt (User sieht es
  sofort). Verfügbare THN-Objekte: „Bodendurchbruch Symbol",
  „Deckendurchbruch Symbol", „Bodenschlitz" — KEINE Wand-Varianten als
  Objekt (Wanddurchbruch/Wandschlitz existieren nur als Fenster-LibParts;
  Mapping WD→Deckendurchbruch, WS→Bodenschlitz, ID trägt die Wahrheit).
  LibPart-Suche: platzierte via GetDetailsOfElements→details.libPart.name;
  nicht platzierte via binärem grep in den TW-Library-Caches
  (`~/Library/Application Support/Graphisoft/TW Daten/…/LIBR_*`).
- **Get2DGeometryOfElements: Antwortfeld heißt `geometryOfElements`** *(2026-07-23)*:
  NICHT `geometries`. Ein `.get('geometries', [])` liefert leer → zip läuft
  ins Leere → 0 Treffer bei 500k Linien = blinde Null (zwei Scans betroffen,
  darunter ein bereits als „belastbar" verkaufter Rechteck-Scan). Antwort-
  Feldnamen IMMER erst an einem Element proben; nach Korrektur `assert
  len(geo) == len(batch)` in den Loop einbauen.
- **AutoCAD-Textrahmen-Erkennung (Rezept)** *(2026-07-23)*: Bestand-DWGs
  hinterlassen Textrahmen als (a) 4 lose achsparallele Linien mit gemeinsamen
  Ecken, (b) geschlossene 4/5-Punkt-Polylinien. Erkennung: kurze Linien
  (<2 m, achsparallel) → Ecken-Hash (4-mm-Raster) → Rechtecke rekonstruieren;
  Polys: bbox-Kantentest. Klassifizierung über Textanker-in-Rechteck. ABER:
  Layer-Whitelist ist Pflicht — dieselben Signaturen sind anderswo ECHTE
  Inhalte (0,15er-Pflasterraster, 0,6×0,6-Stützenquerschnitte MIT Text,
  Bestuhlung, Planköpfe). Nur Beschriftungs-/BS-Layer freigeben, Grenzfälle
  dem User layerweise selektieren. Größenlimit großzügig (bis 2 m — 1,4-m-
  Rahmen existieren), User-Beispiele iterativ als neue Signaturen aufnehmen.
- **Durchbruch-Grafik = Zusammenhangs-Klumpen, nicht Radius-Match** *(2026-07-23)*:
  Zu einem Durchbruch-Text gehören Kreuz-Polylinien, Schraffuren, kurze
  Linien UND die Verbindungslinie zwischen zwei Durchbrüchen — ein 1-m-Radius
  um den Textanker trifft davon fast nichts (1/9 im User-Beispiel). Richtig:
  alle 2D-Typen der Quell-Layer einsammeln, Punkte auf 10-cm-Raster hashen,
  Union-Find über Berührung, dann ganze Gruppe dem Text zuordnen, der sie
  berührt. Ergebnis 4.559 statt 3.078 Elemente. User-Selektion als Vorher-
  Gegenprobe nutzen (8/9 abgedeckt = ok, das 9. war der Text selbst).
- **Cross-Geschoss-Löschen via Selektion**: ChangeSelectionOfElements wirkt
  über alle Geschosse; der User kann mit EINEM Entf die gesamte selektierte
  Kandidatenliste löschen (420 über 6 Geschosse) — schneller als geschoss-
  weises API-Löschen mit Reservierungs-Pingpong. Nulls danach IMMER mit
  Positiv-Kontrolle absichern (safe_details an bekannt existierenden GUIDs).
- **Tapir RotateElements ist bei Stützen ein stiller Blindgänger** *(2026-07-23)*:
  meldet Erfolg, ändert `axisRotationAngle` aber nicht (drei Läufe, 0/21).
  Diagnose per Mikro-Test: dieselbe Stütze ließ sich per MoveElements 1 mm
  bewegen (und zurück) → Reservierung/Schreibrechte OK, der Befehl selbst
  greift nicht. Lösung: eigener Befehl `ELM_SAB SetColumnRotation`
  (absoluter Zielwinkel in Grad, `ACAPI_Element_Change` mit Maske auf
  `axisRotationAngle`, eingebaute Rücklese-Verifikation) — 21/21 auf Anhieb.
  Merksatz: Bei stillen No-Ops IMMER erst Reservierung vs. Befehl per
  Mikro-Test (1-mm-Move + Rückzug) trennen, bevor man den User erneut
  reservieren schickt.
- **Teamwork-Neustart reset­tet Reservierungen** *(2026-07-21)*: Nach
  Archicad-Crash/-Neustart sind ALLE Reservierungen der Session weg — zuvor
  „reservierte" Schreibaktionen werden wieder zu stillen No-Ops (14/86
  Rechtecke überlebten drei „erfolgreiche" Delete-Läufe). Nach jedem Neustart:
  User neu reservieren lassen, dann Voll-Re-Inventur. Hängt Archicad bei
  einem Delete/Timeout (99 % CPU), gilt die Aktion als NICHT ausgeführt, bis
  die Inventur das Gegenteil beweist.

## Teamwork-Reservierung per API <!-- 2026-07-21 -->

Tapir hat `TapirCommand ReserveElements {elements:[{elementId:{guid}}]}` — löst das Silent-No-Op-Problem bei Delete/Move auf unreservierte Elemente direkt per API, ohne manuellen Rechtsklick→Reservieren. Ablauf: ReserveElements → Schreiboperation → unabhängiger Read-back (z. B. `ELM_SAB Get2DGeometryOfElements`: success=false ⇒ gelöscht). Live-verifiziert THN 2026-07-21 (27 Beschriftungs-Rechtecke). Read-back bleibt PFLICHT — auch ReserveElements meldet success ohne Garantie.
- **Reservierung ist flüchtig**: `ReserveElements` muss UNMITTELBAR vor der Schreibaktion im selben Ablauf stehen — ein Reserve aus einem früheren Call reicht nicht, SetDetailsOfElements liefert dann „Failed to change element" (-2130312912). Muster: pro Batch Reserve→Write→Read-back. Layer-Wechsel für 2D-Elemente (Line etc.) via `SetDetailsOfElements {elementsWithDetails:[{elementId,details:{layerIndex}}]}` funktioniert typunabhängig (live-verifiziert THN 2026-07-21, 442 Linien). **Korrektur 2026-10-14:** -2130312912 ist laut DevKit 29 `APIERR_HIDDENLAY` (`APIErrorStart + 304`, `APIdefs_ErrorCodes.h` Z. 171) = Element liegt auf einer **ausgeblendeten Ebene**. Bei diesem Code zuerst `GetLayerAttributes … isHidden` prüfen; die anderen hier genannten Ursachen können zusätzlich vorliegen, erzeugen aber nicht diesen Code (Hinweis S., Fliesenspiegel Futurelab).
- **ELM_SAB 0.9.2 (2026-07-21)**: `SetAddParsOfElements` unterstützt jetzt auch **Zonen** — Raumstempel-GDL-Parameter (Schriftgrößen, bShow*-Schalter, Zeilenreihenfolge iOrder_*, Rahmenstil) per API setzbar, mit eingebauter Rücklese-Verifikation. Array-Parameter (orders_100/50) sind nicht setzbar, werden aber aus den iOrder_*-Skalaren abgeleitet. Workflow Stempel-Design übertragen: GetGDLParametersOfElements auf Referenz + Ziel, Diff bilden, ROOM_*/apartmentName/asAddon_st*/gs_ui_*-Content-Parameter ausschließen, Rest via SetAddParsOfElements (live-verifiziert THN: 115 Stempel).
- **`ReserveElements` meldet Fremd-Reservierungen** im Feld `conflicts[]` (je `{elementId, user:{userId,userName}}`) — das Batch-`executionResult.success=true` bezieht sich nur auf die Anfrage, nicht auf den Erfolg. conflicts IMMER auswerten; Elemente in conflicts bleiben bei Move/Delete/Change still liegen (Teamwork-Silent-No-Op-Ursache identifiziert, live THN 2026-07-21: 287 von Kollegin gehaltene Elemente).
- **ELM_SAB 0.9.4 (2026-07-21)**: `SetColumnDetails {elements:[{elementId, coreWidth?, coreHeight?}]}` — Stützen-Kernmaße schreibbar (columnSegments-Memo, alle Segmente, eingebaute Rücklese-Verifikation). Workflow Bohrpfahl↔DWG-Kreis: 21-Punkt-Polylinien = polygonisierte DWG-Kreise, Durchmesser aus BBox-Mittel; Match Stütze↔Kreis über Zentroid <0,3 m (live THN: 89 Pfähle auf Ø 0,75). 0.9.3-Felder in GetTextsOfElements: angleRad/penIndex/anchor/widthMm.
- **ELM_SAB 0.9.5 (2026-07-21)**: `CreateCurtainWallFromAxes {begCoordinate, endCoordinate, floorIndex, bottomOffset, columnWidths[], rowHeights[] (von unten), opaqueRows[], cellOrderColumnMajor?}` — CW mit FixedSizes-Pattern per API. Frames/Panels aus CW-Werkzeug-DEFAULTS (User vorher Favorit mit 2 Panel-Klassen setzen: Klasse 1 = Glas, Klasse 2 = opak für opaqueRows). Workflow Fassaden-Nachbau: 2D-Pfostenachsen aus vertikalen Kurzlinien clustern (Toleranz 3 cm) → Feldbreiten; Zeilenhöhen aus Foto/bestehender CW; Glasebene ggf. per MoveElements y-bündig schieben (Create setzt Panels je nach Default-Offset ~8 cm versetzt). Live THN Südfassade OG: 7×3 Felder, 21 Panels verifiziert.
- **ELM_SAB 0.9.7 (2026-07-25)**: `CreateCurtainWallFromAxes` kann **nichtrechteckig** — `topProfile[]` (genau `columnWidths+1` Oberkantenhöhen an den Pfostenachsen → Trapez/Giebel/Pultschräge) oder `contour[{u,v}]` (freies Polygon, umlaufend, Schlusspunkt nicht wiederholen). Koordinaten sind **segmentlokal**: `u` = Lauflänge ab `begCoordinate`, `v` = Höhe über CW-Unterkante. Umgesetzt über `memo.cWSegContour` (`API_CWContourData`, Polygon-Konvention `coords[1..nCoords]` mit `coords[nCoords] == coords[1]`, `pends[1] = nCoords`); Konvention aus DevKit-Beispiel `Examples/CurtainWall_Drawer` abgeleitet, das ein Hatch-Polygon um −minX/−minY verschiebt und als Kontur setzt. `height` = max(v); rowHeights bleiben FixedSizes-Muster und wiederholen sich bis zur Firsthöhe. Symmetrischer First braucht **gerade** Spaltenzahl. Antwort liefert jetzt `height`, `contourVertices`, `bottomOffsetApplied`, `zMin`, `zMax`. Live THN Fassade-013: 4×4 mit Giebel → 14 statt 16 Paneele, Traufe 17,50 / First 19,65.
- **`bottomOffset` war bis 0.9.6 ein stiller No-Op (2026-07-25)**: `curtainWall.storyRelLevel` wird beim `ACAPI_Element_Create` ignoriert — CW landet ohne Fehlermeldung exakt auf Geschossniveau. Ab 0.9.7 per `APIEdit_Drag` im selben Undo-Schritt nachgezogen, mit `zMin`/`zMax` aus `ACAPI_Element_CalcBounds` in der Antwort. **Immer gegenprüfen.** Bei älterem Add-On: `MoveElements` mit `dz` hinterher.
- **CW-Bbox-Fallen (2026-07-25)**: `Get3DBoundingBoxes` liefert für CW-**Paneele geschossrelative** z-Werte, für das **Top-Level absolute** — beim Vergleichen umrechnen. Und: sitzt eine CW mehrere Geschosse über ihrem `floorIndex`, ist die Top-Level-Bbox in x **degeneriert** (`xMin == xMax`); sieht kaputt aus, ist nur ein Melde-Quirk — Verifikation über die Sub-Elemente.
- **Kein `ModifyCurtainWall` (2026-07-25)**: Raster/Kontur einer bestehenden CW sind per API nicht änderbar. Rasteränderung = neu bauen + altes löschen; einzige begründete Ausnahme von „Ersetzen = umbauen". Reihenfolge: **erst neu bauen und verifizieren, dann altes löschen** — kein Loch im Modell, falls der Create scheitert.
- **Fassaden-Vollausbau aus 2D (live THN 2026-07-21, 90 Elemente)**: Layer A_05_FASSADE → Linien clustern (Union-Find, BBox-Nähe) → pro Zug Pfosten = Kurzlinien senkrecht zur Fassade (Achs-Cluster 3 cm) → Sektionen = Achsfolgen mit Feldern 0,5–2,6 m (Pfeilerzonen trennen) → Glasebene = Linienpaar 3–9 cm Abstand nahe Pfostenebene → je Sektion CreateCurtainWallFromAxes pro Geschoss + CreateWalls-Brüstungsband. PERFORMANCE: >10 CWs ⇒ Archicad regeneriert minutenlang (API nur IsAlive) — alle Folge-Ops in 6er-Chunks mit 15-s-Retry; Selektion von CWs triggert denselben Rebuild. Flächige Gitter (Glasdach) NICHT als Fassade bauen.
- **Reihenfolge-Lektion Fassaden-Pipeline (2026-07-21)**: ERST Layer sanieren (fehlplatzierte Fassadenlinien via User-verifizierter Selektion auf A_05_FASSADE), DANN Muster parsen und bauen — sonst zerfallen durchgehende Fronten in Scheinabschnitte (Hof-Ost: 4 Kurzstücke mussten nach Layer-Umzug gegen 1 durchgehende Front ersetzt werden). Muster-Scans über alle Layer produzieren massives Rauschen (Bemaßung, Wandkappen, Fremd-/Referenzzonen ±350 m) — Kandidaten IMMER erst selektieren und vom User bestätigen lassen.
- **Silent-No-Op-Ursache #3: GRUPPEN (2026-07-21)**. Elemente in Gruppen verweigern Einzeländerungen (SetDetails/Delete: „Failed to change element" -2130312912 bzw. still wirkungslos), solange „Gruppierung unterbrechen" (Alt+G) AUS ist — kein Reservierungskonflikt, kein Hotlink, kein Lock. Diagnose-Reihenfolge bei wirkungslosen Writes: (1) ReserveElements-conflicts[] prüfen (Fremd-Reservierung), (2) UnlockElements testen, (3) GetHotlinks (leer ⇒ kein Hotlink), (4) User „Gruppierung unterbrechen" einschalten lassen → Retry. Live THN: 98 gruppierte Höhenkoten-Symbole erst nach Alt+G änderbar; erklärt rückwirkend auch die „unlöschbaren" Alt-Etiketten. **Korrektur 2026-10-14:** -2130312912 ist laut DevKit 29 `APIERR_HIDDENLAY` (`APIErrorStart + 304`, `APIdefs_ErrorCodes.h` Z. 171) = Element liegt auf einer **ausgeblendeten Ebene**. Bei diesem Code zuerst `GetLayerAttributes … isHidden` prüfen; die anderen hier genannten Ursachen können zusätzlich vorliegen, erzeugen aber nicht diesen Code (Hinweis S., Fliesenspiegel Futurelab).

## Mehrversionsbau: AC27/28/29 aus einer Quelle <!-- 2026-07-29 -->

Seit 2026-07-29 baut die CI (`.github/workflows/build-elm-sab-tapir.yml`) alle sechs
Kombinationen aus AC27/28/29 und Mac/Windows in die rollende Vorabversion
`elm-sab-tapir-latest`. Lokal ist der Build auf dem SAB-Mac **nicht** möglich: es fehlen
cmake und das vollständige Xcode (nur Command Line Tools installiert). Wer bauen will,
pusht — oder nimmt `workflow_dispatch`.

**Die Versionsstaffelung, die man kennen muss** (steht in `Tools/CMakeCommon.cmake`, greift
automatisch — aber sie bestimmt, was man in eigenen Befehlen schreiben darf):

| | AC27 | AC28 | AC29 |
|---|---|---|---|
| C++-Standard | C++17 | C++17 | C++20 |
| MSVC-Toolset | v142 | v142 | v143 |
| DevKit-Build (upstream-erprobt) | 27.3001 | 28.3001 | 29.3000 |

Konsequenz für neue ELM_SAB-Befehle: **keine C++20-Sprachfeatures** (keine ranges, kein
`std::span`, kein `.starts_with`, keine designated initializers) — sonst bricht der
AC27/28-Build, während AC29 grün bleibt. Der Windows-Runner ist auf `windows-2022`
gepinnt, weil neuere Images v142 nicht mehr mitliefern; das ist kein Versehen, sondern
dieselbe Entscheidung, die Upstream-Tapir 2026-05 getroffen hat. macOS braucht keinen
Split (ein `-G Xcode` für alle), ab AC26 baut CMakeCommon universal für x86_64+arm64.
Nicht versionsabhängig: die MDID (eine Registrierung gilt für alle Versionen) und
`CompileResources.py` (lässt die Dark-Mode-Icons unter AC29 selbst weg).

DevKit-URLs bewusst gleich wie Upstream, nicht die neuesten. Neuer wären 27.6003 /
28.4001 / 29.3100 (in `ELM_SAB_Add-On/Tools/APIDevKitLinks.json` schon hinterlegt), aber
für diesen Quellstand unerprobt. Achtung falls doch gewechselt wird: 29.3100 enthält
**keinen LP_XMLConverter** mehr — für GSM-Arbeit weiterhin 29.3000 ziehen.

**Die AC28-Bruchstelle `textContent`** (kostete den ersten Fehlschlag): In Archicad 28 ist
`API_ElementMemo::textContent` von einem `char**`-Handle auf `GS::UniString*` umgestellt
worden (offizielle AC28-Release-Note). Wer direkt zugreift, baut gegen DevKit 27 nicht.
Tückischer ist der Lesepfad: `item.Add ("content", *memo.textContent)` **kompiliert unter
AC27 fehlerfrei**, bindet an die `const char*`-Überladung und liefert leeren oder falschen
Text — obwohl mit `APIMemoMask_TextContentUni` gelesen wurde. Ein Fehlschlag, der sich als
Erfolg tarnt. Beide Richtungen laufen jetzt über `GetMemoTextContentELM` /
`SetMemoTextContentELM` in `Sources/ELMCommandBase.hpp`; das Muster für den AC27-Zweig
steht in Tapirs `SetTextContentAndParagraphs` (ElementCreationCommands.cpp) und baut dort
für AC25–29. **Merksatz: Bei jedem neuen Befehl, der ein Memo-Feld direkt anfasst, gegen
die AC28-Release-Note prüfen** — die Doxygen-Membertabellen der DevKit-Repos zeigen pro
Version den echten Feldtyp.

**CI-Ergebnis aus einem PRIVATEN Repo ohne Token lesen.** Actions-Logs und Release-Assets
sind von außen nur mit Token erreichbar. `gh` liegt inzwischen unter `~/.local/bin/gh`, ist
aber **bei keinem Host angemeldet** (`gh auth status` → „not logged into any GitHub hosts") —
ein installiertes `gh` ist also kein Zugang, und Anmelden ist Nutzersache.
Lösung: der Publish-Job schreibt seine Bilanz als Waisen-Commit auf einen eigenen Zweig,
der per SSH erreichbar ist:

```bash
git fetch origin ci-status  && git show FETCH_HEAD:status.md      # welche 6 Builds liefen
git fetch origin ci-release && git show FETCH_HEAD:release-inventory.txt  # was hängt am Release
```

**Seit 2026-08-03 liegen auch die gebauten Bundles dort** (`bundles/`), weil ein
Bundle-Tausch sonst am Token scheitert und über den Browser laufen müsste:

```bash
git fetch origin ci-status
git show origin/ci-status:bundles/ELM_SAB_Tapir_AC29_Mac.zip > /tmp/elm.zip
```

Der Zweig wird als Waisen-Commit force-gepusht und wächst darum nicht mit jedem Lauf.

Der Zweig liegt außerhalb der `paths`-Filter und löst darum keinen neuen Lauf aus; die
Build-Logs wandern gleich mit, damit ein Fehlschlag ohne User diagnostizierbar ist.
Zwei Fallen dabei, beide selbst erlebt:

- **`ncipollo/release-action` verschiebt einen BESTEHENDEN Tag nicht** — auch nicht mit
  `commit:`, das wirkt nur beim Anlegen. Ein rollender Tag ist also **kein** Beleg dafür,
  dass der aktuelle Lauf durchgelaufen ist. Wer das als Erfolgssignal nimmt, wartet ewig.
- **Erfolgsprüfung per grep auf „fehlgeschlagen" schlug falsch an**, weil derselbe Text im
  Erklärsatz des Release-Bodys stand („Zu jeder fehlgeschlagenen Version liegt das Log
  bei"). Deshalb nennt die Inventur pro erwarteter Datei ausdrücklich „vorhanden" oder
  „FEHLT", statt eine Liste zum Selbstnachzählen zu liefern. Gegenstück zu den „blinden
  Nullen" weiter oben: hier war es ein **falscher Alarm** statt einer falschen Null — die
  Regel „Antwortstruktur an einem bekannten Positivbeispiel prüfen" gilt in beide
  Richtungen.

Ein `-T v142`-Build ist übrigens auch der Grund, warum AC27/28-Windows-Logs winzig sind
(~2 KB) gegenüber macOS (~320 KB): MSVC ist wortkarg. **Loggröße taugt als
Plausibilitätsprüfung nur im Vergleich derselben Plattform** — AC27-Mac sprang beim Fix von
53 KB (Abbruch) auf 327 KB (voller Durchlauf) und lag damit gleichauf mit AC28/29.

## Welcher Build läuft eigentlich? — `ELM_SAB.GetAddOnVersion` <!-- 2026-08-03 -->

`TapirCommand.GetAddOnVersion` meldet `ADDON_VERSION` aus `Sources/AddOnVersion.hpp` — das
ist die Version des **Tapir-Unterbaus** (1.5.4) und bleibt gleich, egal welcher ELM_SAB-Stand
im Bundle steckt. Zusammen mit dem **rollenden** Tag `elm-sab-tapir-latest` war damit am
laufenden Archicad nicht feststellbar, ob das installierte Bundle aktuell ist: ein vier Tage
altes Bundle antwortete identisch zu einem frischen. Genau das ist die Update-Stolperfalle
weiter oben, nur ohne die Zweit-Instanz als Ausrede.

Seit 0.9.9 gibt es `ELM_SAB.GetAddOnVersion` → `{version, tapirBaseVersion, buildStamp}`.
`version` ist `ELM_SAB_VERSION`, `buildStamp` ist `__DATE__ " " __TIME__` des Builds und
identifiziert den konkreten Bundle-Build auch dann, wenn die Versionsnummer mal vergessen
wird. **Nach jedem Bundle-Tausch als erstes abfragen** — das ist die Rücklese für die
Installation selbst.

Pflicht bei jeder Änderung an ELM_SAB-Befehlen: `ELM_SAB_VERSION` hochziehen und die
`RegisterCommand`-Versionsangabe des geänderten Befehls mitziehen.
`ELM_SAB.GetAddOnVersion` existierte vorher NICHT — ein `4010` auf diesen Namen ist also
kein Beleg dafür, dass der Namespace fehlt (Gegenprobe: irgendein echter ELM_SAB-Befehl).

## ELM_SAB-Kandidaten aus der Vorlagen-Session <!-- 2026-07-30 -->

Der Vorlagen-Umbau hat die verbliebenen API-Lücken scharf abgegrenzt. Nach der Hausregel
(**Schreiboperationen als eigene ELM_SAB-Befehle mit Rücklese-Verifikation; Tapir nur für
Reads und Selektion**) sind das die Kandidaten — nach Nutzen sortiert. Jeder Eintrag nennt
die Lücke, den vermuteten DevKit-Weg und den Verifikationsstand.

| # | Befehl (Arbeitsname) | Schließt diese Lücke | DevKit-Weg | Stand |
|---|---|---|---|---|
| 1 | `GetProfileBuildingMaterialsELM` | Baustoffe in **Profilen** sind per Tapir nicht auslesbar ⇒ jeder „unbenutzt"-Befund bleibt unvollständig, Löschen muss in den Attributmanager | `ACAPI_Attribute_GetDefExt` → `API_AttributeDefExt.profile` (ProfileVectorImage), Baustoff-Indizes der Schichten auslesen | <!-- VERIFY --> Feld existiert, Auslesepfad noch nicht gebaut |
| 2 | `SetElementDefaultELM` | `favorites_apply_favorites_to_element_defaults` scheitert bei **Dach** (`-2130313114`); Defaults sind sonst nur per UI-Doppelklick setzbar | `ACAPI_Element_GetDefaults` / `ACAPI_Element_ChangeDefaults` je `API_ElemTypeID`, inkl. Klassifizierung und Ebene | <!-- VERIFY --> API dokumentiert, Roof-Sonderfall zu prüfen |
| 3 | ~~`CreatePropertyDefinitionELM`~~ **ENTFÄLLT** | Tapir 1.5.4 kann es bereits: `properties_create_property_definitions` mit `possibleEnumValues` **und** `availability` (Classification-Item-GUIDs), dazu `properties_create_property_groups`. Live verifiziert 2026-08-04 (17 Properties, Rücklese 177/177) | — | **erledigt, kein Eigenbau nötig** |
| 3b | `ModifyPropertyDefinitionELM` | **das** ist die echte Lücke: es gibt nur create/delete, kein Modify. Verfügbarkeit einer bestehenden Property nachzuziehen erzwingt sonst Delete+Create (SAFE-02, und GDL-`Property_Value_Of_Parent`-Referenzen brechen an der neuen GUID) | `ACAPI_Property_ChangePropertyDefinition` | <!-- VERIFY --> Bedarf konkret: 18 Capmo-Properties ohne Availability in der Bürovorlage |
| 4 | `GetLibraryPartsELM` | `library_get_available_library_parts` ist **defekt** (immer leer, hoher `skippedCount`) ⇒ eingebettete Teile sind nur im UI prüfbar | `ACAPI_LibraryPart_GetNum` + `ACAPI_LibraryPart_Get`, Filter auf eingebettete Bibliothek | <!-- VERIFY --> Standardpfad, sollte unkritisch sein |
| 5 | `SaveProjectAsELM` | **Sichern / Sichern unter (.tpl)** hat keinen Endpoint ⇒ eine Stunde API-Umbau bleibt ungesichert im RAM | `ACAPI_ProjectOperation_Save` mit `API_FileSavePars` (Format `.pln` / `.tpl`) | <!-- VERIFY --> heikel: Zielpfad-Validierung und „Original nicht überschreiben" gehören in den Befehl |
| 6 | `GetIFCTranslatorSettingsELM` | der **Typ-Zuordnungsbaum** des IFC-Übersetzers ist weder lesbar noch schreibbar ⇒ der wirksamste Vorlagen-Fix bleibt UI + Vorher/Nachher-Probe | IFC-Manager-Funktionen des DevKit (`ACAPI_IFC_*`) | <!-- VERIFY --> unklar, ob der Zuordnungsbaum überhaupt exponiert ist; **erst lesen können, dann über Schreiben nachdenken** |

Reihenfolge-Empfehlung: **1, 2, 4** zuerst — kleine, klar umrissene Befehle, die je eine
dokumentierte Lücke schließen und sich mit einer Rücklese sauber verifizieren lassen.
**3 und 5** danach, weil sie Projektzustand verändern und eine Absicherung im Befehl selbst
brauchen. **6** ist ein Forschungsauftrag, kein Bauauftrag: solange nicht belegt ist, dass
der Baum lesbar ist, bleibt der UI-Weg der richtige — und `dev_get_ifc_type_of_elements`
(Tapir, Read) ist die Erfolgskontrolle dafür.

Was **nicht** nach ELM_SAB gehört, weil Tapir es kann: Favoriten-Round-Trip, Ebenen löschen,
Ebenenkombinationen, Baustoffe anlegen, IFC-Export, Klassifizierung setzen. Vor jedem neuen
Befehl also erst zwei Discovery-Queries — die Lückenliste veraltet mit jedem Tapir-Release.

## ELM_SAB 0.9.12/0.9.13 — CaptureView, Paletten-Rückfrage, arc-Ausbau <!-- 2026-08-06 -->

Aus dem revit-mcp-python-Vergleich (Abwägung: `~/.scratch/elmonkey/pyrevit-vergleich.md`):

- **`ELM_SAB.CaptureView`** speichert das aktive Fenster (2D/3D) als PNG — die visuelle
  Rücklese für Agenten (Highlight → FitInWindow → CaptureView → PNG lesen). Zwei Fallen,
  beide beim Schreiben gefunden: `API_SavePars_Picture` zero-initialisiert wäre
  `APIColorDepth_BW` (Schwarzweiß!) — explizit `APIColorDepth_FromSourceImage` setzen;
  und `ACAPI_ProjectOperation_Save` mit Parametern existiert erst ab AC27 nativ
  (MigrationHelper shimt nur die parameterlose Variante, betrifft uns nicht).
  Seit 0.9.13 mit Datei-Existenz-Rücklese via `IO::fileSystem.Contains` — NoError vom
  Save allein beweist nichts. **Live-Test am Modell steht aus** (Bundle noch nicht eingebaut).
  **Korrektur 2026-10-13 (0.9.25):** `APIColorDepth_FromSourceImage` war selbst falsch. Es ist nur für die Bildkonvertierung definiert, beim Fenster-Export gibt es kein Quellbild → Save lieferte APIERR_BADID, CaptureView hat nie ein Bild geliefert. Seit 0.9.25 gilt die Echtfarbtiefe je Plattform (`MiC`/`TC24`) mit Rückfall `256C`, siehe Abschnitt „ELM_SAB 0.9.25“ unten. **Live verifiziert erst mit 0.9.25 (WiSchu 13.10.).**
- **Paletten-Rückfrage** (TapirPalette::ExecuteScript): Sicherheitsdialog NUR für
  `UnusedViewCleaner.py` (Nutzer-Auftrag nach Vorfall 2026-08-06; Abbrechen = Default-Button).
  0.9.12 hatte zusätzlich einen generischen Dialog vor jedem Skript — als unbeauftragter
  Scope-Creep im Review entfernt. Bewusste Schwäche: Erkennung per Dateinamen-Präfix;
  benennt Upstream das Skript um, läuft es wieder ungefragt.
- **`arc` neu:** `launch` (Start per `open -na` + Port-Polling bis Bridge antwortet — nutzt
  nebenbei den Doppelklick-Start-Bug ab), `view-elements` (GetCurrentWindowType →
  GetAllElements mit OnVisLayer+OnActFloor bzw. In3D; auf Schnitt/Ansicht/Arbeitsblatt nur
  Näherung, wird in der Ausgabe gekennzeichnet), `splash` (Property-Werte → HighlightElements
  mit Okabe-Ito-Palette + Legende; Overlay-only, `--clear` ist Pflicht-Cleanup).
- **Bugfund im Review, live bestätigt:** `API.GetSelectedElements` existiert in der
  offiziellen JSON-API NICHT (Fehler 2002) — es ist ein Tapir-Befehl. `arc selected` hatte
  ihn seit v1.1 über `post()` aufgerufen und meldete deshalb still „(0 selected)", egal was
  selektiert war. Merksatz: Ein leeres Ergebnis an einem bekannten Positivbeispiel
  gegenprüfen — dieselbe „blinde Null" wie bei den Property-Reads.
- **CI-Erkenntnis:** Der 6-Kombi-Lauf braucht nur ~4 min (Jobs parallel) — Plausibilität
  über die Log-GRÖSSE prüfen (~320 KB Mac = Volldurchlauf), nie über die Dauer. Zudem
  triggern `*.md`/`Examples/`-Änderungen unter `ELM_SAB_Tapir/` seit 0.9.12 keinen Build
  mehr (paths-Negation).

## Teamwork: Delete/Move-No-Op trotz success:true — UND die Auflösung <!-- 2026-08-28 korrigiert -->

**Auflösung (gleicher Tag, live verifiziert):** Die No-Ops betreffen den Zustand
VOR einem Teamwork-Senden. Nach „Senden & Empfangen" funktioniert die Kombination
**`ReserveElements` → `DeleteElements` per API einwandfrei**
(2026-08-28: 17.000+ Polylinien geschossweise gelöscht, Rücklese 0).

> **Korrektur 2026-08-31: `TeamworkReceive` per API reicht — und die Befehle wirken.**
> Der frühere Zusatz „die Tapir-Befehle `TeamworkSend`/`TeamworkReceive` existieren,
> liefen aber wirkungslos durch" ist falsch. Live am THN: `DeleteElements` auf 20
> Objekte meldete `success:true` und löschte nichts (Voll-Re-Inventur: 2.480
> unverändert, alle 20 weiter lesbar). Ein einziger **`TeamworkReceive` per Tapir**
> löste die Blockade, danach ReserveElements → DeleteElements → 2.480→2.460, genau
> 20 weg, 0 Kollateralschaden. Ein `TeamworkSend` war dafür NICHT nötig; separat
> ausgeführt lief er ebenfalls sauber durch (Persistenz nach Receive gegengeprüft).
> Merkregel: bei Write-No-Op **erst `TeamworkReceive` per API probieren** — das ist
> unkritisch, weil es nur zieht. `TeamworkSend` veröffentlicht dagegen ALLE offenen
> lokalen Änderungen des Users → vorher fragen.
Merkregeln: (1) Delete-Fehlschlag ⇒ erst senden lassen, dann Reserve+Delete erneut;
(2) `ChangeSelectionOfElements` EXISTIERT (addElementsToSelection/removeElementsFromSelection)
— der frühere „nicht registriert"-Befund war ein Timeout-Artefakt; Selektion in
Batches ≤500, sonst hängt die UI minutenlang; (3) `4001 ongoing user input` beim
Massenlauf: mit Backoff (8 s) wiederholen, User klickt gerade im Modell.

### Ursprünglicher Befund (überholt, Kontext siehe oben)

Am THN (AC29 Teamwork, Gesamtplanung): `DeleteElements` und `MoveElements` liefern
`success:true`, ändern aber NICHTS — auch bei Elementen, die dieselbe Session eben
erst erstellt hat, auch nach `ReserveElements` (meldet ebenfalls Erfolg) und nach
`TeamworkSend`/`TeamworkReceive` (beide existieren als TapirCommand und laufen leer
durch). **Creates funktionieren durchgehend.** Einziger verlässlicher Weg zum
Löschen: User löscht im UI (⌘F Suchen & Auswählen nach Elementtyp+Ebene+Geschoss),
Claude erstellt danach neu. Konsequenz für Workflows: bei Massen-2D-Importen
IMMER damit rechnen, dass eine fehlerhafte Charge nur per UI-Löschung + Neu-Create
ersetzt werden kann — Verifikation ausschließlich per Voll-Re-Inventur
(GetElementsByType + GetDetailsOfElements, Zählung je layerIndex/floorIndex).
`API.SetPropertyValuesOfElements` auf PolyLines → 7203 „Element not supported"
(2D-Elemente tragen keine User-Properties; KI-Stempel dort nicht möglich,
GUID-Register lokal führen). `TapirCommand.SetDetailsOfElements` für PolyLines:
Schema unbekannt, alle probierten Formen 4002.

## Element-Erzeugung: verifizierte Grenzen (2026-08-28) <!-- 2026-08-28 -->

- `TapirCommand.CreateLayers` EXISTIERT: `{"layerDataArray":[{"name":"..."}]}` →
  legt sichtbare Ebene an (THN: Q_22_KAELTE, Index = Listenposition in
  GetAttributesByType-Reihenfolge, per Testelement verifizieren).
- `CreateTexts`: KEIN layerIndex-Parameter — Texte erben die **Werkzeug-Default-
  Ebene** (am THN „98 Schnitt Marker", versteckt+gesperrt!). SetDetailsOfElements
  auf gesperrter Ebene → -2130312912. Ebenen-Attribute sind per API NICHT
  änderbar (kein ModifyLayers/SetLayerAttributes/API.ModifyLayerAttributes) —
  User muss Text-Werkzeug-Ebene im UI setzen, DANN CreateTexts.
- `CreatePolylines` lehnt Polylinien mit identischen Folgepunkten ab
  (-2130313102 je Element, Batch läuft weiter) — nach Koordinatenrundung
  IMMER consecutive-dedupe.
- `SetDetailsOfElements` (Tapir): erlaubte Details NUR floorIndex/layerIndex/
  drawIndex (+typeSpecificDetails Wall/Zone) — additionalProperties strikt.
- Achteck-Polylinien (r≈0,18 m) als Melder-/Punktsymbole: robust, sichtbar,
  schnell — 1.398 Stück in Minuten.

### Nachtrag 2026-08-31: Attribut-Index ≠ Listenposition <!-- 2026-08-31 -->
GetDetailsOfElements meldete für die Text-Werkzeug-Ebene layerIndex 1184, obwohl
GetAttributesByType nur 1067 Layer listet — Attribut-Indizes haben Lücken
(gelöschte Attribute). Listenposition als Index funktionierte bei 182/205/664/1067
nur zufällig/weil niedrig. Regel: Ziel-Index IMMER per Testelement + Sichtprüfung
verifizieren; für Texte gilt ohnehin: Werkzeug-Ebene bestimmt den Layer.
Probe-Text-Polling (Create→GetDetails→Reserve+Delete alle 5 min) ist ein sauberer
Weg, eine User-UI-Umstellung automatisch zu erkennen — hinterlässt nichts.

### Korrektur zur Index-Falle: die verlässliche Index-Tabelle <!-- 2026-08-31b -->
Der Schaden aus der Listenpositions-Annahme war real: 32.000+ Elemente lagen auf
falschen Ebenen (Listenposition 182/205/664/1067 statt wahrer Indizes 1184/25/865/…),
und ein Aufräumlauf löschte 145 fremde Juli-Texte auf der echten Q_22_LUEFTUNG.
Reparatur, vollständig per API: (1) wahre Indizes aus dem Juli-Register
`Zeichnungen/claude/daten/layer_names.json` (Index→Name-Tabelle! IMMER zuerst dort
schauen bzw. Tabelle pflegen); (2) `SetDetailsOfElements {details:{layerIndex:X}}`
verschiebt zuverlässig (vorher chunk-weise ReserveElements, danach ReleaseElements);
(3) verlorene Texte aus `durchbruch_alle_objekte*.json` (textguid/px/py/content)
rekonstruiert und Register nachgeführt. Neue Ebene per CreateLayers: wahren Index
IMMER per Text-Werkzeug-Probe bestimmen (User stellt Tool-Ebene, CreateTexts-Probe
liefert layerIndex), NIE aus der Listenlänge raten.
Wiederherstellungs-Grundregel bei Layer-Aufräumläufen: NIE „alles auf Layer X minus
mein Register" löschen, ohne vorher Positionen/Inhalte der Fremdelemente zu sichern.

## Antwortfelder + Namespaces: was wo liegt <!-- 2026-08-31c -->

Drei Fehlerklassen, die alle wie „Befehl kaputt" aussehen, aber nur falsche Felder
bzw. der falsche Namespace sind. Live am THN geklärt (AC29 5101 GER, Teamwork).

**(1) Add-On-Befehle laufen NUR über `API.ExecuteAddOnCommand`.** Ein direktes
`{"command":"ELM_SAB.GetProjectInfo"}` bzw. `{"command":"TapirCommand.X"}` gibt
`2002 Command not found` — das ist KEIN Beleg, dass das Add-On fehlt. Hülle:
```json
{"command":"API.ExecuteAddOnCommand","parameters":{
  "addOnCommandId":{"commandNamespace":"TapirCommand","commandName":"GetDetailsOfElements"},
  "addOnCommandParameters":{...}}}
```
Antwort steckt in `result.addOnCommandResponse`. Existiert der Befehl im Namespace
nicht, kommt `4010 …does not have the registered Add-On command with the name` —
DAS ist der ehrliche Nichtvorhanden-Befund, nicht 2002.

**(2) Diese Befehle sind eingebaut (`API.`), nicht Tapir.** `TapirCommand.` davor
gibt 4010 und führt in die Irre:
| Aufgabe | richtiger Befehl | Antwortfeld |
|---|---|---|
| Klassifikationssysteme | `API.GetAllClassificationSystems` | `classificationSystems` |
| Klassifikation lesen | `API.GetClassificationsOfElements` (braucht `classificationSystemIds`!) | `elementClassifications` |
| Property-IDs | `API.GetAllPropertyIds {propertyType:"UserDefined"}` | `propertyIds` |
| Property-Definitionen | `API.GetDetailsOfProperties` | **`propertyDefinitions`** (NICHT `propertyDetails`), je Eintrag `propertyDefinition.{name,group.name,type}` |
| Property-Werte | `API.GetPropertyValuesOfElements` | `propertyValuesForElements` |

**(3) `GetDetailsOfElements` — die Felder je Elementtyp.** Vorher an EINEM Element
proben, die Namen sind nicht raten-bar:
- **Objekt/Bibliothekselement:** `details.{origin, angle, dimensions{x,y,z}, libPart{name}}`.
  Es gibt KEIN `coordinate`/`offsettedCoordinate`. `angle` ist die Platzierungs-
  drehung — damit ist „steht das Objekt schief zur Wand?" direkt prüfbar; **Einheit
  (Grad oder Bogenmaß) noch UNGEPRÜFT**, weil alle 996 gelesenen Werte 0 waren. Vor
  der ersten Auswertung ≠ 0 an einem gedrehten Element gegenproben. `dimensions` =
  A/B/ZZYZX, `origin` ist die linke UNTERE Ecke (Mitte = origin + (A/2, B/2), gilt
  nur bei angle 0).
- **Wand:** `floorIndex`, `layerIndex`, `id`, `details.{geometryType, begCoordinate,
  endCoordinate, begThickness, endThickness, height, zCoordinate, bottomOffset}` und
  **`floorPlanPolygons: [{coordinates:[…]}]`** auf der obersten Ebene (nicht in
  `details`!). `geometryType` ∈ Straight / Polygonal / Trapezoid.
- **Text:** hat KEIN `coordinate` im Detail-Response; Positionen kommen aus
  `ELM_SAB.GetTextsOfElements` bzw. `ELM_SAB.Get2DGeometryOfElements`.
- **Hatch:** Detail-Response ohne Koordinaten → Geometrie über
  `ELM_SAB.Get2DGeometryOfElements` (Feld `geometryOfElements`, je Eintrag
  `{success, layerIndex, floorIndex, elementType, coordinates[], arcs[]}`).

**Bonus-Falle: `floorPlanPolygons` ist an Öffnungen AUFGESPLITTET.** Eine gerade
Wand mit drei Türen liefert vier Teilpolygone. Punkt-in-Polygon als Host-Test
verwirft damit genau die Fälle, die man sucht — Durchbruch-Symbole liegen
typischerweise IN der Wandlücke. Für Host-Matching stattdessen **Wandachse +
Dicke** nehmen: `dist(Punkt, Segment beg→end) ≤ thickness/2`. Das ist lückenrobust,
liefert nebenbei den Wandwinkel und die echte Wanddicke. Für `Polygonal`-Wände
(Polywände) bleibt nur der Polygon-Weg — dort alle Teilringe unionieren und
zusätzlich eine Kantendistanz zulassen.

**`4001 Invalid program status (ongoing user input)`** trifft auch kleine
Einzelaufrufe, nicht nur Massenläufe — der User hat einfach ein Werkzeug aktiv
oder zieht gerade etwas. Backoff (5–8 s) und wiederholen; nicht als Fehler melden.
`4001 (no open project)` dagegen heißt: diese Instanz hat kein Projekt offen —
beim Port-Scan sind mehrere Archicad-Instanzen normal, nur eine trägt das Projekt.

## ELM_SAB 0.9.15 — Auf Geschossen zeigen <!-- 2026-09-05 -->

Neu: `ELM_SAB.SetStoryVisibilityOfElements` / `ELM_SAB.GetStoryVisibilityOfElements`.
Nur Object und Lamp (`API_LampType` ist ein Alias auf `API_ObjectType`).

**Set-Parameter:** `elements[]`, dazu entweder `visibility` (Enum `HomeOnly` /
`HomeAndOneUp` / `HomeAndOneDown` / `HomeAndOneUpAndDown` / `AllStories` /
`AllRelevant`) oder `custom` (`{showOnHome, showAllAbove, showAllBelow,
showRelAbove 0|1, showRelBelow 0|1}`), plus `reserve` (Default `true`). Antwort:
`executionResults[]`, bei Erfolg mit zurückgelesenem `visibility`.

**Get-Antwort:** `storyVisibilities[]` je Element mit `visibility`,
`isAutoOnStoryVisibility`, den vier Rohfeldern und `homeStory`.

**Warum es diesen Befehl braucht:** Tapirs `SetDetailsOfElements` kann nur
`floorInd` (= Ursprungsgeschoss), nicht die Geschoss-Sichtbarkeit; die offizielle
JSON-API kennt kein Property dafür.

**Offene Verifikation — noch nicht live verifiziert:** Die DevKit-Header-Doku
sagt, das Relativ-Geschoss-Feature sei für `API_ObjectType` „not extended" —
`showRelAbove/showRelBelow` sind an Objekten unerprobt. Vor jedem Massenlauf an
EINEM Testobjekt setzen, per `GetStoryVisibilityOfElements` und in der Objekt-UI
gegenlesen. Fallback: `AllRelevant` (per DevKit-Beispiel belegt) oder
`AllStories`. Der Build läuft in CI, der Befehl ist bislang ungetestet am
laufenden Archicad — erst nach Live-Test darf dieser Vermerk auf „verifiziert"
geändert werden.

## Leere `executionResults[]` = Undo-Scope hat nicht gestartet <!-- 2026-09-08, THN SuD, live -->

**Symptom (THN, 3037 KI-Durchbruchsobjekte):** `ReserveElements` meldet `success`,
`SetGDLParametersOfElements`, `SetDetailsOfElements` und `MoveElements` antworten
mit `{"executionResults": []}` — leere Liste, kein Fehler, keine Änderung.
`API.SetPropertyValuesOfElements` schreibt an denselben Elementen anstandslos.
1896 Objekte standen deshalb auf Bibliotheks-Defaults (A/B/ZZYZX = 0,70/0,40/0,30),
obwohl sie mit anderen Maßen erzeugt worden waren.

**Ursache:** Diese Tapir-Befehle bauen ihre Ergebnisliste **innerhalb** des Lambdas
von `ACAPI_CallUndoableCommand` auf und **werfen dessen Rückgabewert weg**. Führt
Archicad das Lambda nicht aus, läuft die Schleife nie — die Liste bleibt leer, und
der Befehl meldet trotzdem `succeeded: true`. Es ist **kein** Element-Zustand:
weder Sperre, noch Gruppe, noch Hotlink, noch Teamwork-Reservierung.

**Beleg (live 2026-09-08, AC29/5101, Teamwork):**
- Gemischter Batch aus einem „gesperrten" und einem nachweislich änderbaren Objekt
  → **beide** ohne Ergebnis. Also kein Element-, sondern ein Sitzungszustand.
- `FilterElements` mit `IsEditable` / `InMyWorkspace` schlägt für **alle** Objekte
  fehl, für die änderbaren genauso — taugt nicht als Unterscheidungsmerkmal.
- `GetDetailsOfElements`: identischer Bibliotheksteil (gleiche `ownUnID`) bei
  änderbaren und nicht änderbaren Objekten; `GetGDLParametersOfElements` zeigt
  `isLocked: false`.
- Entscheidend: `ELM_SAB.SetStoryVisibilityOfElements` legt seinen Ergebnisvektor
  **außerhalb** des Lambdas an und lieferte für dasselbe Objekt
  `-2130313215` = `0x81060001` = `APIERR_GENERAL` — exakt den Initialwert, den nur
  ein **nie gelaufenes Lambda** stehen lässt.
- Kein blockierender Dialog (`API.GetProductInfo` antwortet sauber),
  `GetCurrentWindowType` = `FloorPlan`.

**Merksatz:** Ein leeres `executionResults` ist nie „alle übersprungen", sondern
immer „die Schleife lief nicht". Nicht nach Element-Eigenschaften suchen —
den Sitzungszustand prüfen (offener Dialog, aktives Werkzeug/Eingabe,
Teamwork-Schreibrecht) und Archicad notfalls neu starten. Der Zustand ist
klebrig: er hält bis zum Neustart und trifft *alle* Undo-gekapselten Befehle.

## ELM_SAB 0.9.16 — `SetObjectParametersForce` <!-- 2026-09-08 -->

Antwort auf genau den Fall oben. `ELM_SAB.SetObjectParametersForce` setzt GDL-
Parameter (AddPars) von **Object, Lamp, Label und Zone** und schweigt nie:

- Ergebnisvektor liegt **außerhalb** des Undo-Lambdas → jedes Element bekommt ein
  Resultat mit echtem `error.code`, auch wenn nichts lief.
- Der `GSErrCode` von `ACAPI_CallUndoableCommand` wird gemeldet:
  `undoScope: {executed, errorCode, mode, hint}`. `executed: false` benennt das
  Problem im Klartext, statt es zu verstecken.
- `mode: "direct"` — lief das Lambda nicht, wird derselbe Durchgang **ohne**
  Undo-Klammer wiederholt (`allowWithoutUndoScope`, Default `true`). Lieber eine
  Änderung ohne Undo-Eintrag als gar keine; scheitert auch das, steht der Fehler-
  code von `ACAPI_Element_Change` im Ergebnis.
- **Kein `APIFilt`-Vorfilter** — `IsEditable`/`InMyWorkspace` sind am THN wertlos
  (siehe oben) und würden nur wieder still filtern.
- `syncObjectRatios` (Default `true`): `A`/`B` werden bei Objekten zusätzlich in
  `xRatio`/`yRatio` gespiegelt — wer nur die AddPars schreibt, ändert die
  Parameterliste, aber nicht die Maße des platzierten Objekts.
- Ruecklese-Verifikation je Element (`NoError` beweist nichts) und
  Teamwork-Reservierung mit Freigabe der erfolgreichen Elemente (`reserve`).
- `parametersNotInLibPart` zählt Parameter, die es im Bibliotheksteil gar nicht
  gibt — ein Tippfehler im Namen fällt dadurch auf.

**Parameter:** `elements[{elementId:{guid}, gdlParameters[{name, value}]}]`,
optional global `gdlParameters[]` (gilt für alle Elemente ohne eigene Liste),
`reserve`, `syncObjectRatios`, `allowWithoutUndoScope`. Werte gehen als `value`
(Zahl/String/Bool) oder explizit als `numberValue`/`stringValue`/`boolValue`.

**Antwort:** `executionResults[]` (je Element `elementId`, `parametersSet`,
`parametersNotInLibPart` bzw. `error`), `undoScope`, `successCount`,
`elementCount`.

**Testplan vor jedem Massenlauf:** ein einzelnes Objekt setzen, `undoScope.executed`
prüfen, dann per `GetGDLParametersOfElements` **und** `GetDetailsOfElements`
(`dimensions`) gegenlesen. Erst wenn beide stimmen, den Batch fahren.

## ELM_SAB 0.9.17 — Stufe 2 „Auge": Ereignis-Log, EditState, UIState, Deviations <!-- 2026-09-09 -->

Sechs neue Befehle im Namespace `ELM_SAB`. Zweck: Ein Agent sah bisher nur, was er
selbst zurücklas. Alles, was der Nutzer zwischendurch tut — Projekt schließen,
Teamwork-Receive, Bibliothek neu laden, Elemente löschen — blieb unsichtbar, und
genau daraus entstehen die stillen Fehlschläge weiter oben.

### Welche Benachrichtigungen das DevKit hergibt (geprüft in DevKit 29.3100)

Die im Auftrag genannten `ACAPI_Notify_Catch*`-Namen sind der Stand bis AC26. Ab
AC27 heißen sie anders (`MigrationHelper.hpp` shimt die alten Namen für ≤AC26).
Verwendet werden:

| Benachrichtigung | Funktion (AC27–29) | Wofür |
|---|---|---|
| Projekt-Ereignisse | `ACAPI_ProjectOperation_CatchProjectEvent` | New/Open/PreSave/Save/Close/Quit/TempSave, **SendChanges/ReceiveChanges**, ChangeProjectDB/Window/Floor/**Library**, AllInputFinished, UnitChanged, Property-/Klassifikations-Sichtbarkeit, ShowIn3DChanged |
| Selektion | `ACAPI_Notification_CatchSelectionChange` | letztes selektiertes Element (`API_Neig`) |
| Werkzeug | `ACAPI_Notification_CatchToolChange` | Werkzeugwechsel in der Toolbox |
| Neue Elemente | `ACAPI_Element_CatchNewElement (nullptr, …)` | global, alle Typen |
| Änderung/Löschung | `ACAPI_Element_InstallElementObserver` | **nur** für Elemente mit `ACAPI_Element_AttachObserver` |
| Teamwork-Reservierung | `ACAPI_Notification_CatchElementReservationChange` | reserviert / freigegeben / von anderen gelöscht |
| Lockable-Reservierung | `ACAPI_Notification_CatchLockableReservationChange` | Attribute, Favoriten, Modellansichts-Optionen |

**Was es NICHT gibt** (dokumentiert, nicht erfunden): eine globale „irgendein
Element wurde geändert/gelöscht"-Benachrichtigung. Änderung und Löschung kommen
ausschließlich **elementweise** über `AttachObserver`. Ein Rundum-Attach über das
ganze Modell — was Tapirs `SetElementNotificationClient` tut — ist am THN
(>100 k Elemente) zu teuer und wird bewusst nicht gemacht. Dafür gibt es
`WatchElements`.

**Zweiter DevKit-Zwang:** pro Benachrichtigungsart darf nur **ein** Handler
installiert sein. Tapirs `SetElementNotificationClient` installiert eigene Element-
und Reservierungs-Handler und hätte das Log abgeschaltet. Beide Seiten gehen
deshalb über die Weichen `ELMCombinedElementEventHandler` /
`ELMCombinedReservationChangeHandler` (EventLogCommands.cpp); die Weiche
protokolliert zuerst und gibt danach an Tapir weiter — an Tapir nur, wenn dort
überhaupt ein Client registriert ist (`AddElementNotificationClientCommand::HasClients`).

### `ELM_SAB.GetRecentEvents`

Ringpuffer, **5000 Einträge**, gefüllt ab Add-On-Start (erster Eintrag
`EventLogStarted`). Zeitstempel sind ISO 8601 **UTC**, millisekundengenau.

Parameter (alle optional): `since` (ISO-Zeit; der Vergleich ist ein reiner
Zeichenkettenvergleich auf genau den Stempeln, die die Antwort liefert — keine
Zeitzonenfalle), `sinceSeq` (der stabilere Weg zum Weiterlesen), `categories[]`
(`project` / `element` / `selection` / `tool` / `reservation`), `elements[]`,
`limit` (Default 200, es kommen die **jüngsten**).

```json
{"command":"ELM_SAB.GetRecentEvents","parameters":{"sinceSeq":0,"categories":["project","reservation"],"limit":50}}
```
```json
{"events":[
  {"seq":1,"time":"2026-09-09T06:12:03.114Z","category":"project","type":"EventLogStarted","detail":"ELM_SAB 0.9.17"},
  {"seq":7,"time":"2026-09-09T06:31:44.902Z","category":"project","type":"TeamworkReceiveChanges"},
  {"seq":8,"time":"2026-09-09T06:31:47.330Z","category":"reservation","type":"ElementReserved",
   "elementId":{"guid":"A1B2…"},"user":"Sophia Pickel"}],
 "eventCount":3,"matchedCount":3,"capacity":5000,"storedCount":8,"recordedCount":8,
 "droppedCount":0,"oldestSeq":1,"newestSeq":8,"serverTime":"2026-09-09T06:32:10.008Z",
 "installedHandlers":[{"notification":"ProjectEvent (ACAPI_ProjectOperation_CatchProjectEvent)","errorCode":0,"installed":true}, …]}
```

`installedHandlers` ist die Selbstauskunft: welche Benachrichtigung sich
tatsächlich registrieren ließ. Fehler beim Registrieren werden **nicht** in den
`err` des Add-Ons eingemischt — ein fehlendes Log darf das Add-On nicht scheitern
lassen.

### `ELM_SAB.ClearEvents`

Ohne Parameter. `{"clearedCount": 812, "success": true}`. Setzt auch
`recordedCount`/`droppedCount` zurück, nicht die `seq`-Zählung.

### `ELM_SAB.WatchElements`

`{"elements":[{"elementId":{"guid":"…"}}], "watch": true}` → `executionResults[]`
plus `watchedCount`. `watch:false` nimmt den Observer wieder ab. **Erst hiernach**
tauchen `ElementChanged` / `ElementDeleted` / `ElementUndo*` für diese Elemente im
Log auf. Vor einem Massenlauf die betroffenen Elemente anmelden, danach abmelden —
sonst wächst der Puffer mit jeder Nutzeraktion.

### `ELM_SAB.GetElementEditState`

`{"elements":[{"elementId":{"guid":"…"}}]}` → `editStates[]`, `editableCount`,
`elementCount`. Je Element: `exists`, `elemType`, `floorIndex`, `layerIndex`,
`layerName`, `layerHidden`, `layerLocked`, `isLocked`, `inGroup` (+ `groupId`),
`hotlink` (+ `hotlinkId`), in Teamwork zusätzlich `ownerUserId`/`ownerUserName`,
`lockUserId`/`reservedByUser`, `isReservedByMe`, `hasDeleteModifyRight`, bei
bibliotheksteilbasierten Typen `libPartName` und `libPartMissing`, dazu
`filterFlags` und das Gesamturteil `editable` + `reason`.

`reason` in Prüfreihenfolge: `layerHidden` → `layerLocked` → `elementLocked` →
`hotlink` → `reservedByUser:<Name>` → `libraryPartMissing` → `editable`.
Gruppenmitgliedschaft blockiert die API **nicht** und fließt nicht ins Urteil ein,
sondern nur in `note`.

`filterFlags` (`isEditable`, `onVisibleLayer`, `inMyWorkspace`, `hasAccessRight`
aus `ACAPI_Element_Filter`) wird mitgeliefert, aber **bewusst nicht** für das
Urteil benutzt: am THN meldete `IsEditable`/`InMyWorkspace` auch für nachweislich
änderbare Elemente `false` (siehe „Leere `executionResults[]`" oben).

Zwei Feinheiten aus der DevKit-Doku zu `API_Elem_Head`: `lockId` ist **in
Teamwork** die Nutzer-ID des Reservierenden, **außerhalb** ein bool-Sperrschalter —
`isLocked` wird deshalb nur außerhalb von Teamwork aus `lockId` gebildet. Und
„fehlender Bibliotheksteil" steckt in `API_LibPart::missingDef`, nicht im
Fehlercode von `ACAPI_LibraryPart_Get` (Archicad hält für fehlende Teile eine
virtuelle Referenz).

### `ELM_SAB.GetUIState`

Ohne Parameter. Antwort: `modalDialogOpen`, `modalDialogCount`,
`modelessDialogCount`, `currentWindow {type, title, name, index}`,
`teamwork {isTeamworkProject, hasConnection, isOnline, userId, userName, workGroupMode}`,
`project {untitled, name, path}`, `elmSabVersion`.

**Zwei Grenzen offen benannt:**
1. Der **Titel** eines Dialogs ist nicht abrufbar — `DG::Dialog::GetTitle()` ist im
   DevKit `protected`, es gibt keinen öffentlichen Weg an den Text eines fremden
   Dialogs. Gezählt wird über `DG::GetFirstModalDialog()` +
   `GetNextModalDialog()`; es gibt also nur die **Anzahl**, nicht „welcher".
2. ELM_SAB-Befehle laufen per `ScheduleForExecutionOnMainThread`. Blockiert ein
   modaler Dialog den Hauptthread, kann die Antwort ausbleiben. Der belastbare
   Nutzen ist deshalb die **Negativaussage** „`modalDialogCount: 0`, kein Dialog im
   Weg" — ein Timeout ist selbst schon das Signal „Dialog offen". Noch nicht live
   mit offenem Dialog gegengeprüft.

### `ELM_SAB.GetDeviations`

```json
{"command":"ELM_SAB.GetDeviations","parameters":{
  "expected":[{"guid":"A1B2…","dimsMm":[700,400,300]},
              {"guid":"C3D4…","dims":[0.7,0.4,0.3]}],
  "toleranceMm":1.0}}
```
```json
{"deviations":[
  {"elementId":{"guid":"A1B2…"},"exists":true,"elemType":"Object",
   "boundingBox3D":{"xMin":…,"zMax":…},
   "dims":[0.70,0.40,0.30],"dimsSource":"objectRatiosAndBoundingBox",
   "expectedDims":[0.70,0.40,0.30],"deviationMm":[0,0,0],"maxDeviationMm":0,
   "checked":true,"withinTolerance":true},
  {"elementId":{"guid":"C3D4…"},"exists":true,"elemType":"Object",
   "dims":[0.70,0.40,0.25],"expectedDims":[0.70,0.40,0.30],
   "deviationMm":[0,0,-50],"maxDeviationMm":50,
   "checked":true,"withinTolerance":false,"deviatingAxes":"z"}],
 "elementCount":2,"checkedCount":2,"deviatingCount":1,"toleranceMm":1.0}
```

`elements[]` ist optional — fehlt es, werden alle Elemente aus `expected[]`
geprüft. Sollmaße als `dims` (**Meter**, Archicad-Einheit) oder `dimsMm`
(Millimeter). Elemente ohne Sollmaß bekommen `checked:false` und nur ihre
Ist-Maße. Toleranz gilt **pro Achse**, Default 1,0 mm.

`dimsSource` sagt immer, woher die Ist-Maße kommen:
`objectRatiosAndBoundingBox` (Object/Lamp: `xRatio`/`yRatio` = die Maße, mit denen
Nutzer und GDL-Parameter `A`/`B` arbeiten, z aus der 3D-Hülle),
`wallLengthThicknessHeight` (Wand: Achslänge / Dicke / Höhe) oder
`boundingBox3D` (alle übrigen Typen). Wichtig: bei gedrehten oder überstehenden
Symbolen ist die 3D-Hülle **nicht** A/B — deshalb wird bei Objekten bewusst nicht
die Hülle verglichen.

`SetWatch`/`CheckWatch` gibt es bewusst **nicht**: der Vergleich gegen ein Register
bleibt clientseitig, das Add-On liefert nur die Ist-Werte und rechnet die
Abweichung. Ein zweites Sollwert-Register im Add-On wäre eine dritte Quelle der
Wahrheit neben Register und Herkunfts-Properties (Stufe 3).

### Testplan (noch NICHT am laufenden Archicad verifiziert)

Der Build ist grün, die Befehle sind ungetestet. Erst nach diesen Schritten darf
dieser Vermerk auf „verifiziert" geändert werden:

1. `ELM_SAB.GetAddOnVersion` → muss `0.9.17` melden. Sonst läuft das alte Bundle.
2. `ELM_SAB.GetRecentEvents` → `installedHandlers` prüfen: alle sieben
   `installed: true`? Was `false` meldet, gehört in diese Datei nachgetragen.
3. `ELM_SAB.GetUIState` → `currentWindow.type` gegen das tatsächliche Fenster,
   `teamwork.userName` gegen den angemeldeten Nutzer, `modalDialogCount: 0`.
4. Im Archicad ein Element anklicken, dann Werkzeug wechseln → `GetRecentEvents`
   muss `SelectionChanged` und `ToolChanged` zeigen.
5. Ein Testobjekt per `WatchElements` anmelden, es in der UI verschieben, dann
   löschen → `ElementChanged` und `ElementDeleted` müssen im Log stehen.
   **Das ist der Test, der die AttachObserver-Grenze belegt** — ein NICHT
   angemeldetes Element darf keine Change-Ereignisse liefern.
6. Teamwork-Receive auslösen → `TeamworkReceiveChanges` im Log.
7. `GetElementEditState` an vier bekannten Fällen: normales Objekt,
   Objekt auf ausgeblendeter Ebene, Hotlink-Element, von Sophia Pickel
   reserviertes Element. `reason` muss jeweils passen.
8. `GetDeviations` an einem Durchbruchsobjekt mit bekanntem A/B/ZZYZX:
   erst mit korrektem Sollmaß (`withinTolerance: true`), dann mit absichtlich
   falschem (`deviatingAxes` muss die richtige Achse nennen).
9. `ClearEvents`, danach `GetRecentEvents` → `storedCount: 0`.
## ELM_SAB 0.9.18 — CreateWallOpenings: KI-Symbole → echte Wand-Öffnungen <!-- 2026-09-09 -->

`ELM_SAB.CreateWallOpenings` legt statt eines Durchbruch-**Symbols** eine echte
Öffnung (Archicad-Öffnungs-Werkzeug, `API_OpeningID`) in der Wirtswand an und
überträgt Kennzeichnung und Element-ID in einem Zug.

**Warum nicht Tapirs `CreateOpenings`:** das kennt nur
`basePoint`/`ownerElementId`/`width`/`height` und legt im AC29-Zweig immer eine
**polygonale** Öffnung — keine Rundöffnung, kein Anker, keine Grenze, keine
Grundriss-Darstellung. Es reserviert die Wirtswand nicht, meldet
Teamwork-Konflikte nicht, stempelt nichts, und die Ergebnisliste entsteht
**innerhalb** des Undo-Lambdas (siehe „Leere executionResults[]" oben).

**Parameter:** `openings[{sourceObjectGuid?, wallGuid, center{x,y}, width,
height, bottomElevation | topElevation, shape:"rect"|"round", elementId?,
propertyValues[{propertyGuid, value}]?}]`, dazu global `deleteSource` (Standard
false), `reserve` (true), `classify` (true), `classificationItemGuid`,
`kiStampPropertyGuid`, `elementIdPropertyGuid`, `kiStampValue`.

**Antwort:** `results[]` je Eintrag mit `success`, `openingGuid`, `classified`,
`kiStamped`, `elementIdStamped`, `propertiesSet`, `propertiesFailed`,
`sourceDeleted`, `sourceKeptReason`, `placement{x,y,centerZ,storyLevel}` bzw.
`error{code,message}`; dazu `undoScope`, `successCount`, `openingCount`.

**Feldbelegung (verifiziert gegen DevKit 29.3100):**

| Anforderung | Umsetzung |
|---|---|
| Basis | `ACAPI::Element::CreateOpeningDefault()` → `OpeningDefault::Modify(...)` → `Place(wallId, inputPoint)` |
| Owner = Wand | `Place(parentElemId = wallGuid, …)`; Wandtyp wird vorher geprüft |
| Form | `ShapeType::Rectangular` bzw. `Circular` (round: `width` = Durchmesser, `height` wird ignoriert) |
| senkrecht zur Wandachse | `Constraint::Aligned` (richtet die Extrusion an der Wirtswand aus) |
| „durch die Wand" | `LimitType::Infinite` — schneidet alle Schalen einer mehrschichtigen Wand |
| Grundriss symbolisch | `OpeningFloorPlanDisplayMode::Symbolic` |
| Maße | `SetLinkedStatus(NotLinked)` **vor** `SetWidth`/`SetHeight`, sonst zieht Archicad die Höhe mit |
| Höhe | `centerZ = API_StoryInfo.level(wall.header.floorInd) + bottomElevation + height/2`, Anker `APIAnc_MM` |
| Achsprojektion | Lot auf `wall.begC → wall.endC`, auf das Segment geklemmt |
| Klassifikation | `ACAPI_Element_AddClassificationItem(openingGuid, itemGuid)` — nur das **Item**, System implizit |
| KI-Stempel / Element-ID | `ACAPI_Element_GetPropertyValue` → Wert setzen → `ACAPI_Element_SetProperty`, danach **Rücklese** |
| Teamwork | `ACAPI_Teamwork_ReserveElements` für Wirtswände (und Quellobjekte bei `deleteSource`), Konflikt je Eintrag als `APIERR_NOACCESSRIGHT` |

**Was der Header NICHT hergibt — bewusst nicht geraten:**

* **`SetAnchorAltitude` bleibt ungesetzt.** `OpeningExtrusionParameters.hpp`
  dokumentiert den Bezugshorizont dieses Feldes nicht. Die Höhe kommt deshalb
  ausschließlich über `inputPoint.z` bei `Place()` plus Anker `APIAnc_MM`.
  Beim ersten Live-Test ist genau das zu prüfen: sitzt die Unterkante dort, wo
  `bottomElevation` sie haben will?
* **Ebene wird nicht übernommen.** In AC29 hat die `API_Element`-Union **kein**
  `opening`-Mitglied mehr (`API_OpeningType` existiert in den 29er-Headern nicht),
  und `ACAPI::ElementBase`/`ElementDefault` bieten keinen Layer-Zugriff. Die
  Öffnung landet auf der Ebene des Öffnungs-Werkzeugs.
* **`API_Elem_Head.id` ist aus demselben Grund nicht erreichbar.** Die Element-ID
  geht nur über die Property (`General_ElementID`).
* **Properties werden nicht blind vom Quellsymbol kopiert.** Übernommen werden
  der KI-Stempel und — wenn `elementId` fehlt — die Element-ID. Alles weitere
  muss der Aufrufer in `propertyValues` benennen; ein Objekt und eine Öffnung
  haben nicht dieselben Property-Definitionen.
* **Nur Archicad 29.** `ACAPI::Element::Opening` ist `@since Archicad 29`. Unter
  AC27/28 meldet der Befehl `APIERR_NOTSUPPORTED` mit Begründung statt geratene
  Union-Feldnamen für Anker, Grenze und Grundrissdarstellung zu schreiben.

**Weitere Grenzen:**

* **Gekrümmte Wände:** die Projektion nimmt die Sehne `begC→endC`. Bei
  gebogenen Wänden liegt der Punkt daneben — `Place()` projiziert dann selbst auf
  die nächstliegende Oberfläche, also auf eine **Schale** statt auf die Achse.
  `placement{x,y}` in der Antwort zeigt, was tatsächlich benutzt wurde.
* **Geneigte Wände:** von `Constraint::Aligned` abgedeckt (Öffnung kippt mit),
  aber am THN nicht live geprüft.
* **Runde Öffnungen:** `height` wird ignoriert, `width` ist der Durchmesser.
  Ob Archicad bei `Circular` beide Werte gleich hält, entscheidet
  `SetLinkedStatus`; hier steht `NotLinked`, `SetHeight` bekommt denselben Wert.
* **Verschachtelte Undo-Klammer:** `Place()` öffnet laut Header selbst eine
  Undo-Klammer, innerhalb der äußeren des Befehls. Die äußere hält den ganzen
  Lauf samt Stempeln in EINEM Undo-Schritt zusammen. Lehnt Archicad die
  Verschachtelung ab, schaltet `wrapInUndoScope: false` sie ab (dann ein
  Undo-Eintrag je Öffnung) — ohne Neubau. `undoScope.mode` meldet
  `undoable` / `perOpening` / `notExecuted`.
* **Kein Zweitdurchgang ohne Undo-Klammer** (anders als bei
  `SetObjectParametersForce`): eine Öffnung ohne Undo-Eintrag wäre nicht
  zurückrollbar. Startet das Lambda nicht, wurde nichts angelegt — `undoScope`
  sagt es.

**Löschen des Quellsymbols** passiert nur bei `deleteSource: true` **und**
vorhandenem KI-Stempel am Quellobjekt. Handgezeichnete Durchbrüche (ohne Stempel)
bleiben unangetastet — harte Vorgabe aus dem THN-SuD-Projekt. Warum ein Symbol
stehen blieb, steht in `sourceKeptReason`.

**Testplan vor jedem Massenlauf:** EINE Öffnung anlegen (z. B. WD, L 1,20 /
0,25, UK 2,50 ü. FFB, EG Haus WB), dann prüfen: `undoScope.executed`,
`results[0].openingGuid`, `placement.centerZ` gegen die erwartete Höhe,
`kiStamped`/`elementIdStamped`/`classified` alle `true`, und im Modell mit
Schnitt/3D nachsehen, dass die Öffnung **alle Schalen** durchschneidet. Erst
danach den Batch — und erst danach `deleteSource: true`.

## ELM_SAB 0.9.19 — CreateLabels, GetLabelsOfElements, SetLayerOfElements, Get2DGeometry-Fallback <!-- 2026-10-06 -->

Vier Befehle aus der THN-Session vom 2026-10-06. **Stand: live verifiziert 2026-10-06 (THN).**

### `ELM_SAB.CreateLabels`

**Anlass:** Tapirs `CreateLabels` überschreibt `floorInd` mit dem Geschoss des
Parents (`element.header.floorInd = parentElemHead.floorInd`) und spiegelt teils
die `endCoordinate`. Die ELM-Variante (Klasse `CreateLabelsELMCommand`, wegen
Linker anders benannt als Tapirs Klasse) nimmt `floorInd` aus der Eingabe.

**Parameter:** `labelsData[{parentElementId{guid}, floorInd?, begCoordinate?,
midCoordinate?, endCoordinate?, templateLabelId{guid}?}]`.

* `floorInd` fehlt → Geschoss des Parents (nur als Rückfall).
* `begCoordinate` fehlt → Mitte der Parent-Bounds.
* Mit `endCoordinate`: `createAtDefaultPosition=false`, `midCoordinate` fehlt →
  `midC = begC` (gerade Zeigerlinie, nichts abgeleitet). Ohne `endCoordinate`
  Standardposition.
* Mit `templateLabelId`: Vorlage muss ein Symbol-Etikett sein (sonst BADPARS);
  libPart, Ebene, AddPars-Memo, Zeigerlinie, Fonts werden übernommen, die
  Vorlage selbst nur gelesen. Ohne Vorlage: Werkzeug-Default (Memo des Defaults
  wird wie bei Tapir mitgegeben); ein Text-Etikett-Default wird abgelehnt.

**Antwort:** `executionResults[]` je Eintrag: `elementId`, `parentElementId`,
`floorInd`, `begCoordinate`/`midCoordinate`/`endCoordinate`, `libPartName`,
`layerIndex`, `layerName`, `hasLeaderLine`, Prüffelder `floorIndRespected`
(Ist == Soll) und `coordinatesMatch` (alle angegebenen Koordinaten ±1 mm);
dazu `undoScope{lambdaStarted}`. Ergebnisliste entsteht außerhalb des Lambdas.

### `ELM_SAB.GetLabelsOfElements`

**Anlass:** Etikett-Suche je Owner kostete bei 2.200 Etiketten zu viel. Ein
einziger `GetElemList(API_LabelID)`-Durchlauf, Zuordnung über `label.parent`.
**Parameter:** `elements[{elementId{guid}}]`. **Antwort:** `labelsOfElements[]`
in Eingabereihenfolge `{elementId, labels[{guid, labelClass, libPartName?,
floorInd, layerIndex, layerName, beg/mid/endCoordinate, hasLeaderLine}]}` bzw.
`{elementId, error}`; `scannedLabelCount`. Nur die **aktuelle Datenbank**.

### `ELM_SAB.SetLayerOfElements`

**Anlass:** Ebenen umsetzen ohne Rücklese. **Parameter:** `elements[...]`,
`layerId{guid}` oder `layerName`, `reserve` (Standard true). Setzt `header.layer`
im Undo-Scope („ELM_SAB SetLayerOfElements"), Teamwork-Konflikte je Element als
`APIERR_NOACCESSRIGHT`, nach dem Lauf Freigabe der erfolgreichen. **Antwort:**
`executionResults[]` mit `layerIndex`/`layerName` aus der Rücklese,
`undoScope{lambdaStarted}`.

### `ELM_SAB.Get2DGeometryOfElements` — ShapePrims-Fallback

**Anlass:** Der Türaufschlag (Anschlagseite) ist per Elementfeld nicht prüfbar.
Für alle bisher abgelehnten Typen (Door, Window, Object, Lamp, Label …) kommt
statt Fehler `primitives[]` mit `source: "shapePrims"` (gleiche Weiche wie
`CollectCutFillPolygons`: `ACAPI_DrawingPrimitive_ShapePrimsExt` ab AC27).
Arten (Feld `kind`): `line` (beg/end, penIndex), `arc` (origin, radius, begAngle, endAngle,
ratio, angle, whole, penIndex), `poly` (coordinates, subPolyEnds, arcs [live nicht gesehen],
closed, insideHatchBorder), `text` (location, content), `point` (coordinate);
übrige Typen nur gezählt (`skippedPrimitiveCount`). ShapePrims-Fehler →
echter Fehlercode, „ShapePrims fehlgeschlagen". Bekannt: schwere
Legacy-Meshes können hängen, daher einzeln abfragen. Die alten Zweige
(Line/Arc/Circle/PolyLine/Hatch) sind unverändert.

### Live-Verifikation 2026-10-06 (THN)

1. `ELM_SAB.GetAddOnVersion` -> 0.9.19, buildStamp "Oct  6 2026 11:38:34".
2. `GetLabelsOfElements`: WD_U1_033 -> 2 Stempel (Geschoss -2 und -1),
   `scannedLabelCount` 2200, 0,1 s. Vorher dauerte es Minuten über Tapir
   GetElementsByType + GetDetails.
3. `Get2DGeometryOfElements` an einer Tür: `source:"shapePrims"`, 36 Primitive.
   Die Art steht im Feld **`kind`, nicht `type`**: `line` (begCoordinate/
   endCoordinate, penIndex), `poly` (coordinates, subPolyEnds, closed,
   insideHatchBorder), `arc` (origin, radius, begAngle, endAngle, angle, ratio,
   whole, penIndex). Anschlag = arc.origin; Bogenenden = origin + r*(cos, sin)
   (begAngle/endAngle). Beispiel WD_01_003: origin x 73,52 bei Öffnung
   72,54-73,59 -> Anschlag rechts.
4. `CreateLabels`: Tür auf Geschoss -2, Etikett mit `floorInd:-1` +
   `templateLabelId` -> `floorIndRespected:true`, `coordinatesMatch:true`, alle
   GDL-Parameter identisch zur Vorlage.
5. `SetLayerOfElements`: per `layerName` hin (A_01_TRAGWAND, Index 342) und
   zurück (A_013_TUER_BS, Index 1117), Rücklese ok.
6. **Falle:** Danach `TapirCommand.DeleteElements` ohne erneute Reservierung ->
   `success:true` ohne Wirkung (GetElementEditState exists:true,
   GetElementsByType listet es weiter). Nach `ReserveElements` löschte derselbe
   Aufruf. Siehe 0.9.20.

**Offen (nicht live belegt):** Cmd+Z-Probe für CreateLabels (ein Schritt für alle
Etiketten), `SetLayerOfElements` auf eine ausgeblendete Ebene
(`APIERR_NOACCESSRIGHT`), Text-`content` und `poly`-Bögen aus ShapePrims.

## ELM_SAB 0.9.20 — DeleteElements reserviert + prüft Überlebende <!-- 2026-10-06 -->

* **Anlass:** THN-Falle oben und Capmo-Rollout (71 stille Überlebende).
  `ACAPI_Element_Delete` meldet NoError, löscht unreservierte Teamwork-Elemente
  aber nicht.
* **Änderung:** Parameter `reserve` (Standard true), Reservierung im Teamwork
  vor dem Löschen, Prüfung je GUID per Header, reservierte Überlebende werden
  wieder freigegeben. Antwort bleibt `ExecutionResult` (Schema erlaubt keine
  Zusatzfelder, `additionalProperties:false`). Bei Überlebenden `success:false`,
  Fehlertext mit Anzahl und bis zu 10 GUIDs; Code `APIERR_NOACCESSRIGHT` bei
  fremder Reservierung, sonst `APIERR_GENERAL` bzw. der Delete-Fehler.
* **Folge für Aufrufer:** `success:true` heißt jetzt "wirklich weg". Eine
  Rücklese ist für DeleteElements nicht mehr Pflicht, bleibt bei
  Massenlöschungen aber Stichprobe.
* **Bekannte Verwandte:** Das Quellsymbol-Löschen in `ELM_SAB.CreateWallOpenings` hatte dieselbe Lücke — in 0.9.21 behoben (siehe unten).
* **Stand:** live verifiziert 2026-10-06 (THN, Bundle 0.9.21, buildStamp „Oct  6 2026 13:42:13").

### Testplan

1. `GetAddOnVersion` -> 0.9.20. ✅ GetAddOnVersion meldete 0.9.21 (enthält 0.9.20).
2. Element mit ELM_SAB-Befehl reserve+release behandeln (z. B.
   SetLayerOfElements), dann DeleteElements ohne vorherige Reservierung ->
   Element wirklich weg (GetElementEditState exists:false), `success:true`.
   ✅ Teststempel (CreateLabels + SetLayerOfElements hin/zurück → unreserviert) mit Standard-`reserve` gelöscht, GetLabelsOfElements bestätigt „weg".
3. Dasselbe mit `reserve:false` -> `success:false`, Fehlertext nennt GUID und den
   Hinweis "reservieren".
   ✅ `reserve:false` → `{"success":false,"error":{"code":-2130313215,"message":"1 von 1 Elementen nicht geloescht (existieren noch): <GUID>; Elemente reservieren und erneut loeschen"}}`, Element existierte weiter; anschließendes Standard-Delete entfernte es.
4. Element, das ein anderer Teamwork-Nutzer reserviert hat -> `success:false`,
   Code NOACCESSRIGHT, Element unverändert.
5. Gemischter Aufruf (1 löschbar, 1 fremd reserviert) -> `success:false` mit
   "1 von 2". Das löschbare ist weg; Cmd+Z stellt es wieder her.
6. Nach dem Lauf ist kein Element mehr von uns reserviert.

Schritte 4–6: offen (kein zweiter Teamwork-Nutzer im Test; Cmd+Z nicht geprüft).

* **Lehre:** Teamwork: Löschungen, die vor einem Archicad-Neustart nicht gesendet wurden, sind danach wieder da (THN: Teststempel aus dem 0.9.19-Test). Nach API-Tests den User senden lassen.

## ELM_SAB 0.9.21 — CreateWallOpenings prüft Löschung des Quellsymbols <!-- 2026-10-06 -->

* **Anlass:** Dieselbe THN-Falle wie bei 0.9.20: `ACAPI_Element_Delete` meldet
  NoError, löscht unreservierte Teamwork-Elemente aber still nicht. Das KI-Symbol
  stünde dann neben der neuen Öffnung (Doppelung).
* **Änderung:** Nach `ACAPI_Element_Delete` Existenzprüfung per Header.
  `sourceDeleted:true` nur, wenn das Symbol wirklich weg ist; sonst
  `sourceKeptReason` "ACAPI_Element_Delete meldete Erfolg, Element existiert
  weiter (Reservierung?) - Code N". Eine fremd reservierte Quelle (Konflikt bei
  `reserve:true`) wird nicht gelöscht, eigener Grund "Quellobjekt nicht
  reservierbar ...". Freigabe nur für noch existierende eigene Reservierungen.
  Befehlsversion 0.9.21.
* **Folge für Aufrufer:** `reserve:false` mit `deleteSource:true` lässt im
  Teamwork das Quellsymbol typischerweise stehen — jetzt sichtbar statt still.
* **Stand:** live verifiziert 2026-10-06 (THN, Bundle 0.9.21, buildStamp „Oct  6 2026 13:42:13").

### Testplan

1. `GetAddOnVersion` -> 0.9.21. ✅ 0.9.21.
2. THN-Testwand mit KI-Symbol, `deleteSource:true`, Standard `reserve` ->
   Öffnung angelegt, `sourceDeleted:true`, GetElementEditState des Symbols
   exists:false.
   ✅ eigene Testwand (Tapir CreateWalls, 5 m, x 200/y 0 außerhalb des Gebäudes) + Testsymbol (Objekt „Quader", als Durchbrüche›Öffnung klassifiziert, KI-Stempel) → `successCount:1`, `classified/kiStamped/elementIdStamped:true`, `sourceDeleted:true`, GetTypesOfElements des Symbols = 7204 „Element not found". Danach Öffnung + Wand per DeleteElements entfernt (Rücklese: weg).
3. Dasselbe mit `reserve:false` -> `sourceDeleted:false`, `sourceKeptReason`
   enthält "existiert weiter (Reservierung?)", Symbol existiert.
4. Symbol von anderem Nutzer reserviert -> Öffnung entsteht, `sourceKeptReason`
   "nicht reservierbar", Symbol unverändert.
5. Nach dem Lauf ist nichts mehr von uns reserviert; Cmd+Z nimmt Öffnung und
   Löschung in einem Schritt zurück.

Schritte 3–5: offen. Hinweis zu Schritt 3: frisch per API angelegte Elemente gehören dem Anleger — `reserve:false` reproduziert die Falle nur mit einem zuvor freigegebenen Element (vgl. 0.9.20 Schritt 2: SetLayerOfElements hin/zurück).

* **Lehre:** KI-Stempel-Property ist für unklassifizierte Objekte `notAvailable` (6703) → Testsymbol erst klassifizieren (Durchbrüche›Öffnung), dann stempeln.

Hinweis: Danis handgezeichnete Durchbrüche nie als Testobjekt — nur
KI-gestempelte Testsymbole.

## ELM_SAB 0.9.22 — Wand-Baustoff in GetDetailsOfElements <!-- 2026-10-12 -->

* **Anlass:** THN-Nischenbau 12.10.: geteilte Wände bekamen per CreateWalls den Werkzeug-Standard-Baustoff, weil der Baustoff der Originalwand nicht lesbar war.
* **Änderung:** `details.buildingMaterialId` (Basic), `details.profileId` (Profile), `details.compositeId` (Composite, wie bisher); die Felder waren im WallDetails-Schema schon vorgesehen, wurden aber nicht befüllt. Rezept: Wert direkt als `buildingMaterialId` bzw. `compositeId` an CreateWalls übergeben.
* **Stand:** **Live verifiziert 2026-10-13 (WiSchu, mit dem 0.9.23-Bundle, das 0.9.22 enthält)** für Basic-Wände; `compositeId`/`profileId` sind am WiSchu nicht prüfbar (keine Composite- oder Profilwände).

### Testplan

1. ELM_SAB-Versionsabfrage (GetAddOnVersion des ELM_SAB-Namespace) meldet 0.9.22. ✓ (über das 0.9.23-Bundle, das 0.9.22 enthält)
2. Basic-Wand: GetDetailsOfElements -> `buildingMaterialId` lesen, Namen per `API.GetBuildingMaterialAttributes` prüfen (muss dem Baustoff im Info-Fenster entsprechen). ✓ (56/56 gleich, s. Live-Befund)
3. Composite-Wand liefert `compositeId`, kein `buildingMaterialId`. Nicht prüfbar am WiSchu (keine Composite-Wände).

(Nur Lesebefehle, kein Schreiben ins Modell.)

### Live-Befund 2026-10-13 (WiSchu)

* **Verteilung:** 1.386 Wände, alle Basic (Bauteil 1.350 / Stahlbeton 28 / Aluminium 8).
* **Methode:** `details.buildingMaterialId` → Name über native `GetBuildingMaterialAttributes`, verglichen mit der Bauteil-Property „Baustoffname“ (514C85CB-…) über native `GetComponentsOfElements` + `GetPropertyValuesOfElementComponents`. **Ergebnis: 56/56 gleich, 0 Abweichungen.** Rohdaten: `~/.scratch/elmonkey/gegenprobe_0923/teil_c_0922_baustoff.json`.
* **Nicht prüfbar am WiSchu:** `compositeId` und `profileId` (keine Composite- und keine Profilwände).
* **Falle:** „Baustoffname“ ist eine **Komponenten-Property**. Auf Elementebene (`GetPropertyValuesOfElements`) kommt `notAvailable`; lesbar nur über `GetComponentsOfElements` + `GetPropertyValuesOfElementComponents`.

## ELM_SAB 0.9.23 — FindElements: Suche mit und/oder/nicht <!-- 2026-10-13 -->

* **Anlass:** WiSchu-Türenabgleich 13.10. — Tapir `GetElementsByType`/`GetAllElements` lesen die Datenbank des AKTIVEN Fensters: mit einem Layout vorn kamen 0 Türen statt 267. „Türen mit T.-Nummer, nicht Abbruch, je Geschoss" brauchte 4 Aufrufe + Python. Vorbild ist „Suchen & aktivieren": Kriterien beliebig tief mit und/oder/nicht, EIN Aufruf.
* **Das Bundle 0.9.23 enthält ZWEI Änderungen:** `FindElements` (neu) und den `CaptureView`-Fix (Unterabschnitt unten). Beide getrennt testen.
* **Aufruf:** über `API.ExecuteAddOnCommand` mit `addOnCommandId` `{"commandNamespace":"ELM_SAB","commandName":"FindElements"}` (Regel (1) im Abschnitt „Antwortfelder + Namespaces: was wo liegt").
* **Grundsätze:** ausgewertet wird IMMER die Grundriss-Datenbank (alle Geschosse), nie das aktive Fenster; `startDatabaseType` meldet, was beim Aufruf vorn war. Nur lesen (keine Modelländerung, keine Undo-Klammer); `select:true` ändert nur die Auswahl. Kein stiller Nullbefund: unbekannte Schlüssel, Kriterien, Operatoren, Eigenschaften, Klassifizierungen, Ebenen, Geschosse, Design-Optionen = Fehler mit Pfad und Namen (`where.and[2].not.property: ...`); `criteriaStats` zeigt je Knoten, wo Treffer verloren gehen.

### Beispiele

(a) Türen auf Geschoss 2, nicht Abbruch, mit T.-Nummer (live 13.10.: 35 Treffer = Python-Filter):

```json
{"where": {"and": [
    {"type": "Door"},
    {"story": {"index": 2}},
    {"not": {"property": "Umbau-Status", "op": "=", "value": "ToBeDemolished"}},
    {"property": "ID und Kategorien/Element ID", "op": "startsWith", "value": "T.2."}
  ]},
 "return": ["ID und Kategorien/Element ID", "Umbau-Status", "Ebenenname"]}
```

(b) Türenabgleich in EINEM Aufruf — alle Türen aller Geschosse mit Werten, Gruppierung je `floorIndex`/`values` danach in Python (live 13.10.: 267 Türen, 0 Abweichungen, 230 ms):

```json
{"where": {"type": "Door"}, "return": ["ID und Kategorien/Element ID", "Umbau-Status"]}
```

Die Eigenschaft „Element ID" ist im WiSchu zweimal definiert (`Allgemeine Parameter/Element ID` {7E221F33-829B-4FBC-A670-E74DABCE6289} und `ID und Kategorien/Element ID` {B1B54D45-C951-42C9-9AF8-898F0BF212AB}). Der bloße Name liefert den Mehrdeutigkeitsfehler, deshalb steht in den Beispielen „Gruppe/Name" (alternativ die GUID). Siehe Fallen.

(c) Nur zählen, Suchbereich wie im 3D-Fenster:

```json
{"scope": "wie3D", "countOnly": true}
```

(d) `or` + Klassifizierung (Eintrag samt Unterpositionen ODER im System unklassifiziert):

```json
{"where": {"or": [
    {"classification": {"system": "…", "code": "…"}, "includeChildren": true},
    {"classification": {"system": "…"}, "unclassified": true}
  ]}}
```

Live belegt 13.10. (WiSchu, Bundle 0.9.25): System „Archicad Klassifizierung“, `code` „Tür / Tor“ mit `includeChildren` ODER `unclassified` → 267 Treffer; `criteriaStats` zeigen den `or`-Kurzschluss (zweiter Zweig prüft 0). In diesem System steht der Text im **Code**, der Name ist leer → immer `code` (oder `item`-GUID) nehmen, nicht `name` (siehe Fallen).

(e) Beziehung — Türen in Wänden auf Ebenen `A_Wand*` (Muster als Platzhalter):

```json
{"where": {"and": [{"type": "Door"}, {"hostedBy": {"layer": {"pattern": "A_Wand*"}}}]}}
```

Live belegt 13.10. (WiSchu, Bundle 0.9.25) mit den dortigen Ebenennamen: Muster „10 Wand innen*“ 263 = 263 und „10 Wand au*“ 4 = 4 gegen die Referenz (Property Ebenenname der Owner-Wände), GUID-gleich.

### Parameter

`where` (Kriterienbaum; fehlt er, ist jedes Element des Suchbereichs ein Treffer) · `scope` · `return` · `countOnly` · `limit` (Vorgabe 20000, 1…200000; `matchCount` zählt immer alle) · `select` · `databases` (optional `[{databaseId:{guid}}]` statt Grundriss; Elemente der jeweiligen Datenbank so, wie `GetElemList` sie liefert; nicht zusammen mit `select`). Unbekannte Parameter = Fehler.

### Blatt-Referenz

Knoten = Objekt mit GENAU einem Kriteriums-Schlüssel (+ erlaubte Geschwister). Leeres Objekt, zwei Kriterien in einem Knoten oder unbekannter Schlüssel = Fehler mit Pfad und Liste.

| Schlüssel | Form | Geschwister | Kosten |
|---|---|---|---|
| `and` / `or` | nicht-leeres Array von Knoten | – | max(Kinder) |
| `not` | Knoten | – | Kind |
| `type` | `"Door"` oder `["Door","Window"]` (Tapir-Typnamen, Groß/Klein zählt — der Fehler schlägt die richtige Schreibung vor) | – | 0 |
| `story` | Zahl (Index) · `"EG"` (Name) · `{index}` · `{indices:[…]}` · `{name}` · `{names:[…]}` · `{from, to}` (inkl.) | – | 0 |
| `layer` | `"Name"` (exakt) · Objekt mit beliebig vielen von `{name, names, pattern, visible, locked, inCombination}` (UND); `inCombination` = Ebene ist in dieser Ebenen-Kombination sichtbar; `pattern` ohne Treffer = Eintrag in `warnings` | – | 0 |
| `flags` | `"IsVisibleIn3D"` oder Liste der 12 Tapir-Flags (UND); `IncludeSubElemObjects` = Fehler | – | 1 |
| `guids` | Liste aus `"GUID"` · `{guid}` · `{elementId:{guid}}` | – | 0 |
| `inSelection` | `true`/`false` (Auswahl beim Aufruf, im Ausgangsfenster gelesen) | – | 0 |
| `teamwork` | `"frei"`/`"vonMir"`/`"vonAnderen"` (auch `free`/`mine`/`others`); kein Teamwork-Projekt = Fehler | – | 0 |
| `property` | `"Name"` · `"Gruppe/Name"` · `"GUID"` · `{guid}` · `{group, name}` | `op`, `value`, `caseSensitive` | 3 |
| `classification` | `{system, code \| name \| item(GUID), includeChildren, unclassified, version}` | `includeChildren`, `unclassified` | 2 |
| `libPart` | `"Name"` (exakt, ohne Groß/Klein) · `{name}` · `{pattern}` | – | 2 |
| `gdl` | `"paramName"` | `op`, `value`, `caseSensitive` | 3 |
| `ifc` | `{type: "IfcDoor" \| [...]}` · `{pset?, property}` · `{attribute}` | `op`, `value`, `caseSensitive` (nicht bei `type`) | 3 |
| `hostedBy` / `hosts` / `inZone` | Knoten (Unterbaum) | – | max(2, Kind) |
| `bbox` | `{min:{x,y[,z]}, max:{x,y[,z]}, mode: "intersects"\|"inside"}` | – | 2 |
| `near` | `{point:{x,y[,z]}, radius}` (m, Abstand Punkt↔Bounding-Box) | – | 2 |
| `designOption` | `"Name"` · `{name \| id \| guid, set?}` · `{mainModel:true}` (nur AC29) | – | 2 |

**Operatoren (`op`):** `=`/`==`, `≠`/`!=`, `contains`, `startsWith`, `endsWith`, `regex` (ECMAScript, beim Parsen kompiliert), `<`, `≤`/`<=`, `>`, `≥`/`>=`, `between` (`value:[a,b]`, inklusive), `in` (`value:[…]`), `hasValue`, `isEmpty`. Ohne `op`: mit `value` gilt `=`, ohne `value` gilt `hasValue`. Unbekannter Operator, fehlender oder falsch getypter `value` = Fehler. Bei `property` prüft der Parser den Definitionstyp: Zahl-Operatoren nur für Zahlen, Text-Operatoren für Text/Aufzählung, Wahrheitswerte nur `=`/`!=`/`hasValue`/`isEmpty`.

**Semantik:**
* Günstige Blätter laufen zuerst (Kosten-Sortierung, bei Gleichstand Reihenfolge des Aufrufs); `and` bricht bei leerer Menge ab, `or` prüft jedes weitere Kind nur auf die noch offenen Elemente.
* Wert nicht verfügbar (Eigenschaft gilt für den Typ nicht, kein Bibliotheksteil, kein Wirt …) → Blatt false, Zähler `notAvailable`. **`not` über ein Wert-Blatt schließt deshalb Elemente OHNE die Eigenschaft ein** — mit `type` im selben `and` kombinieren.
* Undefiniert/leer: `=` false, `!=` true, `isEmpty` true, `hasValue` false.
* Aufzählungen passen auf `nonLocalizedValue` ODER den Anzeige-String (`"ToBeDemolished"` und `"Abbruch"` liefern dieselbe Menge); Mehrfachauswahl/Listen: irgendein Eintrag passt, `!=` = keiner gleich.
* Text ohne Groß/Klein (Vorgabe), `caseSensitive:true` schaltet um. Zahlen in API-Einheiten (Länge m, Fläche m², Volumen m³), Gleichheit mit Toleranz 1e-6; GDL-Winkel wie GDL (Grad); Property-Winkel in API-Einheit (Gegenprobe).
* Eigenschaften werden je Element GEBÜNDELT gelesen (ein Aufruf mit allen im Baum + `return` benötigten Eigenschaften, gecacht); GDL-Memo einmal je Element für alle `gdl`-Blätter.
* `hostedBy`: Wirt von Tür/Fenster/Dachfenster (owner) und Öffnung (Rückwärtskarte Wand/Decke/Dach/Schale/Balken/Stütze); `hosts`: nur diese Wirt-Typen (Türen, Fenster, Dachfenster, Öffnungen). Andere Typen = `notAvailable`.
* `inZone`: alle Zonen (unabhängig vom `scope`) → Unterbaum → Elemente der passenden Zonen (Raumbeziehung inkl. Wand-/Balken-/Fassadenteile).
* `ifc`: Elemente ohne IFC-Daten = `notAvailable` + Eintrag in `warnings` (erster Fehlercode); nur „IFC-Modul nicht installiert" bricht ab. `typeMismatch` zählt Werte, die nicht typgerecht vergleichbar waren (z. B. Zahl-Operator auf Text-Parameter).

### Antwortfelder

`database` (`"FloorPlan"` bzw. `"databases"`), `startDatabaseType`, `scope`, `scopeCount`, `matchCount`, `returnedCount`, `truncated`, `elements[{elementId, type, floorIndex, storyName, layerName, values}]` (fehlt bei `countOnly`), `returnColumns[{requested, propertyId, group, name}]`, `criteriaStats[{path, criterion, checked, matched, evaluated, notAvailable?, typeMismatch?}]` (alle Knoten in Pre-Order), `selection{requested, changed, selected, notSelectedCount, notSelected[≤500], extraSelectedCount, selectErr?}`, `warnings`, `perDatabase` (nur mit `databases`), `elapsedMs`. `values[]` in `return`-Reihenfolge: `{value, display}` (Aufzählung: `value` = nonLocalizedValue) bzw. `{status:"notAvailable"}` / `{status:"undefined", display}`.
Zum Weitergeben an andere Befehle nur `{elementId}` übernehmen.

### Suchbereich `scope`

| scope | Filter | WiSchu 13.10. vormittags (Handoff) | WiSchu 13.10. Gegenprobe (FindElements = Tapir) | `elapsedMs` |
|---|---|---|---|---|
| `projekt` (Vorgabe) | keiner — alle Geschosse, auch ausgeblendete Ebenen | 19.873 | 40.107 | 95 |
| `sichtbar` | OnVisLayer (inkl. 2D) | 19.503 | 39.736 | 110 |
| `wie3D` | OnVisLayer + In3D (ohne 2D) — was „Suchen & aktivieren" im 3D findet | 10.210 | 10.496 (Tapir nur IsVisibleIn3D: 10.497) | 6.474 |
| `aktuellesGeschoss` | OnActFloor | 6.547 | 24.731 (1. OG) | 137 |

**Referenz immer frisch ziehen:**
* Die absoluten Zahlen sind Momentaufnahmen und haben sich am selben Tag etwa verdoppelt. Ursache ist Ladezustand/Modellinhalt, nicht der Befehl: FindElements und Tapir `GetAllElements` mit gleichen Flags (Grundriss 1. OG vorn) waren in jeder Zeile identisch.
* **Nebenbefund 13.10. (späterer Lauf, Beispiele (d)/(e)):** `scopeCount` `projekt` war 41.039 statt 40.107. Das Modell bzw. der Ladezustand ändert sich laufend, das bestätigt die Regel „Referenz immer frisch ziehen“.
* Vor jedem Vergleich die Referenz im selben Zustand mit Tapir `GetAllElements` + gleichen Flags bei Grundriss vorn ziehen, nie gegen diese Tabelle prüfen.
* `wie3D` ist teuer: die In3D-Prüfung je Element dauert 6,5 s gegenüber ~0,1 s für die anderen. Nur einsetzen, wenn das 3D-Kriterium wirklich gebraucht wird.

### Fallen

* **Aktive Datenbank:** Tapir liest das vordere Fenster, FindElements nie; `startDatabaseType` zeigt, was vorn war (live belegt 13.10.: Layout vorn → Tapir 0 Türen, FindElements 267).
* **Teamwork:** Tapir-Flag `IsEditable` = nur von MIR reserviert → WiSchu Door + OnActualFloor 84, + IsEditable 0. Für „frei/von mir/von anderen" das Blatt `teamwork` nehmen.
* Tapir-Flags kennen kein NICHT und kein ODER — hier per `not`/`or`. `IncludeSubElemObjects` ist kein Filter.
* **Aktuelles Geschoss** über `TapirCommand.GetStories` → `actStory` lesen, nie über `GetUIState.currentWindow.index` (das ist kein Geschoss-Index, live 13.10.: meldete 0 bei 1. OG vorn).
* **Klassifizierung: `code`, nicht `name`.** In der „Archicad Klassifizierung“ steht der Text im **Code/ID** („Tür / Tor“, „Tür“), der **Name ist leer**. `"name":"Tür / Tor"` liefert „unbekannter Eintrag … ähnlich: Tür / Tor“ (die Vorschlagsliste zeigt Code + Name). Immer `code` oder `item` (GUID) verwenden (live 13.10.).
* **Mehrdeutige Eigenschaftsnamen** (WiSchu: „Brüstungshöhe" 4×, „Element ID" 2×) → Fehler mit Kandidaten („… - GUID oder {group, name} angeben"); GUID oder „Gruppe/Name" angeben. „Element ID" gibt es im WiSchu als `Allgemeine Parameter/Element ID` {7E221F33-829B-4FBC-A670-E74DABCE6289} und `ID und Kategorien/Element ID` {B1B54D45-C951-42C9-9AF8-898F0BF212AB}. Für die Element-ID immer `"ID und Kategorien/Element ID"` nehmen (= die von `modell_abgleich.py` gelesene B1B54D45-…). „Gruppe/Name" ist live belegt 13.10. „/" steckt auch in Gruppen- und Eigenschaftsnamen („Balken / Träger") — jede „/"-Position wird probiert.
* **`select` ersetzt die Auswahl** — danach selbst aufheben (keine Auswahl zurücklassen). Bei 0 Treffern bleibt die Auswahl unverändert.
* **`select` + Renovierungsfilter:** Was der Renovierungsfilter der aktiven Ansicht ausblendet, ist nicht auswählbar. Beleg WiSchu 13.10.: 267 Türen → `selected` 263, `notSelectedCount` 4 = genau die 4 Abbruch-Türen T.0.001, T.0.004, T.0.039, T.2.045. Die Rücklese meldet das korrekt in `selection.notSelected`. Daher nach `select` immer `selected` gegen `matchCount` prüfen und `notSelected` lesen. Elemente ANDERER Geschosse werden dagegen ausgewählt. (Nicht live getestet: Wer Abbruch-Elemente mit auswählen will, braucht einen Renovierungsfilter, der Abbruch zeigt.)
* Laufzeit wächst mit teuren Blättern (`property`/`gdl`/`ifc` je Element) → günstige Blätter (`type`, `story`, `layer`) in dasselbe `and`; `elapsedMs` notieren (MCP-Zeitgrenze 600 s).

### Nicht verfügbar

* `RunCriteriaSet` — gespeicherte Kriterien-Sets aus „Suchen & aktivieren" sind per API nicht erreichbar (DevKit 29.3100 geprüft: nur `ACAPI_Selection_*`, `ACAPI_Element_SearchElementByCoord`, `ACAPI_LibraryPart_Search`).
* `designOption` nur in Archicad 29; AC27/28 → Fehler „Kriterium designOption in Archicad NN nicht verfügbar".

### Live-Gegenprobe 2026-10-13 (WiSchu)

Umgebung: AC29 Build 5101, Teamwork, EINE Instanz, Port 19723. `ELM_SAB.GetAddOnVersion` → 0.9.23 / Tapir 1.5.4 / buildStamp „Oct 13 2026 10:09:39" (= CI `4e804b3`). Bundle-Tausch nach den Regeln, Backup 0.9.22 in `~/.scratch/elm-bundle-0.9.23/backup_0.9.22`. Das letzte Archicad-Fenster ohne Projekt wurde per `TapirCommand.QuitArchicad` beendet, weil AppleScript-`quit` mit -128 abbrach.

Rohdaten: `~/.scratch/elmonkey/gegenprobe_0923/teil_a_2026-10-13.json` (Punkte 1–7) und `teil_b_2026-10-13.json` (select, Layout vorn, CaptureView); Skripte `~/.scratch/elmonkey/tools/gegenprobe_findelements_0923.py` und `gegenprobe_findelements_0923_teil_b.py`.

| Nr | Prüfung | Ergebnis |
|---|---|---|
| 1 | Türenabgleich, EIN Aufruf `{"where":{"type":"Door"},"return":["ID und Kategorien/Element ID","Umbau-Status"]}` gegen `modell_abgleich.py` (native `GetElementsByType` + Tapir-Details + Properties) | 267 Türen; je Geschoss −1: 32 / 0: 115 / 1: 84 / 2: 36; je Umbau-Status Existing 242 / New 21 / ToBeDemolished 4. GUID für GUID identisch, 0 Abweichungen, `elapsedMs` 230 (vorher 4 Aufrufe + Python). Treffer liefern `storyName`, `layerName` und `values` als `{value: nonLocalized, display: lokalisiert}` (T.0.004 → `{value:"ToBeDemolished", display:"Abbruch"}`). `returnColumns` löst „ID und Kategorien/Element ID" zu {B1B54D45-C951-42C9-9AF8-898F0BF212AB} auf. |
| 2 | `scope` gegen Tapir `GetAllElements` mit gleichen Flags (Grundriss 1. OG vorn) | Alle vier exakt gleich: projekt 40.107 (95 ms) · sichtbar 39.736 (110 ms) · wie3D 10.496 (6.474 ms; Tapir nur IsVisibleIn3D = 10.497) · aktuellesGeschoss 24.731 (137 ms, 1. OG). Vormittags 19.873 / 19.503 / 10.210 / 6.547 — Zahlen etwa verdoppelt, Ursache Ladezustand/Modellinhalt, nicht der Befehl. Siehe scope-Tabelle. |
| 3 | Beispiel (a) gegen den Python-Filter | 35 = 35, 196 ms. `criteriaStats`: type 40.107→267, story 267→36, not 36→35 (property darunter 36→1), property 35→35 — zeigt die Kosten-Reihenfolge. |
| 4 | Aufzählung: `"value":"Abbruch"` (Anzeige) gegen `"ToBeDemolished"` (nonLocalized) | Dieselben 4 Türen = Referenz. |
| 5 | Fehlerantworten mit Namen | Alle schemakonform beim Client: `error.code` -2130313112 (APIERR_BADPARS), kein 4009. Damit funktioniert `oneOf` auf oberster Ebene → Planer-Risiko aus 261013-fel erledigt. Fünf Fälle: unbekannte Eigenschaft „Umbaustatus" („keine aehnliche Eigenschaft gefunden") · „Brüstungshöhe" mehrdeutig mit 4 Kandidaten (Gruppe/Name {GUID}) · unbekanntes Kriterium „propery" mit Liste der erlaubten · unbekannter Typ „Tuer" mit Liste · unbekanntes Geschoss „99.OG" mit Liste `index:Name`. |
| 6 | „Element ID" als Eigenschaftsname | Im WiSchu ZWEIMAL definiert: `Allgemeine Parameter/Element ID` {7E221F33-829B-4FBC-A670-E74DABCE6289} und `ID und Kategorien/Element ID` {B1B54D45-C951-42C9-9AF8-898F0BF212AB}. Der bloße Name liefert den Mehrdeutigkeitsfehler „… - GUID oder {group, name} angeben". Die Adressierung „Gruppe/Name" ist live belegt. Siehe Fallen. |
| 7 | Teures Blatt ohne `type`-Filter: `{"property":"Umbau-Status","op":"=","value":"ToBeDemolished"}` | Über 40.107 Elemente 1.626 ms (≈1,6 s), 9 Treffer, `notAvailable` 7. Günstiges `type`-Blatt in dasselbe `and` (vgl. Punkt 1: 230 ms). |
| 8 | Layout FR_00-06.1 vorn (per `TapirCommand.ChangeWindow` mit `navigatorItemId`) | FindElements `projekt` weiter 40.107, `startDatabaseType` "Layout", `database` "FloorPlan", Türen 267. Tapir `GetElementsByType` Door im selben Zustand = 0. Die Falle „Aktive Datenbank" ist live reproduziert und gelöst. |
| 9 | `select:true` über alle 267 Türen aller Geschosse (Grundriss 1. OG vorn) | `selected` 263, `notSelectedCount` 4, `extraSelectedCount` 0, Tapir-Auswahl danach 263. Die 4 nicht ausgewählten sind GENAU die 4 Abbruch-Türen T.0.001, T.0.004, T.0.039, T.2.045: Was der Renovierungsfilter der Ansicht ausblendet, ist nicht auswählbar, und die Rücklese meldet das korrekt. Elemente anderer Geschosse werden ausgewählt. Danach Auswahl aufgehoben (0) und Fenster auf Grundriss 1. OG zurückgestellt. |

**Korrektur 13.10. nachmittags:** Vorn war das 1. OG (`TapirCommand.GetStories` → `actStory` 1), nicht Geschoss-Index 0. Der frühere Wert stammte aus `ELM_SAB.GetUIState.currentWindow.index`, und das ist KEIN Geschoss-Index (meldete 0). Beleg: Door + OnActualFloor = 84 = Türen im 1. OG (Punkt 1).

**Teil C (13.10. nachmittags, 0.9.23-Bundle): alle bisher nur gebauten Kriterien.** Methode: je Kriterium die GUID-Menge von FindElements gegen eine unabhängige Referenz, 0 Abweichungen. Rohdaten: `~/.scratch/elmonkey/gegenprobe_0923/teil_c_2026-10-13.json` + `teil_c_ifc.json`; Skript `~/.scratch/elmonkey/tools/gegenprobe_findelements_0923_teil_c.py`.

| Kriterium | Probe | Referenz | Ergebnis |
|---|---|---|---|
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
| designOption | {mainModel:true} | WiSchu hat keine Design-Optionen | 267; unbekannter Name → Fehler „unbekannte Design-Option - vorhanden: “ (leer) |
| databases | Grundriss-DB explizit (GetDatabaseIdFromNavigatorItemId) | – | 267, perDatabase mit scopeCount 40.107 |
| limit | 10 | – | returnedCount 10, truncated true, matchCount 267 |

Am WiSchu nicht prüfbar ist nur die Auflösung von `designOption` per Name/id/guid, weil das Modell keine Design-Optionen hat.

**Beispiele (d) und (e) (13.10. nachmittags, Bundle 0.9.25, scopeCount `projekt` 41.039, s. Nebenbefund bei der scope-Tabelle):**
* **(d) `or` + Klassifizierung:** `{"classification":{"system":"Archicad Klassifizierung","code":"Tür / Tor"},"includeChildren":true}` ODER `unclassified` → 267 Treffer. `criteriaStats` zeigen den `or`-Kurzschluss: der zweite Zweig (`unclassified`) prüft 0 Elemente.
* **(e) `hostedBy` + `layer.pattern`:** `{"and":[{"type":"Door"},{"hostedBy":{"layer":{"pattern":"…"}}}]}` gegen die Referenz (Property Ebenenname der Owner-Wände), GUID für GUID gleich: Muster „10 Wand innen*“ 263 = 263, Muster „10 Wand au*“ 4 = 4 (zusammen die 267 Türen), 224 ms. Rohdaten: `~/.scratch/elmonkey/gegenprobe_0923/beispiele_d_e.json` (enthält die Antwort zu „innen“).

### CaptureView: Fehlerantwort schemakonform, Layout-Verhalten

* **Befund (WiSchu 13.10., Layout „FR_00-06.1", `crop` true und false):** `4009 Invalid command response … 'additionalProperties' … '#/error'`, kein PNG. Ursache: das Antwortschema (`additionalProperties:false`, `required:[imagePath, view2D]`) ließ die Fehlerform `{error:{code,message}}` nicht zu — jede Fehlermeldung wurde von Archicad als ungültige Antwort verschluckt.
* **Änderung 0.9.23:** Antwortschema `oneOf` Erfolg | Fehler; jede Meldung nennt den Fenstertyp und den echten Archicad-Code (Text + `code`-Feld). Layout/MasterLayout: Export wird versucht, sonst „PNG-Export per API nicht möglich (Fehler N) - Zeichnung/Grundriss öffnen oder Layout als PDF publizieren". Keine Typsperre — Grundriss, 3D, Schnitt, Detail … wie bisher. Erfolgsantwort unverändert `{imagePath, view2D}`.
* **Allgemeine Lehre:** ein ELM-Antwortschema mit `required`/`additionalProperties:false` ohne Fehlerzweig verschluckt JEDE Fehlermeldung (Client sieht nur 4009) → neue Befehle über `ELMResponseSchemaWithError` (ELMCommandBase.hpp) deklarieren, Fehler über `CreateErrorResponseELM` (Umlaut-sicher).
* **Live 13.10. (WiSchu, 0.9.23), Fehlerform bestanden:** leerer `outputPath` → `{error:{code:-2130313112, message:"CaptureView: outputPath fehlt oder ist leer"}}`, kein 4009. Mit Layout FR_00-06.1 vorn, `crop` true und false → -2130313115 „CaptureView: Layout-Fenster ('Layout'): PNG-Export per API nicht moeglich (Fehler -2130313115) - Zeichnung/Grundriss oeffnen oder Layout als PDF publizieren", kein PNG. Das Ziel des Fixes (kein 4009, echte Ursache sichtbar) ist erreicht. → ab 0.9.25 liefert Layout ein PNG (live 13.10.).
* **Offener Punkt → erledigt in 0.9.25, live bestätigt 13.10.:** CaptureView-Funktion – Ursache APIERR_BADID. Auch mit Grundriss vorn kommt -2130313115 „CaptureView: Fensterinhalt (Fenstertyp 'FloorPlan') nicht als PNG speicherbar (Fehler -2130313115) - Fenstertyp exportierbar? Pfad beschreibbar?". Probiert wurden zwei Zielpfade (`~/.scratch/…` und `/private/tmp/…`), kein PNG entstand. -2130313115 = **APIERR_BADID** (APIErrorStart + 101, DevKit `APIdefs_ErrorCodes.h` Z. 48). Betroffen ist der Aufruf `ACAPI_ProjectOperation_Save` mit `APIFType_PNGFile` + `API_SavePars_Picture`, für Grundriss UND Layout. Keine Regression: der Save-Code ist seit 0.9.13 unverändert (nur die Meldungen haben sich geändert), und CaptureView hat live NIE ein Bild geliefert. Belege: THN 07.08. („zwei Pfade probiert, Datei entstand nicht"), SuD-Kampagne („scheitert intern, auch mit Archicad im Vordergrund", damals vom 4009 verschluckt) und der Abschnitt „ELM_SAB 0.9.12/0.9.13" („Live-Test am Modell steht aus"; dort nichts geändert). Neu ist nur, dass die Ursache sichtbar ist. Das 3D-Fenster wurde nicht getestet. Workaround `screencapture` war nur bis 0.9.25 nötig. **→ Ursache gefunden, Fix in 0.9.25 (live bestätigt 13.10.: PNG mit `colorDepth` "MiC" für Grundriss, Layout und 3D):** `APIColorDepth_FromSourceImage` ist beim Fenster-Export keine gültige Farbtiefe; Abschnitt „ELM_SAB 0.9.25 — CaptureView: Farbtiefe statt APIERR_BADID“ unten.
* **Offener Punkt → erledigt in 0.9.24** (Abschnitt „ELM_SAB 0.9.24 — Fehlerantwort schemakonform für sechs Befehle" unten): dieselbe Lücke haben noch — Antwortschema mit `required`, ohne Fehlerzweig, mit `return CreateErrorResponse` auf oberster Ebene:
  `CreateWallOpenings` (CreateWallOpeningsCommand.cpp Z. 584/587 — leere openings, AC27/28-Sperre), `SetLayerOfElements` (SetLayerCommand.cpp Z. 102/105), `SetObjectParametersForce` (SetObjectParametersForceCommand.cpp Z. 326), `SetPenOfElements` (SetPenOfElementsCommand.cpp Z. 244), `SetStoryVisibilityOfElements` (StoryVisibilityCommands.cpp Z. 132/141), `SetTextSizeOfElements` (SetTextSizeCommand.cpp Z. 64). Prüfwerkzeug: `python3 -I ~/.scratch/elmonkey/tools/elm_schema_scan.py <ELM_SAB_Tapir/Sources>` (Kandidat = `errorAllowed=False` und `topLevelErrorReturns>0`).

### Stand

**Live verifiziert 2026-10-13 (WiSchu, AC29 Build 5101, Teamwork; buildStamp „Oct 13 2026 10:09:39" = CI `4e804b3`)**, der grüne CI-SHA steht im Quick-Task-SUMMARY 261013-fel.

* **Getesteter Umfang** (Belege im Unterabschnitt „### Live-Gegenprobe 2026-10-13 (WiSchu)"): Kriterien `and`, `not`, `type`, `story` (`{index}`, Name im Fehlerfall), `property` (Name, „Gruppe/Name", Operatoren `=`/`startsWith`, Aufzählung Anzeige + nonLocalized); `scope` alle vier; `return`; `countOnly`; `select`; `criteriaStats`; `startDatabaseType`/`database`; Fehlerantworten. Zusätzlich Teil C (13.10. nachmittags, Unterabschnitt „Live-Gegenprobe 2026-10-13 (WiSchu)“ oben): `or`, `layer` (Name/pattern/visible), `flags`, `guids`, `inSelection`, `teamwork`, `classification` (item/code/includeChildren/unclassified), `libPart`, `gdl`, `ifc` (type/attribute/property), `hostedBy`/`hosts`, `inZone`, `bbox`/`near`, `designOption` `{mainModel:true}` + Fehler bei unbekanntem Namen, die Parameter `databases` und `limit`/`truncated`, außerdem Beispiel (c) über die scope-Probe mit `countOnly` und die Beispiele (d) und (e) (Bundle 0.9.25).
* **CaptureView:** Fehlerform verifiziert. Der Bildexport scheiterte in 0.9.23 mit APIERR_BADID → behoben in 0.9.25, live bestätigt 13.10. (Grundriss, Layout, 3D).
* **Am WiSchu nicht prüfbar:** `designOption` per Name/id/guid (keine Design-Optionen im Modell). Die Beispiele (d) und (e) sind live belegt (Teil C im Unterabschnitt „Live-Gegenprobe 2026-10-13 (WiSchu)“, Bundle 0.9.25); ihre Bausteine (`or`, `classification` mit `includeChildren`/`unclassified`, `hostedBy`, `layer.pattern`) zusätzlich einzeln in der Teil-C-Tabelle.

### Testplan Einbau + Gegenprobe

Macht der Orchestrator mit Mudi. Das Bundle enthält ZWEI Änderungen (FindElements neu + CaptureView-Fix) → beide Blöcke getrennt abhaken.

**Einbau**
1. Bundle-Regeln: 0.9.22-Bundle sichern, Archicad komplett beenden (nie kill), nur EINE Instanz, 0.9.23 einsetzen, starten; `ELM_SAB.GetAddOnVersion` meldet 0.9.23 + Build-Zeitstempel. Nie zwei Dinge gleichzeitig ändern; bei Problemen erst Rückbau auf 0.9.22. ✓ (0.9.23 / Tapir 1.5.4 / buildStamp; Backup 0.9.22)

**FindElements**
2. `scope`-Zahlen mit `countOnly` (ohne `where`) gegen die Tabelle bzw. die Tapir-Referenz. ✓ (alle vier = Tapir, s. scope-Tabelle)
3. Dasselbe mit Layout-Fenster vorn: `projekt`-Zahl unverändert, `startDatabaseType` `"Layout"`. ✓ (40.107, `startDatabaseType` "Layout", Türen 267; Tapir 0)
4. Türenabgleich: EIN FindElements-Aufruf (Beispiel b) gegen `modell_abgleich.py` (`/Users/ap/.scratch/wischu-freising/tools/`, native `GetElementsByType` Door + Properties B1B54D45-C951-42C9-9AF8-898F0BF212AB / 9CF14061-6E9C-45BE-BD8F-D3BCF59D218E): 267 Türen, gleiche Mengen je Geschoss (`floorIndex`) und je Umbau-Status. ✓ (267, 0 Abweichungen, 230 ms)
5. Beispiel (a) gegen denselben Python-Filter (floor 2, umbau ≠ ToBeDemolished, id beginnt mit „T.2."); `"value":"Abbruch"` liefert dieselbe Menge wie `"ToBeDemolished"`. ✓ (35 = 35; „Abbruch" = „ToBeDemolished" = 4)
6. Unbekannte Eigenschaft („Umbaustatus") → Fehler mit Namen; „Brüstungshöhe" → Mehrdeutigkeitsfehler mit 4 Kandidaten. ✓ (plus „Element ID" mehrdeutig, s. Fallen)
7. `select:true` über alle Türen aller Geschosse → `selection`-Rücklese ansehen (werden fremde Geschosse ausgewählt?), danach Auswahl aufheben. ✓ (263/267, 4 Abbruch-Türen vom Renovierungsfilter ausgeblendet; fremde Geschosse werden ausgewählt; Auswahl aufgehoben)
8. `criteriaStats` plausibel (`type`-Blatt matched 267), `elapsedMs` notieren. ✓ (type matched 267; elapsedMs 230 / 196 / 1.626)

**CaptureView**
9. Layout aktiv (z. B. FR_00-06.1), `crop` true und false → klare Fehlermeldung mit Fenstertyp „Layout" und Archicad-Code statt 4009 (oder, falls Archicad den Layout-Export doch kann, ein PNG — dann als „Layout geht" dokumentieren). ✓ (Layout: schemakonformer Fehler -2130313115, kein PNG → „Layout geht nicht"); ab 0.9.25 liefert Layout ein PNG (Abschnitt 0.9.25)
10. Grundriss aktiv → PNG wie bisher, Antwort `{imagePath, view2D:true}`; 3D-Fenster → `view2D:false`. ✗ BADID (Grundriss -2130313115 = APIERR_BADID, kein PNG; 3D-Fenster offen) → ✓ ab 0.9.25 (live 13.10.: Grundriss PNG 832×690 bzw. 4000×3317, 3D PNG `view2D:false`, `colorDepth` "MiC")
11. Leerer `outputPath` → schemakonforme Fehlermeldung (kein 4009). ✓ (-2130313112 „outputPath fehlt oder ist leer")

## ELM_SAB 0.9.24 — Fehlerantwort schemakonform für sechs Befehle <!-- 2026-10-13 -->

* **Anlass:** offener Punkt aus 0.9.23 (CaptureView-Befund WiSchu 13.10.). Antwortschema mit nicht-leerem `required` ohne Fehlerzweig → jede Top-Level-Fehlerantwort `{error:{code,message}}` kam als `4009 Invalid command response … '#/error'` an statt der Meldung.
* **Muster (wie CaptureView 0.9.23):** `GetResponseSchema` → `ELMResponseSchemaWithError (<Erfolgsschema>)`, also `oneOf` Erfolg | Fehler mit byte-gleichem Erfolgsschema. Top-Level-Fehler → `CreateErrorResponseELM`; Code und Text sind unverändert und kommen jetzt Umlaut-sicher an (Ebenennamen!).

  ```cpp
  // vorher
  return GS::UniString (R"({ ...Erfolgsschema... })");
  return CreateErrorResponse (APIERR_BADPARS, "elements ist leer.");
  // nachher
  return ELMResponseSchemaWithError (GS::UniString (R"({ ...Erfolgsschema UNVERAENDERT... })"));
  return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("elements ist leer."));
  ```

* **Unverändert:** Erfolgsantworten, Fehler je Element in `executionResults`/`results`, Registrierungsversionen (Beschreibung um „0.9.24: …" ergänzt). `GetStoryVisibilityOfElements` ist unangetastet, weil es keine Top-Level-Fehler hat (nur `error` je Element).
* **Bedingung für `oneOf`:** Das Erfolgsschema muss ein nicht-leeres `required` ohne `error` haben (und kein `error` in `properties`), sonst passt eine Fehlerantwort auf beide Zweige und `oneOf` scheitert. Geprüft für alle sechs.

### Betroffene Befehle (Stand 0.9.24)

Zeilen beziehen sich auf `ELM_SAB_Tapir/Sources/`. Alle Fehlerpfade kehren VOR jeder Reservierung, Undo-Klammer und Änderung zurück. `APIERR_BADPARS` = -2130313112, `APIERR_NOTSUPPORTED` = -2130312313 (`APIErrorStart` 0x81060000 + 104 bzw. + 903).

| Befehl (`commandName`) | Datei | Top-Level-Fehler (Code) | Meldung | Probe-Aufruf (`addOnCommandParameters`) | Live 13.10. (Bundle 0.9.25) |
|---|---|---|---|---|---|
| `CreateWallOpenings` | CreateWallOpeningsCommand.cpp Z. 585 | `APIERR_BADPARS` (-2130313112) | „openings ist leer." | `{"openings":[]}` | vom Eingabeschema abgefangen (4002, minItems '#/openings') |
| `CreateWallOpenings` | CreateWallOpeningsCommand.cpp Z. 588–593 (nur AC27/28) | `APIERR_NOTSUPPORTED` (-2130312313) | „CreateWallOpenings braucht die ACAPI::Element::Opening-API und damit Archicad 29. Unter AC27/28 …" | unter AC27/28 jeder Aufruf; unter AC29 nicht erreichbar | unter AC29 nicht erreichbar, nicht geprüft |
| `SetLayerOfElements` | SetLayerCommand.cpp Z. 103 | `APIERR_BADPARS` (-2130313112) | „layerId oder layerName erforderlich." | `{"elements":[]}` | ✓ exakter Text |
| `SetLayerOfElements` | SetLayerCommand.cpp Z. 106 | `APIERR_BADPARS` (-2130313112) | „Ebene nicht gefunden: " + Name | `{"elements":[],"layerName":"ZZ_Prüfebene_gibt_es_nicht"}` | ✓ exakter Text, „ü“ unverstümmelt |
| `SetObjectParametersForce` | SetObjectParametersForceCommand.cpp Z. 327 | `APIERR_BADPARS` (-2130313112) | „elements ist leer." | `{"elements":[]}` | vom Eingabeschema abgefangen (4002, minItems '#/elements') |
| `SetPenOfElements` | SetPenOfElementsCommand.cpp Z. 245 | `APIERR_BADPARS` (-2130313112) | „Mindestens ein Stift-Parameter oder clearForegroundRGB/clearBackgroundRGB muss gesetzt sein." | `{"elements":[]}` | ✓ exakter Text |
| `SetStoryVisibilityOfElements` | StoryVisibilityCommands.cpp Z. 133 | `APIERR_BADPARS` (-2130313112) | „Unbekannter visibility-Wert: " + Preset | `{"elements":[],"visibility":"Quatsch"}` | vom Eingabeschema abgefangen (4002, enum '#/visibility') |
| `SetStoryVisibilityOfElements` | StoryVisibilityCommands.cpp Z. 142 | `APIERR_BADPARS` (-2130313112) | „visibility oder custom erforderlich." | `{"elements":[]}` | ✓ exakter Text |
| `SetTextSizeOfElements` | SetTextSizeCommand.cpp Z. 65 | `APIERR_BADPARS` (-2130313112) | „Genau einer von sizeMm oder factor muss angegeben sein (> 0)." | `{"elements":[],"sizeMm":2.5,"factor":1.2}` | ✓ exakter Text |

Drei Proben erreichen den C++-Pfad von außen nicht, weil das Eingabeschema sie vorher mit 4002 und klarer Meldung abweist. Die C++-Pfade bleiben Absicherung, das ist kein Fehler des Fixes. Ergebnis: 5/8 erreichbare Proben mit exaktem Tabellentext, kein 4009.

### Scan vorher/nachher

`python3 -I ~/.scratch/elmonkey/tools/elm_schema_scan.py ELM_SAB_Tapir/Sources` — die sechs Zeilen, Stand `92ab02a` (vorher) und nach dem Umbau (nachher):

```
vorher
CreateWallOpeningsCommand                CreateWallOpeningsCommand.cpp    required=['results', 'undoScope'] addProps=True errorAllowed=False topLevelErrorReturns=2
SetLayerOfElementsCommand                SetLayerCommand.cpp              required=['executionResults'] addProps=True errorAllowed=False topLevelErrorReturns=2
SetObjectParametersForceCommand          SetObjectParametersForceCommand.cpp required=['executionResults', 'undoScope'] addProps=True errorAllowed=False topLevelErrorReturns=1
SetPenOfElementsCommand                  SetPenOfElementsCommand.cpp      required=['executionResults'] addProps=True errorAllowed=False topLevelErrorReturns=1
SetStoryVisibilityOfElementsCommand      StoryVisibilityCommands.cpp      required=['executionResults'] addProps=True errorAllowed=False topLevelErrorReturns=2
SetTextSizeCommand                       SetTextSizeCommand.cpp           required=['executionResults'] addProps=True errorAllowed=False topLevelErrorReturns=1

nachher
CreateWallOpeningsCommand                CreateWallOpeningsCommand.cpp    errorAllowed=True (ELMResponseSchemaWithError) topLevelErrorReturns=2
SetLayerOfElementsCommand                SetLayerCommand.cpp              errorAllowed=True (ELMResponseSchemaWithError) topLevelErrorReturns=2
SetObjectParametersForceCommand          SetObjectParametersForceCommand.cpp errorAllowed=True (ELMResponseSchemaWithError) topLevelErrorReturns=1
SetPenOfElementsCommand                  SetPenOfElementsCommand.cpp      errorAllowed=True (ELMResponseSchemaWithError) topLevelErrorReturns=1
SetStoryVisibilityOfElementsCommand      StoryVisibilityCommands.cpp      errorAllowed=True (ELMResponseSchemaWithError) topLevelErrorReturns=2
SetTextSizeCommand                       SetTextSizeCommand.cpp           errorAllowed=True (ELMResponseSchemaWithError) topLevelErrorReturns=1
```

Hinweis: `topLevelErrorReturns` bleibt gleich, weil der Regex des Scans auch `CreateErrorResponseELM` zählt. Nachher gibt es 0 Kandidaten (`errorAllowed=False` mit `topLevelErrorReturns>0`), alle übrigen Zeilen sind unverändert.

### Stand

**Live verifiziert 2026-10-13 (WiSchu, AC29 Build 5101, Teamwork; Bundle 0.9.25, buildStamp „Oct 13 2026 13:11:29“ = CI `86309a4`)** — CI-SHA im Quick-Task-SUMMARY 261013-jom. Gepusht wurde erst, nachdem die Live-Gegenprobe des CaptureView-Fehlermusters (0.9.23, Testplan 9/11) bestanden war.

### Live-Gegenprobe 2026-10-13 (WiSchu)

Das 0.9.25-Bundle enthält 0.9.24; getestet wurde also mit 0.9.25.

* **(a) Fehlerpfade:** Die Proben aus der Tabelle lieferten den exakten Tabellentext, schemakonform (kein 4009): `SetLayerOfElements` in beiden Fällen (das „ü“ in „Ebene nicht gefunden: ZZ_Prüfebene_gibt_es_nicht“ kam unverstümmelt an), `SetPenOfElements`, `SetStoryVisibilityOfElements` „visibility oder custom erforderlich.“, `SetTextSizeOfElements` → 5/8 (s. Tabelle). Die drei übrigen Proben (`CreateWallOpenings`, `SetObjectParametersForce`, `SetStoryVisibilityOfElements` mit `visibility:"Quatsch"`) wies das Eingabeschema vorher mit 4002 ab (`minItems '#/openings'`, `minItems '#/elements'`, `enum '#/visibility'`). Der AC27/28-Pfad von `CreateWallOpenings` ist unter AC29 nicht erreichbar.
* **(b) Erfolgsform ohne Modelländerung:** Alle sechs Befehle bekamen eine nicht existierende GUID (`reserve:false` wo vorhanden). Jeder antwortete regulär mit einem Fehler je Element (`executionResults` bzw. `results`), KEIN 4009 → `oneOf` lässt die Erfolgsform durch. Beispiele der Elementfehler: `SetLayerOfElements` „Aenderung nicht wirksam (Element nicht lesbar oder Ruecklese weicht ab)“, `SetObjectParametersForce` „Element nicht lesbar“, `SetPenOfElements` „Element konnte nicht geändert werden (in keiner der Datenbanken)“, `SetStoryVisibilityOfElements` „Aenderung nicht wirksam (Ruecklese fehlgeschlagen)“, `SetTextSizeOfElements` „Element nicht gefunden“; `CreateWallOpenings` (Probe ohne `height`) „height muss > 0 sein (ausser shape=round)“ je Öffnung in `results`.
* **(c) Rohdaten und Skript:** `~/.scratch/elmonkey/gegenprobe_0923/gegenprobe_0925_2026-10-13.json` (Schlüssel `D_0924`), `~/.scratch/elmonkey/tools/gegenprobe_elm_sab_0925.py`.
* **Schritt 3 des Testplans (echter Schreibvorgang an eigenem Testelement) bewusst NICHT ausgeführt:** Es wurde nichts ins Teamwork-Modell des Nutzers geschrieben. Ersatz ist die Probe mit nicht existierender GUID (b). Der Schreibweg selbst ist gegenüber 0.9.20/0.9.21 unverändert (dort live verifiziert), 0.9.24 hat nur Schema und Fehlerfunktion geändert; das Erfolgsschema ist byte-gleich.

**Fallen:**
* Leere Pflichtlisten und ungültige Enum-Werte fängt bereits das Eingabeschema mit 4002 ab, die Meldung nennt den JSON-Pfad. Diese Fehlerpfade des Add-ons lassen sich deshalb von außen nicht auslösen.
* Die Probe mit nicht existierender GUID startet bei `SetLayerOfElements`, `SetObjectParametersForce` und `CreateWallOpenings` die Undo-Klammer → leerer Undo-Schritt im Verlauf, keine Änderung.

### Testplan Live-Gegenprobe

1. Einbau nach den Bundle-Regeln von 0.9.23: vorheriges Bundle sichern, Archicad komplett beenden (nie kill), nur EINE Instanz. `ELM_SAB.GetAddOnVersion` meldet 0.9.24 + Build-Zeitstempel. Bei Problemen erst Rückbau. ✓ (GetAddOnVersion 0.9.25; das Bundle enthält 0.9.24)
2. Je Befehl die Probe(n) aus der Tabelle über `API.ExecuteAddOnCommand` (`commandNamespace` „ELM_SAB"). Erwartet: die Antwort enthält `error.code` und `error.message` mit genau dem Tabellentext, KEIN 4009. Bei `SetLayerOfElements` mit Ebenenname muss das „ü" unverstümmelt ankommen. ✓ (5/8 exakter Text, 3 vom Eingabeschema abgefangen, s. Tabelle)
   Begründete Ausnahme von der Regel „Aktionsbefehle nie mit leeren Parametern aufrufen": Die Elementliste ist leer, und der Fehlerpfad kehrt laut Code VOR jeder Reservierung, Undo-Klammer und Änderung zurück (Zeilen in der Tabelle). Es wird nichts ausgeführt.
3. Erfolgsweg unverändert: mindestens ein Schreibbefehl mit gültigen Parametern an einem EIGENEN Testelement, z. B. `SetLayerOfElements` hin und zurück wie in 0.9.20 Schritt 2. Erwartet: `executionResults` wie bisher, kein 4009. Nie Danis Elemente, Teamwork danach senden. Nur mit Freigabe des Nutzers (Update-Regel). Ersetzt durch die Probe mit nicht existierender GUID (Erfolgsform belegt, ohne Modelländerung). Ein Schreibvorgang an einem eigenen Testelement wurde nicht ausgeführt; das Erfolgsschema ist byte-gleich.
4. Danach ist nichts reserviert, und keine Auswahl oder Hervorhebung bleibt zurück. ✓ (Auswahl am Ende 0 laut Rücklese; nichts geändert)

## ELM_SAB 0.9.25 — CaptureView: Farbtiefe statt APIERR_BADID <!-- 2026-10-13 -->

* **Anlass:** offener Punkt aus 0.9.23 (WiSchu 13.10., AC29 Build 5101, 0.9.23 eingebaut): `ACAPI_ProjectOperation_Save (&fsp, &pict)` mit `APIFType_PNGFile`, `colorDepth = APIColorDepth_FromSourceImage`, `view2D = true`, `crop` true/false → -2130313115 = APIERR_BADID für Grundriss (1. OG) UND Layout FR_00-06.1, zwei Zielpfade, keine Datei. CaptureView hat seit 0.9.12 live nie ein Bild geliefert.
* **Ursache (DevKit 29.3100; live bestätigt 13.10.):**
  1. `APIColorDepth_FromSourceImage = -1` (`APIdefs_Elements.h` Z. 10887) ist nur für die Bildkonvertierung erklärt: `API_MimePicture::outDepth` „means to use the same pixel depth the input image has“ (`APIdefs_Goodies.h` Z. 280). Das DevKit nutzt es nur dort (`DG_Test.cpp` Z. 602/1761). Beim Speichern des Fensterinhalts gibt es kein Quellbild → ungültige ID → APIERR_BADID.
  2. Das einzige DevKit-Beispiel für `API_SavePars_Picture` ist `Do_Save_TiffFile` (`Examples/Automate_Functions/Src/Automate_Functions.cpp` Z. 96–115), dort ist `colorDepth = APIColorDepth_256C` (Z. 105). Sonst entspricht unser Aufruf dem Beispiel (fsp `{}`, `fsp.file = &IO::Location`, `dithered=false`, `view2D`, `crop`, Überladung `ACAPI_Automate.h` Z. 168).
  3. `FromSourceImage` steht seit dem ersten Commit `d6fa4f7` (0.9.12) im Code. Das passt zu „nie ein Bild“.
  4. `API_ColorDepthID` ist plattformgetrennt (`APIdefs_Elements.h` Z. 10893–10905): macOS `4C, 4G, ThC, MiC, MiCP`, Windows `64KC, TC24, TC32`, gemeinsam nur `BW, 16C, 256C, 16G, 256G`. Lokal belegt: `APIColorDepth_TC24` ist im Mac-Build undeklariert → Präprozessor-Zweig ist Pflicht.

  **Restunsicherheit:** Theoretisch könnte auch PNG selbst APIERR_BADID auslösen (im DevKit kommt `APIFType_PNGFile` nur in der Enum vor, `APIdefs_Automate.h` Z. 131, kein Beispiel nutzt es). Deshalb gibt es die Rückfallstufe und die gemeldete Tiefe, damit EIN Live-Test eindeutig ist. → ausgeräumt am 13.10.: MiC lieferte bei jeder Probe ein PNG, kein Rückfall.
* **Fix 0.9.25:** Primär gilt die Echtfarbtiefe, auf macOS `APIColorDepth_MiC` („Millions of Colors“), auf Windows `APIColorDepth_TC24` („True Color - 24 bit“), sonst `256C`. `dithered=false`, `view2D`/`crop` wie bisher. `keepSelectionHighlight` bleibt false (bewusst nicht mitgeändert, nur EINE Ursache pro Version).

### Rückfall und Antwortfeld

Liefert der Save APIERR_BADID, folgt genau EIN zweiter Versuch mit `APIColorDepth_256C` (DevKit-belegt). Andere Codes führen zu keinem Rückfall, es laufen höchstens zwei Saves. Die Erfolgsantwort ist `{imagePath, view2D, colorDepth}`, mit `colorDepth` = tatsächlich benutzte Tiefe als String (`"MiC"`, `"TC24"`, `"256C"`). Das Feld ist neu und im Schema optional (`required` weiter `imagePath`, `view2D`, `additionalProperties:false`, Hülle `ELMResponseSchemaWithError`).

Fehlermeldungen nennen den Fenstertyp und die versuchten Tiefen mit Codes, Beispieltext: „CaptureView: Fensterinhalt (Fenstertyp 'FloorPlan') nicht als PNG speicherbar (Farbtiefe MiC: Fehler -2130313115, Rueckfall 256C: Fehler -2130313115) - Fenstertyp exportierbar? Pfad beschreibbar?“. `error.code` ist der Code des letzten Versuchs. Layout/MasterLayout behalten den Hinweis auf Zeichnung/Grundriss bzw. PDF-Publizieren. Die Rücklese (Datei existiert) nennt die Tiefe. Bekannte Schwäche: Die Rücklese prüft nur die Existenz, eine Altdatei am Zielpfad täuscht Erfolg vor.

### Auswertung des Live-Tests

| Antwort | Bedeutung |
|---|---|
| `colorDepth` `"MiC"` (Mac) bzw. `"TC24"` (Windows) und PNG-Datei | Ursache bestätigt: `FromSourceImage` war die ungültige Farbtiefe. ← Ergebnis 13.10. (WiSchu) |
| `colorDepth` `"256C"` und PNG-Datei | Die Echtfarbtiefe ist auf dieser Plattform ebenfalls ungültig, 256C funktioniert → Primärtiefe in einer eigenen Version korrigieren. |
| Fehler mit „MiC … -2130313115, Rueckfall 256C … -2130313115“ (beide Versuche BADID) | Die Farbtiefe ist nicht die (alleinige) Ursache. Zwei nächste Kandidaten, jeder in einer EIGENEN späteren Version (nie zusammen): (a) Dateityp TIFF (`APIFType_TIFFFile`) genau wie im DevKit-Beispiel `Do_Save_TiffFile`; (b) die API-Datenbank passt nicht zum vorderen Fenster — CaptureView setzt die Datenbank nie, `ACAPI_Database_GetCurrentDatabase` gegen `ACAPI_Window_GetCurrentWindow` prüfen (Kandidat 1 aus `~/.scratch/elmonkey/handoff_elmonkey_captureview-badid_2026-10-13.md`). |
| Ein anderer Code als APIERR_BADID | Meldung lesen, es wurde kein Rückfall versucht. |

### Stand

**Live verifiziert 2026-10-13 (WiSchu, AC29 Build 5101, Teamwork; GetAddOnVersion 0.9.25 / Tapir 1.5.4, buildStamp „Oct 13 2026 13:11:29“ = CI `86309a4`; Backup 0.9.23 in `~/.scratch/elm-bundle-0.9.25/backup_0.9.23`)** — CI-SHA im Quick-Task-SUMMARY 261013-kl1.

Das Bundle enthielt gegenüber dem vorher eingebauten 0.9.23 (CI `4e804b3`) ZWEI Änderungen: den 0.9.24-Schemafix für sechs Befehle (`7c07600`, Fehlerantwort schemakonform) und den 0.9.25-CaptureView-Fix (`50c3d5d`). Beide Änderungen des Bundles wurden getrennt abgehakt (0.9.24 im Abschnitt „ELM_SAB 0.9.24“ weiter oben).

Workaround `screencapture` ist nicht mehr nötig.

### Live-Gegenprobe 2026-10-13 (WiSchu)

Jede Probe lieferte ein PNG mit `colorDepth:"MiC"`, ein Rückfall auf `256C` war nie nötig.

| Fenster | crop | Ergebnis |
|---|---|---|
| Grundriss 1. OG, Archicad verdeckt (Claude vorn, per System Events `unix id` belegt) | true | PNG 832×690 RGBA |
| Grundriss, Archicad vorn | true / false | 832×690 / 4000×3317 |
| Layout FR_00-06.1 | true / false | 832×690 / 4000×3317 → **Layout geht ab 0.9.25** |
| 3D | true | PNG, `view2D:false` |
| leerer `outputPath` | – | -2130313112 „CaptureView: outputPath fehlt oder ist leer“ |

* **Bildinhalt gesichtet:** im Grundriss der Türstempel T.1.029, das Layout „Flucht- und Rettungswege“ farbig.
* **These widerlegt:** Die These „CaptureView scheitert an verdeckten Grundriss-Fenstern“ (aus einer anderen Session) ist **widerlegt** (Grundriss bei verdecktem Archicad → PNG). Ursache war allein die Farbtiefe `FromSourceImage`.
* **Rohdaten:** `gegenprobe_0925_2026-10-13.json` (Schlüssel `E_0925`) und `cap_0925_*.png` in `~/.scratch/elmonkey/gegenprobe_0923/`; Skript `~/.scratch/elmonkey/tools/gegenprobe_elm_sab_0925.py`.

### Fallen beim Live-Test

1. **Mehrere Archicad-Instanzen:** Es liefen zwei (WiSchu Port 19723, ein zweites Projekt Port 19724). Für „Archicad vorn“ muss bei mehreren Instanzen der richtige Prozess per `System Events … unix id` nach vorn geholt werden; `tell application "Archicad 29" to activate` ist mehrdeutig.
2. **Modaler Dialog:** Zwischen Versions-Rücklese und Test öffnete der Nutzer „Teamwork-Projekt öffnen/beitreten“ → alle Aufrufe 4001, nichts wurde ausgeführt. Vor Testläufen `GetUIState.modalDialogOpen` unmittelbar davor prüfen.
3. **Archicad beenden:** Ohne offenes Projekt lehnte `TapirCommand.QuitArchicad` einmal mit 4001 „no open project“ ab (vormittags ging es). `osascript`-quit meldete -128, Archicad beendete sich trotzdem nach ca. 6 s → nach -128 erst ein paar Sekunden mit `pgrep` pollen, dann entscheiden.

### Testplan Live-Gegenprobe

Macht der Nutzer.

1. Vorbedingung erledigt: 0.9.24-Bundles gesichert (`~/.scratch/elm-bundle-0.9.24/ci_247aa63/`). Einbau nach den Bundle-Regeln: eingebautes 0.9.23 sichern, Archicad komplett beenden (nie kill), nur EINE Instanz, 0.9.25 einsetzen. `ELM_SAB.GetAddOnVersion` meldet 0.9.25 + Build-Zeitstempel. Bei Problemen erst Rückbau auf 0.9.23. ✓ (Abweichung: zwei Instanzen offen, s. Fallen beim Live-Test)
2. Je Probe ein NEUER Dateiname unter `~/.scratch/…` (vorher prüfen, dass er nicht existiert), weil die Rücklese nur die Existenz prüft. ✓
3. Grundriss vorn, `crop:true` → PNG und `{imagePath, view2D:true, colorDepth:"MiC"}`. `file <png>` zeigt ein Farbbild (RGB). Inhalt gegen ein `screencapture` desselben Fensters prüfen (gleicher Ausschnitt, gleiche Elemente), nicht nach Optik. ✓ (832×690 RGBA, "MiC"; Inhalt gesichtet, Türstempel T.1.029; kein screencapture-Vergleich)
4. Grundriss vorn, `crop:false` → PNG mit größerem Ausschnitt als in Schritt 3. ✓ (4000×3317)
5. 3D-Fenster vorn → PNG, `view2D:false`, `colorDepth` gesetzt. ✓ (`view2D:false`)
6. Layout vorn (z. B. FR_00-06.1) → PNG (dann „Layout geht ab 0.9.25“ dokumentieren) oder Fehler mit Fenstertyp „Layout“, versuchten Tiefen und Codes, kein 4009. ✓ „Layout geht ab 0.9.25“
7. Leerer `outputPath` → schemakonformer Fehler wie in 0.9.23 (kein 4009). ✓ (-2130313112)
8. Gegenprobe 0.9.24 (gleiches Bundle, getrennt von CaptureView): Testplan „ELM_SAB 0.9.24“, Schritte 2–4. ✓ (→ Abschnitt 0.9.24)
9. Auswertung nach der Tabelle oben, Ergebnis mit Datum hier eintragen. Danach keine Auswahl oder Hervorhebung zurücklassen. ✓ (Auswertung Zeile 1; Auswahl am Ende 0)

## ELM_SAB 0.9.26 — Create2DElements: 2D-Elemente anlegen <!-- 2026-10-13 -->

**Stand: eingebaut, noch NICHT live verifiziert (Abnahme nach Spec 8 offen).** Alle Werte unten sind Sollwerte, keine Befunde.

* **Anlass:** Flucht- und Rettungswegeplan WiSchu (Frist Do 15.10.2026, Abgabe Fr 16.10.) braucht farbige Füllflächen (Schraffuren). Tapir legt nur Polylinien an (`CreatePolylines`); Linien, Bögen, Kreise, Splines und Schraffuren fehlen ganz. Verbindliche Spec: `.planning/quick/261013-n2y-elm-sab-0-9-26-create2delements/261013-n2y-SPEC.md`.
* **Umfang:** `ELM_SAB.Create2DElements` ist das Schreib-Gegenstück zu `Get2DGeometryOfElements`. Was der Lesebefehl liefert, geht ohne Umbau zurück. Nur aktive Grundriss-Datenbank (wie `CreateLabels`).

| Typ | `elementType` | Pflichtfelder | Optionale Felder |
|---|---|---|---|
| Linie | `Line` | `begCoordinate`, `endCoordinate` | — |
| Bogen | `Arc` | `origin`, `radius`, `begAngle`, `endAngle` | `ratio` (Ellipse, Standard 1), `angle` (Drehung der Hauptachse, Standard 0), `reflected` |
| Kreis | `Circle` | `origin`, `radius` | `ratio`, `angle` |
| Polylinie | `PolyLine` | `coordinates[]` (≥ 2) | `arcs[]`, `closed` (Standard false) |
| Spline | `Spline` | `coordinates[]` (≥ 2) | `closed` (Standard false), `bezierDirs[]` (sonst `autoSmooth`) |
| Schraffur | `Hatch` | `coordinates[]` (≥ 3) | `arcs[]` und Schraffurfelder (unten) |

  Nicht in v1: Löcher in Schraffuren, Baustoff-Schraffuren, Pfeilspitzen, Arbeitsblätter/Details/andere Datenbanken.

* **Eingabe — gemeinsame Felder:** `elementType` (Pflicht); `floorInd` (fehlt er, gilt das aktuelle Geschoss aus dem Werkzeug-Default); Ebene per `layerName` (Vorrang, `ACAPI_Attribute_Search`) oder `layerIndex`; `pen` (nicht Hatch, `colorOverridePenIndex` wird 0); Linientyp per `lineTypeName` oder `lineTypeIndex` (nicht Hatch); `drawIndex` 1..7. Alles Nicht-Angegebene kommt aus `ACAPI_Element_GetDefaults`. Für den Rundweg werden die Feldnamen des Lesebefehls als Aliase akzeptiert: `floorIndex` → `floorInd`, `penIndex` → `pen`, `ltypeIndex` → `lineTypeIndex`. Zusatzfelder des Lesebefehls (`success`, `whole`, …) werden ignoriert.
* **Eingabe — Geometrie:** Meter, Projektnullpunkt, `{x, y}`; Winkel in Radiant. `arcs[]` = `{begIndex, endIndex, arcAngle}` mit 1-basierten Indizes wie im Lesebefehl. PolyLine `closed:true` und Hatch: der Startpunkt wird als Schlusspunkt angehängt, außer er ist schon da (Toleranz 0,1 mm). Spline `bezierDirs[]` = `{lenPrev, lenNext, dirAng}`, genau ein Eintrag je Punkt.
* **Eingabe — Schraffurfelder:** `fillName` oder `fillIndex` (Typ immer `API_FillHatch`); `foregroundPen`, `backgroundPen` (0 = transparent); `foregroundRGB` / `backgroundRGB` `{red, green, blue}` je 0..1; `contourPen` (0 = keine Kontur); `contourLineTypeName` oder `contourLineTypeIndex`; `showArea` (Standard false).
* **Ablauf:** (1) Prüfen je Element ohne anzulegen (Typ, Pflichtfelder, Punktzahl, Radius > 0, `ratio` > 0, doppelte Folgepunkte, `arcs`-Indizes, `bezierDirs`-Länge, Ebene/Füllung/Linientyp auflösbar, Geschoss existiert). Fehlerhafte Elemente werden übersprungen, der Rest läuft. (2) Anlegen in EINEM Undo-Schritt „ELM_SAB Create2DElements“, keine Teamwork-Reservierung (neue Elemente gehören dem Anleger). (3) Rücklese je Element nach dem Undo-Schritt.
* **Ausgabe:** `executionResults[]` in Eingabereihenfolge plus `undoScope: {lambdaStarted}`. Erfolgseintrag: `success`, `elementId {guid}`, `elementType`, `floorIndex`, `layerIndex`, `layerName`, `floorMatches`, `layerMatches`, `coordinatesMatch`, bei Ringen (Hatch, geschlossene PolyLine) `coordinateOrder` (`same` / `shifted` / `reversed`); Line/Arc/Circle/PolyLine `length` (bei Ellipsen zusätzlich `lengthApproximate:true`); Hatch `area`, `perimeter` (aus `ACAPI_Element_GetQuantities`; schlägt die Mengenabfrage fehl, steht dort `quantityError`), `fillIndex`, `fillName`, `hasForegroundRGB`, `hasBackgroundRGB`; Spline `closed`, `autoSmooth`.
  `success:true` nur, wenn Create NoError UND alle `*Matches` true. Weicht die Rücklese ab, kommt `success:false` MIT `elementId` und allen Ist-Werten plus `error {code, message: "Ruecklese weicht ab: …"}` — das Element existiert, der Aufrufer muss es sehen, um es zu löschen. Prüf-/Create-Fehler: `{success:false, error:{code, message}}`. Fehlt `elements` oder ist es leer: Aufruffehler `{error:{code, message}}` (Schema über `ELMResponseSchemaWithError`, kein 4009).
* **Fallen (im Code abgefangen, beim Live-Test gezielt prüfen):**
  1. PolyLine/Hatch-Memo ist **1-basiert**, Größe `nCoords+1`, Schlusspunkt gleich Startpunkt, `pends[1] = nCoords`, `nSubPolys = 1`.
  2. Spline-Memo ist **0-basiert**, Größe exakt `n`, OHNE Schlusspunkt. Erster == letzter Punkt → Fehler; geschlossen wird per `closed:true`.
  3. `bezierDirs` genau ein Eintrag je Punkt; fehlt das Feld, setzt der Befehl `autoSmooth:true` und `bezierDirs = nullptr`. Lehnt Archicad das live ab, ist der Rückfall `Geometry::CalcSpline` (DevKit `Element_Basics.cpp`, `Do_CreateSpline`).
  4. `arcs`-Indizes sind 1-basiert wie im Lesebefehl und werden unverändert übernommen. Tapirs `GetPolyArcs` addiert einen Offset (Tapir-Eingaben sind 0-basiert) — nicht mischen.
  5. Kreis = `API_CircleID` mit `API_ArcType`-Feldern (`whole = true`, `begAng = 0`, `endAng = 2π`).
  6. Hatch-RGB-Flags (`APIHatch_HasFgRGBColor`, `APIHatch_HasBkgRGBColor`) werden immer zuerst gelöscht und nur bei Angabe gesetzt — ein RGB-Override aus dem Werkzeug-Default rutscht nicht durch.
  7. Ringvergleich toleriert zyklische Verschiebung und Umkehr (Archicad darf Polygone normalisieren), gemeldet in `coordinateOrder`. Offene Linienzüge und Splines nur in gleicher Reihenfolge.
  8. Längen von Ellipsenbögen sind Näherungen (Simpson, 720 Schritte; Vollellipse nach Ramanujan).
  9. Nur aktive Grundriss-Datenbank.

### Get2DGeometryOfElements: Spline-Zweig (0.9.26)

* Splines fielen bisher in den ShapePrims-Fallback. Jetzt eigener Zweig: `elementType:"Spline"`, `coordinates` (0-basiert gelesen, Punktzahl aus der Handle-Größe, denn `API_SplineType` hat kein `nCoords`), `bezierDirs` (nur wenn ihre Anzahl der Punktzahl entspricht), `closed`, `autoSmooth`, `penIndex`, `ltypeIndex`. Die Ausgabe ist 1:1 Eingabe für `Create2DElements`.

### Worked Example — Linie und geschlossene Polylinie mit Bogensegment

Testebene `Z_KI_Test_2D`, Geschoss 0, außerhalb des Gebäudes (Koordinaten vorher gegen den Gebäudeumriss prüfen).

```bash
curl -s localhost:<port> -H 'Content-Type: application/json' -d '{
  "command": "API.ExecuteAddOnCommand",
  "parameters": {
    "addOnCommandId": {"commandNamespace": "ELM_SAB", "commandName": "Create2DElements"},
    "addOnCommandParameters": {"elements": [
      {"elementType": "Line", "floorInd": 0, "layerName": "Z_KI_Test_2D",
       "begCoordinate": {"x": -50.0, "y": -50.0}, "endCoordinate": {"x": -48.0, "y": -50.0}},
      {"elementType": "PolyLine", "floorInd": 0, "layerName": "Z_KI_Test_2D", "closed": true,
       "coordinates": [{"x": -50.0, "y": -48.0}, {"x": -48.0, "y": -48.0}, {"x": -48.0, "y": -46.0}, {"x": -50.0, "y": -46.0}],
       "arcs": [{"begIndex": 2, "endIndex": 3, "arcAngle": 1.5707963}]}
    ]}
  }
}'
```

Erwartete Antwortform (Felder, keine Ist-Werte):

```json
{"executionResults": [
  {"success": true, "elementId": {"guid": "…"}, "elementType": "Line", "floorIndex": 0,
   "layerIndex": "…", "layerName": "Z_KI_Test_2D", "floorMatches": true, "layerMatches": true,
   "coordinatesMatch": true, "length": "…"},
  {"success": true, "elementId": {"guid": "…"}, "elementType": "PolyLine", "floorIndex": 0,
   "layerIndex": "…", "layerName": "Z_KI_Test_2D", "floorMatches": true, "layerMatches": true,
   "coordinatesMatch": true, "coordinateOrder": "same", "length": "…"}
 ],
 "undoScope": {"lambdaStarted": true}}
```

Die PolyLine bekommt den Schlusspunkt angehängt (5 Punkte im Memo); `arcs` bleibt 1-basiert (Segment Punkt 2 → Punkt 3). Soll-Länge der Linie: 2,0 m.

### Rundweg-Beispiel — Spline

1. `Get2DGeometryOfElements` mit der GUID eines vorhandenen Splines → Eintrag `{"elementType":"Spline", "coordinates":[…], "bezierDirs":[…], "closed":…, "autoSmooth":…, "penIndex":…, "ltypeIndex":…, "layerIndex":…, "floorIndex":…, "success":true}`.
2. Diesen Eintrag UNVERÄNDERT als Element in `Create2DElements.elements` geben (Aliase `floorIndex`/`penIndex`/`ltypeIndex` greifen, `success` wird ignoriert).
3. Soll: `coordinatesMatch:true`, gleiche Punktzahl, gleiche Ebene und gleiches Geschoss; danach beide Splines erneut lesen und `bezierDirs` vergleichen.

### Stand

* Eingebaut in 0.9.26: `e7ec585` (Create2DElements, Registrierung, `ELM_SAB_VERSION` 0.9.26) und `d5fa36a` (Spline-Zweig in Get2DGeometryOfElements). Syntaxprüfung gegen DevKit 29.3100 grün, `elm_schema_scan.py` meldet `errorAllowed=True`. Echter AC27/28/29-Build über CI (`ci-status`/`ci-release`) steht aus.
* CI 6/6 grün (`3f38981`), eingebaut 13.10., `GetAddOnVersion` = 0.9.26 (Build 13.10. 15:03).
* **Live-Abnahme 13.10. (WiSchu, Port 19723, Ebene `Z_FR_Rettungswege`, Testelemente bei x ≥ 1000 m, danach gelöscht + Bereich leer nachgeprüft):**
  * Hatch 1 × 1 m mit RGB (WiSchu-Session): `area` 1, `perimeter` 4, alle `*Matches` true, CaptureView zeigt das Quadrat, Löschung rückgelesen.
  * Line (Länge 5), Arc r 2 / 90° (Länge π), PolyLine geschlossen mit 90°-Bogensegment (Länge 8,2214 = nachgerechnet), Spline autoSmooth, Spline mit `bezierDirs`: alle `success:true`.
  * Hatch mit Halbkreis-Bogen + RGB: `area` 3,570796 = 2 + π/2, `perimeter` 7,141593 = 4 + π — exakt.
  * Rundweg echte Projektschraffur (25 Punkte, lochfrei, +1000 m verschoben): Rücklese punktgenau identisch; `area` 10,659830 / `perimeter` 21,169118 = unabhängig per Shoelace gerechnet.
  * Fehler je Element lesbar, gültige Elemente im selben Aufruf trotzdem angelegt: Hatch 2 Punkte, unbekannte Ebene, Radius 0, `bezierDirs` falsche Länge. Unbekannter `elementType` und fehlendes `elements` fängt schon das Eingabeschema (4002); leere Liste → lesbarer Aufruffehler.
  * **Befund 1 (→ 0.9.27):** Circle wird richtig angelegt, Archicad speichert ihn aber als `API_ArcID` mit `whole:true` → Rücklese meldete fälschlich „Typ Koordinaten“ / `success:false`.
  * **Befund 2 (→ 0.9.27):** Rundweg einer Schraffur MIT Loch scheitert mit APIERR_IRREGULARPOLY — `Get2DGeometryOfElements` gab Außenkontur + Loch als eine flache Liste aus (Erkennung: innerer Punkt wiederholt den Startpunkt).
  * Fallen: Füllungsnamen sind projektabhängig („Vollfüllung“ gibt es im WiSchu nicht, dort „Nur Vordergrund-Stift“ Index 64); `API.GetElementsByType` kennt `Hatch` nicht (4002) → `TapirCommand.GetElementsByType`; eine Testebene anzulegen ist in Teamwork ein Attribut-Eingriff → vorhandene sichtbare Ebene nehmen, weit abseits zeichnen, danach löschen.
* Einbau nur bei null laufenden Archicad-Instanzen; vorher das eingebaute 0.9.25-Bundle sichern; danach `ELM_SAB.GetAddOnVersion` = 0.9.26 prüfen. Jeder Push überschreibt `ci-status` und das Release.
* Falle beim Beenden: Nach dem Schließen der Projekte laufen die Archicad-Prozesse ohne Fenster weiter (API meldet „no open project") → leere Instanzen per `NSRunningApplication … terminate` nach PID beenden.

## ELM_SAB 0.9.27 — Create2DElements: Kreis-Rücklese, Löcher erkennen <!-- 2026-10-13 -->

* **Kreis:** Die Rücklese akzeptiert `API_ArcID` mit `whole:true` als Circle (so speichert Archicad Kreise). Vorher `success:false` trotz richtiger Geometrie.
* **Löcher:** `Create2DElements` lehnt eine Hatch ab, deren innerer Punkt den Startpunkt wiederholt (= Teilpolygon/Loch), mit klarer Meldung statt APIERR_IRREGULARPOLY. Löcher anlegen geht ab 0.9.28 per `subPolyEnds` (siehe dort).
* **`Get2DGeometryOfElements`:** PolyLine und Hatch liefern zusätzlich `subPolyEnds` (1-basierte Endindizes je Teilpolygon, wie `memo.pends`). Mehr als ein Eintrag = Löcher; Außenkontur = `coordinates[0 .. subPolyEnds[0]-1]`.
* CI 6/6 grün (`c2a6fb5`), eingebaut 13.10., `GetAddOnVersion` = 0.9.27 (Build 13.10. 16:20). **Live verifiziert (WiSchu):** Circle r 1 → `success:true`, Länge 2π; Schraffur mit Loch (Quelle `63D2A229…`, 29 Punkte) → Element-Fehler „Punkt 22 wiederholt den Startpunkt“; `subPolyEnds` = [22, 29] bzw. [25] bei lochfreier Schraffur; nur Außenkontur übergeben → angelegt, `area` 6,515948 / `perimeter` 14,870535 = Shoelace. Testelemente gelöscht, Löschung rückgelesen.

## ELM_SAB 0.9.28 — Create2DElements: Schraffuren mit Löchern <!-- 2026-10-13 -->

* **Eingabe:** Hatch nimmt optional `subPolyEnds` (1-basierte Endindizes je Teilpolygon) — genau das Feld, das `Get2DGeometryOfElements` seit 0.9.27 ausgibt. `coordinates` ist dann flach: Außenkontur zuerst, danach jedes Loch, **jedes Teilpolygon geschlossen** (Schlusspunkt = Startpunkt). Lesebefehl-Ausgabe einer Loch-Schraffur geht damit 1:1 zurück.
* **Prüfung je Element:** Enden steigend, letzter Wert = Punktzahl, jedes Teilpolygon geschlossen mit ≥ 3 verschiedenen Punkten, keine Doppelpunkte innerhalb eines Teilpolygons, kein `arcs`-Eintrag über eine Teilpolygon-Grenze. `subPolyEnds` mit mehreren Einträgen bei PolyLine → Fehler. Ohne `subPolyEnds` bleibt die klare Loch-Meldung (jetzt mit Hinweis auf das Feld).
* **Memo:** `nSubPolys` = Anzahl Teilpolygone, `pends[s]` = Endindex (1-basiert), `pends[0] = 0`.
* **Rücklese:** Vergleich je Teilpolygon (Außenkontur gegen Außenkontur, Löcher in beliebiger Reihenfolge, jeder Ring verschoben/umgekehrt erlaubt → `coordinateOrder: "normalized"`). Antwort zusätzlich `subPolyEnds` (zurückgelesen) und `holesArea` (`API_HatchQuantity.holesSurf`).
* CI ok (`af01be3`), eingebaut 14.10., `GetAddOnVersion` = 0.9.28. **Live verifiziert (WiSchu):** Rundweg `63D2A229…` mit `subPolyEnds` [22, 29] → `success`, `area` 6,242592 (netto = Außen − Loch, Shoelace), `holesArea` 0,273356, `coordinateOrder` same; Quadrat 4 × 4 mit Loch 2 × 2 + RGB → `area` 12, `holesArea` 4, `perimeter` 24 (inkl. Lochrand); Fehler lesbar: offenes Teilpolygon, Bogen über Grenze, Enden nicht steigend (meldet das erste Teilpolygon als nicht geschlossen). `area` ist also **netto**. Testelemente gelöscht, Löschung rückgelesen.

## Bemaßung in Ansichten — ELMonkey 0.9.29 (bei S. als 0.9.12–0.9.14) <!-- 2026-10-14 -->

Live entstanden beim Fliesenspiegel Futurelab EG (34 Innenansichten, ~300 Maßketten).
Ausgangslage: Tapir 1.5.4 ersetzt in **jeder** Liste (`GetElementsByType`, `GetAllElements`,
Selektion) ein Schnittelement (`API_SectElemID`) durch sein Modell-Bauteil. Assoziative
Maße in Schnitt/Ansicht/Innenansicht hängen aber am Schnittelement. Ohne dessen GUID ist
keine Maßkette möglich. `GetSectionElements` gibt es erst in Upstream-Tapir ≥ 1.7.

| Befehl (Namespace `ELM_SAB`) | ab | Zweck |
|---|---|---|
| `GetSectionElementsELM {databases?, withBounds?}` | 0.9.29 | rohe SectElem-GUIDs je Datenbank + `parentId`, `parentType`, `layerIndex`, `bounds` (Ansichtskoordinaten; `1e35` = in dieser Ansicht nicht gezeichnet) |
| `GetDimensionDetailsELM {elements, databaseId?}` | 0.9.29 | Rohdaten linearer Ketten: je Messpunkt `elementType/elementId/line/special/inIndex/nodeType/nodeStatus/nodeId`, `loc`, `dimVal`, `witnessVal/Form`, Note inkl. **Custom-Text** und Größe; dazu `refC`, `direction`, `defNote`, Ebene |
| `CreateDimensionsLikeTemplateELM {templateDimensionId, templateDatabaseId?, targetDatabaseId?, dimensionsData}` | 0.9.29 | Kette mit **allen** Einstellungen einer Vorlage-Kette anlegen (Ebene, Stifte, Schrift, Marker, Textlage). Punkte assoziativ (`elementId`+Knotendaten) oder statisch (`coordinate`), je Punkt `witnessVal/Form`, `customText` (z. B. „OK FFB"). Antwort mit `witnessCount/associatedCount` |
| `SetDimensionAppearanceELM {dimensions:[{elementId, noteSize?, witnessValues?, witnessCoordinates?}], databaseId?}` | 0.9.29 | bestehende Kette **an Ort** ändern: Schriftgröße aller Maßtexte, Hilfslinienwert je Punkt, Koordinate **statischer** Punkte. Verweigert Ketten mit 2101-Bezug (siehe `mcp-conventions.md`) |
| `GetInteriorElevationsELM {}` | 0.9.29 | alle Innenansichten (Grundriss-Elemente) mit vertikalem Bereich, gemeinsam und **je Segment** (`vertRange`, `vertMin`, `vertMax`, `databaseElemSetId`) |
| `SetInteriorElevationVerticalRangeELM {interiorElevations:[{elementId, vertMax, vertMin?}]}` | 0.9.29 | vertikalen Bereich setzen (Begrenzt). Untergrenze bleibt, wenn nicht angegeben. Mit Rücklese |

- **Ansicht ↔ Innenansicht-Element zuordnen:** `databaseElemSetId` eines Segments == Tapir-Datenbank-ID aus `GetDatabaseIdFromNavigatorItemId`. Die Begrenzung sitzt **je Segment** (`memo.intElevSegments`), der „gemeinsame" Segmentwert war `Infinite 0/0` und ist nicht maßgeblich.
- **Ansichts-Datenbank erst gefüllt, wenn das Fenster einmal offen war:** Vorher liefern Maßketten `dimensionValue 0`. Immer erst `ChangeWindow`, dann lesen.
- **Knotentypen** in Ansichten (aus Rohdaten gelernt): `2100` Polygonknoten von Decke/Tür/Waschtisch (Tür: `nodeId 8`, `nodeStatus 3` = Oberkante links, `5` = Oberkante rechts; Bodenfliese: `nodeId 1`, `nodeStatus 16`); `1100` Objekt-Hotspot; `2101` Fassadenprofil. Kopierte 2100/1100-Bezüge funktionieren in neuen Ketten, **2101 nicht**.

### Lokal bauen, wenn kein GitHub-Push möglich ist <!-- 2026-10-14 -->

Auf dem SAB-Mac fehlen Xcode, cmake und ein GitHub-Login. Gebaut wurde trotzdem lokal, nur für arm64:

```bash
pip3 install --user cmake            # liefert ~/Library/Python/3.9/bin/cmake
# DevKit 29.3000 (MAC) entpacken, Quellen in ein Scratch-Verzeichnis kopieren und dort in
# Tools/CMakeCommon.cmake Zeile 9 auf  set (CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
# setzen (das Original-"PARENT_SCOPE CACHE" bricht mit Unix Makefiles)
cmake -B build29 -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAC_VERSION=29 -DAC_API_DEVKIT_DIR=<devkit>/Support <src>
cmake --build build29 -j 8
codesign --force --deep -s - build29/RelWithDebInfo/ELM_SAB_AC29_Mac.bundle
```

Vorher Syntaxprüfung ohne Build: `clang++ -fsyntax-only -std=c++20 -Dmacintosh=1 -DACExtension -I Support/Inc -I Support/Modules/*/ …`.
Installieren: altes Bundle nach `~/ELM_SAB_Backups/` sichern, neues nach `Add-Ons/`, Archicad
neu starten, `ELM_SAB.GetAddOnVersion` prüfen (`buildStamp`). Das lokale Bundle ist **nur
arm64** — für Intel-Macs, Windows, AC27/28 braucht es weiter die CI (Push nötig).

## ELM_SAB 0.9.31 — Decken-Absturz mit Favoriten-Standards, GDL-Parameter für Türen/Fenster <!-- 2026-10-20 -->

* **CreateSlabs-Absturz** (Meldung Maxl 15.10., BE-Plan 135054): nach `ApplyFavoritesToElementDefaults` (Favorit „Bodenabdeckung") stürzte `CreateSlabs` mit mehreren Decken FATAL ab. Zwei Ursachen in `BuildSlabMemoFromGeometry`: (1) nur `memo.coords` wurde auf null geprüft, die übrigen Listen nicht; (2) **`memo.sideMaterials` aus Favoriten-Standards ist kein vergrößerbarer BM-Block** → „Fatal memory error in BMReallocPtr" (eigener Live-Test 20.10., Bericht `Archicad-20261020(14-25-42)-[37204]-1-FATAL.rpt`). Fix: jede Liste einzeln anlegen/auf Sollgröße bringen; `sideMaterials` immer frisch per `BMAllocatePtr`, alter Zeiger unangetastet.
* **GDL-Parameter für Türen/Fenster** (Beitrag Roman, aus dessen Bundle nachgebaut — Quellcode lag in einem gelöschten Claude-Scratchpad): `ELM_SAB.SetObjectParametersForce` und `TapirCommand.SetGDLParametersOfElements` akzeptieren Door/Window; A/B werden zusätzlich in `openingBase.width/height` gespiegelt (Element-Offsets 176/184, wie `object.xRatio/yRatio`). Binärvergleich: Typprüfung und Schreibstellen identisch zu Romans Build.
* **Live verifiziert 20.10.** (leeres Projekt, 0.9.32-Bundle): Favorit „Bodenabdeckung" + 3 Decken (5/15/8 Punkte) in einem Aufruf → kein Absturz, 6/16/9 Polygonpunkte, Dicke 0,02, Bezug Unterkante. Tür 0,885×2,01 → `SetObjectParametersForce` A/B → 1,01×2,135 (Details und GDL); Fenster 1,26×1,51 → `SetGDLParametersOfElements` → 1,76×1,26.

## ELM_SAB 0.9.32 — Zuverlässigkeit (Wunschliste Michi A1–A6) <!-- 2026-10-20 -->

Quelle: Wunschliste Michi 20.10. (Basis 0.9.18). Grundsatz: Gründe kommen als `[CODE]` am Anfang der Fehlermeldung, damit die Tapir-Antwortschemas gültig bleiben.

* **A1 Vorprüfung** (`ElmPreflight.hpp`): `NOT_FOUND`, `LAYER_HIDDEN`, `LAYER_LOCKED`, `LAYER_NOT_FOUND`, `ELEMENT_LOCKED`, `HOTLINK`, `RESERVED_BY_OTHER`, `WRONG_WINDOW_TYPE`, `UNDO_SCOPE_FAILED`, `NO_EFFECT`. Eingebaut in `MoveElements` (zusätzlich Wirkungsprüfung über die 3D-Box), `RotateElements`, `SetDetailsOfElements`, `DeleteElements` (blockierte werden übersprungen und mit Grund und Anzahl gemeldet) und alle Tapir-Erstellbefehle (Zielebene; Modellelemente zusätzlich Fenstertyp).
* **A4/A6 Ebenen:** alle 9 Tapir-Erstellbefehle (Columns, Slabs, Zones, Polylines, Objects, Lamps, Meshes, Labels, Texts) nehmen `layerName` (Vorrang) oder `layerIndex`; die Antwort meldet je Element `layerIndex`, `layerName`, `floorIndex`. `ELM_SAB.GetLayers {nameContains?}` liefert den echten Attribut-Index.
* **A5** `ELM_SAB.SetLayerAttributes {layers:[{layerName|layerIndex, hidden?, locked?, wireframe?}]}` mit Rücklese.
* **A2** `CreateObjects`/`CreateLamps`: `zReference: "absolute"` (Standard, wie bisher) oder `"story"` + `floorIndex`; Antwort `zAbsolute` und `zStoryRelative`.
* **A3** `ELM_SAB.GetSlabLevels {elements}` → `topZ`/`bottomZ` absolut (3D-Box), geschossbezogen, `referencePlane`, `referenceZ`, `thickness`.
* Windows AC27/28: MSVC behandelt Warnungen als Fehler — Flag-Typ in `SetLayerAttributes` per `decltype` vom Archicad-Feld (AC27/28 `short`).
* **Live verifiziert 20.10.** (leeres Projekt, Testebene `ZZ_KI_Test_0932`): GetLayers findet die Ebene mit echtem Index; Polylinie und Text per `layerName` auf genau dieser Ebene; Stuhl mit `zReference:"story"`, OG, z 2,70 → `zAbsolute` 5,70, 3D-Box-UK 5,70 (Michis Abnahme A2); ohne zReference → EG 2,70; unbekannte Ebene → `[LAYER_NOT_FOUND]`; Ebene ausgeblendet → Delete `[LAYER_HIDDEN]` und Element bleibt (Michis Abnahme A1), Move und Create ebenfalls abgelehnt; gesperrt → Move `[LAYER_LOCKED]`; entsperrt → Move ok. `GetSlabLevels` an den 3 Decken: UK 0,00 / OK 0,02. Testelemente gelöscht, 0 von 10 übrig.
* Fallen: `API.GetElementsByType` kennt `PolyLine` nicht (4002) → `TapirCommand.GetElementsByType`. B1 (Lampen) nicht prüfbar — die Standardbibliothek des leeren Projekts enthält keine Lampen-Bibliotheksteile; `TapirCommand.CreateLamps` existiert.
* Noch offen aus der Wunschliste: B1–B8, C1–C4 (C5 löst `elmonkey-mcp`).


## ELM_SAB 0.9.33 — Wunschliste Michi B2/B3/B6/B7/B8, Sparren-Fix <!-- 2026-10-20 -->

* **B2** `ELM_SAB.ChangePropertyDefinitions {propertyDefinitions:[{propertyId, name?, description?, addEnumValues?:[{displayValue, nonLocalizedValue?}], availability?:{set|add|remove:[{guid}]}}]}` — Eigenschafts-Definition ändern **ohne GUID-Wechsel** (bisher nur Löschen + Neu, GDL-Referenzen brachen). Vorhandene Auswahlwerte behalten ihre ID, doppelte Anzeigewerte werden übersprungen. Rücklese.
* **B3** `ELM_SAB.SetStructureOfElements {elements:[{elementId, structureType: basic|composite|profile, buildingMaterialName|-Index, compositeName|-Index, profileName|-Index}], reserve?}` — Aufbau BESTEHENDER Wände, Decken, Dächer, Schalen, Stützen, Unterzüge (bei Stütze/Unterzug alle Segmente). Vorprüfung, Teamwork-Reservierung, Rücklese (`[NO_EFFECT]` bei Abweichung).
* **B6** `TapirCommand.ApplyFavoritesToElementDefaults`: Dach-/Schalen-Favoriten scheiterten mit `APIERR_BADINDEX` (-2130313114) → bei Dach, Schale oder BADINDEX zweiter Versuch ohne Memo (`ACAPI_Element_ChangeDefaults (&elem, nullptr, &mask)`). **Live 20.10.: wirkungslos, siehe 0.9.35.**
* **B7** `ELM_SAB.SaveProjectCopy {targetPath, overwrite?}` — erst sichern, dann Datei kopieren; das offene Projekt bleibt das Original. Lehnt `[TEAMWORK]`, `[UNTITLED]`, `[ORIGINAL]` (Ziel = Original), `[EXISTS]`, `[NO_FOLDER]` ab. **Live 20.10.: Sichern scheiterte mit `APIERR_READONLY` → Fix in 0.9.34.** `format: tpl` aus der Wunschliste ist nicht umgesetzt.
* **B8** `TapirCommand.ChangeSelectionOfElements`: ein Sammelaufruf statt einzeln + Rücklese (große Auswahlen hängen die Oberfläche nicht mehr minutenlang).
* **Sparren-Fix** `TapirCommand.CreateBeams`/`ModifyBeams`: `slantAngle` wirkte nur zusammen mit `isSlanted` — Tapir setzte das Flag nicht, Archicad ignorierte den Winkel still. Jetzt `isSlanted = |slantAngle| > 1e-9` mit Maske.
* AC27/28: nur `IO::fileSystem.Contains (loc, &bool)` (die 1-Parameter-Form gibt es erst ab AC29) → Helfer `ExistsELM`.
* **Live verifiziert 20.10.** (Demo `training session.pln`): Sparren-Fix (Testsparren 8/20, 35°, 4,5 m: Hub 3,315 m, Oberkante auf den Millimeter), B3 (33/33 Unterzüge Holz basic; 2 Dächer composite „Oberdach ohne Sparren 8,5 cm“, Rücklese Dicke 0,085). B7 schlug fehl (siehe oben). B2, B6, B8 offen.

## ELM_SAB 0.9.34 — Modify2DElements (B4/B5), Speichern, CreateRoofs-Aufbau <!-- 2026-10-20 -->

* **B4/B5** `ELM_SAB.Modify2DElements {elements:[{elementId, …}], reserve?}` — bestehende Line/Arc/Circle/PolyLine/Spline/Hatch ändern, **GUID bleibt**. Nur angegebene Felder; Feldnamen wie `Create2DElements`.
  * alle: `layerName`/`layerIndex`, `drawIndex` (1..7); optional `elementType` als Kontrolle
  * Linien-Typen: `pen`, `lineTypeName`/`lineTypeIndex`, **`removeArrows: true`** (AC29 `startArrowData/endArrowData.arrowVisibility`, AC27/28 `arrowData.begArrow/endArrow`)
  * Geometrie: Line `begCoordinate`/`endCoordinate` (einzeln), Arc/Circle `origin`, `radius`, `begAngle`, `endAngle`, `ratio`, `angle`, `reflected`; PolyLine/Hatch/Spline nur als Ganzes über `coordinates` (+ `arcs`, `subPolyEnds`, `closed`, `bezierDirs`)
  * Hatch: `fillName`/`fillIndex` (stellt auf Füll-Schraffur um), `foregroundPen`, `backgroundPen`, `contourPen`, `contourLineTypeName`/`-Index`, `foregroundRGB`/`backgroundRGB`, `clearForegroundRGB`/`clearBackgroundRGB`, **`fillAngleDeg`** (Ausrichtung „gedreht“ + Drehmatrix wie DevKit `Do_Hatch_Edit`), `showArea`
  * Felder, die nicht zum Typ passen, werden abgelehnt (z. B. `pen` an Hatch, `fillName` an Line); Geschosswechsel nicht (neu anlegen); doppelte GUIDs in einer Liste abgelehnt; ohne änderbares Feld `[NO_CHANGE]`
  * Ablauf: Vorprüfung (`ElmPreflight`) → ein Undo-Schritt `ACAPI_Element_Change` → Rücklese je Element: Geometrie/Geschoss/Ebene wie Create (ohne neue Geometrie gilt die Ist-Geometrie als Soll), dazu jedes geänderte Attribut; Antwort `changed` + Ist-Werte, Abweichung `[NO_EFFECT]`
* **Speichern:** `ACAPI_ProjectOperation_Save ()` ohne Parameter lieferte live `APIERR_READONLY` (lokale .pln, eigene .lck, Cmd+S speichert) — auch `TapirCommand.SaveProject`. Neuer Helfer `SaveProjectInPlaceELM` (`ElmProjectSave.hpp`): `API_FileSavePars` mit `APIFType_PlanFile` + Originalpfad (wie `CaptureView`); Teamwork/unbenannt weiter parameterlos. Gilt für `SaveProjectCopy` (`[SAVE_FAILED]` mit Code-Name) und `SaveProject` (echter Code statt `APIERR_COMMANDFAILED`).
* **`ELM_SAB.CreateRoofs`:** neu `compositeName`, `buildingMaterialName`, `layerName`. `thickness` bei Mehrschicht-Werkzeugstandard stellt auf Basis-Aufbau um und meldet `structureNote` (vorher still wirkungslos: 34,5 statt 10 cm); `compositeName` + `thickness` zusammen wird abgelehnt. Antwort je Dach `structureType`, `compositeName`/`buildingMaterialName`, `thickness`, bei Vorgabe `thicknessMatches`.
* `Create2DElements` intern: Pfad-Geometrie-Prüfung, Memo-Aufbau und Rücklese als gemeinsame Helfer (`C2DParsePathGeometry`, `C2DBuildPolyMemo`, `C2DBuildSplineMemo`, `C2DReadBackFill`), Verhalten unverändert.
* Syntax geprüft gegen DevKit 29.3100 (C++20) und 28.3001 (C++17, AC27/28-Zweig der Pfeilspitzen).
* **Live 20.10.** (AC29 Mac): `training session.pln` 8/8 — SaveProjectCopy (Kopie bytegleich groß, aktives Projekt bleibt), `[EXISTS]`, `[ORIGINAL]`, `[NO_FOLDER]`, Tapir `SaveProject` (mtime springt). Leeres Projekt aus der Bürovorlage 30/37 → Befunde in 0.9.35 behoben: Polygon-Ersatz `APIERR_BADPOLY`, `drawIndex` wirkungslos, RGB+Stift, RGB-Toleranz. B2 live bestätigt (nach Testaufbau-Korrektur: Tapir `CreatePropertyDefinitions` braucht bei `singleEnum` einen `defaultValue`, z. B. `{"basicDefaultValue":{"type":"singleEnum","status":"userUndefined"}}`, sonst `APIERR_BADVALUE`).

## ELM_SAB 0.9.35 — Modify2DElements: Polygon-IDs, drawIndex/RGB-Regeln; Favoriten-Wege <!-- 2026-10-20 -->

* **Polygon-Ersatz** (PolyLine/Hatch über `coordinates`): `ACAPI_Element_Change` lehnte ein frisches Memo ohne `vertexIDs` mit `APIERR_BADPOLY` ab. DevKit `API_Polygon`: jede Koordinate hat eine vertexID, der Schlusspunkt eines Rings trägt die ID seines Startpunkts, Zelle 0 = größte ID, beim Ändern darf die größte ID nicht sinken. Jetzt neue IDs oberhalb der bisherigen Höchst-ID; bis zu vier Wege (A neues Memo + vertexIDs, B wie Tapir `ModifySlabs`, C + Kanten-/Kontur-IDs, D ohne Zähler-Maske), Antwort `polygonVariant`. **Live: Weg A genügt.**
* **`drawIndex`** wird von `ACAPI_Element_Change` ignoriert (live: 3 gesetzt, 9 blieb) → in `Modify2DElements` klar abgelehnt. Beim Anlegen (`Create2DElements`) wirkt es; Archicad-Standard ist 9.
* **RGB:** Mit RGB-Vordergrund setzt Archicad den Vordergrundstift selbst auf 1 (auch beim Anlegen) → `foregroundPen` + `foregroundRGB` (bzw. Hintergrund) im selben Eintrag abgelehnt. Archicad speichert RGB in 8 Bit (0,3 → 76/255 = 0,298) → Rücklese-Toleranz 1/255.
* **B6** `ApplyFavoritesToElementDefaults`: zusätzlich ohne Geschoss des Favoriten, dann ohne Geometrie (Dach `roofClass` + `u`; Schale `u`, `basePlane`, `hasContour`, `numHoles`). **Live weiter `APIERR_BADINDEX`** für alle Dach-/Schalen-Favoriten der Bürovorlage („Unterdach+Dämmung 21,5cm“, „Oberdach 8,5cm“, „Dach komplett, Stehfalz 34,5cm“, Schale „Dach komplett 30cm“); Decken-/Wand-Favoriten gehen; ein im Projekt per `CreateFavoritesFromElements` erzeugter Dach-Favorit geht. Ursache liegt in den gespeicherten Vorlagen-Favoriten (Feld noch nicht ermittelt) → offen; Umweg `CreateRoofs compositeName` oder eigener Favorit.
* **Live verifiziert 20.10.** (AC29 Mac, leeres Projekt aus der Bürovorlage, Port 19723): **40/41** — Create2DElements (6 Typen + Hatch mit Loch, Regression nach Helfer-Umbau), Modify Line (Stift, Linientyp, Pfeile aus, Endpunkt, GUID bleibt, Gegenlese `Get2DGeometryOfElements`), Arc (Radius/Endwinkel), Circle (Mitte/Stift), PolyLine (neue Geometrie geschlossen mit Bogen, Pfeile aus, Weg A), Spline (geschlossen), Hatch (Füllung, alle Stifte, Konturlinientyp, 30°, Fläche; RGB VG+HG gegengelesen per `GetPenOfElements`; RGB löschen + Polygon mit Loch, Fläche 19 m², Winkel bleibt), Ebene für 2 Elemente, 9 Fehlerfälle, CreateRoofs (thickness 0,12 mit `structureNote` „Komplettdach Stehfalz 34,5 cm“ → basic; `compositeName` 0,085; Widerspruch und unbekannter Aufbau abgelehnt), B2 (A,B → A,B,C, Duplikat übersprungen, Name, GUID gleich, Gegenlese native API), B8 (600 Elemente in 0,1 s), B7 `[UNTITLED]`. Offen nur B6. Testelemente und Test-Eigenschaft entfernt. Skript: `~/.scratch/elmonkey/tools/gegenprobe_0934.py <port> datei|leer [version]`.

## ELMonkey 0.10.0 — Tapir-Unterbau 1.7.2 + Archicad 30 <!-- 2026-10-20 -->

* **Drei-Wege-Merge** (Arbeitsrepo `~/.scratch/elmonkey/tapir-merge`): Basis = Upstream-Commit `18734069` (15.07., entspricht unserem Import 79f1218 bis auf die bewusst geänderten Dateien), ours = ELMonkey 0.9.35, theirs = Tapir 1.7.2. 225 Upstream-Dateien geändert (140 neu), 21 Überschneidungen, 11 Dateien mit Konflikten. 314 Tapir- + 44 ELM_SAB-Befehle.
* **Tapirs Lösung übernommen, unsere entfernt:** Decken-Memo frei-dann-neu-anlegen (#452 = Maxls Absturz), Unterzug `isSlanted` (#508, respektiert explizites `isSlanted`), `SetGDLParametersOfElements` mit echtem Elementtyp (Tür/Fenster/Oberlicht), `SaveProject` mit kurzem Grundriss-Wechsel (#681: das parameterlose Sichern wirkt aufs aktive Fenster; aus 3D/Schnitt → APIERR_READONLY). Unser 0.9.34-Weg (Dateiangabe auf den eigenen Pfad) hätte ein schreibgeschützt geöffnetes Projekt überschrieben → `SaveProjectCopy` nutzt jetzt denselben Grundriss-Weg (`ElmProjectSave.hpp`).
* **Zusammengeführt:** Erstellbefehle = Tapirs Basisschleife (Favorit je Element, Neustart vom Werkzeug-Standard je Element) + unsere Fenster-/Ebenen-Vorprüfung, `layerName`, `zReference`, Rücklese; `layerName`/`layerIndex` in allen Create-Schemas (auch im Linien-Baustein und im Objekt/Lampen-Baustein). Move/Rotate = unsere Vorprüfung + Tapirs `APIFilt_IsEditable`-Sperre + unsere Bewegungs-Rücklese. `DeleteElements` = Tapirs Ergebnis **je Element** (`executionResults[]`, vorher ein Gesamtergebnis) + Teamwork-Reservierung + Vorprüfung mit Grund je Element.
* **VersionChecker:** Tapirs Hintergrund-Prüfung (Thread, 15 s Zeitlimit, Archicad wartet beim Start nicht) gegen das ELMonkey-Release (`ELMonkey X.Y.Z` im Namen, Vergleich mit `ELM_SAB_VERSION`), dazu `Refresh`/`ReleasePageUrl` für den Über-Dialog. Tapirs Selbst-Update/Installer-Lauf bleibt aus: Laufzeit-Sperre (`static volatile bool`) statt `#if 0`, damit die Helfer benutzt bleiben (macOS `-Werror -Wunused-function`, MSVC C4505) und nichts unerreichbar ist (C4702).
* **Antwortschema:** Tapirs `CommandBase` meldet Archicad kein Antwortschema mehr (Fehler kamen sonst als 4009); Doku über `GetRawResponseSchema`. `ELMCommandBase::GetRawResponseSchema` liefert für die ELM-Befehle ihr bisheriges Schema.
* **AC30:** CI-Matrix um AC30 (DevKit 30.3000, v143/macos-latest) erweitert, Installer erkennen Archicad 30. DevKit 30: `GS::UniString::ToLowerCase` → `GetLowerCased` (in FindElements per Makro für `ServerMainVers_3000`).
* **Prüfung:** alle 66 Quellen lokal mit den macOS-CI-Schaltern (`-Wall -Wextra -Werror` + Upstream-Ausnahmen) gegen DevKit 27.3001/28.3001/29.3100; CI für AC27–30 × Mac/Win. Neue Werkzeuge: `~/.scratch/elmonkey/tools/syn1.sh <27|28|29> <datei>` (`STRICT=1`), `conflict.py`, `watch_ci_install.sh`.
* **Live verifiziert 21.10.** (AC29 Mac, leeres Projekt aus der Bürovorlage, Release-Bundle): `gegenprobe_0100.py` **11/11** (Maxls Fall Favorit „Bodenabdeckung“ + 3 Decken ohne Absturz, `favoriteName` + `floorIndex` → Geschoss 1, Sparren Hub 3,315, `zReference:story` → zAbsolute 3,0, Move/Delete auf ausgeblendeter Ebene → `[LAYER_HIDDEN]` je Element, sichtbares gelöscht, Katalog 314 + 44) und `gegenprobe_0934.py … 0.10.0` **40/41** wie unter 0.9.35 (offen B6) → keine Regression. `gegenprobe_0100.py <port> datei` in `training session.pln` **8/8**: aus dem 3D-Fenster `SaveProject` (Datei geschrieben) und `SaveProjectCopy` (Kopie gleich groß, aktives Projekt unverändert), danach jeweils wieder 3D-Fenster aktiv (#681-Weg). Summe 59/60.
* Testskripte: `gegenprobe_0934.py <port> leer 0.10.0` (bisherige Reihe) + `gegenprobe_0100.py <port> leer|datei` (Maxls Decken-Fall, Favorit je Element + `floorIndex`, Sparren, `zReference`, Move/Delete auf ausgeblendeter Ebene je Element, Speichern aus dem 3D-Fenster, Katalog 314+44).

## ELMonkey 0.10.1 — Bauteilstifte lesen und setzen <!-- 2026-10-29 -->

* **Anlass:** THN-Ausführungspläne zeigen rote Wandkonturen aus der Zeit, als die Brandschutzeigenschaften in der Wand gepflegt wurden. Brandschutz läuft jetzt über die grafische Überschreibung; die Bauteile sollen schwarz-weiß werden, die Schraffur wieder aus Baustoff/Verbund kommen (keine Überschreibung der Schnittflächenstifte am Element).
* **Warum neu:** Weder Tapir 1.7.2 noch die offizielle JSON-API haben einen Befehl für die Stifte von Bauteilen (am 29.10. mit drei Discovery-Abfragen geprüft). `ELM_SAB.GetPenOfElements`/`SetPenOfElements` können nur 2D-Elemente. Tapirs `GetDetailsOfElements` liefert `cutFillPen` nur für Wand/Unterzug/Decke als Teil der Details, ohne Setzen. Über Eigenschaften sind die Stifte nicht lesbar.
* **Typen:** Wand, Stütze, Unterzug, Decke, Dach, Schale, Fassade, Morph. Andere Typen → `APIERR_BADELEMENTTYPE` je Element.

### `ELM_SAB.GetConstructionPensOfElements` (nur lesen)

Eingabe `{elements:[{elementId:{guid}}], includeComposites?}`. Ausgabe je Element: `elementType`, `layer {index, name, hidden, locked}`, `floorIndex`, `structure` (`structureType` basic/composite/profile + Index/Name; Stütze/Unterzug aus Segment 0 + `nSegments`; Fassade ohne, Morph nur Baustoff), `pens` (DevKit-Feldnamen; `cutFillPen`/`cutFillBackgroundPen` als `{overridden, pen}` — `pen` ist nur bei `overridden:true` maßgeblich) und `ltypes` (Linientyp-Indizes getrennt). Mit `includeComposites:true` top-level `composites[]` je benutztem Mehrschicht-Aufbau genau einmal: `{compositeIndex, compositeName, skins:[{index, framePen, buildingMaterialIndex}], lines:[{index, linePen, ltypeIndex}]}`; Bezug über `structure.compositeIndex`.

```json
{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"ELM_SAB","commandName":"GetConstructionPensOfElements"},
 "addOnCommandParameters":{"elements":[{"elementId":{"guid":"…"}}],"includeComposites":true}}}
→ {"pensOfElements":[{"success":true,"elementType":"Wall","layer":{"index":12,"name":"…","hidden":false,"locked":false},"floorIndex":0,
   "structure":{"structureType":"composite","compositeIndex":7,"compositeName":"…"},
   "pens":{"contPen":20,"contPen3D":20,"aboveViewLinePen":3,"cutFillPen":{"overridden":true,"pen":20},"cutFillBackgroundPen":{"overridden":false,"pen":0}},
   "ltypes":{"contLtype":1}}],
   "composites":[{"compositeIndex":7,"compositeName":"…","skins":[{"index":0,"framePen":20,"buildingMaterialIndex":5}],"lines":[{"index":0,"linePen":1,"ltypeIndex":1}]}]}
```

### `ELM_SAB.SetConstructionPensOfElements` (schreibt)

Eingabe je Element (alle optional außer `elementId`): `cutLinePen`, `uncutLinePen`, `overheadLinePen` (1–255, Rollentabelle), `clearCutFillOverride` (`cutFillPen` + `cutFillBackgroundPen` → nicht überschrieben), `fields {<DevKit-Feld>: pen}` (Rohfelder, Namen wie in `pens`; ein `OverriddenPen`-Feld per `fields` = Überschreibung auf diesen Stift). Dieselben fünf Felder top-level als Vorgabe für alle Elemente (Element-Wert gewinnt), dazu `dryRun` (Standard false) und `reserve` (Standard true). Reihenfolge der Planung: Rollen → `clearCutFillOverride` → `fields` (gewinnt). Ohne jede Änderungsangabe → `APIERR_BADPARS`.

Ablauf: Vorprüfung je Element (`ElmPreflight`) → Planung `{field, before, after}` (schon stimmende Werte zählen nicht; Element ohne Änderung → `unchanged:true`, wird weder reserviert noch geschrieben) → bei `dryRun` Ende (keine Reservierung, kein `ACAPI_Element_Change`) → Teamwork: nur Elemente mit Änderung reservieren, die mir noch nicht gehören; danach Header neu lesen → **ein** Undo-Schritt „ELM_SAB SetConstructionPensOfElements“ für alle (Maske nur für geänderte Felder) → Rücklese je Feld (`readBack`) → nur selbst gesetzte Reservierungen freigeben (vorher eigene bleiben). Antwort: `executionResults[]` in Eingabereihenfolge, `undoScope {executed, errorCode}` (nicht bei dryRun), `dryRun`, `summary {requested, changed, unchanged, failed}`.

```json
{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"ELM_SAB","commandName":"SetConstructionPensOfElements"},
 "addOnCommandParameters":{"elements":[{"elementId":{"guid":"…"}}],"cutLinePen":1,"clearCutFillOverride":true,"dryRun":true}}}
→ {"executionResults":[{"elementId":{"guid":"…"},"success":true,"elementType":"Wall","changes":[
     {"field":"contPen","before":{"pen":20},"after":{"pen":1}},
     {"field":"cutFillPen","before":{"overridden":true,"pen":20},"after":{"overridden":false,"pen":20}}]}],
   "dryRun":true,"summary":{"requested":1,"changed":1,"unchanged":0,"failed":0}}
```

### Rollentabelle (`cutLinePen` / `uncutLinePen` / `overheadLinePen`)

| Typ (API) | cutLinePen → | uncutLinePen → | overheadLinePen → | clearCutFillOverride |
|---|---|---|---|---|
| Wand (API_WallID) | contPen | contPen3D (Annahme „Unverdeckte Linien", im Live-Test am Dialog belegen) | aboveViewLinePen | cutFillPen + cutFillBackgroundPen = APINullValue |
| Stuetze (API_ColumnID) | corePen UND venLinePen (beide sind Schnittkonturen: Kern + Verkleidung) | belowViewLinePen | aboveViewLinePen | wie Wand |
| Unterzug (API_BeamID) | cutContourLinePen | belowViewLinePen | aboveViewLinePen | wie Wand |
| Decke (API_SlabID) | sectContPen (laut CONTEXT) | pen (Kontur) | kein Feld -> [FIELD_NOT_AVAILABLE] | wie Wand |
| Dach (API_RoofID) | shellBase.sectContPen | shellBase.pen | shellBase.aboveViewLinePen | shellBase.cutFillPen + shellBase.cutFillBackgroundPen |
| Schale (API_ShellID) | shellBase.sectContPen | shellBase.pen | shellBase.aboveViewLinePen | wie Dach |
| Fassade (API_CurtainWallID) | frameCutLinePen UND panelCutLinePen | uncutLinePen | overheadLinePen | wie Wand |
| Morph (API_MorphID) | cutLinePen | uncutLinePen | overheadLinePen | wie Wand |

Alle übrigen Stiftfelder (`coreSymbolPen`, `hiddenLinePen`, `refPen`, `hiddenContourLinePen`, `coverFill*Pen`, `floorFill*Pen`, Fassaden-`pen` …) über `fields`; Linientypen (`contLtype`, `refLtype`, `ltypeInd`, `sectContLtype`) nur lesen.

### Fallen (je Element gemeldet, nicht still übersprungen)

* `[LAYER_HIDDEN]` / `[LAYER_LOCKED]` aus der Vorprüfung; lehnt `ACAPI_Element_Change` trotzdem mit `APIERR_NOACCESSRIGHT` ab → `[LAYER_HIDDEN?] … 6001 = meist ausgeblendete Ebene oder Hotlink` (Teamwork-6001-Befund).
* `[HOTLINK]`, `[ELEMENT_LOCKED]`, `[RESERVED_BY_OTHER]` (auch wenn die Reservierung abgelehnt wurde).
* `[FIELD_NOT_AVAILABLE]` — Rolle ohne Feld für den Typ (z. B. `overheadLinePen` an Decke) oder Feldname in `fields` unbekannt/kein Stift.
* `[UNDO_SCOPE_FAILED]` — Undo-Klammer hat das Lambda nicht gestartet; **kein** Rückfall ohne Undo-Klammer.
* `[NO_EFFECT]` — Rücklese weicht vom Soll ab.
* **Rot im Element oder im Verbund:** zeigt `includeComposites` rote `framePen`/`linePen` im Mehrschicht-Aufbau, ist das kein Element-Lauf mehr, sondern eine eigene Entscheidung (Attribut-Änderung wirkt auf alle Bauteile mit diesem Aufbau).

### DevKit-Befund und Stand

* Feldnamen und -typen in DevKit 27.3001, 28.3001, 29.3100 und 30.3000 gleich → keine `#if`-Wächter. `API_OverriddenPen = APIOptional<API_PenIndex>` mit öffentlichen Membern `hasValue`/`value` (keine `HasValue()`-Methode), Löschen per `= APINullValue`.
* Maske per Feldzeiger: `ACAPI_ELEMENT_MASK_SET` setzt nur das erste Byte des Feldes auf -1; die Feldtabelle setzt dasselbe Byte über den Offset im Masken-Element.
* Katalog jetzt 314 Tapir- + 46 ELM_SAB-Befehle.
* **Status:** Syntax STRICT=1 gegen DevKit 27/28/29/30 grün (`syn1.sh` kennt jetzt `30`); CI und Live-Test im leeren Testprojekt offen (`gegenprobe_0100.py <port> stifte`) — **nie THN**.
