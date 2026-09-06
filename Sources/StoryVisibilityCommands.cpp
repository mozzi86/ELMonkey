#include "StoryVisibilityCommands.hpp"
#include "MigrationHelper.hpp"
#include <vector>

namespace {

struct VisSpec {
    bool  isAuto = false;
    bool  showOnHome = true, showAllAbove = false, showAllBelow = false;
    short relAbove = 0, relBelow = 0;
};

// Preset-String -> Feldbelegung. Unbekannter String => false (Aufrufer bekommt BADPARS).
bool PresetToSpec (const GS::UniString& p, VisSpec& s)
{
    s = VisSpec ();
    if (p == "HomeOnly")             { return true; }
    if (p == "HomeAndOneUp")         { s.relAbove = 1; return true; }
    if (p == "HomeAndOneDown")       { s.relBelow = 1; return true; }
    if (p == "HomeAndOneUpAndDown")  { s.relAbove = 1; s.relBelow = 1; return true; }
    if (p == "AllStories")           { s.showAllAbove = true; s.showAllBelow = true; return true; }
    if (p == "AllRelevant")          { s.isAuto = true; return true; }
    return false;
}

GS::UniString SpecToPreset (const API_ObjectType& o)
{
    if (o.isAutoOnStoryVisibility) return "AllRelevant";
    const auto& v = o.visibility;
    if (v.showAllAbove && v.showAllBelow)                 return "AllStories";
    if (v.showRelAbove == 1 && v.showRelBelow == 1)       return "HomeAndOneUpAndDown";
    if (v.showRelAbove == 1 && v.showRelBelow == 0)       return "HomeAndOneUp";
    if (v.showRelAbove == 0 && v.showRelBelow == 1)       return "HomeAndOneDown";
    if (!v.showAllAbove && !v.showAllBelow &&
        v.showRelAbove == 0 && v.showRelBelow == 0)       return "HomeOnly";
    return "Custom";
}

bool IsObjectOrLamp (API_ElemTypeID t) { return t == API_ObjectID || t == API_LampID; }

// Setzt an EINEM Element und liest zurueck. err/ist werden gefuellt.
GSErrCode ApplyToOne (const API_Guid& guid, const VisSpec& s, GS::UniString& istPreset)
{
    API_Element element = {};
    element.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get (&element);
    if (err != NoError) return err;
    if (!IsObjectOrLamp (element.header.type.typeID)) return APIERR_BADELEMENTTYPE;

    element.object.isAutoOnStoryVisibility = s.isAuto;
    element.object.visibility.showOnHome   = s.showOnHome;
    element.object.visibility.showAllAbove = s.showAllAbove;
    element.object.visibility.showAllBelow = s.showAllBelow;
    element.object.visibility.showRelAbove = s.relAbove;
    element.object.visibility.showRelBelow = s.relBelow;

    API_Element mask = {};
    ACAPI_ELEMENT_MASK_CLEAR (mask);
    ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, isAutoOnStoryVisibility);
    ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, visibility.showOnHome);
    ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, visibility.showAllAbove);
    ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, visibility.showAllBelow);
    ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, visibility.showRelAbove);
    ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, visibility.showRelBelow);

    err = ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
    if (err != NoError) return err;

    // Ruecklese — NoError beweist nichts (Lektion aus SetAddPars/SetColumnRotation).
    API_Element check = {};
    check.header.guid = guid;
    if (ACAPI_Element_Get (&check) != NoError) return APIERR_GENERAL;
    istPreset = SpecToPreset (check.object);

    if (s.isAuto)
        return check.object.isAutoOnStoryVisibility ? NoError : APIERR_GENERAL;
    const auto& v = check.object.visibility;
    const bool ok = !check.object.isAutoOnStoryVisibility &&
                    v.showOnHome   == s.showOnHome   && v.showAllAbove == s.showAllAbove &&
                    v.showAllBelow == s.showAllBelow && v.showRelAbove == s.relAbove &&
                    v.showRelBelow == s.relBelow;
    return ok ? NoError : APIERR_GENERAL;   // Feld nicht uebernommen -> ehrlich melden
}

} // namespace

GS::Optional<GS::UniString> SetStoryVisibilityOfElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "items": { "type": "object",
                "properties": { "elementId": { "type": "object",
                    "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                "required": ["elementId"] } },
            "visibility": { "type": "string",
                "enum": ["HomeOnly","HomeAndOneUp","HomeAndOneDown",
                         "HomeAndOneUpAndDown","AllStories","AllRelevant"] },
            "custom": { "type": "object", "properties": {
                "showOnHome":   { "type": "boolean" },
                "showAllAbove": { "type": "boolean" },
                "showAllBelow": { "type": "boolean" },
                "showRelAbove": { "type": "integer", "minimum": 0, "maximum": 1 },
                "showRelBelow": { "type": "integer", "minimum": 0, "maximum": 1 } } },
            "reserve": { "type": "boolean",
                "description": "Teamwork: Elemente vorab reservieren (Standard true) und am Ende wieder freigeben." }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetStoryVisibilityOfElementsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": { "executionResults": { "type": "array", "items": { "type": "object" } } },
        "required": ["executionResults"]
    })");
}

