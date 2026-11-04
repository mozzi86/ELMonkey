---
quick_id: 261013-fel
type: quick
autonomous: true
files_modified:
  - ELM_SAB_Tapir/Sources/FindElementsInternal.hpp
  - ELM_SAB_Tapir/Sources/FindElementsCommand.hpp
  - ELM_SAB_Tapir/Sources/FindElementsCommand.cpp
  - ELM_SAB_Tapir/Sources/FindElementsLeaves.cpp
  - ELM_SAB_Tapir/Sources/IFCCommands.cpp
  - ELM_SAB_Tapir/Sources/IFCCommands.hpp
  - ELM_SAB_Tapir/Sources/AddOnMain.cpp
  - ELM_SAB_Tapir/Sources/AddOnVersion.hpp
  - ELM_SAB_Tapir/Sources/ELMCommandBase.hpp
  - ELM_SAB_Tapir/Sources/CaptureViewCommand.hpp
  - reference/mcp-extension.md
  - ELM_SAB_Tapir/README.md
must_haves:
  truths:
    - "ELM_SAB.FindElements ist im Namespace ELM_SAB registriert (Gruppe 'ELM_SAB Commands', Version '0.9.23', deutsche Kurzbeschreibung)"
    - "ELM_SAB_VERSION = 0.9.23, ADDON_VERSION bleibt 1.5.4"
    - "where-Baum mit and/or/not beliebig tief; günstige Blätter zuerst, Kurzschluss in and/or; alle 18 Blätter aus dem Handoff (type, story, layer, property, classification, libPart, gdl, ifc, flags, teamwork, hostedBy, hosts, inZone, bbox, near, guids, inSelection, designOption) werden geparst und ausgewertet"
    - "Unbekannte Schlüssel, Blätter, Operatoren, Eigenschaften, Klassifizierungen, Ebenen, Geschosse, Design-Optionen = Fehler mit Namen (mehrdeutige Eigenschaftsnamen mit Kandidatenliste); in AC27/28 nicht verfügbare Blätter = Fehler 'Kriterium X in Archicad NN nicht verfügbar'"
    - "Ausgewertet wird IMMER in der Grundriss-Datenbank (alle Geschosse) über ExecuteActionForEachDatabase, das die Ausgangs-Datenbank auf jedem Pfad zurückschaltet; optional 'databases' statt Grundriss"
    - "scope projekt/wie3D/sichtbar/aktuellesGeschoss = GetElemList-Filter None / OnVisLayer|In3D / OnVisLayer / OnActFloor"
    - "Eigenschaften werden typisiert verglichen (Zahl/Bool/Text/Aufzählung); Aufzählungen passen auf nonLocalizedValue UND Anzeige-String; Werte je Element gebündelt (ein API-Aufruf für alle benötigten Eigenschaften) und gecacht"
    - "Antwort: je Treffer elementId/type/floorIndex/storyName/layerName (+values aus return) in EINEM Aufruf; scopeCount, matchCount, returnedCount, truncated, criteriaStats je Knoten, elapsedMs, startDatabaseType; countOnly und limit (Vorgabe 20000)"
    - "select:true ersetzt die Auswahl durch die Treffer und meldet per Rücklese selection{requested, selected, notSelectedCount, notSelected[]}"
    - "FindElements und CaptureView deklarieren ihr Antwortschema als oneOf (Erfolg | Fehlerform {error:{code,message}}) über ELMResponseSchemaWithError — Fehlermeldungen kommen beim Client an statt 4009"
    - "CaptureView nennt in jeder Fehlermeldung den Fenstertyp (WindowTypeNameELM) und den echten Archicad-Fehlercode; Layout/MasterLayout: Export wird versucht, bei Fehler klare Ablehnung mit Hinweis auf Zeichnung/Grundriss bzw. PDF-Publizieren; Erfolgsantwort {imagePath, view2D} unverändert"
    - "CI auf ci-status zeigt für den gepushten HEAD-SHA alle sechs Builds (AC27/28/29 × Mac/Win) ohne 'fehlgeschlagen' (oder Abbruch nach 4 Fix-Runden sauber dokumentiert)"
  artifacts:
    - path: "ELM_SAB_Tapir/Sources/FindElementsInternal.hpp"
      contains: "namespace FindElementsELM"
    - path: "ELM_SAB_Tapir/Sources/FindElementsCommand.hpp"
      contains: "class FindElementsCommand : public ELMCommandBase"
    - path: "ELM_SAB_Tapir/Sources/FindElementsCommand.cpp"
      contains: "ExecuteActionForEachDatabase"
    - path: "ELM_SAB_Tapir/Sources/FindElementsLeaves.cpp"
      contains: "ACAPI_Element_GetPropertyValuesByGuid"
    - path: "ELM_SAB_Tapir/Sources/IFCCommands.hpp"
      contains: "ELMGetIFCData"
    - path: "ELM_SAB_Tapir/Sources/AddOnMain.cpp"
      contains: "RegisterCommand<FindElementsCommand>"
    - path: "ELM_SAB_Tapir/Sources/AddOnVersion.hpp"
      contains: "#define ELM_SAB_VERSION \"0.9.23\""
    - path: "ELM_SAB_Tapir/Sources/ELMCommandBase.hpp"
      contains: "ELMResponseSchemaWithError"
    - path: "ELM_SAB_Tapir/Sources/CaptureViewCommand.hpp"
      contains: "WindowTypeNameELM"
    - path: "reference/mcp-extension.md"
      contains: "## ELM_SAB 0.9.23"
    - path: "ELM_SAB_Tapir/README.md"
      contains: "FindElements"
  key_links:
    - from: "AddOnMain.cpp"
      to: "FindElementsCommand.hpp"
      via: "#include + RegisterCommand<FindElementsCommand> (elmSabCommands, \"0.9.23\", …)"
    - from: "FindElementsCommand.cpp (Engine, Task 1)"
      to: "FindElementsLeaves.cpp (Task 2)"
      via: "Vertrag in FindElementsInternal.hpp: CreateValueLeafState, ParseValueLeaf, EvaluateValueLeaf, ParseReturnList, HasReturnColumns, AddReturnColumns, AddReturnValues"
    - from: "FindElementsLeaves.cpp (ifc-Blatt)"
      to: "IFCCommands.cpp"
      via: "exportierter Helfer ELMGetIFCData nutzt die vorhandenen statischen GetPropertyValueString-Varianten (AC27 / AC28+)"
    - from: "CaptureViewCommand.hpp / FindElementsCommand.cpp"
      to: "ELMCommandBase.hpp"
      via: "ELMResponseSchemaWithError (oneOf Erfolg|Fehler), CreateErrorResponseELM (Meldung als GS::UniString), WindowTypeNameELM"
    - from: "FindElementsCommand::Execute"
      to: "Grundriss-Datenbank"
      via: "ExecuteActionForEachDatabase ({FloorPlan-ID}) — schaltet am Ende IMMER auf die Ausgangs-Datenbank zurück (CommandBase.cpp Z. 648)"
---

# Quick 261013-fel: ELM_SAB 0.9.23 — `FindElements` mit und/oder/nicht + CaptureView-Fehlerantwort

**Das 0.9.23-Bundle enthält ZWEI Änderungen:** (1) neuer Lesebefehl `FindElements` (Tasks 1–2), (2) Fix am bestehenden
`CaptureView` — Fehlerantwort schemakonform, Layout-Verhalten (Task 3, Nachtrag des Nutzers vom 13.10.). Doku + CI in Task 4.

**Anlass:** WiSchu-Türenabgleich 13.10.: Tapir `GetElementsByType` lieferte morgens 0 Türen (liest die Datenbank
des AKTIVEN Fensters — Layout vorn), native API 267. Heute braucht „Türen mit T.-Nummer, nicht Abbruch, je Geschoss"
4 Aufrufe + Python. Ziel: EIN Lesebefehl wie „Suchen & aktivieren" — Kriterien beliebig tief mit and/or/not,
immer Grundriss-Datenbank, Suchbereich `scope`, `criteriaStats`, optionale Auswahl.
Spezifikation: `/Users/ap/.scratch/elmonkey/handoff_elmonkey_findelements_2026-10-13.md` (Abschnitt 2 = Umfang, vom Nutzer freigegeben).

**Feste Vorgaben (nicht neu verhandeln):** Nur lesen (keine Modelländerung, keine Undo-Klammer). Kein lokaler Build
(Bauen = Push → CI AC27/28/29 × Mac/Win). C++17 (AC27/28). Kein Bundle-Tausch, kein Archicad-Aufruf durch den Executor.
`RunCriteriaSet` entfällt (DevKit 29.3100 hat keine API für gespeicherte Kriterien-Sets — nur `ACAPI_Selection_*`,
`ACAPI_Element_SearchElementByCoord`, `ACAPI_LibraryPart_Search`) → nur in der Doku vermerken.
Commits direkt auf `main`, Push erst in Task 4. Keine `.planning`-Dateien committen (macht der Orchestrator).

## Vorab geprüft (Planer) — Fundstellen, keine erneute Suche nötig

Alle Pfade relativ zu `ELM_SAB_Tapir/Sources/`, wenn nicht anders angegeben. Zeilen = Stand `fad4abb`.

**Gerüst / Registrierung / Build**
- `ELMCommandBase.hpp` Z. 13–55: Basisklasse (Namespace "ELM_SAB", Main-Thread, `GetSchemaDefinitions` = NoValue → im
  Eingabeschema KEIN `$ref` möglich, nur Inline-Schema). Muster eines schlanken Lesebefehls: `LabelCommandsELM.hpp`
  Z. 21–27 (Klasse), `LabelCommandsELM.cpp` Z. 277–302 (Inline-Schemas als `R"(...)"`), Z. 304–374 (Execute).
- `AddOnMain.cpp`: ELM-Includes Z. 43–65 (neues `#include "FindElementsCommand.hpp"` nach Z. 65); Gruppe „ELM_SAB Commands"
  Z. 954–1068, letzter Eintrag `RegisterCommand<SetLayerOfElementsCommand>` Z. 1064–1067, `AddCommandGroup (elmSabCommands)` Z. 1068.
  `RegisterCommand<T>` (Z. 67ff.) braucht einen Default-Konstruktor.
- `AddOnVersion.hpp` Z. 11 `#define ELM_SAB_VERSION "0.9.22"`; Z. 5 `ADDON_VERSION "1.5.4"` NICHT ändern.
- `Tools/CMakeCommon.cmake` Z. 156–163: `file (GLOB_RECURSE … Sources/*.cpp)` → neue .cpp/.hpp werden automatisch gebaut,
  CMakeLists NICHT anfassen. Z. 17–23: AC27/28 = `cxx_std_17`, AC29 = `cxx_std_20`. Z. 26–31 Windows `/W4 /WX`;
  Z. 34–45 Mac `-Wall -Wextra -Werror` (unused-parameter/-value/-private-field ausgenommen).
- CI baut AC29 gegen DevKit **29.3000** (`.github/workflows/build-elm-sab-tapir.yml` Z. 53/121), AC28 gegen 28.3001, AC27 gegen 27.3001.
  Ergebnis immer auf Zweig `ci-status` (`status.md`, `logs/build-{mac,win}-AC{27,28,29}.log`, Fehler als `**fehlgeschlagen**`).
  `gh` ist in der Planer-Shell NICHT angemeldet → ci-status ist die verlässliche Quelle.

**Versionssicherheit (per Range-Download der offiziellen DevKit-Zips 27.3001 / 28.3001 geprüft)**
- `GS::ObjectState::GetType (fieldName)` (Typen Bool/UInt/Int/Real/String/List/Object) und `GetFieldNames ()` (HashSet<GS::String>)
  existieren in AC27, AC28, AC29 identisch. `EnumerateFields` hat in AC27 eine andere Signatur → NICHT benutzen.
