#include "SetLayerCommand.hpp"
#include "MigrationHelper.hpp"
#include <string>
#include <vector>

namespace {

// 0.10.4: Stufen, an denen ein Element scheitern kann (fuer die Diagnose je Element).
enum LayerApplyStageELM {
    LayerStage_Ok = 0,
    LayerStage_NotReadable,
    LayerStage_ChangeRejected,
    LayerStage_ReadbackUnreadable,
    LayerStage_ReadbackMismatch
};

struct LayerApplyOutcomeELM {
    GSErrCode           code;
    int                 stage;
    bool                haveActual;
    API_AttributeIndex  actual;
    GS::UniString       elementType;

    LayerApplyOutcomeELM () :
        code (APIERR_GENERAL), stage (LayerStage_NotReadable), haveActual (false),
        actual (ACAPI_CreateAttributeIndex (-1)), elementType ()
    {}
};

// Setzt die Ebene EINES Elements und liest zurueck; protokolliert Stufe, Code und Ist-Ebene.
void ApplyLayerToOne (const API_Guid& guid, const API_AttributeIndex& target, LayerApplyOutcomeELM& out)
{
    API_Element element = {};
    element.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get (&element);
    if (err != NoError) {
        out.stage = LayerStage_NotReadable;
        out.code = err;
        return;
    }
    out.elementType = GetElementTypeNonLocalizedName (GetElemTypeId (element.header));

    element.header.layer = target;

    API_Element mask = {};
    ACAPI_ELEMENT_MASK_CLEAR (mask);
    ACAPI_ELEMENT_MASK_SET (mask, API_Elem_Head, layer);

    err = ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
    if (err != NoError) {
        out.stage = LayerStage_ChangeRejected;
        out.code = err;
        // Nur lesend: welche Ebene hat das Element jetzt?
        API_Element now = {};
        now.header.guid = guid;
        if (ACAPI_Element_Get (&now) == NoError) {
            out.actual = now.header.layer;
            out.haveActual = true;
        }
        return;
    }

    // Ruecklese - NoError beweist nichts.
    API_Element check = {};
    check.header.guid = guid;
    if (ACAPI_Element_Get (&check) != NoError) {
        out.stage = LayerStage_ReadbackUnreadable;
        out.code = APIERR_GENERAL;
        return;
    }
    out.actual = check.header.layer;
    out.haveActual = true;
    if (check.header.layer.ToInt32_Deprecated () == target.ToInt32_Deprecated ()) {
        out.stage = LayerStage_Ok;
        out.code = NoError;
    } else {
        out.stage = LayerStage_ReadbackMismatch;
        out.code = APIERR_GENERAL;
    }
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
    // ELM_SAB 0.9.24: Erfolgszweig unveraendert; ELMResponseSchemaWithError erlaubt zusaetzlich {error:{code,message}} (sonst 4009 statt Meldung).
    return ELMResponseSchemaWithError (GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": { "type": "array", "items": { "type": "object" } },
            "undoScope": { "type": "object" }
        },
        "required": ["executionResults"]
    })"));
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
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("layerId oder layerName erforderlich."));
    }
    if (!resolved)
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("Ebene nicht gefunden: ") + what);

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

    std::vector<GSErrCode>              status (guids.size (), APIERR_GENERAL);
    std::vector<LayerApplyOutcomeELM>   outcome (guids.size ());

    bool lambdaStarted = false;
    ACAPI_CallUndoableCommand ("ELM_SAB SetLayerOfElements", [&] () -> GSErrCode {
        lambdaStarted = true;
        for (size_t i = 0; i < guids.size (); ++i) {
            if (guids[i] == APINULLGuid)          { status[i] = APIERR_BADPARS;       continue; }
            if (conflicts.ContainsKey (guids[i])) { status[i] = APIERR_NOACCESSRIGHT; continue; }
            ApplyLayerToOne (guids[i], target, outcome[i]);
            status[i] = outcome[i].code;
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
            ok.Add ("layerIndex", (Int32) outcome[i].actual.ToInt32_Deprecated ());
            ok.Add ("layerName", LayerNameOfIndex (outcome[i].actual));
            results (ok);
        } else if (status[i] == APIERR_NOACCESSRIGHT && conflicts.ContainsKey (guids[i])) {
            results (CreateFailedExecutionResult (status[i], "Reservierung fehlgeschlagen (anderer Nutzer, ausgeblendete Ebene oder Hotlink)"));
        } else if (status[i] == APIERR_BADPARS && guids[i] == APINULLGuid) {
            results (CreateFailedExecutionResult (status[i], "elementId fehlt"));
        } else {
            const LayerApplyOutcomeELM& o = outcome[i];
            GS::UniString codeText (std::to_string ((long) o.code).c_str ());
            GS::UniString actualName;
            if (o.haveActual)
                actualName = LayerNameOfIndex (o.actual);
            GS::UniString msg;
            switch (o.stage) {
                case LayerStage_NotReadable:
                    msg = GS::UniString ("Element nicht lesbar (ACAPI_Element_Get)");
                    break;
                case LayerStage_ChangeRejected:
                    msg = GS::UniString ("Archicad lehnt Aenderung ab (code ") + codeText + GS::UniString (")");
                    if (o.haveActual)
                        msg += GS::UniString (", Ist-Ebene ") + actualName;
                    break;
                case LayerStage_ReadbackUnreadable:
                    msg = GS::UniString ("Ruecklese nicht moeglich (Element nach Aenderung nicht lesbar)");
                    break;
                default:
                    msg = GS::UniString ("Ruecklese weicht ab (Ist-Ebene ") + actualName + GS::UniString (")");
                    break;
            }
            GS::ObjectState fail = CreateFailedExecutionResult (CreateErrorResponseELM (o.code, msg));
            fail.Add ("elementId", CreateGuidObjectState (guids[i]));
            if (!o.elementType.IsEmpty ())
                fail.Add ("elementType", o.elementType);
            fail.Add ("layerIndexTarget", (Int32) target.ToInt32_Deprecated ());
            if (o.haveActual) {
                fail.Add ("layerIndexActual", (Int32) o.actual.ToInt32_Deprecated ());
                fail.Add ("layerNameActual", actualName);
            }
            results (fail);
        }
    }
    GS::ObjectState undoScope;
    undoScope.Add ("lambdaStarted", lambdaStarted);
    response.Add ("undoScope", undoScope);
    return response;
}
