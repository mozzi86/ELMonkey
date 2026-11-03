#include "CreatePolygonWallsCommand.hpp"

// ---------------------------------------------------------------------------
// Schema
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> CreatePolygonWallsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "wallsData": {
                "type": "array",
                "description": "Array der zu erzeugenden Polygon-Wände.",
                "items": {
                    "type": "object",
                    "properties": {
                        "polygonCoordinates": {
                            "type": "array",
                            "description": "Geschlossene Kontur (erster Punkt NICHT wiederholen; wird automatisch geschlossen).",
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
                        "floorIndex": {
                            "type": "integer",
                            "description": "Geschoss-Index (Story). Optional, Default: aktuelles Geschoss."
                        },
                        "height": {
                            "type": "number",
                            "description": "Wandhöhe in Metern. Optional, Default: Werkzeug-Voreinstellung."
                        },
                        "bottomOffset": {
                            "type": "number",
                            "description": "Fußpunkt-Offset zum Geschoss in Metern. Optional."
                        },
                        "layerIndex": {
                            "type": "integer",
                            "description": "Ziel-Ebene (Attribut-Index). Optional, Default: Werkzeug-Voreinstellung."
                        },
                        "compositeIndex": {
                            "type": "integer",
                            "description": "Mehrschichtiger Aufbau (Attribut-Index). Optional."
                        },
                        "buildingMaterialIndex": {
                            "type": "integer",
                            "description": "Baustoff (Attribut-Index) für Basic-Struktur. Optional."
                        }
                    },
                    "required": ["polygonCoordinates"]
                }
            }
        },
        "required": ["wallsData"]
    })");
}

GS::Optional<GS::UniString> CreatePolygonWallsCommand::GetResponseSchema () const
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

GS::ObjectState CreatePolygonWallsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> wallsData;
    parameters.Get ("wallsData", wallsData);

    GS::ObjectState response;
    const auto& elements = response.AddList<GS::ObjectState> ("elements");

    ACAPI_CallUndoableCommand ("ELM_SAB CreatePolygonWalls", [&] () -> GSErrCode {
        for (const GS::ObjectState& wallData : wallsData) {
            GS::Array<GS::ObjectState> coords;
            wallData.Get ("polygonCoordinates", coords);
            const Int32 n = (Int32) coords.GetSize ();
            if (n < 3) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "Mindestens 3 Punkte nötig"));
                continue;
            }

            API_Element element = {};
            API_ElementMemo memo = {};

#ifdef ServerMainVers_2600
            element.header.type = API_WallID;
#else
            element.header.typeID = API_WallID;
#endif
            GSErrCode err = ACAPI_Element_GetDefaults (&element, &memo);
            if (err != NoError) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (err, "GetDefaults fehlgeschlagen"));
                continue;
            }

            element.wall.type = APIWtyp_Poly;
            element.wall.polyCanChange = true;
            element.wall.referenceLineLocation = APIWallRefLine_Center;

            Int32 floorIndex = 0;
            if (wallData.Get ("floorIndex", floorIndex))
                element.header.floorInd = (short) floorIndex;

            double height = 0.0;
            if (wallData.Get ("height", height))
                element.wall.height = height;

            double bottomOffset = 0.0;
            if (wallData.Get ("bottomOffset", bottomOffset))
                element.wall.bottomOffset = bottomOffset;

            Int32 layerIndex = 0;
            if (wallData.Get ("layerIndex", layerIndex))
                element.header.layer = ACAPI_CreateAttributeIndex (layerIndex);

            Int32 compositeIndex = 0;
            Int32 buildingMaterialIndex = 0;
            if (wallData.Get ("compositeIndex", compositeIndex)) {
                element.wall.modelElemStructureType = API_CompositeStructure;
                element.wall.composite = ACAPI_CreateAttributeIndex (compositeIndex);
            } else if (wallData.Get ("buildingMaterialIndex", buildingMaterialIndex)) {
                element.wall.modelElemStructureType = API_BasicStructure;
                element.wall.buildingMaterial = ACAPI_CreateAttributeIndex (buildingMaterialIndex);
            }

            // Polygon in Element + Memo (1-basiert, Endpunkt = Startpunkt)
            element.wall.poly.nCoords   = n + 1;
            element.wall.poly.nSubPolys = 1;
            element.wall.poly.nArcs     = 0;

            memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((element.wall.poly.nCoords + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
            memo.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((element.wall.poly.nSubPolys + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
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
            (*memo.pends)[1] = element.wall.poly.nCoords;

            element.wall.begC = (*memo.coords)[1];
            element.wall.endC = (*memo.coords)[1];

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
