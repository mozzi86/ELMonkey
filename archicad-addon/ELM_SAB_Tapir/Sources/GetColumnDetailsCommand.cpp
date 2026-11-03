#include "GetColumnDetailsCommand.hpp"

#include <cmath>

GS::Optional<GS::UniString> GetColumnDetailsCommand::GetInputParametersSchema () const
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

GS::Optional<GS::UniString> GetColumnDetailsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "columnDetails": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["columnDetails"]
    })");
}

GS::ObjectState GetColumnDetailsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& columnDetails = response.AddList<GS::ObjectState> ("columnDetails");

    for (const GS::ObjectState& elementItem : elements) {
        const GS::ObjectState* elementId = elementItem.Get ("elementId");
        if (elementId == nullptr) {
            columnDetails (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
            continue;
        }

        API_Element element = {};
        element.header.guid = GetGuidFromObjectState (*elementId);
        GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError) {
            columnDetails (CreateFailedExecutionResult (err, "Element nicht gefunden"));
            continue;
        }
        if (element.header.type.typeID != API_ColumnID) {
            columnDetails (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Stuetzen (Column)"));
            continue;
        }

        GS::ObjectState detail;
        detail.Add ("success", true);
        detail.Add ("floorIndex", (Int32) element.header.floorInd);

        GS::ObjectState origin;
        origin.Add ("x", element.column.origoPos.x);
        origin.Add ("y", element.column.origoPos.y);
        detail.Add ("origin", origin);

        // Winkel in Grad, normalisiert auf [0, 360)
        double deg = element.column.axisRotationAngle * 180.0 / 3.14159265358979323846;
        deg = std::fmod (deg, 360.0);
        if (deg < 0.0) deg += 360.0;
        detail.Add ("axisRotationAngleDeg", deg);
        detail.Add ("isSlanted", element.column.isSlanted);
        detail.Add ("isFlipped", element.column.isFlipped);
        detail.Add ("nSegments", (Int32) element.column.nSegments);

        // Kernmaße aus dem ersten Segment (nominalWidth/Height)
        API_ElementMemo memo = {};
        if (ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_ColumnSegment) == NoError &&
            memo.columnSegments != nullptr && element.column.nSegments > 0) {
            detail.Add ("coreWidth",  memo.columnSegments[0].assemblySegmentData.nominalWidth);
            detail.Add ("coreHeight", memo.columnSegments[0].assemblySegmentData.nominalHeight);
        }
        ACAPI_DisposeElemMemoHdls (&memo);

        columnDetails (detail);
    }

    return response;
}