- `API_RoomRelation` (elementsGroupedByType `GS::HashTable<API_ElemType, GS::Array<API_Guid>>`, wallPart/beamPart/cwSegmentPart je mit `.guid`)
  ist in AC27/28/29 gleich; `ACAPI_Element_GetRelations` und `ACAPI_DisposeRoomRelationHdls` existieren in AC27 und AC29.
- HashTable-Range-for unterscheidet sich: AC28+ `kv.key`/`kv.value`, AC27 `*kv.key`/`*kv.value` — exakt das Muster aus
  `AttributeCommands.cpp` Z. 268–282 (`#ifdef ServerMainVers_2800`) übernehmen.
- `ACAPI_Classification_GetClassificationSystems` / `…SystemRootItems` / `…ItemChildren`, `ACAPI_Selection_DeselectAll`,
  `ACAPI_Element_GetIFCAttributes` (AC27) existieren in AC27; IFCAPI `PropertyAccessor::GetAttributes ()` existiert in AC28 und AC29.
- `API_ClassificationItem` {guid, id, name, description}; `API_ClassificationSystem` {guid, name, description, source, editionVersion, editionDate}
  (DevKit 29 `APIdefs_Environment.h` Z. 1137–1190) → zum Unterscheiden gleichnamiger Systeme `editionVersion` nutzen, NICHT `editionDate` (Typ versionsabhängig).
- Es gibt KEINE Mehr-Element-Property-API (DevKit 29 `ACAPinc.h` Z. 4902/4938 nur je Element) → „gebündelt" heißt: je Element EIN
  `ACAPI_Element_GetPropertyValuesByGuid`-Aufruf mit ALLEN im Baum + `return` benötigten Property-GUIDs, nur für die noch offene Kandidatenmenge, Ergebnis gecacht.

**Wiederverwendbare Tapir-Helfer (Deklarationen in `CommandBase.hpp`, versionsneutral, unguarded im Bestand genutzt)**
- `CreateErrorResponse` Z. 38, `GetGuidFromObjectState` Z. 42, `CreateElementIdObjectState` Z. 63, `CreateGuidObjectState` Z. 71,
  `GetElementTypeNonLocalizedName` / `GetElementTypeFromNonLocalizedName` Z. 92/93 (Namen wie „Door", „Wall"; Implementierung `CommandBase.cpp` Z. 374–540),
  `GetAttributeGuidFromIndex` Z. 96, `DatabaseIdResolver` Z. 100–112 (FloorPlan-ID-Konstante `CommandBase.cpp` Z. 610),
  `ExecuteActionForEachDatabase` Z. 114–118 (Implementierung `CommandBase.cpp` Z. 616–653: merkt Ausgangs-DB, wechselt je ID per
  `ACAPI_Window_GetDatabaseInfo` + `ACAPI_Database_ChangeCurrentDatabase`, schaltet in Z. 648 IMMER zurück, keine frühen Returns in der Schleife).
- `MigrationHelper.hpp`: `GetElemTypeId (const API_Elem_Head&)` Z. 457, `GetAttributeIndex (API_AttributeIndex)` Z. 497, `DisposeAttribute` Z. 539.
- Geschossnamen: `ProjectCommands.cpp` Z. 515–546 (`ACAPI_ProjectSetting_GetStorySettings`, `(*storyInfo.data)[i].index/.uName`, danach `BMKillHandle` auf `storyInfo.data`).
- Teamwork: `InspectionCommands.cpp` Z. 155–163 (`ACAPI_ProjectOperation_Project` → `projectInfo.userId`), Z. 373 (`projectInfo.teamwork`),
  Z. 222–251 (`header.lockId` = Nutzer-ID des Reservierenden in Teamwork; frei = 0).
- Ebenen: `AttributeCommands.cpp` Z. 94–103 (`ACAPI_Attribute_GetAttributesByType (API_LayerID, attrs)`, Name `GS::UniString (attr.header.name)`,
  `DisposeAttribute`); Ebenen-Flags `InspectionCommands.cpp` Z. 202–221 (`APILay_Hidden`, `APILay_Locked`); Ebenen-Kombination
  `AttributeCommands.cpp` Z. 236–290 (`ACAPI_Attribute_GetDef (API_LayerCombID, …)`, `layer_statItems`, Versions-Split Z. 268–282, `ACAPI_DisposeAttrDefsHdls`).
  Namen-Cache-Muster `LabelCommandsELM.cpp` Z. 25–44 (anonymer Namespace → kopieren, nicht verlinken).
- Element-Liste und Filter: `ElementCommands.cpp` Z. 17–44 `ConvertFilterStringToFlag` (statisch → die 12 Zuordnungen kopieren),
  Z. 46–56 `GetParentElemOfSectElem` (statisch → kopieren), Z. 72–115 `GetElementsFromCurrentDatabase` (`ACAPI_Element_GetElemList (API_ZombieElemID, &list, flags)` Z. 95 —
  genau dieser Aufruf ergab live 19.873), `ACAPI_Element_Filter (guid, flags)` Z. 2449.
- Selektion: `ElementCommands.cpp` Z. 1417–1438 (`ACAPI_Selection_Get (&info, &neigs, false)`, `APIERR_NOSEL`, `API_SelEmpty`), Z. 1506
  (`ACAPI_Selection_Select ({ API_Neig (guid) }, true)`); DevKit 29 `ACAPinc.h` Z. 6179: Marquee-Handle `selectionInfo.marquee.coords` muss
  freigegeben werden (`BMKillHandle ((GSHandle*) &info.marquee.coords)`), Z. 3722 `ACAPI_Selection_DeselectAll ()`. Keine Undo-Klammer (Tapir ChangeSelection hat auch keine).
- Properties: `PropertyCommands.cpp` Z. 194–199 (`ACAPI_Property_GetPropertyGroups` + `ACAPI_Property_GetPropertyDefinitions (group.guid, defs)`),
  Z. 233–268 (Wert-Lesen: `value.variantStatus`, `definition.collectionType/valueType`, Enum über `possibleEnumValues[].keyVariant.guidValue`
  → `displayVariant.uniStringValue`), Z. 101–120 (`nonLocalizedValue` ist `GS::Optional<GS::UniString>`), Z. 349 (`ACAPI_Element_GetPropertyValuesByGuid`),
  Z. 382 (`API_Property_NotAvailable` / `API_Property_NotEvaluated`); Anzeige-String `ACAPI_Property_GetPropertyValueString` Z. 262.
- Klassifizierung: `ClassificationCommands.cpp` Z. 87 (`ACAPI_Element_GetClassificationInSystem (elem, system, item)`).
- Bibliotheksteil: `InspectionCommands.cpp` Z. 57–74 `LibIndOf` (anonym → kopieren), `LabelCommandsELM.cpp` Z. 10–23 `LibPartNameOf`
  (`ACAPI_LibraryPart_Get`, `docu_UName`, danach `delete lp.location`).
- GDL: Memo-Weg `SetObjectParametersForceCommand.cpp` Z. 266–273 (`ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_AddPars)`, `memo.params`,
  `ACAPI_DisposeElemMemoHdls`), Namensvergleich `SetAddParsCommand.cpp` Z. 77 (`GS::UniString (p.name)`), Typ-Switch `ElementGDLParameterCommands.cpp` Z. 323–352
  (`value.real` für Zahlen/Bool, `value.uStr` für CString/Title, `typeMod == API_ParArray` = Array).
- IFC: `IFCCommands.cpp` Z. 5–12 (AC28+ Includes `ACAPI/IFC*.hpp`), Z. 264–288 (Typ: AC28+ `IFCAPI::GetObjectAccessor ().CreateElementObjectID (head)` /
  `GetIFCType`, AC27 `ACAPI_Element_GetIFCType`), Z. 338–550 (statische `GetPropertyValueString`-Varianten beider Versionen), Z. 584–620
  (Properties: AC28+ `IFCAPI::PropertyAccessor (*objectID).GetPreviewProperties ()/GetLocalProperties ()`, AC27 `ACAPI_Element_GetIFCProperties (guid, false, &props)`).
  Attribute: AC28+ `PropertyAccessor::GetAttributes ()` → `Attribute::GetName ()` / `GetValue ()` (`std::optional<GS::UniString>`); AC27
  `ACAPI_Element_GetIFCAttributes (guid, false, &attrs)` → `attributeName` / `attributeValue` / `hasValue`.
- Wirt/Öffnung: `ElementCommands.cpp` Z. 1110–1127 (Tür UND Fenster über `elem.window.owner`, Dachfenster `elem.skylight.owner`),
  Z. 1814 `ACAPI_Grouping_GetConnectedElements (hostGuid, API_DoorID|API_WindowID|API_SkylightID|API_OpeningID, &list)` (Wirt → Öffnungen, unguarded).
  `elem.opening.owner` gibt es in AC29 NICHT mehr (`ExtendedElementCommands.cpp` Z. 2239 `#ifndef ServerMainVers_2900`) → Wirt von `Opening`-Elementen über die Rückwärtskarte aus GetConnectedElements.
- Bounds: `ElementCommands.cpp` Z. 2735–2739 (`API_Box3D box = {}; ACAPI_Element_CalcBounds (&head, &box)` — xMin…zMax, Meter).
- Zonen: DevKit 29 `ACAPinc.h` Z. 3255–3290 (`ACAPI_Element_GetRelations (zoneGuid, API_ZombieElemID, &roomRelation)`, danach `ACAPI_DisposeRoomRelationHdls`).
- Design-Optionen (nur AC29): `DesignOptionCommands.cpp` Z. 4–19 (Includes + `using namespace ACAPI::DesignOptions;` unter `ServerMainVers_2900`),
  Z. 326–334 (`CreateDesignOptionManager ()`, `GetAllDesignOptions ()`), Z. 476–500 (`GetDesignOptionStatusOf (guid)` → Variante
  `MissingDesignOption` / `MainModelDesignOption` / `DesignOption` mit `GetGuid/GetName/GetId/GetOwnerSetName`).
- Eigenschafts-Inventar WiSchu (`/Users/ap/.scratch/elmonkey/property_inventar_wischu_2026-10-13.json`, NUR für Doku/Testplan): „Element ID" = `ID und Kategorien`
  B1B54D45-C951-42C9-9AF8-898F0BF212AB (string); „Umbau-Status" = Gruppe `CategoryPropertyDefinitionGroup` 9CF14061-6E9C-45BE-BD8F-D3BCF59D218E (singleEnum:
  Undefined/Nicht definiert, Existing/Bestand, ToBeDemolished/Abbruch, New/Neubau); „Ebenenname" = `Modelldarstellung` 812CD483-… (eingebaute Property!).
  Doppelte Namen existieren (z. B. „Brüstungshöhe" 4×, „Versatz Z" 6×) → Mehrdeutigkeitsfehler testbar. Gruppen- UND Eigenschaftsnamen enthalten „/"
  („Balken / Träger", „F/T Volumen der Öffnung") → „Gruppe/Name" darf nicht naiv am ersten „/" getrennt werden.

