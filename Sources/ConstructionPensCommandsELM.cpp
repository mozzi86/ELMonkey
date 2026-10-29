#include "ConstructionPensCommandsELM.hpp"
#include "ElmPreflight.hpp"
#include "MigrationHelper.hpp"
#include "CommandBase.hpp"

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
