#include "GetTextsCommand.hpp"

GS::Optional<GS::UniString> GetTextsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": {
                            "type": "object",
                            "properties": { "guid": { "type": "string" } },
                            "required": ["guid"]
                        }
                    },
                    "required": ["elementId"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> GetTextsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "textsOfElements": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["textsOfElements"]
    })");
}

GS::ObjectState GetTextsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& textsOfElements = response.AddList<GS::ObjectState> ("textsOfElements");

    for (const GS::ObjectState& elementItem : elements) {
        const GS::ObjectState* elementId = elementItem.Get ("elementId");
        if (elementId == nullptr) {
            textsOfElements (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
            continue;
        }

        API_Element element = {};
        element.header.guid = GetGuidFromObjectState (*elementId);
        GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError) {
            textsOfElements (CreateFailedExecutionResult (err, "Element nicht gefunden"));
            continue;
        }

        const bool isText  = (element.header.type.typeID == API_TextID);
        const bool isLabel = (element.header.type.typeID == API_LabelID);
        if (!isText && !isLabel) {
            textsOfElements (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Text oder Label"));
            continue;
        }
        if (isLabel && element.label.labelClass != APILblClass_Text) {
            textsOfElements (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Text-Labels (kein Symbol-Etikett)"));
            continue;
        }

        API_ElementMemo memo = {};
        err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_TextContentUni);
        if (err != NoError || memo.textContent == nullptr) {
            ACAPI_DisposeElemMemoHdls (&memo);
            textsOfElements (CreateFailedExecutionResult (err, "Text-Inhalt nicht lesbar"));
            continue;
        }

        GS::ObjectState item;
        item.Add ("success", true);
        item.Add ("elementType", isLabel ? "Label" : "Text");
        item.Add ("layerIndex", (Int32) element.header.layer.ToInt32_Deprecated ());
        item.Add ("floorIndex", (Int32) element.header.floorInd);
        item.Add ("content", *memo.textContent);
        item.Add ("sizeMm", isLabel ? element.label.u.text.size : element.text.size);

        GS::ObjectState loc;
        loc.Add ("x", isLabel ? element.label.u.text.loc.x : element.text.loc.x);
        loc.Add ("y", isLabel ? element.label.u.text.loc.y : element.text.loc.y);
        item.Add ("location", loc);

        item.Add ("angleRad", isLabel ? element.label.u.text.angle : element.text.angle);
        item.Add ("penIndex", (Int32) (isLabel ? element.label.u.text.pen : element.text.pen));
        item.Add ("anchor", (Int32) (isLabel ? element.label.u.text.anchor : element.text.anchor));
        item.Add ("widthMm", isLabel ? element.label.u.text.width : element.text.width);

        ACAPI_DisposeElemMemoHdls (&memo);
        textsOfElements (item);
    }

    return response;
}