**CaptureView + Fehlerform (Nachtrag 13.10.)**
- Befund (live WiSchu AC29, Layout „FR_00-06.1", crop true/false): `4009 Invalid command response … 'additionalProperties' … '#/error'`, kein PNG.
  Ursache: `CaptureViewCommand.hpp` (header-only, 120 Zeilen) — `GetResponseSchema` Z. 50–70 mit `additionalProperties:false` +
  `required:[imagePath, view2D]`; alle Fehlerpfade (Z. 76 outputPath, Z. 87 GetCurrentWindow, Z. 105 `ACAPI_ProjectOperation_Save`, Z. 112 Rücklese)
  liefern `CreateErrorResponse` = `{error:{code,message}}` → verletzt das Schema → Client sieht nur 4009, nie die Ursache.
  Fenstertyp kommt aus `ACAPI_Window_GetCurrentWindow` (Z. 84–89, `windowInfo.typeID`).
- `CreateErrorResponse` (`CommandBase.cpp` Z. 55–61) schreibt die Meldung als `errorMessage.ToCStr ().Get ()` (Zeichensatz `CC_Default`) →
  für Meldungen mit Modell-Namen (Umlaute, z. B. „Brüstungshöhe") eigener Helfer, der die Meldung als `GS::UniString` ins ObjectState legt.
- `#/ErrorItem`/`#/Error` stehen in `RFIX/Images/CommonSchemaDefinitions.json` Z. 636–664, sind für ELM-Befehle aber NICHT auflösbar
  (`ELMCommandBase::GetSchemaDefinitions` = NoValue) → Fehlerschema inline. `oneOf` wird vom Archicad-Validator unterstützt (CommonSchema nutzt es 35×,
  z. B. die „…OrError"-Listen, die täglich validiert werden). Kein bestehendes ELM-Antwortschema hat bisher einen Fehlerzweig.
- Fenstertyp-Namen: `InspectionCommands.cpp` Z. 28–53 `WindowTypeName` (anonymer Namespace, alle APIWind_*-Werte bauen in AC27–29) → als
  `WindowTypeNameELM` nach `ELMCommandBase.hpp` kopieren (InspectionCommands NICHT ändern).
- Layout-PNG: DevKit 29.3100 (`ACAPI_Automate.h` Z. 102–168, `API_SavePars_Picture`) dokumentiert KEINEN eigenen Weg für Layouts
  (nur view2D/crop, „crop 2D only") → kein Experiment; Export wird wie bisher versucht, Fehler sauber gemeldet.
- Registrierung CaptureView: `AddOnMain.cpp` `RegisterCommand<CaptureViewCommand> (elmSabCommands, "0.9.12", …)` (Stand fad4abb Z. 1008–1011; nach dem Include aus Task 1 eine Zeile tiefer). README hat KEINE CaptureView-Zeile.
- **Weitere ELM-Befehle mit demselben Schemafehler (nur auflisten, nicht fixen)** — Scan mit `python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py
  /Users/ap/.claude/repos/archicad-skill/ELM_SAB_Tapir/Sources` (Kandidat = Antwortschema mit `required`, ohne Fehlerzweig, UND `return CreateErrorResponse` auf oberster Ebene):
  `CreateWallOpenings` (CreateWallOpeningsCommand.cpp Z. 584/587 — AC27/28-Sperre), `SetLayerOfElements` (SetLayerCommand.cpp Z. 102/105),
  `SetObjectParametersForce` (Z. 326), `SetPenOfElements` (Z. 244), `SetStoryVisibilityOfElements` (StoryVisibilityCommands.cpp Z. 132/141),
  `SetTextSize` (Z. 64). (`GetAddOnVersion` hat `required`, aber keinen Fehlerpfad → unkritisch.)

## Entwurfsentscheidungen des Planers (Claude's Discretion, im Code-Kommentar kurz begründen)
- **`databases` wird umgesetzt** (Handoff: „`databases` nur auf Wunsch" = optionaler Parameter): kostet dank `ExecuteActionForEachDatabase` nur die
  ID-Liste; ohne `databases` läuft genau derselbe Weg mit der FloorPlan-ID. `select` zusammen mit `databases` = Fehler.
- **`where` im Eingabeschema als lockeres Objekt** (`{"type":"object"}`), weil ELMCommandBase keine Schema-Definitionen/`$ref` hat; strenge Prüfung in C++
  mit Pfad im Fehlertext (z. B. `where.and[2].not: unbekanntes Kriterium 'propery' — erlaubt: …`). Auch Top-Level ohne `additionalProperties:false`,
  damit Tippfehler als sprechender C++-Fehler statt als anonymer Schemafehler kommen.
- **Blatt-Modifikatoren als Geschwister-Schlüssel** wie im Handoff-Beispiel: `{ "property": …, "op": …, "value": … }`, `{ "classification": {…}, "includeChildren": true }`.
- **`hostedBy` / `hosts` / `inZone` nehmen einen Unterbaum** (gleiche Grammatik wie `where`), z. B. `{ "hostedBy": { "type": "Wall" } }` oder `{ "inZone": { "guids": [...] } }`.
- **`not` über ein Wert-Blatt schließt Elemente ein, die die Eigenschaft gar nicht haben** (notAvailable → Blatt false → not true). Gewollt und dokumentiert („mit type kombinieren").
- **Zahlen in API-Einheiten** (Länge m, Fläche m², Volumen m³; Gleichheit mit Toleranz 1e-6). GDL-Winkel wie GDL (Grad). Property-Winkel: API-Einheit, in der Gegenprobe prüfen.
- **Text-Vergleiche standardmäßig ohne Groß/Klein** (wie „Suchen & aktivieren"), `"caseSensitive": true` schaltet um.

<interfaces>
Vertrag zwischen Task 1 und Task 2 — Task 1 legt `FindElementsInternal.hpp` GENAU so an (Kommentare deutsch, ASCII-Umlaute wie im Bestand erlaubt).
Task-1-eigene Zusatzmitglieder in `EvalContext` (Caches für Ausgabe) sind erlaubt; die hier gelisteten Namen/Signaturen sind fix.

```cpp
#pragma once
#include "ELMCommandBase.hpp"
#include <map>
#include <memory>
#include <vector>

namespace FindElementsELM {

enum class NodeKind {
    And, Or, Not,
    Type, Story, Layer, Flags, Guids, InSelection, Teamwork,                    // Task 1 (guenstig)
    Property, Classification, LibPart, Gdl, Ifc, BBox, Near, DesignOption,      // Task 2 (Wert)
    HostedBy, Hosts, InZone                                                     // Task 2 (Beziehung, children[0] = Unterbaum)
};

struct LeafData { virtual ~LeafData () = default; };   // je Blatt abgeleitete Struktur, aufgeloest beim Parsen

struct Node {
    NodeKind                            kind = NodeKind::And;
    GS::UniString                       path;          // "where.and[2].not.property"
    GS::UniString                       criterion;     // Schluesselname: "and", "property", ...
    Int32                               cost = 0;      // 0 guenstig .. 3 teuer; Operator = max der Kinder
    UInt32                              statsIndex = 0;
    std::vector<std::unique_ptr<Node>>  children;
    std::unique_ptr<LeafData>           leaf;
};

struct NodeStats {
    GS::UniString path;
    GS::UniString criterion;
    UInt64        checked = 0;
    UInt64        matched = 0;
    UInt64        notAvailable = 0;   // Kriterium nicht anwendbar (Eigenschaft fehlt, kein Bibliotheksteil, kein Wirt ...)
    UInt64        typeMismatch = 0;   // Wert vorhanden, aber Operator passt nicht zum Typ (gdl/ifc)
    bool          evaluated = false;
};

using GuidList = std::vector<API_Guid>;

struct ValueLeafState;                 // definiert in FindElementsLeaves.cpp (Property-Index, Wert-Caches)

struct EvalContext {
    std::vector<NodeStats>                  stats;           // Index = Node::statsIndex (Pre-Order)
    GS::HashTable<API_Guid, API_Elem_Head>  headerCache;
    GS::HashSet<API_Guid>                   selectionAtStart;
    bool                                    isTeamwork = false;
    short                                   myUserId = 0;
    std::vector<GS::UniString>              warnings;
    GS::UniString                           fatalError;      // nicht leer => Auswertung bricht ab; Fehler NACH DB-Rueckschaltung melden
    GSErrCode                               fatalCode = NoError;
    std::shared_ptr<ValueLeafState>         valueState;

    bool Header (const API_Guid& guid, API_Elem_Head& out);  // Task 1: lazy ACAPI_Element_GetHeader + Cache; false = nicht lesbar
};

// ---- Task 1 (FindElementsCommand.cpp) ----
void          EvaluateNode (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out);
bool          WildcardMatch (const GS::UniString& text, const GS::UniString& pattern);   // * und ?, ohne Gross/Klein
bool          IsGuidString (const GS::UniString& s);                                      // 8-4-4-4-12 hex
GS::UniString ToUni (const GS::String& s);                                                // GS::UniString (s.ToCStr ())
GS::UniString JoinNames (const std::vector<GS::UniString>& names, UInt32 maxCount);      // "a, b, c (+12 weitere)"

// ---- Task 2 (FindElementsLeaves.cpp) ----
std::shared_ptr<ValueLeafState> CreateValueLeafState ();
bool ParseValueLeaf    (NodeKind kind, const GS::ObjectState& nodeOs, const GS::UniString& path,
                        ValueLeafState& vs, Node& node, GS::UniString& error);     // Property, Classification, LibPart, Gdl, Ifc, BBox, Near, DesignOption
void EvaluateValueLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out);   // alle Task-2-Arten inkl. HostedBy/Hosts/InZone
bool ParseReturnList   (const GS::ObjectState& parameters, ValueLeafState& vs, GS::UniString& error);
bool HasReturnColumns  (const ValueLeafState& vs);
void AddReturnColumns  (const ValueLeafState& vs, GS::ObjectState& response);
void AddReturnValues   (EvalContext& ctx, const API_Guid& guid, GS::ObjectState& hit);

}   // namespace FindElementsELM
```

Neu in `ELMCommandBase.hpp` (Task 1, am Dateiende, alles `inline`; genutzt von FindElements UND CaptureView):

```cpp
// ELM_SAB 0.9.23: Fehlerform {error:{code,message}} muss im Antwortschema erlaubt sein — sonst meldet Archicad
// dem Client nur 4009 statt der Ursache (CaptureView-Befund WiSchu 2026-10-13).
inline GS::UniString ELMResponseSchemaWithError (const GS::UniString& successSchema);
    // liefert {"oneOf": [ <successSchema>, {"type":"object","properties":{"error":{"type":"object","properties":
    //   {"code":{"type":"integer"},"message":{"type":"string"}},"additionalProperties":false,"required":["code","message"]}},
    //   "additionalProperties":false,"required":["error"]} ]}   (per GS::UniString-Verkettung)
inline GS::ObjectState CreateErrorResponseELM (GSErrCode errorCode, const GS::UniString& message);
    // gleiche Form wie CreateErrorResponse, Meldung aber als GS::UniString (Umlaute sicher)
inline GS::UniString WindowTypeNameELM (API_WindowTypeID type);
    // Kopie der Zuordnung InspectionCommands.cpp Z. 28-53; default: GS::UniString::Printf ("Other:%d", (int) type)
```

Neu in `IFCCommands.hpp` (Task 2), Implementierung in `IFCCommands.cpp` direkt nach Z. 550 (nach den statischen Helfern beider Versionen):

```cpp
// ELM_SAB 0.9.23 (FindElements, ifc-Blatt): IFC-Typ, -Eigenschaften und -Attribute als Text, versionsneutral.
struct ELMIFCText {
    GS::UniString propertySetName;   // leer bei Attributen
    GS::UniString name;
    GS::UniString value;
    bool          hasValue = true;
};
GSErrCode ELMGetIFCData (const API_Elem_Head& head, bool wantProperties, bool wantAttributes,
                         GS::UniString& ifcType, GS::Array<ELMIFCText>& properties, GS::Array<ELMIFCText>& attributes);
```
</interfaces>

## Blatt-Grammatik (für Task 1 = Struktur/Parsing, Task 2 = Wert-Blätter, Task 4 = Doku)

Knoten = Objekt mit GENAU einem Kriteriums-Schlüssel (+ ggf. erlaubte Geschwister). Leeres Objekt, zwei Kriterien in einem Knoten,
unbekannter Schlüssel → Fehler mit Pfad und erlaubter Liste.

| Schlüssel | Form | Geschwister | Kosten |
|---|---|---|---|
| `and` / `or` | nicht-leeres Array von Knoten | – | max(Kinder) |
| `not` | Knoten | – | Kind |
| `type` | `"Door"` oder `["Door","Window"]` (Tapir-Typnamen) | – | 0 |
| `story` | Zahl (Index) · `"EG"` (Name) · `{index}` · `{indices:[…]}` · `{name}` · `{names:[…]}` · `{from, to}` (inkl.) | – | 0 |
| `layer` | `"Name"` (exakt) · Objekt mit beliebig vielen von `{name, names, pattern, visible, locked, inCombination}` (UND) | – | 0 |
| `flags` | `"IsVisibleIn3D"` oder Liste der 12 Tapir-Flags (UND; `IncludeSubElemObjects` = Fehler mit Hinweis) | – | 1 |
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

Operatoren (`op`): `=`/`==`, `≠`/`!=`, `contains`, `startsWith`, `endsWith`, `regex` (ECMAScript, beim Parsen kompiliert), `<`, `≤`/`<=`, `>`, `≥`/`>=`,
`between` (`value:[a,b]` inkl.), `in` (`value:[…]`), `hasValue`, `isEmpty`. Unbekannter Operator / fehlender oder falsch getypter `value` = Fehler.

<tasks>

<task type="auto">
  <name>Task 1: Gerüst, Vertrag, Fehler-Helfer, Auswertungsmotor, Suchbereich, günstige Blätter, Ausgabe, Auswahl, Registrierung + Version</name>
  <files>ELM_SAB_Tapir/Sources/ELMCommandBase.hpp, ELM_SAB_Tapir/Sources/FindElementsInternal.hpp, ELM_SAB_Tapir/Sources/FindElementsCommand.hpp, ELM_SAB_Tapir/Sources/FindElementsCommand.cpp, ELM_SAB_Tapir/Sources/AddOnMain.cpp, ELM_SAB_Tapir/Sources/AddOnVersion.hpp</files>
  <action>
Vor dem Schreiben NUR die oben genannten Zeilenbereiche lesen (nicht ganze Dateien): ELMCommandBase.hpp, LabelCommandsELM.hpp/.cpp Z. 1–60 und 277–374, CommandBase.hpp komplett, CommandBase.cpp Z. 583–653, ElementCommands.cpp Z. 17–56 und 1417–1438, AttributeCommands.cpp Z. 94–103 und 236–290, InspectionCommands.cpp Z. 155–163 und 200–251, ProjectCommands.cpp Z. 515–546.

0. ELMCommandBase.hpp: am Dateiende die drei inline-Helfer aus dem interfaces-Block (ELMResponseSchemaWithError, CreateErrorResponseELM, WindowTypeNameELM) mit Kommentar „ELM_SAB 0.9.23". Bestehenden Inhalt nicht ändern.

1. FindElementsInternal.hpp exakt nach dem Vertrag im interfaces-Block anlegen (Dateikopf-Kommentar: „ELM_SAB 0.9.23 (2026-10-13, WiSchu-Tuerenabgleich): FindElements — gemeinsamer Vertrag Motor (FindElementsCommand.cpp) ↔ Wert-/Beziehungsblaetter (FindElementsLeaves.cpp)").

2. FindElementsCommand.hpp: Klasse FindElementsCommand : public ELMCommandBase nach dem Muster LabelCommandsELM.hpp Z. 21–27, GetName liefert "FindElements", Overrides GetInputParametersSchema, GetResponseSchema, Execute (ProcessControl-Parameter unbenannt lassen — MSVC C4100).

3. FindElementsCommand.cpp (alles im namespace FindElementsELM bzw. anonymem Namespace; nur Execute/Schema als Klassenmember):
   a) Inline-Eingabeschema: Objekt mit properties where {"type":"object"}, scope {"type":"string"}, return {"type":"array","items":{"oneOf":[{"type":"string"},{"type":"object"}]}}, countOnly/select boolean, limit integer, databases array von {databaseId:{guid}}; Beschreibung deutsch (immer Grundriss, nie aktives Fenster; Grammatik-Kurzfassung). KEIN additionalProperties:false. Antwortschema = ELMResponseSchemaWithError (Erfolgszweig: type object, Kernfelder benannt, required [scopeCount, matchCount, criteriaStats], OHNE additionalProperties:false). ALLE Fehlerantworten von FindElements über CreateErrorResponseELM (Modell-Namen mit Umlauten), nie über CreateErrorResponse.
   b) Gemeinsame Helfer aus dem Vertrag implementieren: WildcardMatch (iterativ, beide Seiten per ToLowerCase), IsGuidString, ToUni, JoinNames. Lokale Lese-Helfer: Schlüssel eines Knotens über GetFieldNames, Typ über GetType; Zahl per Get (key, double); String-oder-Stringliste (GetType String → ein Eintrag, List → GS::Array<GS::UniString>); Listen von Objekten per GS::Array<GS::ObjectState>. Passt keine Lesart → Fehler „<Pfad>.<Schlüssel>: erwartet …".
   c) Parser ParseNode (rekursiv, Pfad mitführen, statsIndex in Pre-Order vergeben, NodeStats.path/criterion füllen, cost nach Tabelle setzen; Operator-Knoten cost = max der Kinder; hostedBy/hosts/inZone cost = max(2, Kind)). Erlaubte Geschwister je Blatt laut Tabelle prüfen. and/or: nicht-leeres Array; not: Objekt. Für Property, Classification, LibPart, Gdl, Ifc, BBox, Near, DesignOption → ParseValueLeaf (Task 2) aufrufen und dessen Fehler durchreichen. HostedBy/Hosts/InZone: Unterbaum per ParseNode als children[0] (Pfad z. B. where.and[1].hostedBy).
   d) Günstige Blätter (eigene LeafData-Ableitungen im anonymen Namespace), alles beim Parsen aufgelöst:
      - type: Namen über GetElementTypeFromNonLocalizedName; API_ZombieElemID → Fehler „unbekannter Elementtyp 'X'". Menge von Int32-Typ-IDs.
      - story: Geschossliste einmal laden (ProjectCommands.cpp Z. 515–546-Muster, Handle freigeben). Name ohne Groß/Klein exakt; unbekannter Name/Index oder leerer Bereich → Fehler mit Liste „index:Name". Menge erlaubter floorInd.
      - layer: alle Ebenen einmal laden (Index → Name, Flags). name/names exakt (ohne Groß/Klein; unbekannt → Fehler mit bis zu 10 ähnlichen Namen per contains), pattern per WildcardMatch (keine Ebene passt → Eintrag in ctx.warnings, kein Fehler), visible/locked über APILay_Hidden/APILay_Locked, inCombination: Kombination per GetAttributesByType(API_LayerCombID) über den Namen finden (unbekannt → Fehler mit Liste), GetDef + Versions-Split exakt wie AttributeCommands.cpp Z. 268–282, sichtbar = nicht APILay_Hidden in layer_statItems, Defs/Attribute freigeben. Ergebnis = EINE Menge erlaubter Ebenen-Indizes (GetAttributeIndex), Auswertung = Mitgliedschaft von header.layer.
      - flags: eigene Kopie der 12 Zuordnungen aus ElementCommands.cpp Z. 17–44; unbekannt → Fehler mit Liste; IncludeSubElemObjects → Fehler „kein Filter-Flag (nur für GetElementsByType)". Auswertung ACAPI_Element_Filter (guid, flags).
      - guids: IsGuidString prüfen (ungültig → Fehler mit Wert), Menge.
      - inSelection: Bool; Auswertung gegen ctx.selectionAtStart.
      - teamwork: frei = lockId 0, vonMir = lockId == myUserId, vonAnderen = sonst ≠ 0; Projekt ohne Teamwork → Fehler „teamwork: Projekt ist kein Teamwork-Projekt".
   e) Motor EvaluateNode laut Vertrag: stats[i].evaluated=true, checked += |in|; leere Eingabe → leere Ausgabe. and: Kinder per std::stable_sort nach cost (Zeiger-Vektor, Reihenfolge des Nutzers bei gleichen Kosten), Ergebnis kettenweise weiterreichen, bei leerer Menge oder ctx.fatalError abbrechen. or: Kinder nach cost, jedes nur auf die noch NICHT getroffenen Elemente anwenden, abbrechen wenn nichts mehr offen; Ausgabe in Eingabe-Reihenfolge (GS::HashSet<API_Guid> für Mengen). not: in minus Ergebnis des Kindes. Günstige Blätter direkt, alle übrigen Arten (inkl. HostedBy/Hosts/InZone) → EvaluateValueLeaf. matched += |out|. EvalContext::Header implementieren (lazy, Kopie zurückgeben — keine Zeiger in die HashTable halten).
   f) Execute (Reihenfolge einhalten; ALLE Parameter- und Namensfehler VOR dem DB-Wechsel):
      1. Startzeit std::chrono::steady_clock. Top-Level-Schlüssel prüfen (erlaubt: where, scope, return, countOnly, select, limit, databases) → unbekannt = Fehler mit Namen.
      2. scope: Vorgabe projekt; projekt = APIFilt_None, wie3D = APIFilt_OnVisLayer | APIFilt_In3D, sichtbar = APIFilt_OnVisLayer, aktuellesGeschoss = APIFilt_OnActFloor; sonst Fehler mit den vier Werten. limit Vorgabe 20000, gültig 1…200000. select + databases → Fehler.
      3. ctx.valueState = CreateValueLeafState (); ParseReturnList; Projekt-Info (isTeamwork, myUserId); Geschoss- und Ebenenlisten laden; where parsen (fehlt where → alle Kandidaten des Suchbereichs sind Treffer, criteriaStats leer).
      4. Falls der Baum inSelection enthält: Auswahl im AUSGANGS-Fenster lesen (ElementCommands.cpp Z. 1417–1438, GUIDs über kopiertes GetParentElemOfSectElem, Marquee-Handle freigeben).
      5. Ausgangs-Datenbank per ACAPI_Database_GetCurrentDatabase lesen → startDatabaseType = WindowTypeNameELM (typeID).
      6. Datenbank-IDs: ohne databases genau eine — DatabaseIdResolver::Instance ().GetIdOfDatabase für ein API_DatabaseInfo mit typeID APIWind_FloorPlanID; mit databases deren GUIDs. Dann ExecuteActionForEachDatabase: die action macht ALLES Elementbezogene in der gerade aktiven DB — GetElemList (API_ZombieElemID, &list, scopeFlags) → GuidList, scopeCount addieren, EvaluateNode (oder alle bei fehlendem where), Treffer zählen, Treffer-Objekte bis limit bauen (elementId per CreateElementIdObjectState, type per GetElementTypeNonLocalizedName (GetElemTypeId (head)), floorIndex, storyName, layerName aus Cache; values per AddReturnValues wenn HasReturnColumns; bei databases zusätzlich databaseId), bei select (nur Grundriss-Pfad): bei 0 Treffern Auswahl unverändert lassen und selection {requested:0, changed:false}; sonst ACAPI_Selection_DeselectAll, ACAPI_Selection_Select (Array aus API_Neig je Treffer, true), dann ACAPI_Selection_Get (…, false) zurücklesen (Marquee freigeben) → selection {requested, selected (= Treffer in Auswahl), notSelectedCount, notSelected (max. 500 GUID-Strings), extraSelectedCount, selectErr wenn ≠ NoError}. action liefert APIERR_GENERAL wenn ctx.fatalError gesetzt. actionFailure-Meldungen sammeln.
      7. Nach ExecuteActionForEachDatabase (Rückschaltung erfolgt dort IMMER): Rückgabe ≠ NoError → Fehler „Ausgangs-Datenbank nicht ermittelbar/nicht wiederhergestellt"; ctx.fatalError → CreateErrorResponseELM (fatalCode, fatalError); Grundriss-Pfad mit actionFailure → Fehler „Grundriss-Datenbank nicht erreichbar: …".
      8. Antwort: database ("FloorPlan" bzw. "databases"), startDatabaseType, scope, scopeCount, matchCount, returnedCount, truncated, elements (fehlt bei countOnly), returnColumns (AddReturnColumns, falls return), criteriaStats (alle Knoten in Pre-Order: path, criterion, checked, matched, evaluated; notAvailable/typeMismatch nur wenn > 0), selection (falls select), warnings (falls vorhanden), perDatabase (nur mit databases: databaseId, scopeCount, matchCount, error?), elapsedMs (Int32).
   Kommentar am Dateikopf: Anlass (Tapir-0 bei aktivem Layout-Fenster), Leitregeln (immer Grundriss, kein stiller Nullbefund, nur lesen).

4. AddOnMain.cpp: #include "FindElementsCommand.hpp" nach Z. 65; nach dem SetLayerOfElements-Eintrag (Z. 1064–1067), vor AddCommandGroup: RegisterCommand<FindElementsCommand> (elmSabCommands, "0.9.23", "ELM_SAB: Elementsuche wie 'Suchen & aktivieren' - Kriterien (Typ, Geschoss, Ebene, Eigenschaft, Klassifizierung, Bibliotheksteil, GDL, IFC, Filter-Flags, Teamwork, Wirt/Oeffnung, Zone, Bereich/Naehe, GUIDs, Auswahl, Design-Option) beliebig tief mit and/or/not; liest IMMER die Grundriss-Datenbank (alle Geschosse), nie das aktive Fenster. scope projekt/wie3D/sichtbar/aktuellesGeschoss, return-Eigenschaften, countOnly/limit, criteriaStats je Kriterium, optional select mit Ruecklese. Nur lesen.").

5. AddOnVersion.hpp: ELM_SAB_VERSION "0.9.22" → "0.9.23" (ADDON_VERSION unverändert).

Compiler-Fallen (CI = /W4 /WX bzw. -Wall -Wextra -Werror): keine designated initializers, kein std::format, keine Ranges/Concepts/<=>, kein .contains()/starts_with (C++20); kein std::max/std::min ohne Klammern ((std::max) (a, b)) wegen Windows-Makros; size_t → UInt32/Int32/UInt64 explizit casten (C4267/C4244); keine verdeckten Namen in verschachtelten Schleifen (C4456/C4457); keine unbenutzten Variablen, Lambdas-Captures oder anonymen Funktionen (clang -Wunused-function); Initialisierung `= {}`; Range-for mit const auto&.

Commit (nur diese 6 Dateien, NICHT pushen — linkt erst zusammen mit Task 2): „feat(ELM_SAB): 0.9.23 FindElements – Auswertung, Suchbereich, guenstige Kriterien" mit Abschlusszeile „Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>".
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill/ELM_SAB_Tapir/Sources && for h in ELMResponseSchemaWithError CreateErrorResponseELM WindowTypeNameELM; do grep -q "inline .*$h (" ELMCommandBase.hpp || { echo "Helfer fehlt: $h"; exit 1; }; done && grep -c 'ELMResponseSchemaWithError' FindElementsCommand.cpp && ! grep -v '^\s*//' FindElementsCommand.cpp | grep -q 'CreateErrorResponse (' && test -f FindElementsInternal.hpp && test -f FindElementsCommand.hpp && test -f FindElementsCommand.cpp && grep -c 'namespace FindElementsELM' FindElementsInternal.hpp && grep -c 'class FindElementsCommand : public ELMCommandBase' FindElementsCommand.hpp && grep -v '^\s*//' AddOnMain.cpp | grep -c 'RegisterCommand<FindElementsCommand> (' && grep -c '#include "FindElementsCommand.hpp"' AddOnMain.cpp && grep -c '#define ELM_SAB_VERSION "0.9.23"' AddOnVersion.hpp && grep -c '#define ADDON_VERSION "1.5.4"' AddOnVersion.hpp && for t in ExecuteActionForEachDatabase APIWind_FloorPlanID APIFilt_OnVisLayer APIFilt_In3D APIFilt_OnActFloor ACAPI_Selection_DeselectAll ACAPI_Selection_Select ACAPI_Selection_Get stable_sort EvaluateValueLeaf ParseValueLeaf AddReturnValues criteriaStats scopeCount elapsedMs truncated startDatabaseType wie3D aktuellesGeschoss; do n=$(grep -v '^\s*//' FindElementsCommand.cpp | grep -c "$t"); [ "$n" -ge 1 ] || { echo "FEHLT: $t"; exit 1; }; done && ! grep -nE 'std::format|<=>|#include <(ranges|concepts|span|format)>|\.(contains|starts_with|ends_with) *\(|consteval|constinit|\{ *\.[A-Za-z_]+ *=|std::(max|min) *\(' ELMCommandBase.hpp FindElementsInternal.hpp FindElementsCommand.hpp FindElementsCommand.cpp && python3 -I -c "import sys;[sys.exit('Klammern ungleich: '+f) for f in ['ELMCommandBase.hpp','FindElementsInternal.hpp','FindElementsCommand.cpp'] if open(f,encoding='utf-8').read().count('{')!=open(f,encoding='utf-8').read().count('}')]" && cd ../.. && git log -1 --format=%B | grep -c 'Co-Authored-By: Claude Opus 5.5' && git show --stat --format= HEAD</automated>
  </verify>
  <done>Fehler-/Schema-/Fenstertyp-Helfer in ELMCommandBase.hpp, Vertrag, Kommando-Klasse, Motor (and/or/not, Kosten-Sortierung, Kurzschluss, Stats), scope-Zuordnung, Grundriss-Weg über ExecuteActionForEachDatabase (+ optional databases), günstige Blätter, Antwortaufbau, select mit Rücklese, Registrierung "0.9.23" und ELM_SAB_VERSION 0.9.23 liegen vor; lokaler Commit mit genau den 6 Dateien; nicht gepusht.</done>
</task>

<task type="auto">
  <name>Task 2: Wert- und Beziehungsblätter, return-Werte, IFC-Helfer</name>
  <files>ELM_SAB_Tapir/Sources/FindElementsLeaves.cpp, ELM_SAB_Tapir/Sources/IFCCommands.cpp, ELM_SAB_Tapir/Sources/IFCCommands.hpp</files>
  <action>
Vor dem Schreiben NUR lesen: FindElementsInternal.hpp (aus Task 1), PropertyCommands.cpp Z. 101–120 und 189–268 und 340–395, ClassificationCommands.cpp Z. 80–95, LabelCommandsELM.cpp Z. 10–23, InspectionCommands.cpp Z. 57–74, SetObjectParametersForceCommand.cpp Z. 255–275, ElementGDLParameterCommands.cpp Z. 318–352, IFCCommands.cpp Z. 1–12, 240–290, 338–345, 530–629, DesignOptionCommands.cpp Z. 1–20 und 449–500, ElementCommands.cpp Z. 1110–1127, 1810–1820, 2730–2742.

1. IFCCommands.hpp/.cpp: Struktur ELMIFCText und Funktion ELMGetIFCData laut interfaces-Block. Implementierung in IFCCommands.cpp direkt nach Z. 550 (dort sind beide statischen GetPropertyValueString-Varianten sichtbar): Typ wie Z. 264–288; Properties (nur wenn wantProperties) wie Z. 584–620 (AC28+: Preview- UND Local-Properties; AC27: ACAPI_Element_GetIFCProperties (guid, false, &props)); Attribute (nur wenn wantAttributes) AC28+ über IFCAPI::PropertyAccessor (*objectID).GetAttributes () (Name GetName, Wert GetValue → hasValue), AC27 über ACAPI_Element_GetIFCAttributes (guid, false, &attrs) (attributeName/attributeValue/hasValue). Fehler des Accessors als GSErrCode zurückgeben (AC28+: UnwrapErr ().kind). Bestehende Befehle NICHT verändern. Kommentar „ELM_SAB 0.9.23 (FindElements)".

2. FindElementsLeaves.cpp (namespace FindElementsELM + anonymer Namespace für Interna; includes: FindElementsInternal.hpp, MigrationHelper.hpp, IFCCommands.hpp, <regex>, <cmath>, <vector>, <memory>; unter #ifdef ServerMainVers_2900 die Design-Option-Includes + using wie DesignOptionCommands.cpp Z. 4–19). Alle Vertragsfunktionen implementieren:
   a) ValueLeafState: Property-Index (alle Gruppen/Definitionen einmal, lazy beim ersten Bedarf: GUID → {Gruppe, Name, Definition}), Liste aller benötigten Property-GUIDs (aus Blättern + return, in Parse-Reihenfolge, ohne Doppelte), Wert-Cache je Element (GS::HashTable<API_Guid, std::vector<PropVal>> o. ä., nur extrahierte Werte speichern, NICHT API_Property — Speicher), Caches für GDL/IFC/Klassifizierung/Bibliotheksteil/Bounds/Wirt, Rückwärtskarte Öffnung→Wirt, return-Spalten.
   b) Eigenschafts-Adresse auflösen (für property-Blatt UND return): GUID-String bzw. {guid} → Definition muss existieren (sonst Fehler „Eigenschaft {GUID} nicht im Projekt"); {group, name} → exakt (ohne Groß/Klein); String → Kandidaten = (a) Definitionen mit Name == ganzer String, (b) für JEDE „/"-Position: Gruppe = Teil davor, Name = Teil danach (je getrimmt). Genau ein Treffer → ok; keiner → Fehler „unbekannte Eigenschaft 'X'" + bis zu 10 Vorschläge (Name enthält X); mehrere → Fehler „Eigenschaft 'X' mehrdeutig: Gruppe/Name {GUID}, …" (Kandidatenliste per JoinNames).
   c) Vergleich (gemeinsam für property, gdl, ifc): op normalisieren (Unicode- und ASCII-Formen laut Tabelle), value lesen (Zahl, Bool, Text; Listen für in/between; between = genau 2 Zahlen a ≤ b), caseSensitive (Vorgabe false), regex beim Parsen mit std::regex (ECMAScript, icase wenn nicht caseSensitive) kompilieren — std::regex_error → Fehler mit Pfad. Bei property den Definitionstyp beim Parsen prüfen: Zahl-Operatoren (<, ≤, >, ≥, between) nur für Integer/Real; Text-Operatoren (contains/startsWith/endsWith/regex) für Text und Aufzählungen; Bool nur =/≠/hasValue/isEmpty; Verstoß = Fehler mit Typnamen. Semantik: Wert nicht verfügbar (NotAvailable/NotEvaluated/Lesefehler) → false + stats.notAvailable; undefiniert/leer → = false, ≠ true, isEmpty true, hasValue false; Zahl-Gleichheit |a−b| ≤ 1e-6; Aufzählung (Einfach-Auswahl): passt, wenn nonLocalizedValue ODER Anzeige-String passt (Enum-Schlüssel über keyVariant.guidValue gegen die beim Parsen gemerkten possibleEnumValues); Mehrfach-Auswahl/Listen: =, in, Text-Operatoren = irgendein Eintrag passt, ≠ = keiner gleich. regex per std::regex_search auf UTF-8 (ToCStr (CC_UTF8)); Ausnahmen beim Matchen fangen → ctx.fatalError.
   d) property-Auswertung: für die Elemente aus in, die noch nicht im Cache sind, je Element EIN ACAPI_Element_GetPropertyValuesByGuid mit ALLEN benötigten GUIDs; Werte extrahieren (Status, variantStatus, Zahl, Bool, Text, Enum-Schlüssel, Listen) und cachen; dann vergleichen.
   e) classification: Systeme per ACAPI_Classification_GetClassificationSystems, Name ohne Groß/Klein (+ optional version gegen editionVersion); unbekannt → Fehler mit Liste „Name (Version)"; gleichnamig ohne version → Fehler mit Kandidaten. Eintrag über code (item.id), name oder item (GUID) per Baumdurchlauf (RootItems + ItemChildren rekursiv); unbekannt → Fehler mit Namen. includeChildren (als Geschwister ODER im Objekt) → Menge inkl. aller Nachfahren. unclassified:true → Element hat im System keinen Eintrag (Aufruf-Fehler oder item.guid == APINULLGuid). Auswertung ACAPI_Element_GetClassificationInSystem je Element mit Cache.
   f) libPart: libInd über kopiertes LibIndOf (ACAPI_Element_Get), Name über LibPartNameOf-Muster (location freigeben), Cache libInd → Name; exakter Name ohne Groß/Klein bzw. WildcardMatch; Element ohne Bibliotheksteil → false + notAvailable.
   g) gdl: ACAPI_Element_GetMemo (APIMemoMask_AddPars) je Element einmal für ALLE gdl-Blätter (Cache Name → Wert), Memo immer freigeben; Zahl-Typen (Integer, Length, Angle, RealNum, PenCol, LineTyp, Mater, FillPat, BuildingMaterial, Profile, ColRGB, Intens, LightSw, Boolean) → value.real; CString/Title → value.uStr; Array-Parameter oder Text-Op auf Zahl/Zahl-Op auf Text → false + typeMismatch; Parameter fehlt → false + notAvailable.
   h) ifc: über ELMGetIFCData (nur benötigte Teile anfordern, Cache je Element). type: Gleichheit ohne Groß/Klein gegen ifcType (Liste erlaubt). property: Treffer, wenn irgendeine Eigenschaft mit passendem Namen (und pset, falls angegeben) den Vergleich erfüllt; Zahl-Ops parsen den Text als Zahl, sonst typeMismatch. attribute: analog über Attribute. Elemente ohne IFC-Daten → notAvailable; APIERR_MODULNOTINSTALLED oder Accessor-Fehler beim ersten Element → ctx.fatalError „ifc: IFC-Modul nicht verfügbar (Code N)".
   i) hostedBy: Wirt je Element — Tür/Fenster elem.window.owner, Dachfenster elem.skylight.owner, API_OpeningID über Rückwärtskarte (lazy einmal aufbauen: für alle Elemente der Typen Wall, Slab, Roof, Shell, Beam, Column per GetElemList, dann ACAPI_Grouping_GetConnectedElements (host, API_OpeningID, &list)); andere Typen → notAvailable. Wirte einsammeln (ohne Doppelte), Unterbaum children[0] per EvaluateNode auf diese Wirt-Menge, Treffer = Elemente, deren Wirt im Ergebnis liegt. hosts: je Kandidat GetConnectedElements für API_DoorID, API_WindowID, API_SkylightID, API_OpeningID; Vereinigung per EvaluateNode auswerten; Treffer = Kandidat mit mindestens einer passenden Öffnung.
   j) inZone: alle Zonen (GetElemList API_ZoneID, unabhängig vom scope) → Unterbaum per EvaluateNode → je passender Zone ACAPI_Element_GetRelations (zone, API_ZombieElemID, &rel), Vereinigung aus elementsGroupedByType (HashTable-Iteration mit #ifdef ServerMainVers_2800-Split wie AttributeCommands.cpp Z. 268–282) + wallPart/beamPart/cwSegmentPart[].guid, danach ACAPI_DisposeRoomRelationHdls; Treffer = Kandidat in dieser Menge.
   k) bbox / near: ACAPI_Element_CalcBounds je Element (Cache); bbox intersects (Vorgabe) bzw. inside; z nur prüfen, wenn min.z/max.z bzw. point.z angegeben; near: Abstand Punkt ↔ Box (0 innerhalb) ≤ radius; Bounds nicht berechenbar → notAvailable. Parse-Fehler bei fehlenden x/y, min > max, radius < 0.
   l) designOption: unter #ifdef ServerMainVers_2900 Manager + GetAllDesignOptions beim Parsen; name/id/guid (+ set gegen GetOwnerSetName) auflösen, unbekannt → Fehler mit verfügbaren Namen, mehrdeutig ohne set → Fehler mit Kandidaten; mainModel:true → MainModelDesignOption. Auswertung GetDesignOptionStatusOf je Element. #else: ParseValueLeaf liefert Fehler „Kriterium designOption in Archicad 28 nicht verfügbar" bzw. „… Archicad 27 …" (über #ifdef ServerMainVers_2800 unterscheiden) — keine unbenutzten Funktionen/Variablen im #else-Zweig.
   m) return: ParseReturnList liest parameters "return" (GS::Array<GS::UniString> ODER GS::Array<GS::ObjectState>; gemischt → Fehler „return: einheitlich Strings oder Objekte"), löst wie b) auf, Fehler mit Namen. AddReturnColumns: returnColumns [{requested, propertyId{guid}, group, name}]. AddReturnValues: hit.values = Array in return-Reihenfolge, je Eintrag {value (Zahl/Bool/Text; Aufzählung = nonLocalizedValue, sonst Anzeige), display (ACAPI_Property_GetPropertyValueString bzw. Text)} oder {status:"notAvailable"} / {status:"undefined", display}; Werte über denselben gebündelten Cache.
   n) EvaluateValueLeaf: stats[node.statsIndex].notAvailable/typeMismatch hochzählen (checked/matched setzt der Motor). Reihenfolge der Ausgabe = Reihenfolge von in.

