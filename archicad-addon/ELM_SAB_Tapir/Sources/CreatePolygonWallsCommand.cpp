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
                        },
                        "typeOverride": {
                            "type": "integer",
                            "description": "ELMonkey 0.11.7 (Diagnose): erzwingt element.wall.type (0 Normal, 1 Trapez, 2 Poly) nach allen anderen Feldern."
                        },
                        "minimal": {
                            "type": "boolean",
                            "description": "ELMonkey 0.11.5 (Diagnose): true = exakt wie das Graphisoft-Forum-Beispiel: GetDefaults ohne Memo, nur type/Struktur/poly/coords/pends/vertexIDs/edgeIDs, keine Referenzlinie, kein begC/endC, kein polyCanChange."
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
            bool minimal = false;
            wallData.Get ("minimal", minimal);

            GSErrCode err = ACAPI_Element_GetDefaults (&element, minimal ? nullptr : &memo);
            if (err != NoError) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (err, "GetDefaults fehlgeschlagen"));
                continue;
            }

            element.wall.type = APIWtyp_Poly;
            if (!minimal) {
                // 0.11.9: Voreinstellungen, die eine Polygonwand verhindern koennen, hart setzen.
                // INT-Vorlage liefert sequence 5000 (SDK: 0..999) -> AC verwirft das Polygon und baut eine
                // Normalwand aus begC/endC (05.11.2026, Dump in frischer INT-Datei).
                if (element.wall.sequence < 0 || element.wall.sequence > 999)
                    element.wall.sequence = 0;
                element.wall.profileType = APISect_Normal;
                element.wall.profileAttr = APIInvalidAttributeIndex;
                element.wall.logHeight = 0.0;
                element.wall.beamFlags = 0;
                element.wall.angle = 0.0;
                element.wall.thickness1 = element.wall.thickness;
                element.wall.polyCanChange = true;
                // 0.11.3: Polygonwaende haben laut SDK-Header immer APIWallRefLine_Outside (Center ist dort ungueltig).
                element.wall.referenceLineLocation = APIWallRefLine_Outside;
                element.wall.offset = 0.0;
            }

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
            // 0.11.8: vollstaendiger Blick auf die Voreinstellung (Suche nach dem Feld, das Poly verhindert)
            debug.Add ("defaultProfileType", (Int32) element.wall.profileType);
            debug.Add ("defaultComposite", element.wall.composite.ToInt32_Deprecated ());
            debug.Add ("defaultBuildingMaterial", element.wall.buildingMaterial.ToInt32_Deprecated ());
            debug.Add ("defaultThickness", element.wall.thickness);
            debug.Add ("defaultThickness1", element.wall.thickness1);
            debug.Add ("defaultOffset", element.wall.offset);
            debug.Add ("defaultFlipped", element.wall.flipped);
            debug.Add ("defaultRefLineLocation", (Int32) element.wall.referenceLineLocation);
            debug.Add ("defaultPolyCanChange", element.wall.polyCanChange);
            debug.Add ("defaultMaterialsChained", element.wall.materialsChained);
            debug.Add ("defaultZoneRel", (Int32) element.wall.zoneRel);
            debug.Add ("defaultWallConnPriority", element.wall.wallConnPriority);
            debug.Add ("defaultUseCompositePriority", element.wall.useCompositePriority);
            debug.Add ("defaultSequence", element.wall.sequence);
            debug.Add ("defaultLogHeight", element.wall.logHeight);
            debug.Add ("defaultBeamFlags", element.wall.beamFlags);
            debug.Add ("defaultAngle", element.wall.angle);
            debug.Add ("defaultMemoGables", memo.gables != nullptr);
            debug.Add ("defaultRefInd", element.wall.refInd);
            debug.Add ("defaultOppInd", element.wall.oppInd);

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
            // 0.11.6: Muster einer von Hand gezeichneten Polygonwand (AC29 live, GetPolygonWallInfo):
            // vertexIDs = [n, 1..n, 1], edgeIDs = [0, 1..n, 1] — der Schlusspunkt traegt die ID des
            // Startpunkts, [0] ist die Anzahl der Ecken (n), nicht nCoords. Bis 0.11.5 stand dort
            // [nCoords, 1..nCoords] -> doppelte Ecke mit eigener ID -> AC verwirft das Polygon.
            (*memo.vertexIDs)[0] = (UInt32) n;
            (*memo.edgeIDs)[0]   = 0;
            for (Int32 k = 1; k <= n; ++k) {
                (*memo.vertexIDs)[k] = (UInt32) k;
                (*memo.edgeIDs)[k]   = (UInt32) k;
            }
            (*memo.vertexIDs)[n + 1] = 1;
            (*memo.edgeIDs)[n + 1]   = 1;
            // 0.11.6: parcs-Handle immer anlegen (echte Polygonwaende haben es auch ohne Boegen).
            {
                const Int32 nArcAlloc = element.wall.poly.nArcs > 0 ? element.wall.poly.nArcs : 1;
                memo.parcs = reinterpret_cast<API_PolyArc**> (BMAllocateHandle (nArcAlloc * sizeof (API_PolyArc), ALLOCATE_CLEAR, 0));
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
            if (!minimal) {
                element.wall.rLinInd    = refEdge;
                element.wall.rLinEndInd = refEndEdge;
                // 0.11.6: wie bei Handwaenden: Referenzmaterial-Kanten = Referenzlinie, Gegenseite = gegenueberliegende Kante
                element.wall.refInd     = refEdge;
                element.wall.refEndInd  = refEndEdge;
                element.wall.oppInd     = ((refEndEdge + n / 2 - 1) % n) + 1;
                element.wall.oppEndInd  = element.wall.oppInd;
                element.wall.polyCanChange = false;
                element.wall.flipped = false;
                element.wall.begC = (*memo.coords)[refEdge];
                element.wall.endC = (*memo.coords)[refEdge + 1];
                if (std::fabs (element.wall.endC.x - element.wall.begC.x) + std::fabs (element.wall.endC.y - element.wall.begC.y) < 1e-6) {
                    ACAPI_DisposeElemMemoHdls (&memo);
                    elements (CreateFailedExecutionResult (APIERR_BADPARS, "Referenzlinien-Kante hat Laenge 0"));
                    continue;
                }
            }

            Int32 typeOverride = -1;
            if (wallData.Get ("typeOverride", typeOverride) && typeOverride >= 0 && typeOverride <= 2)
                element.wall.type = (API_WallTypeID) typeOverride;
            debug.Add ("sentType", (Int32) element.wall.type);

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

// ---------------------------------------------------------------------------
// GetPolygonWallInfo (0.11.5, Diagnose)
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> GetPolygonWallInfoCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": { "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                    "required": ["elementId"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> GetPolygonWallInfoCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": { "walls": { "type": "array", "items": { "type": "object" } } },
        "required": ["walls"]
    })");
}

