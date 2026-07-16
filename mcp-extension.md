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