Gleiche Compiler-Fallen wie Task 1; zusätzlich: std::optional nur unter AC28+-Zweigen; im AC27-Zweig keine IFCAPI-Typen; keine statische Funktion, die in einem der drei Versionszweige unbenutzt bleibt (sonst -Wunused-function auf Mac) — versionsspezifische Helfer in dieselben #ifdef-Blöcke legen wie ihre Aufrufer.

Commit (nur diese 3 Dateien, NICHT pushen): „feat(ELM_SAB): 0.9.23 FindElements – Eigenschafts-, Wert- und Beziehungskriterien" mit Abschlusszeile „Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>".
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill/ELM_SAB_Tapir/Sources && test -f FindElementsLeaves.cpp && for f in CreateValueLeafState ParseValueLeaf EvaluateValueLeaf ParseReturnList HasReturnColumns AddReturnColumns AddReturnValues; do n=$(grep -cE "^(bool|void|std::shared_ptr<ValueLeafState>) +$f *\(" FindElementsLeaves.cpp); [ "$n" -ge 1 ] || { echo "Vertragsfunktion fehlt (Definition auf Spalte 0 erwartet): $f"; exit 1; }; done && for t in ACAPI_Element_GetPropertyValuesByGuid ACAPI_Property_GetPropertyDefinitions nonLocalizedValue possibleEnumValues mehrdeutig ACAPI_Element_GetClassificationInSystem ACAPI_Classification_GetClassificationItemChildren APIMemoMask_AddPars ACAPI_DisposeElemMemoHdls ELMGetIFCData ACAPI_Grouping_GetConnectedElements ACAPI_Element_GetRelations ACAPI_DisposeRoomRelationHdls ACAPI_Element_CalcBounds GetDesignOptionStatusOf 'nicht verf' ServerMainVers_2900 std::regex; do n=$(grep -v '^\s*//' FindElementsLeaves.cpp | grep -c "$t"); [ "$n" -ge 1 ] || { echo "FEHLT: $t"; exit 1; }; done && grep -c 'ELMGetIFCData' IFCCommands.hpp && grep -c 'ELMGetIFCData' IFCCommands.cpp && grep -c 'GetAttributes ()' IFCCommands.cpp && grep -c 'ACAPI_Element_GetIFCAttributes' IFCCommands.cpp && ! grep -nE 'std::format|<=>|#include <(ranges|concepts|span|format)>|\.(contains|starts_with|ends_with) *\(|consteval|constinit|\{ *\.[A-Za-z_]+ *=|std::(max|min) *\(' FindElementsLeaves.cpp IFCCommands.cpp IFCCommands.hpp && python3 -I -c "import sys;f='FindElementsLeaves.cpp';s=open(f,encoding='utf-8').read();sys.exit('Klammern ungleich') if s.count('{')!=s.count('}') else None" && cd ../.. && git log -1 --format=%B | grep -c 'Co-Authored-By: Claude Opus 5.5' && git show --stat --format= HEAD && git status --short ELM_SAB_Tapir</automated>
  </verify>
  <done>Alle Vertragsfunktionen definiert; property (typisiert, Enum nonLocalized+Anzeige, Mehrdeutigkeitsfehler), classification (includeChildren, unclassified), libPart, gdl, ifc (Typ/Pset/Attribut, AC27+AC28+), hostedBy/hosts, inZone, bbox/near, designOption (AC29, sonst benannter Fehler), return-Werte; IFC-Helfer exportiert ohne bestehende Befehle zu ändern; lokaler Commit mit genau den 3 Dateien; Arbeitsbaum unter ELM_SAB_Tapir sauber.</done>
