#include "ConstructionPensCommandsELM.hpp"
#include "ElmPreflight.hpp"
#include "MigrationHelper.hpp"
#include "CommandBase.hpp"

#include <map>
#include <string>
#include <vector>

namespace {

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

// Nachbildung von AttrName aus ModelEditCommandsELM.cpp (dort im anonymen Namespace).
GS::UniString AttrNameELM (API_AttrTypeID type, const API_AttributeIndex& idx)
{
    API_Attribute attr = {};
    attr.header.typeID = type;
    attr.header.index = idx;
    if (ACAPI_Attribute_Get (&attr) == NoError) return GS::UniString (attr.header.name);
    return GS::UniString ();
}

const char* UnsupportedTypesText ()
{
    return " wird nicht unterstuetzt (Wand, Stuetze, Unterzug, Decke, Dach, Schale, Fassade, Morph)";
}

// --------------------------------------------------------------------------------------------
// Feldtabelle: EINE Quelle fuer Get und Set (kein doppelter Typ-Switch).
// Namen exakt wie im DevKit (APIdefs_Elements.h), bei Dach/Schale ohne Praefix "shellBase.".
// --------------------------------------------------------------------------------------------
struct PenFieldELM {
    enum Kind { Pen, OverriddenPen, Ltype };
    const char* name;
    Kind        kind;
    void*       ptr;
};

void AddPen (std::vector<PenFieldELM>& v, const char* name, short& f)                { v.push_back ({ name, PenFieldELM::Pen, &f }); }
void AddOvr (std::vector<PenFieldELM>& v, const char* name, API_OverriddenPen& f)    { v.push_back ({ name, PenFieldELM::OverriddenPen, &f }); }
void AddLty (std::vector<PenFieldELM>& v, const char* name, API_AttributeIndex& f)   { v.push_back ({ name, PenFieldELM::Ltype, &f }); }

void AddShellBase (std::vector<PenFieldELM>& v, API_ShellBaseType& b)
{
    AddPen (v, "pen", b.pen);
    AddPen (v, "sectContPen", b.sectContPen);
    AddPen (v, "floorFillPen", b.floorFillPen);
    AddPen (v, "floorFillBGPen", b.floorFillBGPen);
    AddPen (v, "aboveViewLinePen", b.aboveViewLinePen);
    AddOvr (v, "cutFillPen", b.cutFillPen);
    AddOvr (v, "cutFillBackgroundPen", b.cutFillBackgroundPen);
    AddLty (v, "ltypeInd", b.ltypeInd);
    AddLty (v, "sectContLtype", b.sectContLtype);
}

std::vector<PenFieldELM> CollectPenFieldsELM (API_Element& e, API_ElemTypeID typeId)
{
    std::vector<PenFieldELM> v;
    switch (typeId) {
        case API_WallID:
            AddPen (v, "contPen", e.wall.contPen);
            AddPen (v, "contPen3D", e.wall.contPen3D);
            AddPen (v, "aboveViewLinePen", e.wall.aboveViewLinePen);
            AddOvr (v, "cutFillPen", e.wall.cutFillPen);
            AddOvr (v, "cutFillBackgroundPen", e.wall.cutFillBackgroundPen);
            AddLty (v, "contLtype", e.wall.contLtype);
            break;
        case API_ColumnID:
            AddPen (v, "corePen", e.column.corePen);
            AddPen (v, "venLinePen", e.column.venLinePen);
            AddPen (v, "coreSymbolPen", e.column.coreSymbolPen);
            AddPen (v, "hiddenLinePen", e.column.hiddenLinePen);
            AddPen (v, "belowViewLinePen", e.column.belowViewLinePen);
            AddPen (v, "aboveViewLinePen", e.column.aboveViewLinePen);
            AddPen (v, "coverFillForegroundPen", e.column.coverFillForegroundPen);
            AddPen (v, "coverFillBackgroundPen", e.column.coverFillBackgroundPen);
            AddOvr (v, "cutFillPen", e.column.cutFillPen);
            AddOvr (v, "cutFillBackgroundPen", e.column.cutFillBackgroundPen);
            AddLty (v, "contLtype", e.column.contLtype);
            break;
        case API_BeamID:
            AddPen (v, "cutContourLinePen", e.beam.cutContourLinePen);
            AddPen (v, "refPen", e.beam.refPen);
            AddPen (v, "hiddenLinePen", e.beam.hiddenLinePen);
            AddPen (v, "belowViewLinePen", e.beam.belowViewLinePen);
            AddPen (v, "aboveViewLinePen", e.beam.aboveViewLinePen);
            AddPen (v, "coverFillForegroundPen", e.beam.coverFillForegroundPen);
            AddPen (v, "coverFillBackgroundPen", e.beam.coverFillBackgroundPen);
            AddOvr (v, "cutFillPen", e.beam.cutFillPen);
            AddOvr (v, "cutFillBackgroundPen", e.beam.cutFillBackgroundPen);
            AddLty (v, "refLtype", e.beam.refLtype);
            break;
        case API_SlabID:
            AddPen (v, "pen", e.slab.pen);
            AddPen (v, "sectContPen", e.slab.sectContPen);
            AddPen (v, "hiddenContourLinePen", e.slab.hiddenContourLinePen);
            AddPen (v, "floorFillPen", e.slab.floorFillPen);
            AddPen (v, "floorFillBGPen", e.slab.floorFillBGPen);
            AddOvr (v, "cutFillPen", e.slab.cutFillPen);
            AddOvr (v, "cutFillBackgroundPen", e.slab.cutFillBackgroundPen);
            AddLty (v, "ltypeInd", e.slab.ltypeInd);
            AddLty (v, "sectContLtype", e.slab.sectContLtype);
            break;
        case API_RoofID:
            AddShellBase (v, e.roof.shellBase);
            break;
        case API_ShellID:
            AddShellBase (v, e.shell.shellBase);
            break;
        case API_CurtainWallID:
            AddPen (v, "pen", e.curtainWall.pen);
            AddPen (v, "overheadLinePen", e.curtainWall.overheadLinePen);
            AddPen (v, "uncutLinePen", e.curtainWall.uncutLinePen);
            AddPen (v, "frameCutLinePen", e.curtainWall.frameCutLinePen);
            AddPen (v, "panelCutLinePen", e.curtainWall.panelCutLinePen);
            AddOvr (v, "cutFillPen", e.curtainWall.cutFillPen);
            AddOvr (v, "cutFillBackgroundPen", e.curtainWall.cutFillBackgroundPen);
            break;
        case API_MorphID:
            AddPen (v, "cutLinePen", e.morph.cutLinePen);
            AddPen (v, "uncutLinePen", e.morph.uncutLinePen);
            AddPen (v, "overheadLinePen", e.morph.overheadLinePen);
            AddPen (v, "coverFillPen", e.morph.coverFillPen);
            AddPen (v, "coverFillBGPen", e.morph.coverFillBGPen);
            AddOvr (v, "cutFillPen", e.morph.cutFillPen);
            AddOvr (v, "cutFillBackgroundPen", e.morph.cutFillBackgroundPen);
            break;
        default:
            break;
    }
    return v;
}

// Schluessel "pen" (CONTEXT). Tapirs CreateOverriddenPenObjectState nutzt "penIndex" - bewusst
// nicht verwendet, damit Get-Ausgabe und Set-Eingabe (fields) dieselbe Form haben.
// APIOptional hat keine HasValue()-Methode, nur die Member hasValue/value.
GS::ObjectState OverriddenPenOS (const API_OverriddenPen& p)
{
    GS::ObjectState os;
    os.Add ("overridden", p.hasValue);
    os.Add ("pen", (Int32) p.value);
    return os;
}

// Aufbau des Elements; isComposite/compositeOut fuer includeComposites.
GS::ObjectState StructureOS (const API_Element& e, API_ElemTypeID typeId, const API_Guid& guid,
                             bool& isComposite, API_AttributeIndex& compositeOut)
{
    isComposite = false;
    GS::ObjectState os;
    auto addFromBase = [&] (API_ModelElemStructureType t, const API_AttributeIndex& mat,
                            const API_AttributeIndex& comp, const API_AttributeIndex* prof) {
        if (t == API_CompositeStructure) {
            os.Add ("structureType", GS::UniString ("composite"));
            os.Add ("compositeIndex", comp.ToInt32_Deprecated ());
            os.Add ("compositeName", AttrNameELM (API_CompWallID, comp));
            isComposite = true;
            compositeOut = comp;
        } else if (t == API_ProfileStructure && prof != nullptr) {
            os.Add ("structureType", GS::UniString ("profile"));
            os.Add ("profileIndex", prof->ToInt32_Deprecated ());
            os.Add ("profileName", AttrNameELM (API_ProfileID, *prof));
        } else {
            os.Add ("structureType", GS::UniString ("basic"));
            os.Add ("buildingMaterialIndex", mat.ToInt32_Deprecated ());
            os.Add ("buildingMaterialName", AttrNameELM (API_BuildingMaterialID, mat));
        }
    };

    switch (typeId) {
        case API_WallID:
            addFromBase (e.wall.modelElemStructureType, e.wall.buildingMaterial, e.wall.composite, &e.wall.profileAttr);
            break;
        case API_SlabID:
            addFromBase (e.slab.modelElemStructureType, e.slab.buildingMaterial, e.slab.composite, nullptr);
            break;
        case API_RoofID:
            addFromBase (e.roof.shellBase.modelElemStructureType, e.roof.shellBase.buildingMaterial, e.roof.shellBase.composite, nullptr);
            break;
        case API_ShellID:
            addFromBase (e.shell.shellBase.modelElemStructureType, e.shell.shellBase.buildingMaterial, e.shell.shellBase.composite, nullptr);
            break;
        case API_ColumnID:
        case API_BeamID: {
            // Aufbau steckt in den Segmenten (wie Ruecklese in SetStructureOfElements): Segment 0 + Anzahl.
            const bool isColumn = (typeId == API_ColumnID);
            const UInt32 n = isColumn ? e.column.nSegments : e.beam.nSegments;
            os.Add ("nSegments", (Int32) n);
            API_ElementMemo memo = {};
            const UInt64 memoMask = isColumn ? APIMemoMask_ColumnSegment : APIMemoMask_BeamSegment;
            if (n > 0 && ACAPI_Element_GetMemo (guid, &memo, memoMask) == NoError &&
                (isColumn ? memo.columnSegments != nullptr : memo.beamSegments != nullptr)) {
                const API_AssemblySegmentData& s0 = isColumn ? memo.columnSegments[0].assemblySegmentData : memo.beamSegments[0].assemblySegmentData;
                if (s0.modelElemStructureType == API_ProfileStructure) {
                    os.Add ("structureType", GS::UniString ("profile"));
                    os.Add ("profileIndex", s0.profileAttr.ToInt32_Deprecated ());
                    os.Add ("profileName", AttrNameELM (API_ProfileID, s0.profileAttr));
                } else {
                    os.Add ("structureType", GS::UniString ("basic"));
                    os.Add ("buildingMaterialIndex", s0.buildingMaterial.ToInt32_Deprecated ());
                    os.Add ("buildingMaterialName", AttrNameELM (API_BuildingMaterialID, s0.buildingMaterial));
                }
            }
            ACAPI_DisposeElemMemoHdls (&memo);
            break;
        }
        case API_MorphID:
            os.Add ("buildingMaterialIndex", e.morph.buildingMaterial.ToInt32_Deprecated ());
            os.Add ("buildingMaterialName", AttrNameELM (API_BuildingMaterialID, e.morph.buildingMaterial));
            break;
        default:   // Fassade: kein structureType
            break;
    }
    return os;
}

// Verbund lesen (Muster AttributeCommands.cpp GetCompositeAttributes): Schicht-Konturstift framePen
// je Schicht, linePen je Trennlinie (nComps + 1).
GS::ObjectState CompositeOS (const API_AttributeIndex& index)
{
    GS::ObjectState os;
    os.Add ("compositeIndex", index.ToInt32_Deprecated ());
    API_Attribute attr = {};
    attr.header.typeID = API_CompWallID;
    attr.header.index = index;
    if (ACAPI_Attribute_Get (&attr) != NoError) {
        os.Add ("error", ErrOS (APIERR_BADINDEX, GS::UniString ("[NOT_FOUND] Mehrschicht-Aufbau nicht lesbar")));
        return os;
    }
    os.Add ("compositeName", GS::UniString (attr.header.name));
    const auto& skins = os.AddList<GS::ObjectState> ("skins");
    const auto& lines = os.AddList<GS::ObjectState> ("lines");
    API_AttributeDefExt defs = {};
    if (ACAPI_Attribute_GetDefExt (API_CompWallID, index, &defs) == NoError) {
        if (defs.cwall_compItems != nullptr) {
            Int32 count = (Int32) (BMGetHandleSize ((GSHandle) defs.cwall_compItems) / sizeof (API_CWallComponent));
            count = GS::Min (count, (Int32) attr.compWall.nComps);
            for (Int32 i = 0; i < count; ++i) {
                const API_CWallComponent& c = (*defs.cwall_compItems)[i];
                GS::ObjectState s;
                s.Add ("index", i);
                s.Add ("framePen", (Int32) c.framePen);
                s.Add ("buildingMaterialIndex", c.buildingMaterial.ToInt32_Deprecated ());
                skins (s);
            }
        }
        if (defs.cwall_compLItems != nullptr) {
            Int32 count = (Int32) (BMGetHandleSize ((GSHandle) defs.cwall_compLItems) / sizeof (API_CWallLineComponent));
            count = GS::Min (count, (Int32) attr.compWall.nComps + 1);
            for (Int32 i = 0; i < count; ++i) {
                const API_CWallLineComponent& l = (*defs.cwall_compLItems)[i];
                GS::ObjectState s;
                s.Add ("index", i);
                s.Add ("linePen", (Int32) l.linePen);
                s.Add ("ltypeIndex", l.ltypeInd.ToInt32_Deprecated ());
                lines (s);
            }
        }
        ACAPI_DisposeAttrDefsHdlsExt (&defs);
    }
    return os;
}


// Feld per Zeiger maskieren. ACAPI_ELEMENT_MASK_SET braucht Typ + Feldname zur Uebersetzungszeit
// und setzt nur das ERSTE Byte des Feldes auf -1 (APIdefs_Elements.h). Die Feldtabelle arbeitet mit
// Zeigern, daher dasselbe Byte ueber den Offset im Masken-Element setzen. Nur Felder aus
// CollectPenFieldsELM kommen hier an (kein Fremdbyte ueberschreibbar, T-f9d-01).
void SetMaskForFieldELM (API_Element& mask, const API_Element& element, const void* fieldPtr)
{
    const std::ptrdiff_t offset = static_cast<const char*> (fieldPtr) - reinterpret_cast<const char*> (&element);
    reinterpret_cast<char*> (&mask)[offset] = -1;
}

// Rollentabelle (verbindlich, PLAN 261029-f9d). Leer = Rolle hat fuer den Typ kein Feld.
std::vector<const char*> RoleFieldsELM (API_ElemTypeID typeId, const std::string& role)
{
    const bool cut = role == "cutLinePen", uncut = role == "uncutLinePen", over = role == "overheadLinePen";
    switch (typeId) {
        case API_WallID:        if (cut) return { "contPen" };           if (uncut) return { "contPen3D" };       if (over) return { "aboveViewLinePen" }; break;
        case API_ColumnID:      if (cut) return { "corePen", "venLinePen" }; if (uncut) return { "belowViewLinePen" }; if (over) return { "aboveViewLinePen" }; break;
        case API_BeamID:        if (cut) return { "cutContourLinePen" }; if (uncut) return { "belowViewLinePen" }; if (over) return { "aboveViewLinePen" }; break;
        case API_SlabID:        if (cut) return { "sectContPen" };       if (uncut) return { "pen" };              break;
        case API_RoofID:
        case API_ShellID:       if (cut) return { "sectContPen" };       if (uncut) return { "pen" };              if (over) return { "aboveViewLinePen" }; break;
        case API_CurtainWallID: if (cut) return { "frameCutLinePen", "panelCutLinePen" }; if (uncut) return { "uncutLinePen" }; if (over) return { "overheadLinePen" }; break;
        case API_MorphID:       if (cut) return { "cutLinePen" };        if (uncut) return { "uncutLinePen" };     if (over) return { "overheadLinePen" }; break;
        default: break;
    }
    return {};
}

int FindFieldELM (const std::vector<PenFieldELM>& fields, const std::string& name)
{
    for (size_t i = 0; i < fields.size (); ++i)
        if (name == fields[i].name) return (int) i;
    return -1;
}

// Aenderungswunsch je Element (Top-Level-Vorgabe, Element-Wert gewinnt).
struct PenRequestELM {
    bool  hasRole[3] = { false, false, false };   // cutLinePen, uncutLinePen, overheadLinePen
    Int32 role[3]    = { 0, 0, 0 };
    bool  hasClear   = false;
    bool  clear      = false;
    std::map<std::string, Int32> fields;