GS::ObjectState GetPolygonWallInfoCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elems;
    parameters.Get ("elements", elems);

    GS::ObjectState response;
    const auto& walls = response.AddList<GS::ObjectState> ("walls");

    for (const GS::ObjectState& e : elems) {
        const GS::ObjectState* idOs = e.Get ("elementId");
        const API_Guid guid = idOs != nullptr ? GetGuidFromObjectState (*idOs) : APINULLGuid;
        API_Element el = {};
        el.header.guid = guid;
        GSErrCode err = ACAPI_Element_Get (&el);
        if (err != NoError) {
            walls (CreateFailedExecutionResult (err, "ACAPI_Element_Get fehlgeschlagen"));
            continue;
        }
#ifdef ServerMainVers_2600
        const bool isWall = el.header.type == API_WallID;
#else
        const bool isWall = el.header.typeID == API_WallID;
#endif
        if (!isWall) {
            walls (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Kein Wand-Element"));
            continue;
        }
        GS::ObjectState w;
        w.Add ("elementId", CreateGuidObjectStateELM (guid));
        w.Add ("type", (Int32) el.wall.type);
        w.Add ("profileType", (Int32) el.wall.profileType);
        w.Add ("materialsChained", el.wall.materialsChained);
        w.Add ("zoneRel", (Int32) el.wall.zoneRel);
        w.Add ("wallConnPriority", el.wall.wallConnPriority);
        w.Add ("useCompositePriority", el.wall.useCompositePriority);
        w.Add ("sequence", el.wall.sequence);
        w.Add ("logHeight", el.wall.logHeight);
        w.Add ("beamFlags", el.wall.beamFlags);
        w.Add ("composite", el.wall.composite.ToInt32_Deprecated ());
        w.Add ("buildingMaterial", el.wall.buildingMaterial.ToInt32_Deprecated ());
        w.Add ("structureType", (Int32) el.wall.modelElemStructureType);
        w.Add ("referenceLineLocation", (Int32) el.wall.referenceLineLocation);
        w.Add ("polyCanChange", el.wall.polyCanChange);
        w.Add ("flipped", el.wall.flipped);
        w.Add ("rLinInd", el.wall.rLinInd);
        w.Add ("rLinEndInd", el.wall.rLinEndInd);
        w.Add ("refInd", el.wall.refInd);
        w.Add ("refEndInd", el.wall.refEndInd);
        w.Add ("oppInd", el.wall.oppInd);
        w.Add ("oppEndInd", el.wall.oppEndInd);
        w.Add ("thickness", el.wall.thickness);
        w.Add ("thickness1", el.wall.thickness1);
        w.Add ("offset", el.wall.offset);
        w.Add ("height", el.wall.height);
        w.Add ("relativeTopStory", (Int32) el.wall.relativeTopStory);
        w.Add ("angle", el.wall.angle);
        w.Add ("begC", Create2DCoordinateObjectState (el.wall.begC));
        w.Add ("endC", Create2DCoordinateObjectState (el.wall.endC));
        w.Add ("nCoords", el.wall.poly.nCoords);
        w.Add ("nSubPolys", el.wall.poly.nSubPolys);
        w.Add ("nArcs", el.wall.poly.nArcs);

        API_ElementMemo memo = {};
        if (ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_Polygon) == NoError) {
            const auto& coords = w.AddList<GS::ObjectState> ("coords");
            if (memo.coords != nullptr) {
                for (Int32 k = 1; k <= el.wall.poly.nCoords; ++k)
                    coords (Create2DCoordinateObjectState ((*memo.coords)[k]));
            }
            const auto& pends = w.AddList<Int32> ("pends");
            if (memo.pends != nullptr) {
                for (Int32 k = 0; k <= el.wall.poly.nSubPolys; ++k)
                    pends ((*memo.pends)[k]);
            }
            const auto& vids = w.AddList<Int32> ("vertexIDs");
            if (memo.vertexIDs != nullptr) {
                for (Int32 k = 0; k <= el.wall.poly.nCoords; ++k)
                    vids ((Int32) (*memo.vertexIDs)[k]);
            }
            const auto& eids = w.AddList<Int32> ("edgeIDs");
            if (memo.edgeIDs != nullptr) {
                for (Int32 k = 0; k <= el.wall.poly.nCoords; ++k)
                    eids ((Int32) (*memo.edgeIDs)[k]);
            }
            w.Add ("hasParcs", memo.parcs != nullptr);
        }
        ACAPI_DisposeElemMemoHdls (&memo);
        walls (w);
    }
    return response;
}