</task>

<task type="auto">
  <name>Task 3: CaptureView — Fehlerantwort schemakonform, Fenstertyp + Archicad-Code, Layout-Verhalten</name>
  <files>ELM_SAB_Tapir/Sources/CaptureViewCommand.hpp, ELM_SAB_Tapir/Sources/AddOnMain.cpp</files>
  <action>
Vor dem Schreiben NUR lesen: CaptureViewCommand.hpp (komplett, 120 Zeilen), die drei Helfer am Ende von ELMCommandBase.hpp (aus Task 1), in AddOnMain.cpp die Zeilen der Registrierung RegisterCommand<CaptureViewCommand>.

1. GetResponseSchema: den bestehenden Erfolgs-Schematext (Z. 52–69, imagePath/view2D, additionalProperties:false, required) UNVERÄNDERT als Erfolgszweig an ELMResponseSchemaWithError übergeben (Rückgabe der Funktion statt des Raw-Strings). Erfolgsantwort im Execute bleibt exakt {imagePath, view2D}.
2. Execute: jedes CreateErrorResponse durch CreateErrorResponseELM ersetzen. Fenstertyp-Name einmal nach ACAPI_Window_GetCurrentWindow über WindowTypeNameELM (windowInfo.typeID) bestimmen und in JEDE spätere Meldung schreiben; den echten Archicad-Code als Zahl in den Text (GS::UniString::Printf mit %d bzw. %T für UniStrings) UND als code-Feld. Meldungen:
   - outputPath fehlt → APIERR_BADPARS, wie bisher.
   - GetCurrentWindow scheitert → echter err, „CaptureView: aktives Fenster nicht ermittelbar (Fehler N)".
   - ACAPI_ProjectOperation_Save scheitert, typeID APIWind_LayoutID oder APIWind_MasterLayoutID → echter err, „CaptureView: Layout-Fenster ('<Typ>'): PNG-Export per API nicht moeglich (Fehler N) — Zeichnung/Grundriss oeffnen oder Layout als PDF publizieren".
   - Save scheitert, anderer Typ → echter err, „CaptureView: Fensterinhalt (Fenstertyp '<Typ>') nicht als PNG speicherbar (Fehler N) — Fenstertyp exportierbar? Pfad beschreibbar?".
   - Save NoError, Datei fehlt → APIERR_COMMANDFAILED, „CaptureView: Save meldete Erfolg (Fenstertyp '<Typ>'), aber die PNG-Datei existiert nicht (Ruecklese)".
   KEINE vorgeschaltete Typliste/Sperre — der Export wird für jeden Fenstertyp versucht (Grundriss, 3D, Schnitt, Detail … bleiben wie bisher). Keine Layout-Experimente (DevKit zeigt keinen eigenen Weg).