    bool Any () const { return hasRole[0] || hasRole[1] || hasRole[2] || (hasClear && clear) || !fields.empty (); }
};

const char* const RoleNamesELM[3] = { "cutLinePen", "uncutLinePen", "overheadLinePen" };

void ReadRequestELM (const GS::ObjectState& os, PenRequestELM& req)
{
    for (int r = 0; r < 3; ++r) {
        Int32 v = 0;
        if (os.Get (RoleNamesELM[r], v)) { req.hasRole[r] = true; req.role[r] = v; }
    }
    bool c = false;
    if (os.Get ("clearCutFillOverride", c)) { req.hasClear = true; req.clear = c; }
    const GS::ObjectState* f = os.Get ("fields");
    if (f != nullptr) {
        const GS::HashSet<GS::String> names = f->GetFieldNames ();
        for (const GS::String& n : names) {
            Int32 v = 0;
            if (f->Get (n, v)) req.fields[std::string (n.ToCStr ())] = v;
        }
    }
}

struct PenValueELM {
    bool  overridden = true;   // fuer Pen-Felder immer true
    Int32 pen        = 0;
    bool operator== (const PenValueELM& o) const { return overridden == o.overridden && (!overridden || pen == o.pen); }
};

PenValueELM ReadFieldELM (const PenFieldELM& f)
{
    PenValueELM v;
    if (f.kind == PenFieldELM::OverriddenPen) {
        const API_OverriddenPen& op = *static_cast<const API_OverriddenPen*> (f.ptr);
        v.overridden = op.hasValue;
        v.pen = (Int32) op.value;
    } else {
        v.pen = (Int32) *static_cast<const short*> (f.ptr);
    }
    return v;
}

void WriteFieldELM (const PenFieldELM& f, const PenValueELM& v)
{
    if (f.kind == PenFieldELM::OverriddenPen) {
        API_OverriddenPen& op = *static_cast<API_OverriddenPen*> (f.ptr);
        if (v.overridden) op = (API_PenIndex) v.pen;
        else              op = APINullValue;
    } else {
        *static_cast<short*> (f.ptr) = (short) v.pen;
    }
}

GS::ObjectState PenValueOS (PenFieldELM::Kind kind, const PenValueELM& v)
{
    if (kind == PenFieldELM::OverriddenPen) {
        GS::ObjectState os;
        os.Add ("overridden", v.overridden);
        os.Add ("pen", v.pen);
        return os;
    }
    return GS::ObjectState ("pen", v.pen);
}

struct PlannedChangeELM {
    std::string       field;
    int               fieldIdx = -1;
    PenFieldELM::Kind kind     = PenFieldELM::Pen;
    PenValueELM       before;
    PenValueELM       after;
};

struct ElementPlanELM {
    API_Guid       guid = APINULLGuid;
    API_ElemTypeID typeId = API_ZombieElemID;
    API_Element    element = {};
    std::vector<PlannedChangeELM> changes;
    GSErrCode      err = NoError;
    GS::UniString  message;
    bool           written = false;
    std::vector<PenValueELM> readBack;
    bool Failed () const { return err != NoError; }
};

void PlanChangeELM (ElementPlanELM& plan, const std::vector<PenFieldELM>& fields, int idx, const PenValueELM& want)
{
    const PenValueELM before = ReadFieldELM (fields[idx]);
    for (PlannedChangeELM& c : plan.changes) {
        if (c.fieldIdx == idx) { c.after = want; return; }   // spaetere Angabe gewinnt (fields nach Rollen)
    }
    PlannedChangeELM c;
    c.field = fields[idx].name;
    c.fieldIdx = idx;
    c.kind = fields[idx].kind;
    c.before = before;
    c.after = want;
    plan.changes.push_back (c);
}

} // namespace

