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
(„Einige Add-Ons konnten nicht geladen werden").

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
  polygonOutline** (Host-Matching!), `API.Get2DBoundingBoxes` (offizieller
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
- **Reservierung ist flüchtig**: `ReserveElements` muss UNMITTELBAR vor der Schreibaktion im selben Ablauf stehen — ein Reserve aus einem früheren Call reicht nicht, SetDetailsOfElements liefert dann „Failed to change element" (-2130312912). Muster: pro Batch Reserve→Write→Read-back. Layer-Wechsel für 2D-Elemente (Line etc.) via `SetDetailsOfElements {elementsWithDetails:[{elementId,details:{layerIndex}}]}` funktioniert typunabhängig (live-verifiziert THN 2026-07-21, 442 Linien).
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
- **Silent-No-Op-Ursache #3: GRUPPEN (2026-07-21)**. Elemente in Gruppen verweigern Einzeländerungen (SetDetails/Delete: „Failed to change element" -2130312912 bzw. still wirkungslos), solange „Gruppierung unterbrechen" (Alt+G) AUS ist — kein Reservierungskonflikt, kein Hotlink, kein Lock. Diagnose-Reihenfolge bei wirkungslosen Writes: (1) ReserveElements-conflicts[] prüfen (Fremd-Reservierung), (2) UnlockElements testen, (3) GetHotlinks (leer ⇒ kein Hotlink), (4) User „Gruppierung unterbrechen" einschalten lassen → Retry. Live THN: 98 gruppierte Höhenkoten-Symbole erst nach Alt+G änderbar; erklärt rückwirkend auch die „unlöschbaren" Alt-Etiketten.

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