3. Dateikopf-Kommentar ergänzen: „ELM_SAB 0.9.23 (2026-10-13, WiSchu Layout FR_00-06.1): Fehlerantworten verletzten das Antwortschema → Client sah nur 4009. Jetzt oneOf Erfolg|Fehler, Fenstertyp + Archicad-Code im Text."
4. AddOnMain.cpp: Versionsargument der CaptureView-Registrierung ("0.9.12") NICHT ändern; an die Beschreibung anhängen: „ 0.9.23: Fehler kommen schemakonform beim Client an (Fenstertyp + Archicad-Code), Layout-Fenster werden versucht und sonst klar abgelehnt."
5. Liste der weiteren betroffenen ELM-Befehle erneut erzeugen (nicht fixen): python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py /Users/ap/.claude/repos/archicad-skill/ELM_SAB_Tapir/Sources — erwartet: CaptureView und FindElements jetzt errorAllowed=True; Kandidaten CreateWallOpenings, SetLayerOfElements, SetObjectParametersForce, SetPenOfElements, SetStoryVisibilityOfElements, SetTextSize. Ergebnis für Task 4 (Doku, offener Punkt) und SUMMARY festhalten.

Compiler-Fallen wie Task 1 (header-only: keine unbenutzten lokalen Variablen; Printf-Argumente als (int) casten).