// ============================================================================================
// GetConstructionPensOfElements
// ============================================================================================

GS::Optional<GS::UniString> GetConstructionPensOfElementsELMCommand::GetInputParametersSchema () const
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
                        "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] }
                    },
                    "required": ["elementId"]
                }
            },
            "includeComposites": { "type": "boolean", "description": "Zusaetzlich framePen/linePen der benutzten Mehrschicht-Aufbauten (je Verbund einmal). Standard false." }
        },
        "additionalProperties": false,
        "required": ["elements"]
    })";
}

GS::Optional<GS::UniString> GetConstructionPensOfElementsELMCommand::GetResponseSchema () const
{
    return ELMResponseSchemaWithError (R"({
        "type": "object",
        "properties": {
            "pensOfElements": { "type": "array", "items": { "type": "object" } },
            "composites": { "type": "array", "items": { "type": "object" } }
        },
        "required": ["pensOfElements"]
    })");
}

GS::ObjectState GetConstructionPensOfElementsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> items;
    parameters.Get ("elements", items);
    if (items.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("elements ist leer."));
    bool includeComposites = false;
    parameters.Get ("includeComposites", includeComposites);

    GS::ObjectState response;
    const auto& list = response.AddList<GS::ObjectState> ("pensOfElements");
    std::vector<API_AttributeIndex> usedComposites;

    for (const GS::ObjectState& it : items) {
        const API_Guid guid = GuidOf (it.Get ("elementId"));
        API_Element element = {};
        element.header.guid = guid;
        if (guid == APINULLGuid || ACAPI_Element_Get (&element) != NoError) {
            GS::ObjectState f = FailOS (APIERR_BADID, GS::UniString ("[NOT_FOUND] Element ") + APIGuidToString (guid) + GS::UniString (" existiert nicht"));
            f.Add ("elementId", CreateGuidObjectStateELM (guid));
            list (f);
            continue;
        }
        const API_ElemTypeID typeId = GetElemTypeId (element.header);
        const std::vector<PenFieldELM> fields = CollectPenFieldsELM (element, typeId);
        if (fields.empty ()) {
            GS::ObjectState f = FailOS (APIERR_BADELEMENTTYPE, GS::UniString ("Elementtyp ") + GetElementTypeNonLocalizedName (typeId) + GS::UniString (UnsupportedTypesText ()));
            f.Add ("elementId", CreateGuidObjectStateELM (guid));
            list (f);
            continue;
        }

        GS::ObjectState r;
        r.Add ("success", true);
        r.Add ("elementId", CreateGuidObjectStateELM (guid));
        r.Add ("elementType", GetElementTypeNonLocalizedName (typeId));

        const ElmLayerStateELM ls = GetLayerStateELM (element.header.layer);
        GS::ObjectState layer;
        layer.Add ("index", element.header.layer.ToInt32_Deprecated ());
        layer.Add ("name", ls.name);
        layer.Add ("hidden", ls.hidden);
        layer.Add ("locked", ls.locked);
        r.Add ("layer", layer);
        r.Add ("floorIndex", (Int32) element.header.floorInd);

        bool isComposite = false;
        API_AttributeIndex compIdx;
        r.Add ("structure", StructureOS (element, typeId, guid, isComposite, compIdx));
        if (includeComposites && isComposite) {
            bool known = false;
            for (const API_AttributeIndex& c : usedComposites)
                if (c == compIdx) { known = true; break; }
            if (!known) usedComposites.push_back (compIdx);
        }

        GS::ObjectState pens;
        GS::ObjectState ltypes;
        for (const PenFieldELM& f : fields) {
            switch (f.kind) {
                case PenFieldELM::Pen:           pens.Add (f.name, (Int32) *static_cast<short*> (f.ptr)); break;
                case PenFieldELM::OverriddenPen: pens.Add (f.name, OverriddenPenOS (*static_cast<API_OverriddenPen*> (f.ptr))); break;
                case PenFieldELM::Ltype:         ltypes.Add (f.name, static_cast<API_AttributeIndex*> (f.ptr)->ToInt32_Deprecated ()); break;
            }
        }
        r.Add ("pens", pens);
        r.Add ("ltypes", ltypes);
        list (r);
    }

    if (includeComposites) {
        const auto& comps = response.AddList<GS::ObjectState> ("composites");
        for (const API_AttributeIndex& c : usedComposites)
            comps (CompositeOS (c));
    }
    return response;
}

