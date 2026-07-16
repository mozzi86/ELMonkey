#include "GetPenOfElementsCommand.hpp"

GS::Optional<GS::UniString> GetPenOfElementsCommand::GetInputParametersSchema () const
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
                            "properties": {
                                "guid": { "type": "string" }
                            },
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

GS::Optional<GS::UniString> GetPenOfElementsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "pensOfElements": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["pensOfElements"]
    })");
}

GS::ObjectState GetPenOfElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& pensOfElements = response.AddList<GS::ObjectState> ("pensOfElements");

    for (const GS::ObjectState& elementItem : elements) {
        const GS::ObjectState* elementId = elementItem.Get ("elementId");
        if (elementId == nullptr) {
            pensOfElements (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
            continue;
        }

        API_Element element = {};
        element.header.guid = GetGuidFromObjectState (*elementId);
        GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError) {
            pensOfElements (CreateFailedExecutionResult (err, "Element nicht gefunden"));
            continue;
        }

        GS::ObjectState pens;
        pens.Add ("success", true);

        switch (element.header.type.typeID) {
            case API_HatchID: {
                pens.Add ("elementType", "Hatch");
                pens.Add ("contourPen", (Int32) element.hatch.contPen.penIndex);
                pens.Add ("fillForegroundPen", (Int32) element.hatch.fillPen.penIndex);
                pens.Add ("fillBackgroundPen", (Int32) element.hatch.fillBGPen);
                const bool hasFgRGB = (element.hatch.hatchFlags & APIHatch_HasFgRGBColor) != 0;
                const bool hasBgRGB = (element.hatch.hatchFlags & APIHatch_HasBkgRGBColor) != 0;
                pens.Add ("hasForegroundRGB", hasFgRGB);
                pens.Add ("hasBackgroundRGB", hasBgRGB);
                if (hasFgRGB) {
                    GS::ObjectState fg;
                    fg.Add ("red",   element.hatch.foregroundRGB.f_red);
                    fg.Add ("green", element.hatch.foregroundRGB.f_green);
                    fg.Add ("blue",  element.hatch.foregroundRGB.f_blue);
                    pens.Add ("foregroundRGB", fg);
                }
                if (hasBgRGB) {
                    GS::ObjectState bg;
                    bg.Add ("red",   element.hatch.backgroundRGB.f_red);
                    bg.Add ("green", element.hatch.backgroundRGB.f_green);
                    bg.Add ("blue",  element.hatch.backgroundRGB.f_blue);
                    pens.Add ("backgroundRGB", bg);
                }
                break;
            }
            case API_LineID:
                pens.Add ("elementType", "Line");
                pens.Add ("contourPen", (Int32) element.line.linePen.penIndex);
                break;
            case API_PolyLineID:
                pens.Add ("elementType", "PolyLine");
                pens.Add ("contourPen", (Int32) element.polyLine.linePen.penIndex);
                break;
            case API_ArcID:
                pens.Add ("elementType", "Arc");
                pens.Add ("contourPen", (Int32) element.arc.linePen.penIndex);
                break;
            case API_CircleID:
                pens.Add ("elementType", "Circle");
                pens.Add ("contourPen", (Int32) element.circle.linePen.penIndex);
                break;
            case API_SplineID:
                pens.Add ("elementType", "Spline");
                pens.Add ("contourPen", (Int32) element.spline.linePen.penIndex);
                break;
            case API_TextID:
                pens.Add ("elementType", "Text");
                pens.Add ("contourPen", (Int32) element.text.pen);
                pens.Add ("hasFill", element.text.usedFill);
                pens.Add ("fillPen", (Int32) element.text.fillPen);
                break;
            default:
                pensOfElements (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Elementtyp wird nicht unterstützt"));
                continue;
        }

        pensOfElements (pens);
    }

    return response;
}
