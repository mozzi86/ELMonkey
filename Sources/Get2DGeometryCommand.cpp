#include "Get2DGeometryCommand.hpp"

GS::Optional<GS::UniString> Get2DGeometryCommand::GetInputParametersSchema () const
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

GS::Optional<GS::UniString> Get2DGeometryCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "geometryOfElements": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["geometryOfElements"]
    })");
}

static GS::ObjectState CoordOS (double x, double y)
{
    GS::ObjectState os;
    os.Add ("x", x);
    os.Add ("y", y);
    return os;
}

GS::ObjectState Get2DGeometryCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& geometryOfElements = response.AddList<GS::ObjectState> ("geometryOfElements");

    for (const GS::ObjectState& elementItem : elements) {
        const GS::ObjectState* elementId = elementItem.Get ("elementId");
        if (elementId == nullptr) {
            geometryOfElements (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
            continue;
        }

        API_Element element = {};
        element.header.guid = GetGuidFromObjectState (*elementId);
        GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError) {
            geometryOfElements (CreateFailedExecutionResult (err, "Element nicht gefunden"));
            continue;
        }

        GS::ObjectState geo;
        geo.Add ("success", true);
        geo.Add ("layerIndex", (Int32) element.header.layer.ToInt32_Deprecated ());
        geo.Add ("floorIndex", (Int32) element.header.floorInd);

        switch (element.header.type.typeID) {
            case API_LineID: {
                geo.Add ("elementType", "Line");
                geo.Add ("begCoordinate", CoordOS (element.line.begC.x, element.line.begC.y));
                geo.Add ("endCoordinate", CoordOS (element.line.endC.x, element.line.endC.y));
                geo.Add ("ltypeIndex", (Int32) element.line.ltypeInd.ToInt32_Deprecated ());
                geo.Add ("penIndex", (Int32) element.line.linePen.penIndex);
                break;
            }
            case API_ArcID:
            case API_CircleID: {
                const bool isCircle = (element.header.type.typeID == API_CircleID);
                geo.Add ("elementType", isCircle ? "Circle" : "Arc");
                geo.Add ("origin", CoordOS (element.arc.origC.x, element.arc.origC.y));
                geo.Add ("radius", element.arc.r);
                geo.Add ("ratio", element.arc.ratio);
                geo.Add ("angle", element.arc.angle);
                geo.Add ("begAngle", element.arc.begAng);
                geo.Add ("endAngle", element.arc.endAng);
                geo.Add ("reflected", element.arc.reflected);
                geo.Add ("whole", element.arc.whole);
                geo.Add ("ltypeIndex", (Int32) element.arc.ltypeInd.ToInt32_Deprecated ());
                geo.Add ("penIndex", (Int32) element.arc.linePen.penIndex);
                break;
            }
            case API_PolyLineID: {
                geo.Add ("elementType", "PolyLine");
                geo.Add ("ltypeIndex", (Int32) element.polyLine.ltypeInd.ToInt32_Deprecated ());
                geo.Add ("penIndex", (Int32) element.polyLine.linePen.penIndex);
                API_ElementMemo memo = {};
                err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_Polygon);
                if (err != NoError || memo.coords == nullptr) {
                    ACAPI_DisposeElemMemoHdls (&memo);
                    geometryOfElements (CreateFailedExecutionResult (err, "Polygon-Memo nicht lesbar"));
                    continue;
                }
                const auto& coords = geo.AddList<GS::ObjectState> ("coordinates");
                const Int32 n = element.polyLine.poly.nCoords;
                for (Int32 i = 1; i <= n; ++i)
                    coords (CoordOS ((*memo.coords)[i].x, (*memo.coords)[i].y));
                const auto& arcs = geo.AddList<GS::ObjectState> ("arcs");
                if (memo.parcs != nullptr) {
                    for (Int32 i = 0; i < element.polyLine.poly.nArcs; ++i) {
                        GS::ObjectState a;
                        a.Add ("begIndex", (Int32) (*memo.parcs)[i].begIndex);
                        a.Add ("endIndex", (Int32) (*memo.parcs)[i].endIndex);
                        a.Add ("arcAngle", (*memo.parcs)[i].arcAngle);
                        arcs (a);
                    }
                }
                ACAPI_DisposeElemMemoHdls (&memo);
                break;
            }
            case API_HatchID: {
                geo.Add ("elementType", "Hatch");
                API_ElementMemo memo = {};
                err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_Polygon);
                if (err != NoError || memo.coords == nullptr) {
                    ACAPI_DisposeElemMemoHdls (&memo);
                    geometryOfElements (CreateFailedExecutionResult (err, "Polygon-Memo nicht lesbar"));
                    continue;
                }
                const auto& coords = geo.AddList<GS::ObjectState> ("coordinates");
                const Int32 n = element.hatch.poly.nCoords;
                for (Int32 i = 1; i <= n; ++i)
                    coords (CoordOS ((*memo.coords)[i].x, (*memo.coords)[i].y));
                const auto& arcs = geo.AddList<GS::ObjectState> ("arcs");
                if (memo.parcs != nullptr) {
                    for (Int32 i = 0; i < element.hatch.poly.nArcs; ++i) {
                        GS::ObjectState a;
                        a.Add ("begIndex", (Int32) (*memo.parcs)[i].begIndex);
                        a.Add ("endIndex", (Int32) (*memo.parcs)[i].endIndex);
                        a.Add ("arcAngle", (*memo.parcs)[i].arcAngle);
                        arcs (a);
                    }
                }
                ACAPI_DisposeElemMemoHdls (&memo);
                break;
            }
            default:
                geometryOfElements (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Line, Arc, Circle, PolyLine, Hatch"));
                continue;
        }

        geometryOfElements (geo);
    }

    return response;
}
