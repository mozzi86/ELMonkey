#include "LayerLevelCommandsELM.hpp"
#include "ElmPreflight.hpp"
#include "MigrationHelper.hpp"
#include "CommandBase.hpp"

static GS::ObjectState ErrOSELM (GSErrCode code, const GS::UniString& message)
{
    GS::ObjectState e;
    e.Add ("code", (Int32) code);
    e.Add ("message", message);
    return e;
}

// ---- GetLayers -------------------------------------------------------------------------------

GS::Optional<GS::UniString> GetLayersELMCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "nameContains": { "type": "string", "description": "Optional: nur Ebenen, deren Name diesen Text enthaelt (Gross-/Kleinschreibung egal)." }
        },
        "additionalProperties": false
    })";
}

GS::ObjectState GetLayersELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::UniString filter;
    parameters.Get ("nameContains", filter);
    filter.SetToLowerCase ();

    GS::Array<API_Attribute> attrs;
    ACAPI_Attribute_GetAttributesByType (API_LayerID, attrs);

    GS::ObjectState response;
    const auto& list = response.AddList<GS::ObjectState> ("layers");
    Int32 count = 0;
    for (const API_Attribute& a : attrs) {
        const GS::UniString name (a.header.name);
        if (!filter.IsEmpty ()) {
            GS::UniString lower = name;
            lower.SetToLowerCase ();
            if (!lower.Contains (filter)) continue;
        }
        GS::ObjectState l;
        l.Add ("index", (Int32) GetAttributeIndex (a.header.index));
        l.Add ("name", name);
        l.Add ("hidden", (a.header.flags & APILay_Hidden) != 0);
        l.Add ("locked", (a.header.flags & APILay_Locked) != 0);
        l.Add ("wireframe", (a.header.flags & APILay_ForceToWire) != 0);
        l.Add ("xref", (a.header.flags & APILay_Xref) != 0);
        l.Add ("guid", APIGuidToString (a.header.guid));
        list (l);
        ++count;
    }
    response.Add ("count", count);
    return response;
}

// ---- SetLayerAttributes ----------------------------------------------------------------------

GS::Optional<GS::UniString> SetLayerAttributesELMCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "layers": {
                "type": "array",
                "minItems": 1,
                "items": {
                    "type": "object",
                    "properties": {
                        "layerName":  { "type": "string",  "description": "Ebene per Name (Vorrang vor layerIndex)." },
                        "layerIndex": { "type": "integer", "description": "Ebene per echtem Attribut-Index (siehe GetLayers)." },
                        "hidden":     { "type": "boolean" },
                        "locked":     { "type": "boolean" },
                        "wireframe":  { "type": "boolean" }
                    },
                    "additionalProperties": false
                }
            }
        },
        "additionalProperties": false,
        "required": ["layers"]
    })";
}

GS::ObjectState SetLayerAttributesELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> layers;
    parameters.Get ("layers", layers);
    if (layers.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("layers ist leer."));

    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("results");

    for (const GS::ObjectState& item : layers) {
        GS::ObjectState r;
        API_AttributeIndex idx;
        GS::UniString layerName;
        Int32 layerIndex = 0;
        if (item.Get ("layerName", layerName) && !layerName.IsEmpty ()) {
            if (!ResolveLayerByNameELM (layerName, idx)) {
                r.Add ("success", false);
                r.Add ("layerName", layerName);
                r.Add ("error", ErrOSELM (APIERR_BADNAME, GS::UniString ("[LAYER_NOT_FOUND] Ebene '") + layerName + GS::UniString ("' gibt es nicht")));
                results (r);
                continue;
            }
        } else if (item.Get ("layerIndex", layerIndex) && layerIndex > 0) {
            idx = ACAPI_CreateAttributeIndex (layerIndex);
        } else {
            r.Add ("success", false);
            r.Add ("error", ErrOSELM (APIERR_BADPARS, GS::UniString ("layerName oder layerIndex angeben")));
            results (r);
            continue;
        }

        API_Attribute attr = {};
        attr.header.typeID = API_LayerID;
        attr.header.index  = idx;
        GSErrCode err = ACAPI_Attribute_Get (&attr);
        if (err != NoError) {
            r.Add ("success", false);
            r.Add ("error", ErrOSELM (err, GS::UniString ("[LAYER_NOT_FOUND] Ebene nicht lesbar")));
            results (r);
            continue;
        }
        r.Add ("layerIndex", (Int32) GetAttributeIndex (attr.header.index));
        r.Add ("layerName", GS::UniString (attr.header.name));

        bool wantHidden = false, wantLocked = false, wantWire = false;
        const bool hasHidden = item.Get ("hidden", wantHidden);
        const bool hasLocked = item.Get ("locked", wantLocked);
        const bool hasWire   = item.Get ("wireframe", wantWire);
        GSFlags flags = attr.header.flags;
        if (hasHidden) flags = wantHidden ? (flags | APILay_Hidden) : (flags & ~APILay_Hidden);
        if (hasLocked) flags = wantLocked ? (flags | APILay_Locked) : (flags & ~APILay_Locked);
        if (hasWire)   flags = wantWire   ? (flags | APILay_ForceToWire) : (flags & ~APILay_ForceToWire);

        if (flags != attr.header.flags) {
            attr.header.flags = flags;
            API_AttributeDef def = {};
            err = ACAPI_CallUndoableCommand ("ELM_SAB SetLayerAttributes", [&] () -> GSErrCode {
                return ACAPI_Attribute_Modify (&attr, &def);
            });
            ACAPI_DisposeAttrDefsHdls (&def);
            if (err != NoError) {
                r.Add ("success", false);
                r.Add ("error", ErrOSELM (err, GS::UniString ("ACAPI_Attribute_Modify abgelehnt (Teamwork: Ebenen-Recht/Reservierung?)")));
                results (r);
                continue;
            }
        }

        // Ruecklese
        API_Attribute check = {};
        check.header.typeID = API_LayerID;
        check.header.index  = idx;
        const bool readOk = ACAPI_Attribute_Get (&check) == NoError;
        const bool isHidden = readOk && (check.header.flags & APILay_Hidden) != 0;
        const bool isLocked = readOk && (check.header.flags & APILay_Locked) != 0;
        const bool isWire   = readOk && (check.header.flags & APILay_ForceToWire) != 0;
        r.Add ("hidden", isHidden);
        r.Add ("locked", isLocked);
        r.Add ("wireframe", isWire);
        const bool ok = readOk && (!hasHidden || isHidden == wantHidden) && (!hasLocked || isLocked == wantLocked) &&
                        (!hasWire || isWire == wantWire);
        r.Add ("success", ok);
        if (!ok)
            r.Add ("error", ErrOSELM (APIERR_GENERAL, GS::UniString ("[NO_EFFECT] Ruecklese weicht vom Sollzustand ab")));
        results (r);
    }
    return response;
}

