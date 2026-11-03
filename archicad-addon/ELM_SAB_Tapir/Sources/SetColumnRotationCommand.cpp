#include "SetColumnRotationCommand.hpp"

#include <cmath>

GS::Optional<GS::UniString> SetColumnRotationCommand::GetInputParametersSchema () const
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
                        },
                        "angleDeg": { "type": "number" }
                    },
                    "required": ["elementId", "angleDeg"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetColumnRotationCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "results": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["results"]
    })");
}

GS::ObjectState SetColumnRotationCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("results");

    ACAPI_CallUndoableCommand ("SetColumnRotation", [&] () -> GSErrCode {
        for (const GS::ObjectState& elementItem : elements) {
            const GS::ObjectState* elementId = elementItem.Get ("elementId");
            double angleDeg = 0.0;
            if (elementId == nullptr || !elementItem.Get ("angleDeg", angleDeg)) {
                results (CreateFailedExecutionResult (APIERR_BADPARS, "elementId oder angleDeg fehlt"));
                continue;
            }

            API_Element element = {};
            element.header.guid = GetGuidFromObjectState (*elementId);
            GSErrCode err = ACAPI_Element_Get (&element);
            if (err != NoError) {
                results (CreateFailedExecutionResult (err, "Element nicht gefunden"));
                continue;
            }
            if (element.header.type.typeID != API_ColumnID) {
                results (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Stuetzen (Column)"));
                continue;
            }

            element.column.axisRotationAngle = angleDeg * 3.14159265358979323846 / 180.0;

            API_Element mask = {};
            ACAPI_ELEMENT_MASK_CLEAR (mask);
            ACAPI_ELEMENT_MASK_SET (mask, API_ColumnType, axisRotationAngle);

            err = ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
            if (err != NoError) {
                results (CreateFailedExecutionResult (err, "ACAPI_Element_Change fehlgeschlagen"));
                continue;
            }

            // Read-back: Erfolg nur behaupten, wenn der Winkel wirklich gesetzt ist
            API_Element check = {};
            check.header.guid = element.header.guid;
            err = ACAPI_Element_Get (&check);
            double istDeg = check.column.axisRotationAngle * 180.0 / 3.14159265358979323846;
            istDeg = std::fmod (istDeg, 360.0);
            if (istDeg < 0.0) istDeg += 360.0;

            GS::ObjectState item;
            double sollDeg = std::fmod (angleDeg, 360.0);
            if (sollDeg < 0.0) sollDeg += 360.0;
            item.Add ("success", err == NoError && std::fabs (istDeg - sollDeg) < 0.01);
            item.Add ("angleDeg", istDeg);
            results (item);
        }
        return NoError;
    });

    return response;
}
