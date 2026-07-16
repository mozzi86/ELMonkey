#include "ELMCreateRoofsCommand.hpp"

#include <cmath>

// ---------------------------------------------------------------------------
// Schema
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> ELMCreateRoofsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "roofsData": {
                "type": "array",
                "description": "Array der zu erzeugenden Ein-Flächen-Dächer (PlaneRoof).",
                "items": {
                    "type": "object",
                    "properties": {
                        "polygonCoordinates": {
                            "type": "array",
                            "description": "Dachflächen-Kontur (erster Punkt NICHT wiederholen; wird automatisch geschlossen).",
                            "minItems": 3,
                            "items": {
                                "type": "object",
                                "properties": {
                                    "x": { "type": "number" },
                                    "y": { "type": "number" }
                                },
                                "required": ["x", "y"]
                            }
                        },
                        "pivotLine": {
                            "type": "object",
                            "description": "Traufkante (Drehachse der Dachebene) als 2D-Strecke.",
                            "properties": {
                                "p1": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "required": ["x", "y"] },
                                "p2": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "required": ["x", "y"] }
                            },
                            "required": ["p1", "p2"]
                        },
                        "angleDeg": {
                            "type": "number",
                            "description": "Dachneigung in Grad (> 0)."
                        },
                        "zCoordinate": {
                            "type": "number",
                            "description": "Absolute Höhe (Projektnull) der Pivot-Linie / Traufkante."
                        },
                        "posSign": {
                            "type": "boolean",
                            "description": "Steig-Seite relativ zur Pivot-Richtung p1→p2. Default true; per Kalibrier-Dach live prüfen."
                        },
                        "floorIndex": {
                            "type": "integer",
                            "description": "Geschoss-Index (Story). Optional, Default: aktuelles Geschoss."
                        },
                        "thickness": {
                            "type": "number",
                            "description": "Dachdicke in Metern. Optional, Default: Werkzeug-Voreinstellung."
                        },
                        "layerIndex": {
                            "type": "integer",
                            "description": "Ziel-Ebene (Attribut-Index). Optional, Default: Werkzeug-Voreinstellung."
                        }
                    },
                    "required": ["polygonCoordinates", "pivotLine", "angleDeg", "zCoordinate"]
                }
            }
        },
        "required": ["roofsData"]
    })");
}

GS::Optional<GS::UniString> ELMCreateRoofsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["elements"]
    })");
}

// ---------------------------------------------------------------------------
// Ausführung
// ---------------------------------------------------------------------------

GS::ObjectState ELMCreateRoofsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> roofsData;
    parameters.Get ("roofsData", roofsData);

    GS::ObjectState response;
    const auto& elements = response.AddList<GS::ObjectState> ("elements");

    ACAPI_CallUndoableCommand ("ELM_SAB CreateRoofs", [&] () -> GSErrCode {
        for (const GS::ObjectState& roofData : roofsData) {
            GS::Array<GS::ObjectState> coords;
            roofData.Get ("polygonCoordinates", coords);
            const Int32 n = (Int32) coords.GetSize ();
            if (n < 3) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "Mindestens 3 Punkte nötig"));
                continue;
            }

            GS::ObjectState pivot;
            if (!roofData.Get ("pivotLine", pivot)) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "pivotLine fehlt"));
                continue;
            }
            GS::ObjectState p1, p2;
            pivot.Get ("p1", p1);
            pivot.Get ("p2", p2);
            API_Coord c1 = {}, c2 = {};
            p1.Get ("x", c1.x); p1.Get ("y", c1.y);
            p2.Get ("x", c2.x); p2.Get ("y", c2.y);
            if (std::hypot (c2.x - c1.x, c2.y - c1.y) < 0.001) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "pivotLine degeneriert"));
                continue;
            }

            double angleDeg = 0.0;
            roofData.Get ("angleDeg", angleDeg);
            if (angleDeg <= 0.0 || angleDeg >= 89.0) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "angleDeg außerhalb (0, 89)"));
                continue;
            }

            double zCoordinate = 0.0;
            roofData.Get ("zCoordinate", zCoordinate);

            API_Element element = {};
            API_ElementMemo memo = {};

#ifdef ServerMainVers_2600
            element.header.type = API_RoofID;
#else
            element.header.typeID = API_RoofID;
#endif
            GSErrCode err = ACAPI_Element_GetDefaults (&element, &memo);
            if (err != NoError) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (err, "GetDefaults fehlgeschlagen"));
                continue;
            }

            element.roof.roofClass = API_PlaneRoofID;

            Int32 floorIndex = 0;
            if (roofData.Get ("floorIndex", floorIndex))
                element.header.floorInd = (short) floorIndex;

            // Story-Level ermitteln, damit zCoordinate absolut bleibt
            double storyLevel = 0.0;
            {
                API_StoryInfo storyInfo = {};
                if (ACAPI_ProjectSetting_GetStorySettings (&storyInfo) == NoError && storyInfo.data != nullptr) {
                    const short storyCount = storyInfo.lastStory - storyInfo.firstStory + 1;
                    for (short k = 0; k < storyCount; ++k) {
                        if ((*storyInfo.data)[k].index == element.header.floorInd) {
                            storyLevel = (*storyInfo.data)[k].level;
                            break;
                        }
                    }
                    BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));
                }
            }

            element.roof.shellBase.level = zCoordinate - storyLevel;

            double thickness = 0.0;
            if (roofData.Get ("thickness", thickness) && thickness > 0.0)
                element.roof.shellBase.thickness = thickness;

            Int32 layerIndex = 0;
            if (roofData.Get ("layerIndex", layerIndex))
                element.header.layer = ACAPI_CreateAttributeIndex (layerIndex);

            element.roof.u.planeRoof.baseLine.c1 = c1;
            element.roof.u.planeRoof.baseLine.c2 = c2;
            element.roof.u.planeRoof.angle = angleDeg * 3.14159265358979323846 / 180.0;

            bool posSign = true;
            roofData.Get ("posSign", posSign);
            element.roof.u.planeRoof.posSign = posSign;

            // Polygon in Element + Memo (1-basiert, Endpunkt = Startpunkt)
            element.roof.u.planeRoof.poly.nCoords   = n + 1;
            element.roof.u.planeRoof.poly.nSubPolys = 1;
            element.roof.u.planeRoof.poly.nArcs     = 0;

            memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((element.roof.u.planeRoof.poly.nCoords + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
            memo.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((element.roof.u.planeRoof.poly.nSubPolys + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
            if (memo.coords == nullptr || memo.pends == nullptr) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"));
                continue;
            }

            Int32 i = 1;
            for (const GS::ObjectState& c : coords) {
                double x = 0.0, y = 0.0;
                c.Get ("x", x);
                c.Get ("y", y);
                (*memo.coords)[i].x = x;
                (*memo.coords)[i].y = y;
                ++i;
            }
            (*memo.coords)[i] = (*memo.coords)[1];
            (*memo.pends)[1] = element.roof.u.planeRoof.poly.nCoords;

            err = ACAPI_Element_Create (&element, &memo);
            ACAPI_DisposeElemMemoHdls (&memo);

            if (err != NoError) {
                elements (CreateFailedExecutionResult (err, "ACAPI_Element_Create fehlgeschlagen"));
                continue;
            }

            GS::ObjectState ok;
            ok.Add ("elementId", CreateGuidObjectStateELM (element.header.guid));
            elements (ok);
        }
        return NoError;
    });

    return response;
}