// ---- GetSlabLevels ---------------------------------------------------------------------------

GS::Optional<GS::UniString> GetSlabLevelsELMCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "minItems": 1,
                "items": { "type": "object", "properties": { "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] } }, "required": ["elementId"] }
            }
        },
        "additionalProperties": false,
        "required": ["elements"]
    })";
}

static GS::UniString SlabRefPlaneName (API_SlabReferencePlaneLocationID r)
{
    switch (r) {
        case APISlabRefPlane_Top:        return "Top";
        case APISlabRefPlane_CoreTop:    return "CoreTop";
        case APISlabRefPlane_CoreBottom: return "CoreBottom";
        case APISlabRefPlane_Bottom:     return "Bottom";
        default:                         return "Unknown";
    }
}

GS::ObjectState GetSlabLevelsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);
    if (elements.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("elements ist leer."));

    const Stories stories = GetStories ();
    GS::ObjectState response;
    const auto& list = response.AddList<GS::ObjectState> ("slabLevels");

    for (const GS::ObjectState& item : elements) {
        GS::ObjectState r;
        const GS::ObjectState* eid = item.Get ("elementId");
        GS::UniString guidStr;
        if (eid == nullptr || !eid->Get ("guid", guidStr)) {
            r.Add ("success", false);
            r.Add ("error", ErrOSELM (APIERR_BADPARS, GS::UniString ("elementId.guid fehlt")));
            list (r);
            continue;
        }
        const API_Guid guid = APIGuidFromString (guidStr.ToCStr ().Get ());
        r.Add ("elementId", CreateGuidObjectStateELM (guid));

        API_Element element = {};
        element.header.guid = guid;
        if (ACAPI_Element_Get (&element) != NoError || GetElemTypeId (element.header) != API_SlabID) {
            r.Add ("success", false);
            r.Add ("error", ErrOSELM (APIERR_BADID, GS::UniString ("[NOT_FOUND] keine Decke mit dieser GUID")));
            list (r);
            continue;
        }

        const short floorInd = element.header.floorInd;
        const auto st = stories.find (floorInd);
        const double storyLevel = (st != stories.end ()) ? st->second.level : 0.0;
        r.Add ("floorIndex", (Int32) floorInd);
        r.Add ("storyLevel", storyLevel);
        r.Add ("thickness", element.slab.thickness);
        r.Add ("referencePlane", SlabRefPlaneName (element.slab.referencePlaneLocation));
        r.Add ("referenceOffsetFromStory", element.slab.level);
        r.Add ("referenceZ", storyLevel + element.slab.level);

        // Massgeblich: 3D-Begrenzungsquader (absolut) - so haben Kollegen die UK zuverlaessig ermittelt.
        API_Box3D box = {};
        if (ACAPI_Element_CalcBounds (&element.header, &box) == NoError) {
            r.Add ("topZ", box.zMax);
            r.Add ("bottomZ", box.zMin);
            r.Add ("topZStoryRelative", box.zMax - storyLevel);
            r.Add ("bottomZStoryRelative", box.zMin - storyLevel);
            r.Add ("success", true);
        } else {
            r.Add ("success", false);
            r.Add ("error", ErrOSELM (APIERR_GENERAL, GS::UniString ("3D-Begrenzung nicht berechenbar")));
        }
        list (r);
    }
    return response;
}
