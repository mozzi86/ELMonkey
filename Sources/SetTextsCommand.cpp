#include "SetTextsCommand.hpp"

GS::Optional<GS::UniString> SetTextsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "description": "Ziel-Elemente mit neuem Inhalt (Text oder Text-Label).",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": {
                            "type": "object",
                            "properties": { "guid": { "type": "string" } },
                            "required": ["guid"]
                        },
                        "content": {
                            "type": "string",
                            "description": "Neuer Textinhalt. Zeilenumbruch als \n."
                        }
                    },
                    "required": ["elementId", "content"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetTextsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["executionResults"]
    })");
}

GS::ObjectState SetTextsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& executionResults = response.AddList<GS::ObjectState> ("executionResults");

    ACAPI_CallUndoableCommand ("ELM_SAB SetTextsOfElements", [&] () -> GSErrCode {
        for (const GS::ObjectState& elementItem : elements) {
            const GS::ObjectState* elementId = elementItem.Get ("elementId");
            GS::UniString content;
            if (elementId == nullptr || !elementItem.Get ("content", content)) {
                executionResults (CreateFailedExecutionResult (APIERR_BADPARS, "elementId oder content fehlt"));
                continue;
            }

            API_Element element = {};
            element.header.guid = GetGuidFromObjectState (*elementId);
            GSErrCode err = ACAPI_Element_Get (&element);
            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "Element nicht gefunden"));
                continue;
            }

            const bool isText  = (element.header.type.typeID == API_TextID);
            const bool isLabel = (element.header.type.typeID == API_LabelID);
            if (!isText && !isLabel) {
                executionResults (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Text oder Label"));
                continue;
            }
            if (isLabel && element.label.labelClass != APILblClass_Text) {
                executionResults (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Text-Labels (kein Symbol-Etikett)"));
                continue;
            }

            API_ElementMemo memo = {};
            err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_TextContentUni);
            if (err != NoError || memo.textContent == nullptr) {
                ACAPI_DisposeElemMemoHdls (&memo);
                executionResults (CreateFailedExecutionResult (err, "Text-Inhalt nicht lesbar"));
                continue;
            }

            // \n aus JSON auf Archicad-Zeilenumbruch (\r) mappen
            GS::UniString normalized = content;
            normalized.ReplaceAll ("\r\n", "\n");
            normalized.ReplaceAll ("\n", "\r");
            *memo.textContent = normalized;

            API_Element mask = {};
            ACAPI_ELEMENT_MASK_CLEAR (mask);
            err = ACAPI_Element_Change (&element, &mask, &memo, APIMemoMask_TextContentUni, true);
            ACAPI_DisposeElemMemoHdls (&memo);

            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "Inhalt konnte nicht geändert werden"));
                continue;
            }
            executionResults (CreateSuccessfulExecutionResult ());
        }
        return NoError;
    });

    return response;
}
