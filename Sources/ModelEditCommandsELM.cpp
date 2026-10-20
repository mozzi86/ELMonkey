#include "ModelEditCommandsELM.hpp"
#include "ElmPreflight.hpp"
#include "MigrationHelper.hpp"
#include "CommandBase.hpp"

#include "FileSystem.hpp"
#include "Location.hpp"

#include <set>
#include <vector>

namespace {

// AC27/28 kennen nur Contains (loc, bool*) - die 1-Parameter-Form gibt es erst ab AC29.
bool ExistsELM (const IO::Location& loc)
{
    bool result = false;
    return IO::fileSystem.Contains (loc, &result) == NoError && result;
}

GS::ObjectState ErrOS (GSErrCode code, const GS::UniString& message)
{
    GS::ObjectState e;
    e.Add ("code", (Int32) code);
    e.Add ("message", message);
    return e;
}

GS::ObjectState FailOS (GSErrCode code, const GS::UniString& message)
{
    GS::ObjectState r;
    r.Add ("success", false);
    r.Add ("error", ErrOS (code, message));
    return r;
}

API_Guid GuidOf (const GS::ObjectState* idOs)
{
    if (idOs == nullptr) return APINULLGuid;
    GS::UniString s;
    if (!idOs->Get ("guid", s)) return APINULLGuid;
    return APIGuidFromString (s.ToCStr ().Get ());
}

API_Guid NewGuid ()
{
    GS::Guid g;
    g.Generate ();
    return GSGuid2APIGuid (g);
}

// Attribut per Name oder Index aufloesen. false + msg bei Fehler; true + found=false, wenn nichts angegeben.
bool ResolveAttr (const GS::ObjectState& item, const char* nameField, const char* indexField, API_AttrTypeID type,
                  API_AttributeIndex& out, bool& found, GS::UniString& msg)
{
    found = false;
    GS::UniString name;
    Int32 index = 0;
    if (item.Get (nameField, name) && !name.IsEmpty ()) {
        API_Attr_Head head = {};
        head.typeID = type;
        head.uniStringNamePtr = &name;
        if (ACAPI_Attribute_Search (&head) != NoError) {
            msg = GS::UniString ("[ATTRIBUTE_NOT_FOUND] '") + name + GS::UniString ("' (") + GS::UniString (nameField) + GS::UniString (") gibt es nicht");
            return false;
        }
        out = head.index;
        found = true;
        return true;
    }
    if (item.Get (indexField, index) && index > 0) {
        API_Attribute attr = {};
        attr.header.typeID = type;
        attr.header.index = ACAPI_CreateAttributeIndex (index);
        if (ACAPI_Attribute_Get (&attr) != NoError) {
            msg = GS::UniString ("[ATTRIBUTE_NOT_FOUND] ") + GS::UniString (indexField) + GS::UniString (" ") + GS::ValueToUniString (index) + GS::UniString (" gibt es nicht");
            return false;
        }
        out = attr.header.index;
        found = true;
    }
    return true;
}

GS::UniString AttrName (API_AttrTypeID type, const API_AttributeIndex& idx)
{
    API_Attribute attr = {};
    attr.header.typeID = type;
    attr.header.index = idx;
    if (ACAPI_Attribute_Get (&attr) == NoError) return GS::UniString (attr.header.name);
    return GS::UniString ();
}

GS::UniString StructName (API_ModelElemStructureType t)
{
    switch (t) {
        case API_BasicStructure:     return "basic";
        case API_CompositeStructure: return "composite";
        case API_ProfileStructure:   return "profile";
        default:                     return "unknown";
    }
}

} // namespace

// ============================================================================================
// B2 ChangePropertyDefinitions
// ============================================================================================