Commit (nur diese 2 Dateien, NICHT pushen): „fix(ELM_SAB): 0.9.23 CaptureView – Fehlerantwort schemakonform, Fenstertyp und Archicad-Code" mit Abschlusszeile „Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>".
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill/ELM_SAB_Tapir/Sources && grep -c 'ELMResponseSchemaWithError' CaptureViewCommand.hpp && test "$(grep -v '^\s*//' CaptureViewCommand.hpp | grep -c 'CreateErrorResponse (')" = 0 && test "$(grep -v '^\s*//' CaptureViewCommand.hpp | grep -c 'CreateErrorResponseELM (')" -ge 4 && grep -c 'WindowTypeNameELM' CaptureViewCommand.hpp && grep -c 'APIWind_LayoutID' CaptureViewCommand.hpp && grep -c 'APIWind_MasterLayoutID' CaptureViewCommand.hpp && grep -c 'PDF' CaptureViewCommand.hpp && grep -c '"imagePath"' CaptureViewCommand.hpp && grep -A3 'RegisterCommand<CaptureViewCommand>' AddOnMain.cpp | grep -c '0.9.12' && grep -A3 'RegisterCommand<CaptureViewCommand>' AddOnMain.cpp | grep -c '0.9.23' && ! grep -nE 'std::format|<=>|\{ *\.[A-Za-z_]+ *=|std::(max|min) *\(' CaptureViewCommand.hpp && python3 -I /Users/ap/.scratch/elmonkey/tools/elm_schema_scan.py . | grep -E 'CaptureViewCommand|FindElementsCommand' && cd ../.. && git log -1 --format=%B | grep -c 'Co-Authored-By: Claude Opus 5.5' && git show --stat --format= HEAD</automated>
  </verify>
  <done>CaptureView-Antwortschema lässt Erfolg und Fehlerform zu; jede Fehlermeldung nennt Fenstertyp und echten Archicad-Code; Layout/MasterLayout wird versucht und bei Fehler mit Hinweis (Zeichnung/Grundriss bzw. PDF) abgelehnt; Erfolgsantwort unverändert; Registrierungsbeschreibung ergänzt; Kandidatenliste weiterer Befehle erzeugt; lokaler Commit mit genau 2 Dateien.</done>
</task>

<task type="auto">
  <name>Task 4: Doku (Referenz + README), Commit, Push, CI-Schleife</name>
  <files>reference/mcp-extension.md, ELM_SAB_Tapir/README.md</files>
  <action>
1. reference/mcp-extension.md: nach dem Abschnitt „## ELM_SAB 0.9.22 …" (Dateiende, Z. ~1416–1428) anhängen: „## ELM_SAB 0.9.23 — FindElements: Suche mit und/oder/nicht <!-- 2026-10-13 -->" mit diesen Unterpunkten (deutsch, knapp, Stil der Nachbarabschnitte):
   - **Anlass:** WiSchu-Türenabgleich 13.10. — Tapir GetElementsByType/GetAllElements lesen die Datenbank des AKTIVEN Fensters (Layout vorn → 0 Türen statt 267); „Türen mit T.-Nummer, nicht Abbruch, je Geschoss" brauchte 4 Aufrufe + Python. Vorbild „Suchen & aktivieren".
   - **Aufruf** über API.ExecuteAddOnCommand mit addOnCommandId {commandNamespace "ELM_SAB", commandName "FindElements"} (Verweis auf die bestehende Regel weiter oben in der Datei).
   - **Beispiele** (als JSON-Blöcke): (a) Türen-Beispiel: where and [type Door, story {index:2}, not property "Umbau-Status" = "ToBeDemolished", property "Element ID" startsWith "T.2."], return ["Element ID","Umbau-Status","Ebenenname"]; (b) Türenabgleich in EINEM Aufruf: where {type:"Door"}, return ["Element ID","Umbau-Status"] → floorIndex/values je Treffer in Python gruppieren; (c) scope: {"scope":"wie3D","countOnly":true}; (d) or + classification: {"or":[{"classification":{"system":"…","code":"…"},"includeChildren":true},{"classification":{"system":"…"},"unclassified":true}]}; (e) Beziehung: {"and":[{"type":"Door"},{"hostedBy":{"layer":{"pattern":"A_Wand*"}}}]}.
   - **Blatt-Referenz**: die Tabelle „Blatt-Grammatik" und die Operator-Liste aus diesem Plan übernehmen (inkl. Geschwister-Schlüssel und Semantik: notAvailable → false, not schließt Elemente ohne die Eigenschaft ein, Enum = nonLocalizedValue ODER Anzeige, ohne Groß/Klein als Vorgabe, Zahlen in API-Einheiten, GDL-Winkel in Grad).
   - **Antwortfelder**: database, startDatabaseType, scope, scopeCount, matchCount, returnedCount, truncated, elements[{elementId, type, floorIndex, storyName, layerName, values}], returnColumns, criteriaStats[{path, criterion, checked, matched, evaluated, notAvailable?, typeMismatch?}], selection, warnings, perDatabase, elapsedMs. Hinweis: zum Weitergeben an andere Befehle nur {elementId} übernehmen.
   - **scope-Tabelle** mit Filter und WiSchu-Erwartung (13.10., Grundriss vorn): projekt (kein Filter, alle Geschosse inkl. ausgeblendeter Ebenen) 19.873 · sichtbar (OnVisLayer, inkl. 2D) 19.503 · wie3D (OnVisLayer + In3D, ohne 2D) 10.210 · aktuellesGeschoss (OnActFloor) 6.547. Zahlen ändern sich mit dem Modell → vor dem Vergleich Referenz mit Tapir GetAllElements + gleichen Flags bei Grundriss vorn ziehen.
   - **Fallen:** aktive Datenbank (Tapir liest das vordere Fenster — FindElements nie; startDatabaseType zeigt, was vorn war); Teamwork: IsEditable = nur von MIR reserviert → Door+OnActualFloor 84, +IsEditable 0; Tapir-Flags kennen kein NICHT und kein ODER (hier per not/or); `IncludeSubElemObjects` ist kein Filter; mehrdeutige Eigenschaftsnamen (WiSchu: „Brüstungshöhe" 4×) → GUID oder „Gruppe/Name"; „/" steckt auch in Gruppen-/Eigenschaftsnamen; select ersetzt die Auswahl (danach selbst aufheben — keine Auswahl zurücklassen); Laufzeit wächst mit teuren Blättern (property/gdl/ifc) → günstige Blätter in dasselbe and.
   - **Unterpunkt „CaptureView: Fehlerantwort schemakonform, Layout-Verhalten"**: Befund WiSchu 13.10. (Layout FR_00-06.1, crop true/false → 4009 'additionalProperties' '#/error', kein PNG; Ursache: Fehlerantworten verletzten das eigene Antwortschema); Änderung (oneOf Erfolg|Fehler, Fenstertyp + echter Archicad-Code im Text, Layout/MasterLayout wird versucht und sonst mit Hinweis „Zeichnung/Grundriss öffnen oder Layout als PDF publizieren" abgelehnt, keine Typsperre, Erfolgsantwort unverändert); allgemeine Lehre: ein ELM-Antwortschema mit `required`/`additionalProperties:false` ohne Fehlerzweig verschluckt JEDE Fehlermeldung → neue Befehle über ELMResponseSchemaWithError deklarieren. **Offener Punkt (nicht gefixt):** Liste aus Task 3 (CreateWallOpenings, SetLayerOfElements, SetObjectParametersForce, SetPenOfElements, SetStoryVisibilityOfElements, SetTextSize — mit Datei/Zeile) + Hinweis auf das Scan-Werkzeug ~/.scratch/elmonkey/tools/elm_schema_scan.py.
   - **Nicht verfügbar:** `RunCriteriaSet` — gespeicherte Kriterien-Sets aus „Suchen & aktivieren" sind per API nicht erreichbar (DevKit 29.3100 geprüft: nur ACAPI_Selection_*, ACAPI_Element_SearchElementByCoord, ACAPI_LibraryPart_Search). designOption nur AC29 (AC27/28 → benannter Fehler).
   - **Stand:** gebaut (CI), noch NICHT live verifiziert (CI-SHA steht im Quick-Task-SUMMARY 261013-fel).
   - **Testplan Einbau + Gegenprobe** (macht der Orchestrator mit Mudi). Das Bundle enthält ZWEI Änderungen (FindElements neu + CaptureView-Fix) → beide Testblöcke getrennt abhaken. 1) Bundle-Regeln: 0.9.22-Bundle sichern, Archicad komplett beenden (nie kill), nur EINE Instanz, 0.9.23 einsetzen, starten, ELM_SAB.GetAddOnVersion meldet 0.9.23 + Build-Zeitstempel; nie zwei Dinge gleichzeitig ändern, bei Problemen erst Rückbau auf 0.9.22. 2) scope-Zahlen mit countOnly (ohne where) gegen die Tabelle bzw. Tapir-Referenz. 3) Dasselbe mit Layout-Fenster vorn: projekt-Zahl unverändert, startDatabaseType "Layout". 4) Türenabgleich: EIN FindElements-Aufruf (Beispiel b) gegen modell_abgleich.py (/Users/ap/.scratch/wischu-freising/tools/, native GetElementsByType Door + Properties B1B54D45-C951-42C9-9AF8-898F0BF212AB / 9CF14061-6E9C-45BE-BD8F-D3BCF59D218E): 267 Türen, gleiche Mengen je Geschoss (floorIndex) und je Umbau-Status. 5) Beispiel (a) gegen denselben Python-Filter (floor 2, umbau ≠ ToBeDemolished, id beginnt mit „T.2."); `"value":"Abbruch"` liefert dieselbe Menge wie „ToBeDemolished". 6) Unbekannte Eigenschaft („Umbaustatus") → Fehler mit Namen; „Brüstungshöhe" → Mehrdeutigkeitsfehler mit 4 Kandidaten. 7) select:true über alle Türen aller Geschosse → selection-Rücklese ansehen (werden fremde Geschosse ausgewählt?), danach Auswahl aufheben. 8) criteriaStats plausibel (type-Blatt matched 267), elapsedMs notieren. **CaptureView-Block:** 9) Layout aktiv (z. B. FR_00-06.1), crop true und false → klare Fehlermeldung mit Fenstertyp „Layout" und Archicad-Code statt 4009 (oder, falls Archicad den Layout-Export doch kann, ein PNG — dann als „Layout geht" dokumentieren). 10) Grundriss aktiv → PNG wie bisher, Antwort {imagePath, view2D:true}; 3D-Fenster → view2D:false. 11) Leerer outputPath → schemakonforme Fehlermeldung (kein 4009).
