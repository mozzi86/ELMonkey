#include "CreateCurtainWallCommand.hpp"

GS::Optional<GS::UniString> CreateCurtainWallCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "begCoordinate": {
                "type": "object",
                "properties": { "x": { "type": "number" }, "y": { "type": "number" } },
                "required": ["x", "y"]
            },
            "endCoordinate": {
                "type": "object",
                "properties": { "x": { "type": "number" }, "y": { "type": "number" } },
                "required": ["x", "y"]
            },
            "floorIndex": { "type": "integer" },
            "bottomOffset": { "type": "number", "description": "Unterkante relativ zum Geschoss (storyRelLevel), Meter." },
            "columnWidths": {
                "type": "array", "items": { "type": "number", "exclusiveMinimum": 0 }, "minItems": 1,
                "description": "Spaltenbreiten von beg nach end (Pfosten-Achsmasse), Meter. Summe sollte der Wandlaenge entsprechen."
            },
            "rowHeights": {
                "type": "array", "items": { "type": "number", "exclusiveMinimum": 0 }, "minItems": 1,
                "description": "Zeilenhoehen von UNTEN nach oben, Meter. Summe = CW-Hoehe."
            },
            "opaqueRows": {
                "type": "array", "items": { "type": "integer", "minimum": 0 },
                "description": "Zeilen-Indizes (0 = unterste), die die ZWEITE Panel-Klasse der Defaults bekommen (Bruestung/opak). Leer = alles erste Klasse."
            },
            "cellOrderColumnMajor": { "type": "boolean", "description": "Zell-Reihenfolge transponieren, falls die Zeilen-Zuordnung vertauscht ankommt. Default false (row-major)." }
        },
        "required": ["begCoordinate", "endCoordinate", "columnWidths", "rowHeights"]
    })");
}

GS::Optional<GS::UniString> CreateCurtainWallCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "items": { "type": "object" } },
            "error": { "type": "object" }
        }
    })");
}