GS::Optional<GS::UniString> ChangePropertyDefinitionsELMCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "propertyDefinitions": {
                "type": "array",
                "minItems": 1,
                "items": {
                    "type": "object",
                    "properties": {
                        "propertyId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] },
                        "name": { "type": "string", "description": "Neuer Name (optional)." },
                        "description": { "type": "string", "description": "Neue Beschreibung (optional)." },
                        "addEnumValues": {
                            "type": "array",
                            "description": "Auswahlwerte ANHAENGEN (vorhandene bleiben mit ihrer ID). Doppelte Anzeigewerte werden uebersprungen.",
                            "items": { "type": "object", "properties": { "displayValue": { "type": "string" }, "nonLocalizedValue": { "type": "string" } }, "required": ["displayValue"] }
                        },
                        "availability": {
                            "type": "object",
                            "description": "Verfuegbarkeit fuer Klassifizierungen: set ersetzt die Liste, add/remove aendern sie.",
                            "properties": {
                                "set":    { "type": "array", "items": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                                "add":    { "type": "array", "items": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                                "remove": { "type": "array", "items": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] } }
                            }
                        }
                    },
                    "required": ["propertyId"]
                }
            }
        },
        "additionalProperties": false,
        "required": ["propertyDefinitions"]
    })";
}

GS::ObjectState ChangePropertyDefinitionsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> defs;
    parameters.Get ("propertyDefinitions", defs);
    if (defs.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("propertyDefinitions ist leer."));

    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("results");

    for (const GS::ObjectState& item : defs) {
        const API_Guid guid = GuidOf (item.Get ("propertyId"));
        API_PropertyDefinition def;
        def.guid = guid;
        if (guid == APINULLGuid || ACAPI_Property_GetPropertyDefinition (def) != NoError) {
            results (FailOS (APIERR_BADID, GS::UniString ("[NOT_FOUND] Eigenschaft nicht gefunden")));
            continue;
        }

        GS::UniString newName, newDescr;
        if (item.Get ("name", newName) && !newName.IsEmpty ()) def.name = newName;
        if (item.Get ("description", newDescr)) def.description = newDescr;

        // Auswahlwerte anhaengen
        GS::Array<GS::ObjectState> addEnums;
        Int32 enumsAdded = 0, enumsSkipped = 0;
        GS::Array<GS::UniString> wantedEnums;
        if (item.Get ("addEnumValues", addEnums) && !addEnums.IsEmpty ()) {
            if (def.collectionType != API_PropertySingleChoiceEnumerationCollectionType &&
                def.collectionType != API_PropertyMultipleChoiceEnumerationCollectionType) {
                results (FailOS (APIERR_BADPARS, GS::UniString ("[NOT_ENUMERATION] '") + def.name + GS::UniString ("' ist keine Auswahl-Eigenschaft")));
                continue;
            }
            for (const GS::ObjectState& e : addEnums) {
                GS::UniString display;
                if (!e.Get ("displayValue", display) || display.IsEmpty ()) continue;
                wantedEnums.Push (display);
                bool exists = false;
                for (const API_SingleEnumerationVariant& v : def.possibleEnumValues)
                    if (v.displayVariant.uniStringValue == display) { exists = true; break; }
                if (exists) { ++enumsSkipped; continue; }
                API_SingleEnumerationVariant v;
                v.keyVariant.type = API_PropertyGuidValueType;
                v.keyVariant.guidValue = NewGuid ();
                v.displayVariant.type = def.valueType;
                v.displayVariant.uniStringValue = display;
                GS::UniString nonLoc;
                if (e.Get ("nonLocalizedValue", nonLoc) && !nonLoc.IsEmpty ()) v.nonLocalizedValue = nonLoc;
                def.possibleEnumValues.Push (v);
                ++enumsAdded;
            }
        }

        // Verfuegbarkeit
        const GS::ObjectState* avail = item.Get ("availability");
        std::set<API_Guid> wantIn, wantOut;
        if (avail != nullptr) {
            GS::Array<GS::ObjectState> setList, addList, removeList;
            if (avail->Get ("set", setList)) {
                def.availability.Clear ();
                for (const GS::ObjectState& c : setList) { const API_Guid g = GuidOf (&c); if (g != APINULLGuid) { def.availability.Push (g); wantIn.insert (g); } }
            }
            if (avail->Get ("add", addList)) {
                for (const GS::ObjectState& c : addList) {
                    const API_Guid g = GuidOf (&c);
                    if (g == APINULLGuid) continue;
                    wantIn.insert (g);
                    if (!def.availability.Contains (g)) def.availability.Push (g);
                }
            }
            if (avail->Get ("remove", removeList)) {
                for (const GS::ObjectState& c : removeList) {
                    const API_Guid g = GuidOf (&c);
                    if (g == APINULLGuid) continue;
                    wantOut.insert (g);
                    def.availability.DeleteAll (g);
                }
            }
        }

        GSErrCode err = ACAPI_CallUndoableCommand ("ELM_SAB ChangePropertyDefinitions", [&] () -> GSErrCode {
            return ACAPI_Property_ChangePropertyDefinition (def);
        });
        if (err != NoError) {
            results (FailOS (err, GS::UniString ("ACAPI_Property_ChangePropertyDefinition abgelehnt (Teamwork: Eigenschaften reservieren?)")));
            continue;
        }

        // Ruecklese
        API_PropertyDefinition check;
        check.guid = guid;
        GS::ObjectState r;
        r.Add ("propertyId", CreateGuidObjectStateELM (guid));
        if (ACAPI_Property_GetPropertyDefinition (check) != NoError) {
            r.Add ("success", false);
            r.Add ("error", ErrOS (APIERR_GENERAL, GS::UniString ("[NO_EFFECT] Eigenschaft nach dem Aendern nicht mehr lesbar")));
            results (r);
            continue;
        }
        bool ok = check.guid == guid;
        for (const GS::UniString& w : wantedEnums) {
            bool present = false;
            for (const API_SingleEnumerationVariant& v : check.possibleEnumValues)
                if (v.displayVariant.uniStringValue == w) { present = true; break; }
            ok = ok && present;
        }
        for (const API_Guid& g : wantIn)  ok = ok && check.availability.Contains (g);
        for (const API_Guid& g : wantOut) ok = ok && !check.availability.Contains (g);
        if (!newName.IsEmpty ()) ok = ok && check.name == newName;

        r.Add ("success", ok);
        r.Add ("guidUnchanged", check.guid == guid);
        r.Add ("name", check.name);
        r.Add ("enumValueCount", (Int32) check.possibleEnumValues.GetSize ());
        r.Add ("enumValuesAdded", enumsAdded);
        r.Add ("enumValuesSkippedAsDuplicate", enumsSkipped);
        r.Add ("availabilityCount", (Int32) check.availability.GetSize ());
        if (!ok) r.Add ("error", ErrOS (APIERR_GENERAL, GS::UniString ("[NO_EFFECT] Ruecklese weicht vom Sollzustand ab")));
        results (r);
    }
    return response;
}

