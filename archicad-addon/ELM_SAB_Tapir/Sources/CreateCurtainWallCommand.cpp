#include "CreateCurtainWallCommand.hpp"
#include "MigrationHelper.hpp"

#include <cmath>

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
            "bottomOffset": { "type": "number", "description": "Unterkante relativ zum Geschoss, Meter. Wird nach dem Create per Drag nachgezogen (storyRelLevel wirkt beim Create nicht) und rueckgelesen." },
            "angle": { "type": "number", "exclusiveMinimum": 0, "exclusiveMaximum": 180, "description": "Neigung in Grad, Konvention wie CW-Dialog und Tapir-GetDetailsOfElements: 90 = senkrecht, 135 = um 45 Grad zur Extrusionsseite gekippt. columnWidths/rowHeights/contour bleiben IN-PLANE-Masse; die vertikale Hoehe setzt der Befehl intern auf totalHeight*sin(angle). Optional, Default 90." },
            "columnWidths": {
                "type": "array", "items": { "type": "number", "exclusiveMinimum": 0 }, "minItems": 1,
                "description": "Spaltenbreiten von beg nach end (Pfosten-Achsmasse), Meter. Summe sollte der Wandlaenge entsprechen."
            },
            "rowHeights": {
                "type": "array", "items": { "type": "number", "exclusiveMinimum": 0 }, "minItems": 1,
                "description": "Zeilenhoehen von UNTEN nach oben, Meter. Bei nichtrechteckiger Kontur wird das Muster nach oben wiederholt."
            },
            "opaqueRows": {
                "type": "array", "items": { "type": "integer", "minimum": 0 },
                "description": "Zeilen-Indizes (0 = unterste), die die ZWEITE Panel-Klasse der Defaults bekommen (Bruestung/opak). Leer = alles erste Klasse."
            },
            "cellOrderColumnMajor": { "type": "boolean", "description": "Zell-Reihenfolge transponieren, falls die Zeilen-Zuordnung vertauscht ankommt. Default false (row-major)." },
            "contour": {
                "type": "array", "minItems": 3,
                "items": {
                    "type": "object",
                    "properties": { "u": { "type": "number" }, "v": { "type": "number" } },
                    "required": ["u", "v"]
                },
                "description": "Optionale Fassaden-Kontur in SEGMENT-LOKAL-Koordinaten: u = Lauflaenge ab begCoordinate (0..Summe columnWidths), v = Hoehe ueber CW-Unterkante. Umlaufend, Schlusspunkt NICHT wiederholen. Fehlt der Parameter, entsteht ein Rechteck."
            },
            "topProfile": {
                "type": "array", "minItems": 2, "items": { "type": "number", "minimum": 0 },
                "description": "Bequemer Weg zu Trapez/Giebel statt 'contour': Oberkanten-Hoehen v an den Pfostenachsen, also genau columnWidths+1 Werte (u = 0, c0, c0+c1, ...). Unterkante ist durchgehend v=0. Wird ignoriert, wenn 'contour' gesetzt ist."
            },
            "mullionClass": { "type": "integer", "minimum": 0, "description": "0-basierter Index in die Frame-Klassen der CW-Defaults fuer die SENKRECHTEN Rahmen (Pfosten). Default 0 = erste Klasse." },
            "transomClass": { "type": "integer", "minimum": 0, "description": "0-basierter Index in die Frame-Klassen der CW-Defaults fuer die WAAGERECHTEN Rahmen (Riegel). Default 0 = erste Klasse." }
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
            "height": { "type": "number", "description": "IN-PLANE-Gesamthoehe (Summe rowHeights bzw. Kontur-vMax)." },
            "verticalHeight": { "type": "number", "description": "Vertikale Hoehe = height*sin(angle); Soll-Wert fuer die bbox-dz-Ruecklese." },
            "angleApplied": { "type": "number" },
            "contourVertices": { "type": "integer" },
            "bottomOffsetApplied": { "type": "boolean" },
            "zMin": { "type": "number" },
            "zMax": { "type": "number" },
            "error": { "type": "object" }
        }
    })");
}