// ---------------------------------------------------------------------------
// ClonePolygonWall (0.11.7, Diagnose)
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> ClonePolygonWallCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] },
            "dx": { "type": "number", "description": "Versatz in x (m). Default 0." },
            "dy": { "type": "number", "description": "Versatz in y (m). Default 0." },
            "forcePoly": { "type": "boolean", "description": "true = element.wall.type vor dem Create auf APIWtyp_Poly setzen (Default: wie gelesen)." }
        },
        "required": ["elementId"]
    })");
}

GS::Optional<GS::UniString> ClonePolygonWallCommand::GetResponseSchema () const
{
    return GS::UniString (R"({ "type": "object" })");
}

GS::ObjectState ClonePolygonWallCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    const GS::ObjectState* idOs = parameters.Get ("elementId");
    const API_Guid guid = idOs != nullptr ? GetGuidFromObjectState (*idOs) : APINULLGuid;
    double dx = 0.0, dy = 0.0; bool forcePoly = false;
    parameters.Get ("dx", dx); parameters.Get ("dy", dy); parameters.Get ("forcePoly", forcePoly);

    GS::ObjectState response;
    API_Element src = {};
    src.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get (&src);
    if (err != NoError)
        return CreateFailedExecutionResult (err, "ACAPI_Element_Get fehlgeschlagen");
#ifdef ServerMainVers_2600
    if (src.header.type != API_WallID)
#else
    if (src.header.typeID != API_WallID)
#endif
        return CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Kein Wand-Element");

    API_ElementMemo memo = {};
    err = ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_All);
    if (err != NoError) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return CreateFailedExecutionResult (err, "ACAPI_Element_GetMemo fehlgeschlagen");
    }

    response.Add ("sourceType", (Int32) src.wall.type);
    response.Add ("sourceNCoords", src.wall.poly.nCoords);
    response.Add ("memoCoords", memo.coords != nullptr);
    response.Add ("memoPends", memo.pends != nullptr);
    response.Add ("memoParcs", memo.parcs != nullptr);
    response.Add ("memoVertexIDs", memo.vertexIDs != nullptr);
    response.Add ("memoEdgeIDs", memo.edgeIDs != nullptr);
    response.Add ("memoGables", memo.gables != nullptr);

    src.header.guid = APINULLGuid;
    src.wall.begC.x += dx; src.wall.begC.y += dy;
    src.wall.endC.x += dx; src.wall.endC.y += dy;
    if (memo.coords != nullptr) {
        for (Int32 k = 1; k <= src.wall.poly.nCoords; ++k) {
            (*memo.coords)[k].x += dx;
            (*memo.coords)[k].y += dy;
        }
    }
    if (forcePoly)
        src.wall.type = APIWtyp_Poly;
    response.Add ("sentType", (Int32) src.wall.type);

    GSErrCode cerr = NoError;
    ACAPI_CallUndoableCommand ("ELM_SAB ClonePolygonWall", [&] () -> GSErrCode {
        cerr = ACAPI_Element_Create (&src, &memo);
        return cerr;
    });
    ACAPI_DisposeElemMemoHdls (&memo);
    if (cerr != NoError)
        return CreateFailedExecutionResult (cerr, "ACAPI_Element_Create fehlgeschlagen");

    response.Add ("elementId", CreateGuidObjectStateELM (src.header.guid));
    API_Element got = {};
    got.header.guid = src.header.guid;
    if (ACAPI_Element_Get (&got) == NoError) {
        response.Add ("createdType", (Int32) got.wall.type);
        response.Add ("createdNCoords", got.wall.poly.nCoords);
        response.Add ("createdRLinInd", got.wall.rLinInd);
        response.Add ("isPolygonal", got.wall.type == APIWtyp_Poly);
    }
    return response;
}