// ============================================================================================
// B3 SetStructureOfElements
// ============================================================================================

GS::Optional<GS::UniString> SetStructureOfElementsELMCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "minItems": 1,
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] },
                        "structureType": { "type": "string", "enum": ["basic", "composite", "profile"] },
                        "buildingMaterialName": { "type": "string" }, "buildingMaterialIndex": { "type": "integer" },
                        "compositeName": { "type": "string" },        "compositeIndex": { "type": "integer" },
                        "profileName": { "type": "string" },          "profileIndex": { "type": "integer" }
                    },
                    "required": ["elementId", "structureType"]
                }
            },
            "reserve": { "type": "boolean", "description": "Teamwork: vorab reservieren und danach freigeben (Standard true)." }
        },
        "additionalProperties": false,
        "required": ["elements"]
    })";
}

GS::ObjectState SetStructureOfElementsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> items;
    parameters.Get ("elements", items);
    if (items.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("elements ist leer."));
    bool reserve = true;
    parameters.Get ("reserve", reserve);

    const bool teamwork = reserve && ACAPI_Teamwork_HasConnection ();
    if (teamwork) {
        GS::Array<API_Guid> toReserve;
        for (const GS::ObjectState& it : items) { const API_Guid g = GuidOf (it.Get ("elementId")); if (g != APINULLGuid) toReserve.Push (g); }
        GS::HashTable<API_Guid, short> conflicts;
        ACAPI_Teamwork_ReserveElements (toReserve, &conflicts, false);
    }

    const short myUserId = MyTeamworkUserIdELM ();
    std::map<Int32, GS::UniString> layerCache;
    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("results");
    GS::Array<API_Guid> touched;

    for (const GS::ObjectState& it : items) {
        const API_Guid guid = GuidOf (it.Get ("elementId"));
        const GS::UniString reason = PreflightElementELM (guid, myUserId, layerCache);
        if (!reason.IsEmpty ()) { results (FailOS (ReasonToErrCodeELM (reason), reason)); continue; }

        GS::UniString st;
        it.Get ("structureType", st);
        API_ModelElemStructureType wantType = API_BasicStructure;
        if (st == "composite") wantType = API_CompositeStructure;
        else if (st == "profile") wantType = API_ProfileStructure;
        else if (st != "basic") { results (FailOS (APIERR_BADPARS, "structureType muss basic, composite oder profile sein")); continue; }

        API_AttributeIndex mat, comp, prof;
        bool hasMat = false, hasComp = false, hasProf = false;
        GS::UniString msg;
        if (!ResolveAttr (it, "buildingMaterialName", "buildingMaterialIndex", API_BuildingMaterialID, mat, hasMat, msg) ||
            !ResolveAttr (it, "compositeName", "compositeIndex", API_CompWallID, comp, hasComp, msg) ||
            !ResolveAttr (it, "profileName", "profileIndex", API_ProfileID, prof, hasProf, msg)) {
            results (FailOS (APIERR_BADNAME, msg));
            continue;
        }
        if ((wantType == API_BasicStructure && !hasMat) || (wantType == API_CompositeStructure && !hasComp) || (wantType == API_ProfileStructure && !hasProf)) {
            results (FailOS (APIERR_BADPARS, GS::UniString ("structureType '") + st + GS::UniString ("' braucht ") +
                (wantType == API_BasicStructure ? "buildingMaterialName/-Index" : wantType == API_CompositeStructure ? "compositeName/-Index" : "profileName/-Index")));
            continue;
        }

        API_Element element = {};
        element.header.guid = guid;
        if (ACAPI_Element_Get (&element) != NoError) { results (FailOS (APIERR_BADID, "[NOT_FOUND] Element nicht lesbar")); continue; }
        const API_ElemTypeID typeId = GetElemTypeId (element.header);

        API_Element mask = {};
        ACAPI_ELEMENT_MASK_CLEAR (mask);
        API_ElementMemo memo = {};
        UInt64 memoMask = 0;   // APIMemoMask_*Segment sind 64-Bit-Werte
        GS::UniString typeErr;

        switch (typeId) {
            case API_WallID:
                element.wall.modelElemStructureType = wantType;
                ACAPI_ELEMENT_MASK_SET (mask, API_WallType, modelElemStructureType);
                if (hasMat)  { element.wall.buildingMaterial = mat;  ACAPI_ELEMENT_MASK_SET (mask, API_WallType, buildingMaterial); }
                if (hasComp) { element.wall.composite = comp;        ACAPI_ELEMENT_MASK_SET (mask, API_WallType, composite); }
                if (hasProf) { element.wall.profileAttr = prof;      ACAPI_ELEMENT_MASK_SET (mask, API_WallType, profileAttr); }
                break;
            case API_SlabID:
                if (wantType == API_ProfileStructure) { typeErr = "Decken haben keinen Profil-Aufbau"; break; }
                element.slab.modelElemStructureType = wantType;
                ACAPI_ELEMENT_MASK_SET (mask, API_SlabType, modelElemStructureType);
                if (hasMat)  { element.slab.buildingMaterial = mat; ACAPI_ELEMENT_MASK_SET (mask, API_SlabType, buildingMaterial); }
                if (hasComp) { element.slab.composite = comp;       ACAPI_ELEMENT_MASK_SET (mask, API_SlabType, composite); }
                break;
            case API_RoofID:
                if (wantType == API_ProfileStructure) { typeErr = "Daecher haben keinen Profil-Aufbau"; break; }
                element.roof.shellBase.modelElemStructureType = wantType;
                ACAPI_ELEMENT_MASK_SET (mask, API_RoofType, shellBase.modelElemStructureType);
                if (hasMat)  { element.roof.shellBase.buildingMaterial = mat; ACAPI_ELEMENT_MASK_SET (mask, API_RoofType, shellBase.buildingMaterial); }
                if (hasComp) { element.roof.shellBase.composite = comp;       ACAPI_ELEMENT_MASK_SET (mask, API_RoofType, shellBase.composite); }
                break;
            case API_ShellID:
                if (wantType == API_ProfileStructure) { typeErr = "Schalen haben keinen Profil-Aufbau"; break; }
                element.shell.shellBase.modelElemStructureType = wantType;
                ACAPI_ELEMENT_MASK_SET (mask, API_ShellType, shellBase.modelElemStructureType);
                if (hasMat)  { element.shell.shellBase.buildingMaterial = mat; ACAPI_ELEMENT_MASK_SET (mask, API_ShellType, shellBase.buildingMaterial); }
                if (hasComp) { element.shell.shellBase.composite = comp;       ACAPI_ELEMENT_MASK_SET (mask, API_ShellType, shellBase.composite); }
                break;
            case API_ColumnID:
            case API_BeamID: {
                if (wantType == API_CompositeStructure) { typeErr = "Stuetzen/Unterzuege haben keinen Mehrschicht-Aufbau (Segmente: basic oder profile)"; break; }
                const bool isColumn = (typeId == API_ColumnID);
                memoMask = isColumn ? APIMemoMask_ColumnSegment : APIMemoMask_BeamSegment;
                if (ACAPI_Element_GetMemo (guid, &memo, memoMask) != NoError) { typeErr = "Segment-Memo nicht lesbar"; break; }
                const UInt32 n = isColumn ? element.column.nSegments : element.beam.nSegments;
                for (UInt32 s = 0; s < n; ++s) {
                    API_AssemblySegmentData& seg = isColumn ? memo.columnSegments[s].assemblySegmentData : memo.beamSegments[s].assemblySegmentData;
                    seg.modelElemStructureType = wantType;
                    if (hasMat)  seg.buildingMaterial = mat;
                    if (hasProf) seg.profileAttr = prof;
                }
                break;
            }
            default:
                typeErr = GS::UniString ("Elementtyp ") + GetElementTypeNonLocalizedName (typeId) + GS::UniString (" wird nicht unterstuetzt (Wand, Decke, Dach, Schale, Stuetze, Unterzug)");
                break;
        }
        if (!typeErr.IsEmpty ()) {
            ACAPI_DisposeElemMemoHdls (&memo);
            results (FailOS (APIERR_BADELEMENTTYPE, typeErr));
            continue;
        }

        GSErrCode err = ACAPI_CallUndoableCommand ("ELM_SAB SetStructureOfElements", [&] () -> GSErrCode {
            return ACAPI_Element_Change (&element, &mask, memoMask != 0 ? &memo : nullptr, memoMask, true);
        });
        ACAPI_DisposeElemMemoHdls (&memo);
        if (err != NoError) { results (FailOS (err, "ACAPI_Element_Change abgelehnt (Aufbau fuer diesen Elementtyp zugelassen?)")); continue; }
        touched.Push (guid);

        // Ruecklese
        API_Element check = {};
        check.header.guid = guid;
        GS::ObjectState r;
        r.Add ("elementId", CreateGuidObjectStateELM (guid));
        bool ok = ACAPI_Element_Get (&check) == NoError;
        API_ModelElemStructureType gotType = API_BasicStructure;
        API_AttributeIndex gotMat, gotComp, gotProf;
        if (ok) {
            switch (typeId) {
                case API_WallID:  gotType = check.wall.modelElemStructureType; gotMat = check.wall.buildingMaterial; gotComp = check.wall.composite; gotProf = check.wall.profileAttr; break;
                case API_SlabID:  gotType = check.slab.modelElemStructureType; gotMat = check.slab.buildingMaterial; gotComp = check.slab.composite; break;
                case API_RoofID:  gotType = check.roof.shellBase.modelElemStructureType; gotMat = check.roof.shellBase.buildingMaterial; gotComp = check.roof.shellBase.composite; break;
                case API_ShellID: gotType = check.shell.shellBase.modelElemStructureType; gotMat = check.shell.shellBase.buildingMaterial; gotComp = check.shell.shellBase.composite; break;
                case API_ColumnID:
                case API_BeamID: {
                    API_ElementMemo cm = {};
                    if (ACAPI_Element_GetMemo (guid, &cm, memoMask) == NoError) {
                        const API_AssemblySegmentData& s0 = (typeId == API_ColumnID) ? cm.columnSegments[0].assemblySegmentData : cm.beamSegments[0].assemblySegmentData;
                        gotType = s0.modelElemStructureType; gotMat = s0.buildingMaterial; gotProf = s0.profileAttr;
                    } else ok = false;
                    ACAPI_DisposeElemMemoHdls (&cm);
                    break;
                }
                default: break;
            }
        }
        ok = ok && gotType == wantType;
        if (ok && wantType == API_BasicStructure)     ok = gotMat == mat;
        if (ok && wantType == API_CompositeStructure) ok = gotComp == comp;
        if (ok && wantType == API_ProfileStructure)   ok = gotProf == prof;
        r.Add ("success", ok);
        r.Add ("elementType", GetElementTypeNonLocalizedName (typeId));
        r.Add ("structureType", StructName (gotType));
        if (wantType == API_BasicStructure)     r.Add ("buildingMaterialName", AttrName (API_BuildingMaterialID, gotMat));
        if (wantType == API_CompositeStructure) r.Add ("compositeName", AttrName (API_CompWallID, gotComp));
        if (wantType == API_ProfileStructure)   r.Add ("profileName", AttrName (API_ProfileID, gotProf));
        if (!ok) r.Add ("error", ErrOS (APIERR_GENERAL, GS::UniString ("[NO_EFFECT] Ruecklese weicht vom Sollaufbau ab")));
        results (r);
    }

    if (teamwork && !touched.IsEmpty ())
        ACAPI_Teamwork_ReleaseElements (touched, false);
    return response;
}