// Kontur-Polygon (Segment-Lokalkoordinaten) in memo.cWSegContour legen.
// Archicad-Konvention: coords[1..nCoords], coords[nCoords] == coords[1],
// also nCoords = Anzahl echter Ecken + 1. pends[1] = nCoords.
static bool FillSegmentContour (API_ElementMemo& memo, const GS::Array<API_Coord>& corners)
{
    const Int32 nDistinct = (Int32) corners.GetSize ();
    if (nDistinct < 3)
        return false;
    const Int32 nCoords = nDistinct + 1;

    memo.cWSegContour = reinterpret_cast<API_CWContourData*> (BMpAllClear (sizeof (API_CWContourData)));
    if (memo.cWSegContour == nullptr)
        return false;
    API_CWContourData& c = memo.cWSegContour[0];

    c.polygon.nCoords    = nCoords;
    c.polygon.nSubPolys  = 1;
    c.polygon.nArcs      = 0;

    c.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((nCoords + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
    c.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((c.polygon.nSubPolys + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
    c.vertexIDs = reinterpret_cast<UInt32**> (BMAllocateHandle ((nCoords + 1) * sizeof (UInt32), ALLOCATE_CLEAR, 0));
    c.parcs  = nullptr;
    if (c.coords == nullptr || c.pends == nullptr || c.vertexIDs == nullptr)
        return false;

    for (Int32 k = 1; k <= nDistinct; ++k) {
        (*c.coords)[k] = corners[k - 1];
        (*c.vertexIDs)[k] = (UInt32) k;
    }
    (*c.coords)[nCoords] = corners[0];              // Polygon schliessen
    (*c.vertexIDs)[nCoords] = (UInt32) nCoords;
    (*c.pends)[1] = nCoords;

    return true;
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
    double angle = 90.0;
    parameters.Get ("angle", angle);

    if (begOS == nullptr || endOS == nullptr || columnWidths.IsEmpty () || rowHeights.IsEmpty ())
        return CreateErrorResponse (APIERR_BADPARS, "begCoordinate/endCoordinate/columnWidths/rowHeights noetig");
    if (angle <= 0.0 || angle >= 180.0)
        return CreateErrorResponse (APIERR_BADPARS, "angle muss zwischen 0 und 180 Grad liegen (exklusiv; 90 = senkrecht)");

    const API_Coord beg = Get2DCoordinateFromObjectState (*begOS);
    const API_Coord end = Get2DCoordinateFromObjectState (*endOS);

    double totalWidth = 0.0;
    for (double w : columnWidths)
        totalWidth += w;
    double patternHeight = 0.0;
    for (double h : rowHeights)
        patternHeight += h;

    // --- Kontur bestimmen: explizit ("contour") oder aus "topProfile" gebaut ---
    GS::Array<API_Coord> contour;
    GS::Array<GS::ObjectState> contourOS;
    if (parameters.Get ("contour", contourOS) && contourOS.GetSize () >= 3) {
        for (const GS::ObjectState& p : contourOS) {
            API_Coord c = {};
            p.Get ("u", c.x);
            p.Get ("v", c.y);
            contour.Push (c);
        }
    } else {
        GS::Array<double> topProfile;
        if (parameters.Get ("topProfile", topProfile) && !topProfile.IsEmpty ()) {
            if (topProfile.GetSize () != columnWidths.GetSize () + 1)
                return CreateErrorResponse (APIERR_BADPARS, "topProfile braucht genau columnWidths+1 Werte (Hoehen an den Pfostenachsen)");
            for (double v : topProfile) {
                if (v <= 0.0)
                    return CreateErrorResponse (APIERR_BADPARS, "topProfile-Hoehen muessen > 0 sein");
            }
            // Unterkante links -> rechts, dann Oberkante rechts -> links (umlaufend).
            contour.Push (API_Coord { 0.0, 0.0 });
            contour.Push (API_Coord { totalWidth, 0.0 });
            double u = totalWidth;
            for (UIndex i = topProfile.GetSize (); i > 0; --i) {
                contour.Push (API_Coord { u, topProfile[i - 1] });
                if (i >= 2)
                    u -= columnWidths[i - 2];
            }
        }
    }

    const bool hasContour = !contour.IsEmpty ();
    double totalHeight = patternHeight;
    if (hasContour) {
        double vMax = 0.0;
        for (const API_Coord& c : contour) {
            if (c.y > vMax)
                vMax = c.y;
        }
        if (vMax <= 0.0)
            return CreateErrorResponse (APIERR_BADPARS, "Kontur ohne positive Hoehe");
        totalHeight = vMax;
    }

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

    if (hasContour && !FillSegmentContour (memo, contour)) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return CreateErrorResponse (APIERR_MEMFULL, "Kontur-Handles konnten nicht angelegt werden");
    }

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
    {
        memo.cWSegSecondaryPattern.nPattern = (UInt32) rowHeights.GetSize ();
        memo.cWSegSecondaryPattern.endWithID = memo.cWSegSecondaryPattern.nPattern - 1;
        memo.cWSegSecondaryPattern.logic = APICWSePL_FixedSizes;
        double* p = reinterpret_cast<double*> (BMpAll (sizeof (double) * memo.cWSegSecondaryPattern.nPattern));
        for (UInt32 i = 0; i < memo.cWSegSecondaryPattern.nPattern; ++i)
            p[i] = rowHeights[i];
        BMpKill (reinterpret_cast<GSPtr*> (&memo.cWSegSecondaryPattern.pattern));
        memo.cWSegSecondaryPattern.pattern = p;
    }
    // Neigung (0.9.14): Das Feld ist GRAD-basiert — Tapir GetDetailsOfElements gibt
    // elem.curtainWall.angle roh aus und lieferte live 90 bzw. 116.7 (kein DEGRAD!).
    // curtainWall.height ist die VERTIKALE Hoehe (live 2026-08-07: 116.7-Grad-CW hat
    // height = bbox-dz = 2.148, in-plane = height/sin = 2.404); rowHeights und contour
    // bleiben in-plane. Ohne die sin-Skalierung fuellte das FixedSizes-Muster bei
    // Neigung ueber die Soll-Flaeche hinaus. GetDefaults liefert den Dialog-Winkel
    // NICHT mit (live verifiziert: Dialog 135 -> erzeugt 90), darum immer explizit.
    const double angleRad = angle * 3.14159265358979323846 / 180.0;
    element.curtainWall.angle = angle;
    element.curtainWall.height = totalHeight * std::sin (angleRad);

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

    // Frame-Klassen: APICWFrameClass_Division (=1) ist eine RESERVIERTE Generik-
    // Klasse ohne Profil — Rahmen daraus haben Querschnitt 0 und sind unsichtbar.
    // Echte Profile beginnen bei APICWFrameClass_FirstCustomClass (=4), analog zu
    // den Paneelen. (Bug bis 0.9.7: alle Pfosten/Riegel unsichtbar, live 2026-07-25.)
    const UInt32 nFrameClasses = BMpGetSize (reinterpret_cast<GSPtr> (memo.cWallFrameDefaults)) / sizeof (API_CWFrameType);
    if (nFrameClasses == 0) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return CreateErrorResponse (APIERR_GENERAL, "CW-Defaults ohne Frame-Klassen — Favorit mit Pfosten-/Riegelprofil setzen");
    }
    element.curtainWall.nFrameDefaults = nFrameClasses;

    Int32 mullionClass = 0, transomClass = 0;
    parameters.Get ("mullionClass", mullionClass);
    parameters.Get ("transomClass", transomClass);
    if (mullionClass < 0 || (UInt32) mullionClass >= nFrameClasses ||
        transomClass < 0 || (UInt32) transomClass >= nFrameClasses) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return CreateErrorResponse (APIERR_BADPARS, "mullionClass/transomClass ausserhalb der vorhandenen Frame-Klassen");
    }
    const short mullionID = (short) (APICWFrameClass_FirstCustomClass + mullionClass);
    const short transomID = (short) (APICWFrameClass_FirstCustomClass + transomClass);

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
            cells[i].leftFrameID    = mullionID;    // senkrecht = Pfosten
            cells[i].bottomFrameID  = transomID;    // waagerecht = Riegel
            cells[i].crossingFrameID = mullionID;
        }
        BMpKill (reinterpret_cast<GSPtr*> (&memo.cWSegPatternCells));
        memo.cWSegPatternCells = cells;
    }

    GS::ObjectState response;
    err = APIERR_GENERAL;
    bool dragDone = false;
    ACAPI_CallUndoableCommand ("ELM_SAB CreateCurtainWallFromAxes", [&] () -> GSErrCode {
        err = ACAPI_Element_Create (&element, &memo);
        if (err != NoError)
            return err;
        // storyRelLevel wird beim Create ignoriert (live verifiziert 2026-07-25):
        // Hoehenlage per Drag nachziehen, sonst sitzt die Fassade auf Geschossniveau.
        if (std::fabs (bottomOffset) > 1e-9) {
            GS::Array<API_Neig> toEdit = { API_Neig (element.header.guid) };
            API_EditPars editPars = {};
            editPars.typeID = APIEdit_Drag;
            editPars.endC = API_Vector3D { 0.0, 0.0, bottomOffset };
            editPars.withDelete = true;
            const GSErrCode dragErr = ACAPI_Element_Edit (&toEdit, editPars);
            if (dragErr != NoError)
                return dragErr;
            dragDone = true;
        }
        return NoError;
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

    response.Add ("height", totalHeight);
    response.Add ("verticalHeight", totalHeight * std::sin (angleRad));
    response.Add ("angleApplied", angle);
    response.Add ("contourVertices", (Int32) contour.GetSize ());
    response.Add ("bottomOffsetApplied", dragDone || std::fabs (bottomOffset) <= 1e-9);

    // Ruecklese-Verifikation der tatsaechlichen Hoehenlage
    API_Elem_Head elemHead = {};
    elemHead.guid = element.header.guid;
    API_Box3D box3D = {};
    if (ACAPI_Element_CalcBounds (&elemHead, &box3D) == NoError) {
        response.Add ("zMin", box3D.zMin);
        response.Add ("zMax", box3D.zMax);
    }

    return response;
}