// ============================================================================================
// SetConstructionPensOfElements
// ============================================================================================

#define ELM_PEN_PROPS \
    "\"cutLinePen\": { \"type\": \"integer\", \"minimum\": 1, \"maximum\": 255, \"description\": \"Schnitt-/Konturstift, je Typ auf das passende Feld (Rollentabelle).\" }," \
    "\"uncutLinePen\": { \"type\": \"integer\", \"minimum\": 1, \"maximum\": 255 }," \
    "\"overheadLinePen\": { \"type\": \"integer\", \"minimum\": 1, \"maximum\": 255 }," \
    "\"clearCutFillOverride\": { \"type\": \"boolean\", \"description\": \"cutFillPen/cutFillBackgroundPen auf nicht ueberschrieben (Schraffur aus Baustoff/Verbund).\" }," \
    "\"fields\": { \"type\": \"object\", \"additionalProperties\": { \"type\": \"integer\", \"minimum\": 1, \"maximum\": 255 }, \"description\": \"Rohfelder mit DevKit-Namen wie in GetConstructionPensOfElements.pens.\" }"

GS::Optional<GS::UniString> SetConstructionPensOfElementsELMCommand::GetInputParametersSchema () const
{
    return GS::UniString (
        "{ \"type\": \"object\", \"properties\": {"
        "\"elements\": { \"type\": \"array\", \"minItems\": 1, \"items\": { \"type\": \"object\", \"properties\": {"
        "\"elementId\": { \"type\": \"object\", \"properties\": { \"guid\": { \"type\": \"string\" } }, \"required\": [\"guid\"] },"
        ELM_PEN_PROPS
        "}, \"additionalProperties\": false, \"required\": [\"elementId\"] } },"
        ELM_PEN_PROPS ","
        "\"dryRun\": { \"type\": \"boolean\", \"description\": \"Nur melden, was sich aendern wuerde (kein Schreiben, keine Reservierung). Standard false.\" },"
        "\"reserve\": { \"type\": \"boolean\", \"description\": \"Teamwork: vorab reservieren und danach freigeben (Standard true).\" }"
        "}, \"additionalProperties\": false, \"required\": [\"elements\"] }");
}