// ============================================================================================
// B7 SaveProjectCopy
// ============================================================================================

GS::Optional<GS::UniString> SaveProjectCopyELMCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "targetPath": { "type": "string", "description": "Absoluter Zielpfad der Kopie (.pln)." },
            "overwrite":  { "type": "boolean", "description": "Vorhandene Zieldatei ueberschreiben (Standard false). Das Original wird NIE ueberschrieben." }
        },
        "additionalProperties": false,
        "required": ["targetPath"]
    })";
}

GS::ObjectState SaveProjectCopyELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::UniString targetPath;
    parameters.Get ("targetPath", targetPath);
    bool overwrite = false;
    parameters.Get ("overwrite", overwrite);
    if (targetPath.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("targetPath fehlt."));

    API_ProjectInfo info = {};
    if (ACAPI_ProjectOperation_Project (&info) != NoError)
        return CreateErrorResponseELM (APIERR_GENERAL, GS::UniString ("Projektinfo nicht lesbar."));
    if (info.teamwork)
        return CreateErrorResponseELM (APIERR_REFUSEDCMD, GS::UniString ("[TEAMWORK] Teamwork-Projekt: keine lokale Datei zum Kopieren. Sicherungspunkt per TeamworkSend; ein 'Sichern unter' wuerde auf eine lokale Kopie umschalten."));
    if (info.untitled || info.location == nullptr)
        return CreateErrorResponseELM (APIERR_REFUSEDCMD, GS::UniString ("[UNTITLED] Unbenanntes Projekt: erst in Archicad sichern, dann kopieren."));

    const IO::Location source = *info.location;
    const IO::Location target (targetPath);
    GS::UniString sourcePath;
    source.ToPath (&sourcePath);
    if (sourcePath == targetPath)
        return CreateErrorResponseELM (APIERR_REFUSEDPAR, GS::UniString ("[ORIGINAL] targetPath ist die Originaldatei - verweigert."));
    if (ExistsELM (target) && !overwrite)
        return CreateErrorResponseELM (APIERR_REFUSEDPAR, GS::UniString ("[EXISTS] Zieldatei existiert bereits (overwrite:true zum Ersetzen)."));
    IO::Location targetDir = target;
    targetDir.DeleteLastLocalName ();
    if (!ExistsELM (targetDir))
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("[NO_FOLDER] Zielordner existiert nicht."));

    GSErrCode err = ACAPI_ProjectOperation_Save ();
    if (err != NoError)
        return CreateErrorResponseELM (err, GS::UniString ("Sichern des Originals fehlgeschlagen - keine Kopie angelegt."));

    if (ExistsELM (target) && overwrite)
        IO::fileSystem.Delete (target);
    err = IO::fileSystem.Copy (source, target);
    if (err != NoError || !ExistsELM (target))
        return CreateErrorResponseELM (err != NoError ? err : APIERR_GENERAL, GS::UniString ("Kopieren fehlgeschlagen."));

    GS::ObjectState response;
    response.Add ("success", true);
    response.Add ("originalPath", sourcePath);
    response.Add ("copyPath", targetPath);
    response.Add ("activeProjectUnchanged", true);
    return response;
}
