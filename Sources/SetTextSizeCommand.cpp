#include "SetTextSizeCommand.hpp"

#include <cmath>

GS::Optional<GS::UniString> SetTextSizeCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "description": "Ziel-Elemente (Text oder Text-Label/Etikett).",
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
            },
            "sizeMm": {
                "type": "number",
                "description": "Absolute Schriftgröße in mm. Entweder sizeMm oder factor angeben.",
                "exclusiveMinimum": 0
            },
            "factor": {
                "type": "number",
                "description": "Relativer Faktor auf die aktuelle Größe (z.B. 0.5 = halbieren).",
                "exclusiveMinimum": 0
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetTextSizeCommand::GetResponseSchema () const
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

GS::ObjectState SetTextSizeCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    double sizeMm = 0.0;
    double factor = 0.0;
    const bool hasSize   = parameters.Get ("sizeMm", sizeMm) && sizeMm > 0.0;
    const bool hasFactor = parameters.Get ("factor", factor) && factor > 0.0;
    if (hasSize == hasFactor) {
        return CreateErrorResponse (APIERR_BADPARS, "Genau einer von sizeMm oder factor muss angegeben sein (> 0).");
    }

    GS::ObjectState response;
    const auto& executionResults = response.AddList<GS::ObjectState> ("executionResults");

    ACAPI_CallUndoableCommand ("ELM_SAB SetTextSizeOfElements", [&] () -> GSErrCode {
        for (const GS::ObjectState& elementItem : elements) {
            const GS::ObjectState* elementId = elementItem.Get ("elementId");
            if (elementId == nullptr) {
                executionResults (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
                continue;
            }

            API_Element element = {};
            element.header.guid = GetGuidFromObjectState (*elementId);
            GSErrCode err = ACAPI_Element_Get (&element);
            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "Element nicht gefunden"));
                continue;
            }

            API_Element mask = {};
            ACAPI_ELEMENT_MASK_CLEAR (mask);
            double oldSize = 0.0;

            switch (element.header.type.typeID) {
                case API_TextID:
                    oldSize = element.text.size;
                    element.text.size = hasSize ? sizeMm : element.text.size * factor;
                    ACAPI_ELEMENT_MASK_SET (mask, API_TextType, size);
                    err = ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
                    break;

                case API_LabelID: {
                    if (element.label.labelClass != APILblClass_Text) {
                        executionResults (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Text-Labels (kein Symbol-Etikett)"));
                        continue;
                    }
                    // DevKit-Muster (Element_Test/Do_Label_Edit): Memo MUSS mitgegeben
                    // werden, textSize (top-level) und u.text.size gemeinsam setzen.
                    oldSize = element.label.u.text.size;
                    const double newLabelSize = hasSize ? sizeMm : oldSize * factor;

                    API_ElementMemo memo = {};
                    err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_TextContentUni);
                    if (err != NoError) {
                        executionResults (CreateFailedExecutionResult (err, "Label-Memo nicht lesbar"));
                        continue;
                    }

                    element.label.textSize = newLabelSize;
                    element.label.u.text.size = newLabelSize;
                    ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, textSize);
                    ACAPI_ELEMENT_MASK_SET (mask, API_LabelType, u.text.size);
                    err = ACAPI_Element_Change (&element, &mask, &memo, 0, true);

                    // Rücklese-Verifikation: Archicad quittiert Label-Änderungen
                    // teils mit NoError, ohne etwas zu schreiben.
                    if (err == NoError) {
                        API_Element check = {};
                        check.header.guid = element.header.guid;
                        if (ACAPI_Element_Get (&check) == NoError &&
                            std::fabs (check.label.u.text.size - newLabelSize) > 1e-9) {
                            // Eskalation: Voll-Maske (Element wurde frisch gelesen,
                            // nur die Groesse ist veraendert).
                            ACAPI_ELEMENT_MASK_SETFULL (mask);
                            err = ACAPI_Element_Change (&element, &mask, &memo, 0, true);
                            if (err == NoError &&
                                ACAPI_Element_Get (&check) == NoError &&
                                std::fabs (check.label.u.text.size - newLabelSize) > 1e-9) {
                                err = APIERR_GENERAL; // ehrlich scheitern statt still
                            }
                        }
                    }
                    ACAPI_DisposeElemMemoHdls (&memo);
                    break;
                }

                default:
                    executionResults (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Text oder Label"));
                    continue;
            }

            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "Größe konnte nicht geändert werden"));
                continue;
            }

            GS::ObjectState ok;
            ok.Add ("success", true);
            ok.Add ("oldSizeMm", oldSize);
            ok.Add ("newSizeMm", hasSize ? sizeMm : oldSize * factor);
            executionResults (ok);
        }
        return NoError;
    });

    return response;
}