GS::ObjectState SetStoryVisibilityOfElementsCommand::Execute (
    const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    VisSpec spec;
    GS::UniString preset;
    const GS::ObjectState* custom = parameters.Get ("custom");
    if (parameters.Get ("visibility", preset)) {
        if (!PresetToSpec (preset, spec))
            return CreateErrorResponse (APIERR_BADPARS, "Unbekannter visibility-Wert: " + preset);
    } else if (custom != nullptr) {
        Int32 ra = 0, rb = 0;
        custom->Get ("showOnHome", spec.showOnHome);
        custom->Get ("showAllAbove", spec.showAllAbove);
        custom->Get ("showAllBelow", spec.showAllBelow);
        if (custom->Get ("showRelAbove", ra)) spec.relAbove = (short) ra;
        if (custom->Get ("showRelBelow", rb)) spec.relBelow = (short) rb;
    } else {
        return CreateErrorResponse (APIERR_BADPARS, "visibility oder custom erforderlich.");
    }

    bool reserve = true;
    parameters.Get ("reserve", reserve);

    std::vector<API_Guid> guids;
    for (const GS::ObjectState& item : elements) {
        const GS::ObjectState* id = item.Get ("elementId");
        guids.push_back (id != nullptr ? GetGuidFromObjectState (*id) : APINULLGuid);
    }

    // Teamwork: einmal fuer alle reservieren. Konflikte landen als Fehler beim Element.
    GS::HashTable<API_Guid, short> conflicts;
    if (reserve && ACAPI_Teamwork_HasConnection ()) {
        GS::Array<API_Guid> toReserve;
        for (const API_Guid& g : guids) if (g != APINULLGuid) toReserve.Push (g);
        ACAPI_Teamwork_ReserveElements (toReserve, &conflicts, false);
    }

    std::vector<GSErrCode>     status (guids.size (), APIERR_GENERAL);
    std::vector<GS::UniString> ist (guids.size ());

    ACAPI_CallUndoableCommand ("ELM_SAB SetStoryVisibility", [&] () -> GSErrCode {
        for (size_t i = 0; i < guids.size (); ++i) {
            if (guids[i] == APINULLGuid)      { status[i] = APIERR_BADPARS;  continue; }
            if (conflicts.ContainsKey (guids[i])) { status[i] = APIERR_NOACCESSRIGHT; continue; }
            status[i] = ApplyToOne (guids[i], spec, ist[i]);
        }
        return NoError;
    });

    if (reserve && ACAPI_Teamwork_HasConnection ()) {
        GS::Array<API_Guid> toRelease;
        for (size_t i = 0; i < guids.size (); ++i)
            if (status[i] == NoError) toRelease.Push (guids[i]);
        ACAPI_Teamwork_ReleaseElements (toRelease, false);
    }

    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("executionResults");
    for (size_t i = 0; i < guids.size (); ++i) {
        if (status[i] == NoError) {
            GS::ObjectState ok = CreateSuccessfulExecutionResult ();
            ok.Add ("visibility", ist[i]);            // Ruecklese-Wert mitgeben
            results (ok);
        } else if (status[i] == APIERR_NOACCESSRIGHT) {
            results (CreateFailedExecutionResult (status[i], "Reservierung fehlgeschlagen (anderer Nutzer, ausgeblendete Ebene oder Hotlink)"));
        } else if (status[i] == APIERR_BADELEMENTTYPE) {
            results (CreateFailedExecutionResult (status[i], "Nur Object und Lamp"));
        } else {
            results (CreateFailedExecutionResult (status[i], "Aenderung nicht wirksam (Ruecklese fehlgeschlagen)"));
        }
    }
    return response;
}

// --- Get: gleiche Schleife, nur lesend ---------------------------------------
GS::Optional<GS::UniString> GetStoryVisibilityOfElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({ "type":"object","properties":{ "elements":{"type":"array",
        "items":{"type":"object","properties":{"elementId":{"type":"object",
        "properties":{"guid":{"type":"string"}},"required":["guid"]}},"required":["elementId"]}}},
        "required":["elements"] })");
}

GS::Optional<GS::UniString> GetStoryVisibilityOfElementsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({ "type":"object",
        "properties":{"storyVisibilities":{"type":"array","items":{"type":"object"}}},
        "required":["storyVisibilities"] })");
}

GS::ObjectState GetStoryVisibilityOfElementsCommand::Execute (
    const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& out = response.AddList<GS::ObjectState> ("storyVisibilities");
    for (const GS::ObjectState& item : elements) {
        const GS::ObjectState* id = item.Get ("elementId");
        API_Element e = {};
        e.header.guid = (id != nullptr) ? GetGuidFromObjectState (*id) : APINULLGuid;
        if (e.header.guid == APINULLGuid || ACAPI_Element_Get (&e) != NoError ||
            !IsObjectOrLamp (e.header.type.typeID)) {
            out (GS::ObjectState ("error", GS::UniString ("Element nicht lesbar oder kein Object/Lamp")));
            continue;
        }
        GS::ObjectState o;
        o.Add ("elementId",    CreateGuidObjectState (e.header.guid));
        o.Add ("visibility",   SpecToPreset (e.object));
        o.Add ("isAutoOnStoryVisibility", e.object.isAutoOnStoryVisibility);
        o.Add ("showOnHome",   e.object.visibility.showOnHome);
        o.Add ("showAllAbove", e.object.visibility.showAllAbove);
        o.Add ("showAllBelow", e.object.visibility.showAllBelow);
        o.Add ("showRelAbove", (Int32) e.object.visibility.showRelAbove);
        o.Add ("showRelBelow", (Int32) e.object.visibility.showRelBelow);
        o.Add ("homeStory",    (Int32) e.header.floorInd);
        out (o);
    }
    return response;
}