2. ELM_SAB_Tapir/README.md: in der Tabelle „Die SAB-eigenen Befehle" nach der SetLayerOfElements-Zeile (Z. 93) eine Zeile `FindElements` ergänzen: Suche wie „Suchen & aktivieren" mit and/or/not über Typ, Geschoss, Ebene, Eigenschaft, Klassifizierung, Bibliotheksteil, GDL, IFC, Flags, Teamwork, Wirt/Öffnung, Zone, Bereich, Auswahl, Design-Option; immer Grundriss-Datenbank; scope, return, criteriaStats, optional select. Ab 0.9.23. Falls die Tabelle eine CaptureView-Zeile hat, dort „Ab 0.9.23 schemakonforme Fehlermeldung mit Fenstertyp; Layout-Fenster werden versucht, sonst klar abgelehnt" ergänzen (sonst nichts hinzufügen).
3. Commit „docs(ELM_SAB): 0.9.23 FindElements + CaptureView-Fix – Referenz, Fallen, Testplan" (nur diese 2 Dateien) mit Abschlusszeile „Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>". Dann `git push origin main` (pusht die Commits aus Task 1, 2, 3 und den Doku-Commit; löst die CI aus).
4. CI warten: per Hintergrundbefehl (run_in_background) bzw. Monitor mit until-Schleife, KEIN Vordergrund-sleep — Muster: S=$(git rev-parse --short=7 HEAD); until git fetch -q origin ci-status && git show origin/ci-status:status.md | grep -q "$S"; do sleep 60; done; git show origin/ci-status:status.md (status.md nennt den HEAD des Pushs, also den Doku-Commit bzw. den letzten Fix-Commit; ein zweiter Push bricht den laufenden Build ab — concurrency cancel-in-progress). Grün = SHA steht drin und kein „fehlgeschlagen". Rot: git show origin/ci-status:logs/build-<mac|win>-AC<NN>.log | grep -nE "error|Error" | head -60 (falls gh angemeldet ist, ergänzend gh run view --log-failed), gezielt fixen (typisch: -Werror unused variable/function, MSVC C4100/C4244/C4267/C4456/C4702, fehlendes Include, Versions-#ifdef), als „fix(ELM_SAB): 0.9.23 FindElements – <Ursache>" mit Co-Authored-By-Zeile committen, pushen, erneut warten. Max. 4 Fix-Runden, danach stoppen und Ursache + letzte Log-Auszüge im SUMMARY dokumentieren. Den grünen CI-SHA nur im SUMMARY festhalten (kein weiterer Doku-Commit dafür).
   KEIN Bundle-Tausch, kein Bundle installieren, Archicad nicht ansprechen.
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && grep -c '^## ELM_SAB 0.9.23' reference/mcp-extension.md && for t in 'RunCriteriaSet' '19.873' '10.210' '19.503' '6.547' 'startDatabaseType' 'criteriaStats' 'noch NICHT live verifiziert' 'modell_abgleich.py' 'ToBeDemolished' 'IsEditable' 'CaptureView: Fehlerantwort schemakonform' 'SetTextSize' 'elm_schema_scan.py' 'ZWEI'; do grep -q "$t" reference/mcp-extension.md || { echo "Doku fehlt: $t"; exit 1; }; done && grep -c 'FindElements' ELM_SAB_Tapir/README.md && git fetch -q origin main ci-status && git log origin/main --format=%B -n 6 | grep -c 'Co-Authored-By: Claude Opus 5.5' && git merge-base --is-ancestor HEAD origin/main && git show origin/ci-status:status.md | head -12 && git show origin/ci-status:status.md | grep -q "$(git rev-parse --short=7 HEAD)" && ! git show origin/ci-status:status.md | grep -q 'fehlgeschlagen'</automated>
  </verify>
  <done>Doku-Abschnitt 0.9.23 (Anlass, Beispiele, Blatt-Referenz, Antwortfelder, scope-Tabelle mit WiSchu-Werten, Fallen, CaptureView-Unterpunkt mit offener Befehlsliste, RunCriteriaSet nicht verfügbar, Stand, Testplan mit zwei getrennten Blöcken) + README-Zeile vorhanden; alle Commits auf origin/main; ci-status nennt einen SHA dieser Serie mit allen sechs Builds ohne „fehlgeschlagen" (oder Abbruch nach 4 Fix-Runden sauber dokumentiert). Kein Bundle installiert.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| MCP/Claude → ELM_SAB JSON-Befehl | lokale, vom Nutzer gestartete Anfrage; Eingabe (where-Baum, Regex, Listen) ist frei formuliert |
| Befehl → Archicad-Datenbankzustand | Befehl wechselt die aktuelle Datenbank und (bei select) die Auswahl des Nutzers |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-261013-01 | Tampering | Datenbankwechsel in Execute | mitigate | nur über ExecuteActionForEachDatabase (Rückschaltung in CommandBase.cpp Z. 648 auf jedem Pfad); alle Parse-/Namensfehler vor dem Wechsel; Laufzeitfehler über ctx.fatalError erst nach Rückschaltung melden |
| T-261013-02 | Tampering | Modell | mitigate | ausschließlich Lese-APIs (GetElemList/Get/GetHeader/GetMemo/GetPropertyValues…); keine ACAPI_Element_Change/Create/Delete, keine Undo-Klammer — per grep in Task 1/2 prüfbar |
| T-261013-03 | Tampering | Nutzer-Auswahl (select) | accept | nur bei ausdrücklichem select:true; bei 0 Treffern unverändert; Rücklese meldet Abweichungen; Doku verlangt Aufheben nach Tests |
| T-261013-04 | Denial of Service | regex-Operator, riesige Ergebnislisten | mitigate | Regex beim Parsen kompiliert, Ausnahmen beim Matchen abgefangen; limit 1…200000 (Vorgabe 20000), notSelected max. 500, countOnly; elapsedMs zur Diagnose (MCP-Zeitgrenze 600 s) |
| T-261013-06 | Repudiation | CaptureView-Fehlerpfad | mitigate | Fehlerform im Antwortschema erlaubt (oneOf), Fenstertyp + echter Code im Text — Fehlschläge werden nicht mehr als anonymes 4009 verschluckt |
| T-261013-05 | Information Disclosure | Antwort | accept | nur Projektdaten an den lokalen Aufrufer, wie bestehende Lesebefehle |
</threat_model>

<verification>
- Task-Verifies 1–4 grün (Struktur, Vertrag, keine C++20-Konstrukte, Commits mit Co-Authored-By, Push, ci-status).
- Nur Lese-APIs: cd /Users/ap/.claude/repos/archicad-skill/ELM_SAB_Tapir/Sources && ! grep -nE 'ACAPI_Element_(Change|Create|Delete|ChangeMore)|ACAPI_CallUndoableCommand' FindElementsCommand.cpp FindElementsLeaves.cpp CaptureViewCommand.hpp
- git diff fad4abb..origin/main --stat zeigt genau die 12 Dateien aus files_modified (keine .planning-Dateien, CMakeLists unverändert).
</verification>

<success_criteria>
- CaptureView meldet Fehler (v. a. Layout-Fenster) schemakonform mit Fenstertyp + Archicad-Code; weitere betroffene ELM-Befehle als offener Punkt dokumentiert.
- ELM_SAB 0.9.23 mit `FindElements` gebaut für AC27/28/29 × Mac/Win (ci-status), registriert mit deutscher Beschreibung.
- Alle Blätter aus Handoff Abschnitt 2 + `scope`, `return`, `countOnly`, `limit`, `criteriaStats`, `select` (mit Rücklese) und optional `databases` umgesetzt; Fehler immer mit Namen.
- Doku-Abschnitt + README-Zeile + Testplan für Einbau und WiSchu-Gegenprobe vorhanden; Stand „noch NICHT live verifiziert".
</success_criteria>

## Quellen-Abdeckung (Audit)

| Quelle | Punkt | Task |
|---|---|---|
| GOAL | ein Lesebefehl, beliebig tiefes and/or/not wie „Suchen & aktivieren" | 1 (Motor) |
| Handoff §2 | Blätter type, story, layer (inkl. sichtbar/geschützt/Ebenen-Kombination), flags, guids, inSelection, teamwork | 1 |
| Handoff §2 | Blätter property (alle 14 Operatoren), classification (includeChildren, unclassified), libPart, gdl, ifc (Typ, Attribut, Pset), hostedBy/hosts, inZone, bbox/near, designOption | 2 |
| Handoff Regeln | günstige Kriterien zuerst, Eigenschaften gebündelt, Kurzschluss | 1 (Sortierung/Kurzschluss), 2 (Bündel-Cache) |
| Handoff Regeln / Vorgabe 6 | kein stiller Nullbefund: Fehler mit Namen, criteriaStats, AC27/28-Fehler je Blatt | 1, 2 |
| Handoff Regeln | immer Grundriss-DB, `databases` nur auf Wunsch | 1 |
| Handoff Regeln | Rückgabe GUID/Typ/Geschoss/Ebene + return in EINEM Aufruf; optional Auswahl | 1 (Ausgabe, select), 2 (return-Werte) |
| Handoff scope | projekt / wie3D / sichtbar / aktuellesGeschoss + WiSchu-Werte | 1 (Code), 4 (Doku/Testplan) |
| Vorgabe 2 | RunCriteriaSet entfällt, Doku-Vermerk | 4 |
| Vorgabe 3 | select mit ACAPI_Selection_Get-Rücklese, Standard false | 1 |
| Vorgaben 4/5/7 | nur lesen; sequenziell auf main; kein Bundle-Tausch, Testplan dokumentiert | 1–4 |
| Technik | ELM_SAB_VERSION 0.9.23, ADDON_VERSION unverändert, Registrierung deutsch, eigene Dateien, C++17, CI-Schleife max. 4 Runden | 1, 4 |
| Handoff §3 | Gegenprobe Türenabgleich 267 / je Geschoss / je Umbau-Status | 4 (Testplan; Ausführung Orchestrator) |
| Nachtrag 13.10. (1)(2)(4) | CaptureView: oneOf Erfolg/Fehler, Fenstertyp + echter Code, Erfolgsantwort unverändert | 1 (Helfer), 3 |
| Nachtrag 13.10. (3) | Layout: Export versuchen, sonst klare Ablehnung; keine Typsperre; kein Layout-Experiment (DevKit ohne Weg) | 3 |
| Nachtrag 13.10. (5) | weitere ELM-Befehle mit Schemafehler nur auflisten | 3 (Scan), 4 (Doku) |
| Nachtrag Doku | Unterpunkt CaptureView + Testschritte, Hinweis „ZWEI Änderungen im Bundle" | 4 |

<output>
Nach Abschluss `.planning/quick/261013-fel-elm-sab-0-9-23-findelements-mit-und-oder/261013-fel-SUMMARY.md` anlegen (Commits, CI-Lauf/SHA, Fix-Runden, Abweichungen) — NICHT committen (macht der Orchestrator).
</output>