GS::ObjectState CreateCurtainWallCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    const GS::ObjectState* begOS = parameters.Get ("begCoordinate");
    const GS::ObjectState* endOS = parameters.Get ("endCoordinate");
    GS::Array<double> columnWidths, rowHeights;
    parameters.Get ("columnWidths", columnWidths);
    parameters.Get ("rowHeights", rowHeights);
    GS::Array<Int32> opaqueRows;
    parameters.Get ("opaqueRows", opaqueRows);
    bool columnMajor = false;
    parameters.Get ("cellOrderColumnMajor", columnMajor);
    Int32 floorIndex = 0;
    const bool hasFloor = parameters.Get ("floorIndex", floorIndex);
    double bottomOffset = 0.0;
    parameters.Get ("bottomOffset", bottomOffset);

    if (begOS == nullptr || endOS == nullptr || columnWidths.IsEmpty () || rowHeights.IsEmpty ())
        return CreateErrorResponse (APIERR_BADPARS, "begCoordinate/endCoordinate/columnWidths/rowHeights noetig");

    const API_Coord beg = Get2DCoordinateFromObjectState (*begOS);
    const API_Coord end = Get2DCoordinateFromObjectState (*endOS);

    API_Element element = {};
    API_ElementMemo memo = {};
    element.header.type = API_CurtainWallID;
    GSErrCode err = ACAPI_Element_GetDefaults (&element, &memo);
    if (err != NoError) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return CreateErrorResponse (err, "GetDefaults fehlgeschlagen");
    }

    if (hasFloor)
        element.header.floorInd = (short) floorIndex;
    element.curtainWall.storyRelLevel = bottomOffset;

    // Segment-Polylinie: coords-Handle mit Index-1-Konvention (Slot 0 unbenutzt)
    memo.coords = reinterpret_cast<API_Coord**> (BMhAllClear (3 * sizeof (API_Coord)));
    if (memo.coords == nullptr) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return CreateErrorResponse (APIERR_MEMFULL, "coords-Handle");
    }
    (*memo.coords)[1] = beg;
    (*memo.coords)[2] = end;
    memo.parcs = nullptr;
    element.curtainWall.nSegments = 1;

    // Primaeres Muster = Spalten (fixe Breiten)
    {
        memo.cWSegPrimaryPattern.nPattern = (UInt32) columnWidths.GetSize ();
        memo.cWSegPrimaryPattern.endWithID = memo.cWSegPrimaryPattern.nPattern - 1;
        memo.cWSegPrimaryPattern.logic = APICWSePL_FixedSizes;
        double* p = reinterpret_cast<double*> (BMpAll (sizeof (double) * memo.cWSegPrimaryPattern.nPattern));
        for (UInt32 i = 0; i < memo.cWSegPrimaryPattern.nPattern; ++i)
            p[i] = columnWidths[i];
        BMpKill (reinterpret_cast<GSPtr*> (&memo.cWSegPrimaryPattern.pattern));
        memo.cWSegPrimaryPattern.pattern = p;
    }
    // Sekundaeres Muster = Zeilen (fixe Hoehen, von unten)
    double totalHeight = 0.0;
    {
        memo.cWSegSecondaryPattern.nPattern = (UInt32) rowHeights.GetSize ();
        memo.cWSegSecondaryPattern.endWithID = memo.cWSegSecondaryPattern.nPattern - 1;
        memo.cWSegSecondaryPattern.logic = APICWSePL_FixedSizes;
        double* p = reinterpret_cast<double*> (BMpAll (sizeof (double) * memo.cWSegSecondaryPattern.nPattern));
        for (UInt32 i = 0; i < memo.cWSegSecondaryPattern.nPattern; ++i) {
            p[i] = rowHeights[i];
            totalHeight += rowHeights[i];
        }
        BMpKill (reinterpret_cast<GSPtr*> (&memo.cWSegSecondaryPattern.pattern));
        memo.cWSegSecondaryPattern.pattern = p;
    }
    element.curtainWall.height = totalHeight;

    // Panel-Klassen der Defaults verwenden: 1. Klasse = Standard (Glas),
    // 2. Klasse (falls vorhanden) fuer opaqueRows.
    const UInt32 nPanelClasses = BMpGetSize (reinterpret_cast<GSPtr> (memo.cWallPanelDefaults)) / sizeof (API_CWPanelType);
    if (nPanelClasses == 0) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return CreateErrorResponse (APIERR_GENERAL, "CW-Defaults ohne Panel-Klassen");
    }
    element.curtainWall.nPanelDefaults = nPanelClasses;
    const short glassID  = APICWPanelClass_FirstCustomClass;
    const short opaqueID = (short) (APICWPanelClass_FirstCustomClass + (nPanelClasses > 1 ? 1 : 0));

    GS::HashSet<Int32> opaqueSet;
    for (Int32 r : opaqueRows)
        opaqueSet.Add (r);

    // Zellen
    {
        const UInt32 nPrim = memo.cWSegPrimaryPattern.nPattern;
        const UInt32 nSec  = memo.cWSegSecondaryPattern.nPattern;
        const UInt32 nCells = nPrim * nSec;
        API_CWSegmentPatternCellData* cells = reinterpret_cast<API_CWSegmentPatternCellData*> (BMpAllClear (nCells * sizeof (API_CWSegmentPatternCellData)));
        for (UInt32 i = 0; i < nCells; ++i) {
            const UInt32 rowIdx = columnMajor ? (i % nSec) : (i / nPrim);
            const short panelID = opaqueSet.Contains ((Int32) rowIdx) ? opaqueID : glassID;
            cells[i].crossingFrameType = APICWCFT_NoCrossingFrame;
            cells[i].leftPanelID    = panelID;
            cells[i].rightPanelID   = panelID;
            cells[i].leftFrameID    = APICWFrameClass_Division;
            cells[i].bottomFrameID  = APICWFrameClass_Division;
            cells[i].crossingFrameID = APICWFrameClass_Division;
        }
        BMpKill (reinterpret_cast<GSPtr*> (&memo.cWSegPatternCells));
        memo.cWSegPatternCells = cells;
    }

    GS::ObjectState response;
    err = APIERR_GENERAL;
    ACAPI_CallUndoableCommand ("ELM_SAB CreateCurtainWallFromAxes", [&] () -> GSErrCode {
        err = ACAPI_Element_Create (&element, &memo);
        return err;
    });
    ACAPI_DisposeElemMemoHdls (&memo);

    if (err != NoError)
        return CreateErrorResponse (err, "ACAPI_Element_Create (CurtainWall) fehlgeschlagen");

    const auto& elements = response.AddList<GS::ObjectState> ("elements");
    GS::ObjectState idOS;
    GS::ObjectState guidOS;
    guidOS.Add ("guid", APIGuidToString (element.header.guid));
    idOS.Add ("elementId", guidOS);
    elements (idOS);
    return response;
}