#undef ELM_PEN_PROPS

GS::Optional<GS::UniString> SetConstructionPensOfElementsELMCommand::GetResponseSchema () const
{
    return ELMResponseSchemaWithError (R"({
        "type": "object",
        "properties": {
            "executionResults": { "type": "array", "items": { "type": "object" } },
            "undoScope": { "type": "object" },
            "dryRun": { "type": "boolean" },
            "summary": { "type": "object" }
        },
        "required": ["executionResults"]
    })");
}

GS::ObjectState SetConstructionPensOfElementsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> items;
    parameters.Get ("elements", items);
    if (items.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("elements ist leer."));
    bool dryRun = false;
    parameters.Get ("dryRun", dryRun);
    bool reserve = true;
    parameters.Get ("reserve", reserve);

    PenRequestELM defaults;
    ReadRequestELM (parameters, defaults);

    // Wunsch je Element = Top-Level-Vorgabe, ueberschrieben durch Element-Werte.
    std::vector<PenRequestELM> requests;
    bool anyChange = false;
    for (const GS::ObjectState& it : items) {
        PenRequestELM req = defaults;
        PenRequestELM own;
        ReadRequestELM (it, own);
        for (int r = 0; r < 3; ++r)
            if (own.hasRole[r]) { req.hasRole[r] = true; req.role[r] = own.role[r]; }
        if (own.hasClear) { req.hasClear = true; req.clear = own.clear; }
        for (const auto& kv : own.fields) req.fields[kv.first] = kv.second;
        anyChange = anyChange || req.Any ();
        requests.push_back (req);
    }
    if (!anyChange)
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("Mindestens cutLinePen, uncutLinePen, overheadLinePen, clearCutFillOverride oder fields angeben."));

    const short myUserId = MyTeamworkUserIdELM ();
    std::map<Int32, GS::UniString> layerCache;
    std::vector<ElementPlanELM> plans (items.GetSize ());

    // ---- 1. Vorpruefung + Planung (gemeinsam fuer dryRun und echten Lauf, nur lesen) ----
    for (UIndex i = 0; i < items.GetSize (); ++i) {
        ElementPlanELM& plan = plans[i];
        plan.guid = GuidOf (items[i].Get ("elementId"));
        const GS::UniString reason = PreflightElementELM (plan.guid, myUserId, layerCache);
        if (!reason.IsEmpty ()) { plan.err = ReasonToErrCodeELM (reason); plan.message = reason; continue; }

        plan.element.header.guid = plan.guid;
        if (ACAPI_Element_Get (&plan.element) != NoError) {
            plan.err = APIERR_BADID; plan.message = GS::UniString ("[NOT_FOUND] Element nicht lesbar"); continue;
        }
        plan.typeId = GetElemTypeId (plan.element.header);
        const std::vector<PenFieldELM> fields = CollectPenFieldsELM (plan.element, plan.typeId);
        if (fields.empty ()) {
            plan.err = APIERR_BADELEMENTTYPE;
            plan.message = GS::UniString ("Elementtyp ") + GetElementTypeNonLocalizedName (plan.typeId) + GS::UniString (UnsupportedTypesText ());
            continue;
        }
        const GS::UniString typeName = GetElementTypeNonLocalizedName (plan.typeId);
        const PenRequestELM& req = requests[i];

        // Rollen
        for (int r = 0; r < 3 && !plan.Failed (); ++r) {
            if (!req.hasRole[r]) continue;
            const std::vector<const char*> targets = RoleFieldsELM (plan.typeId, RoleNamesELM[r]);
            if (targets.empty ()) {
                plan.err = APIERR_BADPARS;
                plan.message = GS::UniString ("[FIELD_NOT_AVAILABLE] ") + GS::UniString (RoleNamesELM[r]) + GS::UniString (" hat fuer ") + typeName + GS::UniString (" kein Feld - ueber fields setzen");
                break;
            }
            for (const char* t : targets) {
                const int idx = FindFieldELM (fields, t);
                if (idx < 0) continue;   // Rollentabelle und Feldtabelle stimmen ueberein; Schutz nur fuer Konsistenz
                PenValueELM want; want.pen = req.role[r];
                PlanChangeELM (plan, fields, idx, want);
            }
        }
        // Schnittflaechen-Ueberschreibung loeschen
        if (!plan.Failed () && req.hasClear && req.clear) {
            for (const char* t : { "cutFillPen", "cutFillBackgroundPen" }) {
                const int idx = FindFieldELM (fields, t);
                if (idx < 0) continue;
                PenValueELM want = ReadFieldELM (fields[idx]);
                want.overridden = false;
                PlanChangeELM (plan, fields, idx, want);
            }
        }
        // Rohfelder (zuletzt: gewinnen gegen Rolle und clearCutFillOverride)
        for (const auto& kv : req.fields) {
            if (plan.Failed ()) break;
            const int idx = FindFieldELM (fields, kv.first);
            if (idx < 0 || fields[idx].kind == PenFieldELM::Ltype) {
                plan.err = APIERR_BADPARS;
                plan.message = GS::UniString ("[FIELD_NOT_AVAILABLE] Feld '") + GS::UniString (kv.first.c_str ()) + GS::UniString ("' gibt es fuer ") + typeName + GS::UniString (" nicht (oder ist kein Stift)");
                break;
            }
            PenValueELM want; want.pen = kv.second; want.overridden = true;
            PlanChangeELM (plan, fields, idx, want);
        }
        if (plan.Failed ()) { plan.changes.clear (); continue; }

        // Werte, die schon stimmen, sind keine Aenderung.
        std::vector<PlannedChangeELM> real;
        for (const PlannedChangeELM& c : plan.changes)
            if (!(c.before == c.after)) real.push_back (c);
        plan.changes.swap (real);
    }

    GS::ObjectState response;
    const bool teamwork = reserve && ACAPI_Teamwork_HasConnection ();
    GS::Array<API_Guid> reservedByMe;
    bool lambdaRan = false;
    GSErrCode undoErr = NoError;
    bool undoCalled = false;

    if (!dryRun) {
        // ---- 2. Teamwork: nur Elemente mit Aenderung, die mir noch nicht gehoeren ----
        if (teamwork) {
            GS::Array<API_Guid> toReserve;
            for (const ElementPlanELM& p : plans)
                if (!p.Failed () && !p.changes.empty () && p.element.header.userId != myUserId)   // Teamwork-Besitzer = userId, nicht lockId
                    toReserve.Push (p.guid);
            if (!toReserve.IsEmpty ()) {
                GS::HashTable<API_Guid, short> conflicts;
                ACAPI_Teamwork_ReserveElements (toReserve, &conflicts, false);
                for (ElementPlanELM& p : plans) {
                    if (p.Failed () || p.changes.empty () || !toReserve.Contains (p.guid)) continue;
                    API_Elem_Head head = {};
                    head.guid = p.guid;
                    if (ACAPI_Element_GetHeader (&head) == NoError && head.userId == myUserId) {
                        reservedByMe.Push (p.guid);   // nur diese werden am Ende freigegeben
                    } else {
                        p.err = APIERR_NOACCESSRIGHT;
                        p.message = GS::UniString ("[RESERVED_BY_OTHER] Reservierung abgelehnt (anderer Nutzer)");
                    }
                }
            }
        }

        // ---- 3. EIN Undo-Schritt fuer alle Elemente ----
        bool anyToWrite = false;
        for (const ElementPlanELM& p : plans)
            if (!p.Failed () && !p.changes.empty ()) anyToWrite = true;
        if (anyToWrite) {
            undoCalled = true;
            undoErr = ACAPI_CallUndoableCommand ("ELM_SAB SetConstructionPensOfElements", [&] () -> GSErrCode {
                lambdaRan = true;
                for (ElementPlanELM& p : plans) {
                    if (p.Failed () || p.changes.empty ()) continue;
                    const std::vector<PenFieldELM> fields = CollectPenFieldsELM (p.element, p.typeId);
                    API_Element mask = {};
                    ACAPI_ELEMENT_MASK_CLEAR (mask);
                    for (const PlannedChangeELM& c : p.changes) {
                        WriteFieldELM (fields[c.fieldIdx], c.after);
                        SetMaskForFieldELM (mask, p.element, fields[c.fieldIdx].ptr);
                    }
                    const GSErrCode err = ACAPI_Element_Change (&p.element, &mask, nullptr, 0, true);
                    if (err == NoError) {
                        p.written = true;
                    } else {
                        p.err = err;
                        p.message = (err == APIERR_NOACCESSRIGHT)
                            ? GS::UniString ("[LAYER_HIDDEN?] ACAPI_Element_Change abgelehnt (APIERR_NOACCESSRIGHT, 6001 = meist ausgeblendete Ebene oder Hotlink)")
                            : GS::UniString ("ACAPI_Element_Change abgelehnt (Fehler ") + GS::ValueToUniString ((Int32) err) + GS::UniString (")");
                    }
                }
                return NoError;
            });
            if (!lambdaRan) {
                // Kein Rueckfall ohne Undo-Klammer: ein Undo-Schritt ist Vorgabe.
                for (ElementPlanELM& p : plans) {
                    if (p.Failed () || p.changes.empty ()) continue;
                    p.err = undoErr != NoError ? undoErr : APIERR_GENERAL;
                    p.message = GS::UniString ("[UNDO_SCOPE_FAILED] Undo-Klammer hat das Lambda nicht gestartet - nichts geschrieben (offener Dialog, aktives Werkzeug, Teamwork-Schreibrecht pruefen)");
                }
            }
        }

        // ---- 4. Ruecklese ----
        for (ElementPlanELM& p : plans) {
            if (!p.written) continue;
            API_Element check = {};
            check.header.guid = p.guid;
            if (ACAPI_Element_Get (&check) != NoError) {
                p.err = APIERR_GENERAL; p.message = GS::UniString ("[NO_EFFECT] Ruecklese nicht moeglich"); continue;
            }
            const std::vector<PenFieldELM> fields = CollectPenFieldsELM (check, p.typeId);
            for (const PlannedChangeELM& c : p.changes) {
                const PenValueELM got = ReadFieldELM (fields[c.fieldIdx]);
                p.readBack.push_back (got);
                if (!(got == c.after) && !p.Failed ()) {
                    p.err = APIERR_GENERAL;
                    p.message = GS::UniString ("[NO_EFFECT] Ruecklese weicht ab: ") + GS::UniString (c.field.c_str ()) +
                        GS::UniString (" soll ") + (c.after.overridden ? GS::ValueToUniString (c.after.pen) : GS::UniString ("nicht ueberschrieben")) +
                        GS::UniString (" ist ") + (got.overridden ? GS::ValueToUniString (got.pen) : GS::UniString ("nicht ueberschrieben"));
                }
            }
        }

        // ---- 5. Nur selbst gesetzte Reservierungen freigeben (Teamwork-Etikette) ----
        if (!reservedByMe.IsEmpty ())
            ACAPI_Teamwork_ReleaseElements (reservedByMe, false);
    }

    // ---- 6. Antwort in Eingabereihenfolge ----
    Int32 changed = 0, unchanged = 0, failed = 0;
    const auto& results = response.AddList<GS::ObjectState> ("executionResults");
    for (const ElementPlanELM& p : plans) {
        GS::ObjectState r;
        r.Add ("elementId", CreateGuidObjectStateELM (p.guid));
        r.Add ("success", !p.Failed ());
        if (p.typeId != API_ZombieElemID)
            r.Add ("elementType", GetElementTypeNonLocalizedName (p.typeId));
        const auto& ch = r.AddList<GS::ObjectState> ("changes");
        for (size_t k = 0; k < p.changes.size (); ++k) {
            const PlannedChangeELM& c = p.changes[k];
            GS::ObjectState co;
            co.Add ("field", GS::UniString (c.field.c_str ()));
            co.Add ("before", PenValueOS (c.kind, c.before));
            co.Add ("after", PenValueOS (c.kind, c.after));
            if (k < p.readBack.size ())
                co.Add ("readBack", PenValueOS (c.kind, p.readBack[k]));
            ch (co);
        }
        if (p.Failed ()) {
            r.Add ("error", ErrOS (p.err, p.message));
            ++failed;
        } else if (p.changes.empty ()) {
            r.Add ("unchanged", true);
            ++unchanged;
        } else {
            ++changed;
        }
        results (r);
    }

    response.Add ("dryRun", dryRun);
    if (!dryRun) {
        GS::ObjectState undoScope;
        undoScope.Add ("executed", lambdaRan);
        undoScope.Add ("errorCode", (Int32) undoErr);
        if (!undoCalled) undoScope.Add ("nothingToWrite", true);
        response.Add ("undoScope", undoScope);
    }
    GS::ObjectState summary;
    summary.Add ("requested", (Int32) plans.size ());
    summary.Add ("changed", changed);
    summary.Add ("unchanged", unchanged);
    summary.Add ("failed", failed);
    response.Add ("summary", summary);
    return response;
}
