#include "SetColumnDetailsCommand.hpp"

#include <cmath>

GS::Optional<GS::UniString> SetColumnDetailsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "description": "Stuetzen mit zu setzenden Kernmassen (Meter).",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": {
                            "type": "object",
                            "properties": { "guid": { "type": "string" } },
                            "required": ["guid"]
                        },
                        "coreWidth":  { "type": "number" },
                        "coreHeight": { "type": "number" }
                    },
                    "required": ["elementId"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetColumnDetailsCommand::GetResponseSchema () const
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

GS::ObjectState SetColumnDetailsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& executionResults = response.AddList<GS::ObjectState> ("executionResults");

    ACAPI_CallUndoableCommand ("ELM_SAB SetColumnDetails", [&] () -> GSErrCode {
        for (const GS::ObjectState& elementItem : elements) {
            const GS::ObjectState* elementId = elementItem.Get ("elementId");
            if (elementId == nullptr) {
                executionResults (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
                continue;
            }

            double coreWidth = 0.0, coreHeight = 0.0;
            const bool hasWidth  = elementItem.Get ("coreWidth", coreWidth);
            const bool hasHeight = elementItem.Get ("coreHeight", coreHeight);
            if (!hasWidth && !hasHeight) {
                executionResults (CreateFailedExecutionResult (APIERR_BADPARS, "coreWidth oder coreHeight noetig"));
                continue;
            }

            API_Element element = {};
            element.header.guid = GetGuidFromObjectState (*elementId);
            GSErrCode err = ACAPI_Element_Get (&element);
            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "Element nicht gefunden"));
                continue;
            }
            if (element.header.type.typeID != API_ColumnID) {
                executionResults (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Stuetzen (Column)"));
                continue;
            }

            API_ElementMemo memo = {};
            err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_ColumnSegment);
            if (err != NoError || memo.columnSegments == nullptr || element.column.nSegments == 0) {
                ACAPI_DisposeElemMemoHdls (&memo);
                executionResults (CreateFailedExecutionResult (err != NoError ? err : APIERR_GENERAL, "Segment-Memo nicht lesbar"));
                continue;
            }

            for (UInt32 i = 0; i < element.column.nSegments; ++i) {
                if (hasWidth)  memo.columnSegments[i].assemblySegmentData.nominalWidth  = coreWidth;
                if (hasHeight) memo.columnSegments[i].assemblySegmentData.nominalHeight = coreHeight;
            }

            API_Element mask = {};
            ACAPI_ELEMENT_MASK_CLEAR (mask);
            err = ACAPI_Element_Change (&element, &mask, &memo, APIMemoMask_ColumnSegment, true);
            ACAPI_DisposeElemMemoHdls (&memo);
            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "Aenderung fehlgeschlagen"));
                continue;
            }

            // Ruecklese-Verifikation (NoError beweist nichts).
            API_ElementMemo checkMemo = {};
            bool verified = false;
            if (ACAPI_Element_GetMemo (element.header.guid, &checkMemo, APIMemoMask_ColumnSegment) == NoError &&
                checkMemo.columnSegments != nullptr) {
                verified = true;
                if (hasWidth  && std::fabs (checkMemo.columnSegments[0].assemblySegmentData.nominalWidth  - coreWidth)  > 1e-9) verified = false;
                if (hasHeight && std::fabs (checkMemo.columnSegments[0].assemblySegmentData.nominalHeight - coreHeight) > 1e-9) verified = false;
            }
            ACAPI_DisposeElemMemoHdls (&checkMemo);

            if (!verified) {
                executionResults (CreateFailedExecutionResult (APIERR_GENERAL, "Aenderung nicht wirksam (Ruecklese-Verifikation fehlgeschlagen)"));
                continue;
            }

            GS::ObjectState ok;
            ok.Add ("success", true);
            executionResults (ok);
        }
        return NoError;
    });

    return response;
}
