#include "SetLayerCommand.hpp"
#include "MigrationHelper.hpp"
#include <vector>

namespace {

// Setzt die Ebene EINES Elements und liest zurueck.
GSErrCode ApplyLayerToOne (const API_Guid& guid, const API_AttributeIndex& target, API_AttributeIndex& istLayer)
{
    API_Element element = {};
    element.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get (&element);
    if (err != NoError) return err;

    element.header.layer = target;

    API_Element mask = {};
    ACAPI_ELEMENT_MASK_CLEAR (mask);
    ACAPI_ELEMENT_MASK_SET (mask, API_Elem_Head, layer);

    err = ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
    if (err != NoError) return err;

    // Ruecklese - NoError beweist nichts.
    API_Element check = {};
    check.header.guid = guid;
    if (ACAPI_Element_Get (&check) != NoError) return APIERR_GENERAL;
    istLayer = check.header.layer;
    return (check.header.layer.ToInt32_Deprecated () == target.ToInt32_Deprecated ()) ? NoError : APIERR_GENERAL;
}

GS::UniString LayerNameOfIndex (const API_AttributeIndex& idx)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = API_LayerID;
    attr.header.index = idx;
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError)
        return GS::UniString ();
    return name;
}

} // namespace

GS::Optional<GS::UniString> SetLayerOfElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "items": { "type": "object",
                "properties": { "elementId": { "type": "object",
                    "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                "required": ["elementId"] } },
            "layerId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"],
                "description": "Ebene per Attribut-GUID (Vorrang vor layerName)." },
            "layerName": { "type": "string", "description": "Ebene per Name." },
            "reserve": { "type": "boolean",
                "description": "Teamwork: Elemente vorab reservieren (Standard true) und am Ende wieder freigeben." }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetLayerOfElementsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": { "type": "array", "items": { "type": "object" } },
            "undoScope": { "type": "object" }
        },
        "required": ["executionResults"]
    })");
}

GS::ObjectState SetLayerOfElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    // Ebene aufloesen.
    API_AttributeIndex target = ACAPI_CreateAttributeIndex (-1);
    bool resolved = false;
    GS::UniString what;
    const GS::ObjectState* layerIdOS = parameters.Get ("layerId");
    GS::UniString layerName;
    if (layerIdOS != nullptr) {
        const API_Guid lg = GetGuidFromObjectState (*layerIdOS);
        what = APIGuidToString (lg);
        if (lg != APINULLGuid) {
            const API_AttributeIndex idx = GetAttributeIndexFromGuid (API_LayerID, lg);
            if (idx.ToInt32_Deprecated () > 0) { target = idx; resolved = true; }
        }
    } else if (parameters.Get ("layerName", layerName)) {
        what = layerName;
        API_Attr_Head head = {};
        head.typeID = API_LayerID;
        head.uniStringNamePtr = &layerName;
        if (ACAPI_Attribute_Search (&head) == NoError) { target = head.index; resolved = true; }
    } else {
        return CreateErrorResponse (APIERR_BADPARS, "layerId oder layerName erforderlich.");
    }
    if (!resolved)
        return CreateErrorResponse (APIERR_BADPARS, "Ebene nicht gefunden: " + what);

    bool reserve = true;
    parameters.Get ("reserve", reserve);

    std::vector<API_Guid> guids;
    for (const GS::ObjectState& item : elements) {
        const GS::ObjectState* id = item.Get ("elementId");
        guids.push_back (id != nullptr ? GetGuidFromObjectState (*id) : APINULLGuid);
    }

    // Teamwork: einmal fuer alle reservieren; Konflikte landen als Fehler beim Element.
    GS::HashTable<API_Guid, short> conflicts;
    const bool teamwork = reserve && ACAPI_Teamwork_HasConnection ();
    if (teamwork) {
        GS::Array<API_Guid> toReserve;
        for (const API_Guid& g : guids) if (g != APINULLGuid) toReserve.Push (g);
        ACAPI_Teamwork_ReserveElements (toReserve, &conflicts, false);
    }

    std::vector<GSErrCode>         status (guids.size (), APIERR_GENERAL);
    std::vector<API_AttributeIndex> ist (guids.size (), ACAPI_CreateAttributeIndex (-1));

    bool lambdaStarted = false;
    ACAPI_CallUndoableCommand ("ELM_SAB SetLayerOfElements", [&] () -> GSErrCode {
        lambdaStarted = true;
        for (size_t i = 0; i < guids.size (); ++i) {
            if (guids[i] == APINULLGuid)          { status[i] = APIERR_BADPARS;       continue; }
            if (conflicts.ContainsKey (guids[i])) { status[i] = APIERR_NOACCESSRIGHT; continue; }
            status[i] = ApplyLayerToOne (guids[i], target, ist[i]);
        }
        return NoError;
    });

    if (teamwork) {
        GS::Array<API_Guid> toRelease;
        for (size_t i = 0; i < guids.size (); ++i)
            if (status[i] == NoError) toRelease.Push (guids[i]);
        ACAPI_Teamwork_ReleaseElements (toRelease, false);
    }

    // Antwort NACH dem Lambda.
    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("executionResults");
    for (size_t i = 0; i < guids.size (); ++i) {
        if (!lambdaStarted) {
            results (CreateFailedExecutionResult (APIERR_GENERAL, "Undo-Lambda wurde nicht gestartet - nichts geaendert"));
        } else if (status[i] == NoError) {
            GS::ObjectState ok = CreateSuccessfulExecutionResult ();
            ok.Add ("elementId", CreateGuidObjectState (guids[i]));
            ok.Add ("layerIndex", (Int32) ist[i].ToInt32_Deprecated ());
            ok.Add ("layerName", LayerNameOfIndex (ist[i]));
            results (ok);
        } else if (status[i] == APIERR_NOACCESSRIGHT) {
            results (CreateFailedExecutionResult (status[i], "Reservierung fehlgeschlagen (anderer Nutzer, ausgeblendete Ebene oder Hotlink)"));
        } else if (status[i] == APIERR_BADPARS) {
            results (CreateFailedExecutionResult (status[i], "elementId fehlt"));
        } else {
            results (CreateFailedExecutionResult (status[i], "Aenderung nicht wirksam (Element nicht lesbar oder Ruecklese weicht ab)"));
        }
    }
    GS::ObjectState undoScope;
    undoScope.Add ("lambdaStarted", lambdaStarted);
    response.Add ("undoScope", undoScope);
    return response;
}
