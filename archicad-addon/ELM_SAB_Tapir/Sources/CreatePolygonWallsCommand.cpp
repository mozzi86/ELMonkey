#include "CreatePolygonWallsCommand.hpp"
#include <cmath>

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
                        },
                        "polygonArcs": {
                            "type": "array",
                            "description": "ELM_SAB 0.10.8: Bogensegmente wie bei CreateSlabs (0-basiert): begIndex/endIndex = Punktindizes in polygonCoordinates, Schlusskante = endIndex n (Anzahl Punkte); arcAngle in Radiant, positiv = Bogen rechts der Richtung begIndex->endIndex.",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "begIndex": { "type": "integer" },
                                    "endIndex": { "type": "integer" },
                                    "arcAngle": { "type": "number" }
                                },
                                "required": ["begIndex", "endIndex", "arcAngle"]
                            }
                        },
                        "renovationStatus": { "type": "string", "enum": ["Existing", "New", "ToBeDemolished", "Bestand", "Neubau", "Abbruch"], "description": "ELM_SAB 0.10.8: Umbaustatus der neuen Wand." },
                        "referenceLineEdge": {
                            "type": "integer",
                            "description": "ELMonkey 0.11.2: Kante der Kontur, auf der die Referenzlinie der Polygonwand liegt (1-basiert, Kante k = Punkt k -> Punkt k+1). Optional, Default 1."
                        },
                        "referenceLineEndEdge": {
                            "type": "integer",
                            "description": "ELMonkey 0.11.2: Endkante der Referenzlinie (1-basiert). Optional, Default = referenceLineEdge."
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
            // 0.11.3: Polygonwaende haben laut SDK-Header immer APIWallRefLine_Outside (Center ist dort ungueltig).
            element.wall.referenceLineLocation = APIWallRefLine_Outside;
            element.wall.offset = 0.0;

            Int32 floorIndex = 0;
            if (wallData.Get ("floorIndex", floorIndex))
                element.header.floorInd = (short) floorIndex;

            // Werte der Werkzeug-Voreinstellung, bevor wir eingreifen (Diagnose, 0.11.2)
            GS::ObjectState debug;
            debug.Add ("defaultType", (Int32) element.wall.type);
            debug.Add ("defaultMemoCoords", memo.coords != nullptr);
            debug.Add ("defaultMemoParcs", memo.parcs != nullptr);
            debug.Add ("defaultNArcs", element.wall.poly.nArcs);

            // 0.11.4: Die Voreinstellung kann selbst eine Polygonwand sein (defaultType 2 live in der
            // INT-Vorlage) und bringt dann eigene Memo-Handles (coords/pends/parcs/...) mit. Die wurden bis
            // 0.11.3 nur teilweise ueberschrieben (parcs blieb stehen, nArcs = 0) -> AC29 verwirft das Polygon
            // und speichert eine Normalwand. Deshalb: Memo der Voreinstellung komplett verwerfen.
            ACAPI_DisposeElemMemoHdls (&memo);
            memo = {};
            debug.Add ("defaultRLinInd", element.wall.rLinInd);
            debug.Add ("defaultRLinEndInd", element.wall.rLinEndInd);
            debug.Add ("defaultRelativeTopStory", (Int32) element.wall.relativeTopStory);
            debug.Add ("defaultStructureType", (Int32) element.wall.modelElemStructureType);
            debug.Add ("defaultProfileAttr", element.wall.profileAttr.ToInt32_Deprecated ());

            double height = 0.0;
            if (wallData.Get ("height", height)) {
                element.wall.height = height;
                // 0.11.2: Eine oben ans Geschoss gelinkte Voreinstellung ueberschreibt die Hoehe — Link loesen.
                element.wall.relativeTopStory = 0;
            }

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

            {
                const auto renErr = ApplyRenovationStatusELM (element, wallData);
                if (renErr.HasValue ()) {
                    ACAPI_DisposeElemMemoHdls (&memo);
                    elements (CreateFailedExecutionResult (APIERR_BADPARS, renErr.Get ()));
                    continue;
                }
            }

            // ELM_SAB 0.10.8: Bogensegmente (0-basiert wie CreateSlabs -> Memo 1-basiert)
            GS::Array<GS::ObjectState> arcsIn;
            wallData.Get ("polygonArcs", arcsIn);
            const GS::Array<API_PolyArc> polyArcs = GetPolyArcs (arcsIn, 1);
            bool arcsOk = true;
            for (const API_PolyArc& a : polyArcs) {
                if (a.begIndex < 1 || a.endIndex < 1 || a.begIndex > n + 1 || a.endIndex > n + 1 || a.begIndex == a.endIndex || a.arcAngle == 0.0) {
                    arcsOk = false;
                }
            }
            if (!arcsOk) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "polygonArcs: Index ausserhalb 0..n, gleiche Indizes oder Winkel 0"));
                continue;
            }

            // Polygon in Element + Memo (1-basiert, Endpunkt = Startpunkt)
            element.wall.poly.nCoords   = n + 1;
            element.wall.poly.nSubPolys = 1;
            element.wall.poly.nArcs     = (Int32) polyArcs.GetSize ();

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

            // 0.11.3: vertexIDs/edgeIDs sind fuer Polygonwaende Pflicht (Graphisoft-Forum, akzeptierte Loesung
            // "Wall Geometry Method: Polygonal", 2023). Ohne sie speichert AC29 die Wand als Normalwand.
            memo.vertexIDs = reinterpret_cast<UInt32**> (BMAllocateHandle ((element.wall.poly.nCoords + 1) * sizeof (UInt32), ALLOCATE_CLEAR, 0));
            memo.edgeIDs   = reinterpret_cast<UInt32**> (BMAllocateHandle ((element.wall.poly.nCoords + 1) * sizeof (UInt32), ALLOCATE_CLEAR, 0));
            if (memo.vertexIDs == nullptr || memo.edgeIDs == nullptr) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (APIERR_MEMFULL, "Speicher-Allokation (vertexIDs/edgeIDs) fehlgeschlagen"));
                continue;
            }
            (*memo.vertexIDs)[0] = (UInt32) element.wall.poly.nCoords;
            (*memo.edgeIDs)[0]   = 0;
            for (Int32 k = 1; k <= element.wall.poly.nCoords; ++k) {
                (*memo.vertexIDs)[k] = (UInt32) k;
                (*memo.edgeIDs)[k]   = (UInt32) k;
            }
            if (element.wall.poly.nArcs > 0) {
                memo.parcs = reinterpret_cast<API_PolyArc**> (BMAllocateHandle (element.wall.poly.nArcs * sizeof (API_PolyArc), ALLOCATE_CLEAR, 0));
                if (memo.parcs == nullptr) {
                    ACAPI_DisposeElemMemoHdls (&memo);
                    elements (CreateFailedExecutionResult (APIERR_MEMFULL, "Speicher-Allokation (Boegen) fehlgeschlagen"));
                    continue;
                }
                Int32 iArc = 0;
                for (const API_PolyArc& a : polyArcs) {
                    (*memo.parcs)[iArc++] = a;
                }
            }

            // ELMonkey 0.11.2: Referenzlinie der Polygonwand = Kante rLinInd..rLinEndInd (1-basiert,
            // API_WallType::rLinInd). Ohne diese Felder (0 aus der Voreinstellung) macht AC29 aus der
            // Polygonwand eine Normalwand von begC nach endC — bis 0.11.0 mit begC == endC sogar ein
            // Nullvektor, an dem ProfileTo3DTransformer::SetupCoordinateSystemFromWall mit SIGABRT abbrach.
            Int32 refEdge = 1, refEndEdge = 0;
            wallData.Get ("referenceLineEdge", refEdge);
            if (!wallData.Get ("referenceLineEndEdge", refEndEdge))
                refEndEdge = refEdge;
            if (refEdge < 1 || refEdge > n || refEndEdge < 1 || refEndEdge > n) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "referenceLineEdge/referenceLineEndEdge ausserhalb 1..n"));
                continue;
            }
            element.wall.rLinInd    = refEdge;
            element.wall.rLinEndInd = refEndEdge;
            element.wall.begC = (*memo.coords)[refEdge];
            element.wall.endC = (*memo.coords)[refEdge + 1];
            if (std::fabs (element.wall.endC.x - element.wall.begC.x) + std::fabs (element.wall.endC.y - element.wall.begC.y) < 1e-6) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "Referenzlinien-Kante hat Laenge 0"));
                continue;
            }

            err = ACAPI_Element_Create (&element, &memo);
            ACAPI_DisposeElemMemoHdls (&memo);

            if (err != NoError) {
                elements (CreateFailedExecutionResult (err, "ACAPI_Element_Create fehlgeschlagen"));
                continue;
            }

            // Ruecklese: was hat Archicad daraus gemacht? (0.11.2)
            API_Element got = {};
            got.header.guid = element.header.guid;
            if (ACAPI_Element_Get (&got) == NoError) {
                debug.Add ("createdType", (Int32) got.wall.type);
                debug.Add ("createdRLinInd", got.wall.rLinInd);
                debug.Add ("createdRLinEndInd", got.wall.rLinEndInd);
                debug.Add ("createdRelativeTopStory", (Int32) got.wall.relativeTopStory);
                debug.Add ("createdHeight", got.wall.height);
                debug.Add ("createdNCoords", got.wall.poly.nCoords);
            }

            GS::ObjectState ok;
            ok.Add ("elementId", CreateGuidObjectStateELM (element.header.guid));
            ok.Add ("isPolygonal", got.wall.type == APIWtyp_Poly);
            ok.Add ("debug", debug);
            elements (ok);
        }
        return NoError;
    });

    return response;
}
