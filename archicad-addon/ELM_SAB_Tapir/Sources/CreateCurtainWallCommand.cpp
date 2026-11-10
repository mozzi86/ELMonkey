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
            "angle": { "type": "number", "minimum": 0, "exclusiveMaximum": 180, "description": "Neigung in Grad, Konvention wie CW-Dialog und Tapir-GetDetailsOfElements: 90 = senkrecht, 135 = um 45 Grad zur Extrusionsseite gekippt, 0 = LIEGEND (Boden/Abhangdecke; v laeuft dann waagerecht nach links der Basislinie beg->end, height = in-plane). columnWidths/rowHeights/contour bleiben IN-PLANE-Masse; die vertikale Hoehe setzt der Befehl intern auf totalHeight*sin(angle). Optional, Default 90." },
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
            "transomClass": { "type": "integer", "minimum": 0, "description": "0-basierter Index in die Frame-Klassen der CW-Defaults fuer die WAAGERECHTEN Rahmen (Riegel). Default 0 = erste Klasse." },
            "planeMatrix": { "type": "array", "items": { "type": "number" }, "minItems": 12, "maxItems": 12, "description": "Optional curtainWall.planeMatrix (API_Tranmat.tmx, 12 Werte), z. B. aus GetCurtainWallPlaneELM einer liegenden Musterfassade. Archicad setzt angle 0 allein auf 90 zurueck (live 2026-11-09)." },
            "planeOffset": { "type": "number", "description": "Optional curtainWall.planeOffset." }
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
            "angleReadBack": { "type": "number" },
            "planeMatrixReadBack": { "type": "array", "items": { "type": "number" } },
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
    if (angle < 0.0 || angle >= 180.0)
        return CreateErrorResponse (APIERR_BADPARS, "angle muss in [0, 180) Grad liegen (90 = senkrecht, 0 = liegend)");
    // angle 0 = liegende Fassade (Bodenfliesen, Abhangdecke). Live-Test AC29 2026-11-09:
    // von Hand gezeichnete liegende CWs melden angle 0 und height = IN-PLANE-Ausdehnung.
    const bool lying = angle < 1e-9;

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
    element.curtainWall.height = lying ? totalHeight : totalHeight * std::sin (angleRad);
    GS::Array<double> planeMatrix;
    if (parameters.Get ("planeMatrix", planeMatrix)) {
        if (planeMatrix.GetSize () != 12) {
            ACAPI_DisposeElemMemoHdls (&memo);
            return CreateErrorResponse (APIERR_BADPARS, "planeMatrix braucht genau 12 Werte");
        }
        for (UIndex i = 0; i < 12; ++i)
            element.curtainWall.planeMatrix.tmx[i] = planeMatrix[i];
    }
    double planeOffset = 0.0;
    if (parameters.Get ("planeOffset", planeOffset))
        element.curtainWall.planeOffset = planeOffset;

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
    response.Add ("verticalHeight", lying ? 0.0 : totalHeight * std::sin (angleRad));
    response.Add ("angleApplied", angle);
    {
        API_Element created = {};
        created.header.guid = element.header.guid;
        if (ACAPI_Element_Get (&created) == NoError) {
            response.Add ("angleReadBack", created.curtainWall.angle);
            const auto& tm = response.AddList<double> ("planeMatrixReadBack");
            for (UIndex i = 0; i < 12; ++i)
                tm (created.curtainWall.planeMatrix.tmx[i]);
        }
    }
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


// ---------------------------------------------------------------------------
// GetCurtainWallPlaneELM (0.11.12.2) — Ebene (Neigung, planeMatrix, Kontur, Segmente) einer
// Fassade lesen. Anlass: liegende Fassaden (Bodenfliesen, Abhangdecken). Archicad
// setzt angle 0 beim Create auf 90 zurueck; liegende Fassaden haben eine eigene planeMatrix.
// ---------------------------------------------------------------------------
GS::Optional<GS::UniString> GetCurtainWallPlaneCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] }
                    },
                    "required": ["elementId"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> GetCurtainWallPlaneCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "curtainWalls": { "type": "array", "items": { "type": "object" } }
        },
        "required": ["curtainWalls"]
    })");
}

static void AddCwPlane (GS::ObjectState& os, const API_Element& element)
{
    os.Add ("angle", element.curtainWall.angle);
    os.Add ("height", element.curtainWall.height);
    os.Add ("planeOffset", element.curtainWall.planeOffset);
    os.Add ("storyRelLevel", element.curtainWall.storyRelLevel);
    os.Add ("flipped", element.curtainWall.flipped);
    os.Add ("floorIndex", (Int32) element.header.floorInd);
    const auto& tm = os.AddList<double> ("planeMatrix");
    for (UIndex i = 0; i < 12; ++i)
        tm (element.curtainWall.planeMatrix.tmx[i]);
}

GS::ObjectState GetCurtainWallPlaneCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& list = response.AddList<GS::ObjectState> ("curtainWalls");
    for (const GS::ObjectState& item : elements) {
        const GS::ObjectState* elementId = item.Get ("elementId");
        if (elementId == nullptr) {
            list (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
            continue;
        }
        API_Element element = {};
        element.header.guid = GetGuidFromObjectState (*elementId);
        const GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError) {
            list (CreateFailedExecutionResult (err, "Element nicht gefunden"));
            continue;
        }
        if (element.header.type.typeID != API_CurtainWallID) {
            list (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "keine Fassade"));
            continue;
        }
        GS::ObjectState os;
        os.Add ("success", true);
        AddCwPlane (os, element);
        os.Add ("polyNCoords", (Int32) element.curtainWall.polygon.nCoords);
        os.Add ("polyNSubPolys", (Int32) element.curtainWall.polygon.nSubPolys);
        os.Add ("polyNArcs", (Int32) element.curtainWall.polygon.nArcs);
        os.Add ("nSegments", (Int32) element.curtainWall.nSegments);
        API_ElementMemo memo = {};
        if (ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_Polygon | APIMemoMask_CWallSegments | APIMemoMask_CWSegContour) == NoError) {
            if (memo.cWSegContour != nullptr) {
                const UInt32 nc = (UInt32) (BMpGetSize (reinterpret_cast<GSPtr> (memo.cWSegContour)) / sizeof (API_CWContourData));
                const auto& cl = os.AddList<GS::ObjectState> ("segmentContours");
                for (UInt32 k = 0; k < nc; ++k) {
                    const API_CWContourData& cd = memo.cWSegContour[k];
                    GS::ObjectState co;
                    co.Add ("nCoords", (Int32) cd.polygon.nCoords);
                    if (cd.coords != nullptr) {
                        const auto& pts = co.AddList<GS::ObjectState> ("uv");
                        for (Int32 q = 1; q <= cd.polygon.nCoords; ++q) {
                            GS::ObjectState pt;
                            pt.Add ("u", (*cd.coords)[q].x);
                            pt.Add ("v", (*cd.coords)[q].y);
                            pts (pt);
                        }
                    }
                    cl (co);
                }
            }
            if (memo.coords != nullptr) {
                const Int32 n = (Int32) (BMhGetSize (reinterpret_cast<GSHandle> (memo.coords)) / sizeof (API_Coord));
                const auto& cl = os.AddList<GS::ObjectState> ("coords");
                for (Int32 k = 1; k < n; ++k) {
                    GS::ObjectState c;
                    c.Add ("x", (*memo.coords)[k].x);
                    c.Add ("y", (*memo.coords)[k].y);
                    cl (c);
                }
            }
            if (memo.pends != nullptr) {
                const Int32 n = (Int32) (BMhGetSize (reinterpret_cast<GSHandle> (memo.pends)) / sizeof (Int32));
                const auto& pl = os.AddList<Int32> ("pends");
                for (Int32 k = 0; k < n; ++k)
                    pl ((*memo.pends)[k]);
            }
            if (memo.cWallSegments != nullptr) {
                const UInt32 ns = (UInt32) (BMpGetSize (reinterpret_cast<GSPtr> (memo.cWallSegments)) / sizeof (API_CWSegmentType));
                const auto& sl = os.AddList<GS::ObjectState> ("segments");
                for (UInt32 k = 0; k < ns; ++k) {
                    GS::ObjectState so;
                    so.Add ("begX", memo.cWallSegments[k].begC.x);
                    so.Add ("begY", memo.cWallSegments[k].begC.y);
                    so.Add ("begZ", memo.cWallSegments[k].begC.z);
                    so.Add ("endX", memo.cWallSegments[k].endC.x);
                    so.Add ("endY", memo.cWallSegments[k].endC.y);
                    so.Add ("endZ", memo.cWallSegments[k].endC.z);
                    so.Add ("extX", memo.cWallSegments[k].extrusion.x);
                    so.Add ("extY", memo.cWallSegments[k].extrusion.y);
                    so.Add ("extZ", memo.cWallSegments[k].extrusion.z);
                    sl (so);
                }
            }
            ACAPI_DisposeElemMemoHdls (&memo);
        }
        list (os);
    }
    return response;
}

// SetCurtainWallSegmentELM (0.11.12.2): Kontur/Raster/Laenge/Hoehe einer BESTEHENDEN Fassade
// aendern. Anlass: liegende Fassaden lassen sich per Create nicht anlegen (Archicad setzt angle 0
// auf 90, planeMatrix -> APIERR_BADPARS, Ebene nachtraeglich setzen zerstoert das Element;
// Live-Test AC29 2026-11-09). Weg: liegendes Muster per MoveElements(copy) duplizieren, dann hiermit
// an den Raum anpassen. Raster wie DevKit Element_Test (CW-Memo), Kontur am Segment-Memo.
GS::Optional<GS::UniString> SetCurtainWallSegmentCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] },
            "contour": {
                "type": "array", "minItems": 3,
                "items": { "type": "object", "properties": { "u": { "type": "number" }, "v": { "type": "number" } }, "required": ["u", "v"] },
                "description": "Neue Segment-Kontur in Segment-Lokal-Koordinaten (u ab Basislinienstart, v quer dazu), Schlusspunkt nicht wiederholen."
            },
            "columnWidths": { "type": "array", "items": { "type": "number", "exclusiveMinimum": 0 }, "minItems": 1 },
            "rowHeights": { "type": "array", "items": { "type": "number", "exclusiveMinimum": 0 }, "minItems": 1 },
            "baseLine": {
                "type": "array", "minItems": 2, "maxItems": 2,
                "items": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "required": ["x", "y"] },
                "description": "Neue Basislinie (Anfang, Ende) in Weltkoordinaten wie GetCurtainWallPlaneELM.coords. Bei liegenden Fassaden: Raumkante, die Extrusion laeuft nach links."
            },
            "height": { "type": "number", "exclusiveMinimum": 0, "description": "Neue curtainWall.height (bei liegenden Fassaden = Ausdehnung quer zur Basislinie)." }
        },
        "required": ["elementId"]
    })");
}

GS::Optional<GS::UniString> SetCurtainWallSegmentCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "success": { "type": "boolean" },
            "step": { "type": "string" },
            "errorCode": { "type": "integer" },
            "nSegments": { "type": "integer" },
            "nPanels": { "type": "integer" },
            "nFrames": { "type": "integer" },
            "height": { "type": "number" },
            "contourVertices": { "type": "integer" },
            "error": { "type": "object" }
        }
    })");
}

static void ReplacePattern (API_CWSegmentPatternData& pat, const GS::Array<double>& sizes)
{
    pat.nPattern = (UInt32) sizes.GetSize ();
    pat.endWithID = pat.nPattern - 1;
    pat.logic = APICWSePL_FixedSizes;
    double* p = reinterpret_cast<double*> (BMpAll (sizeof (double) * pat.nPattern));
    for (UInt32 i = 0; i < pat.nPattern; ++i)
        p[i] = sizes[i];
    BMpKill (reinterpret_cast<GSPtr*> (&pat.pattern));
    pat.pattern = p;
}

GS::ObjectState SetCurtainWallSegmentCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    const GS::ObjectState* elementId = parameters.Get ("elementId");
    if (elementId == nullptr)
        return CreateErrorResponse (APIERR_BADPARS, "elementId fehlt");

    GS::Array<API_Coord> contour;
    {
        GS::Array<GS::ObjectState> contourOS;
        if (parameters.Get ("contour", contourOS)) {
            for (const GS::ObjectState& c : contourOS) {
                API_Coord pt = {};
                c.Get ("u", pt.x);
                c.Get ("v", pt.y);
                contour.Push (pt);
            }
        }
    }
    GS::Array<double> columnWidths, rowHeights;
    const bool hasCols = parameters.Get ("columnWidths", columnWidths) && !columnWidths.IsEmpty ();
    const bool hasRows = parameters.Get ("rowHeights", rowHeights) && !rowHeights.IsEmpty ();
    double height = 0.0;
    GS::Array<API_Coord> polygon;
    {
        GS::Array<GS::ObjectState> polyOS;
        if (parameters.Get ("baseLine", polyOS)) {
            for (const GS::ObjectState& c : polyOS) {
                API_Coord pt = {};
                c.Get ("x", pt.x);
                c.Get ("y", pt.y);
                polygon.Push (pt);
            }
        }
    }
    const bool hasPolygon = polygon.GetSize () == 2;
    const bool hasHeight = parameters.Get ("height", height);

    API_Element cw = {};
    cw.header.guid = GetGuidFromObjectState (*elementId);
    GSErrCode err = ACAPI_Element_Get (&cw);
    if (err != NoError || cw.header.type.typeID != API_CurtainWallID)
        return CreateErrorResponse (err != NoError ? err : APIERR_BADELEMENTTYPE, "Fassade nicht gefunden");

    const UInt64 cwMemoMask = APIMemoMask_Polygon | APIMemoMask_CWallSegments | APIMemoMask_CWSegContour |
                              APIMemoMask_CWSegPrimaryPattern | APIMemoMask_CWSegSecPattern | APIMemoMask_CWSegPanelPattern;
    API_ElementMemo cwMemo = {};
    err = ACAPI_Element_GetMemo (cw.header.guid, &cwMemo, cwMemoMask);
    if (err != NoError)
        return CreateErrorResponse (err, "GetMemo der Fassade fehlgeschlagen");
    const UInt32 nSeg = cwMemo.cWallSegments != nullptr ? (UInt32) (BMpGetSize (reinterpret_cast<GSPtr> (cwMemo.cWallSegments)) / sizeof (API_CWSegmentType)) : 0;
    if (nSeg != 1) {
        ACAPI_DisposeElemMemoHdls (&cwMemo);
        return CreateErrorResponse (APIERR_BADPARS, "nur Fassaden mit genau einem Segment");
    }

    GS::UniString step = "start";
    err = ACAPI_CallUndoableCommand ("ELM_SAB SetCurtainWallSegmentELM", [&] () -> GSErrCode {
        // 1) Fassade: Hoehe, Basislinienlaenge, Raster (wie DevKit Element_Test)
        if (hasHeight || hasPolygon || hasCols || hasRows || !contour.IsEmpty ()) {
            step = "curtainWall";
            API_Element mask;
            ACAPI_ELEMENT_MASK_CLEAR (mask);
            UInt64 changeMask = 0;
            if (hasHeight) {
                cw.curtainWall.height = height;
                ACAPI_ELEMENT_MASK_SET (mask, API_CurtainWallType, height);
            }
            if (hasPolygon && cwMemo.coords != nullptr) {
                // Basislinie = offene Polylinie mit 2 Punkten (Index-1-Konvention), live: polyNCoords 2, pends [0,2]
                (*cwMemo.coords)[1] = polygon[0];
                (*cwMemo.coords)[2] = polygon[1];
                changeMask |= APIMemoMask_Polygon;
            }
            if (!contour.IsEmpty ()) {
                // Kontur ueber das CW-Memo, wie beim Create (am Segment-Element wirkt sie nicht, live 2026-11-09)
                if (cwMemo.cWSegContour != nullptr)
                    ACAPI_FreeCWContourPtr (&cwMemo.cWSegContour);
                if (!FillSegmentContour (cwMemo, contour))
                    return APIERR_MEMFULL;
                changeMask |= APIMemoMask_CWSegContour;
            }
            if (hasCols || hasRows) {
                const API_CWSegmentPatternCellData tmpl = cwMemo.cWSegPatternCells != nullptr ? cwMemo.cWSegPatternCells[0] : API_CWSegmentPatternCellData {};
                if (hasCols)
                    ReplacePattern (cwMemo.cWSegPrimaryPattern, columnWidths);
                if (hasRows)
                    ReplacePattern (cwMemo.cWSegSecondaryPattern, rowHeights);
                const UInt32 nCells = cwMemo.cWSegPrimaryPattern.nPattern * cwMemo.cWSegSecondaryPattern.nPattern;
                API_CWSegmentPatternCellData* cells = reinterpret_cast<API_CWSegmentPatternCellData*> (BMpAllClear (nCells * sizeof (API_CWSegmentPatternCellData)));
                for (UInt32 i = 0; i < nCells; ++i)
                    cells[i] = tmpl;
                BMpKill (reinterpret_cast<GSPtr*> (&cwMemo.cWSegPatternCells));
                cwMemo.cWSegPatternCells = cells;
                changeMask |= APIMemoMask_CWSegPrimaryPattern | APIMemoMask_CWSegSecPattern | APIMemoMask_CWSegPanelPattern;
            }
            const GSErrCode e1 = ACAPI_Element_Change (&cw, &mask, changeMask != 0 ? &cwMemo : nullptr, changeMask, true);
            if (e1 != NoError)
                return e1;
        }
        step = "done";
        return NoError;
    });
    ACAPI_DisposeElemMemoHdls (&cwMemo);

    GS::ObjectState response;
    response.Add ("success", err == NoError);
    response.Add ("step", step);
    if (err != NoError)
        response.Add ("errorCode", (Int32) err);
    API_Element after = {};
    after.header.guid = cw.header.guid;
    if (ACAPI_Element_Get (&after) == NoError) {
        response.Add ("nSegments", (Int32) after.curtainWall.nSegments);
        response.Add ("nPanels", (Int32) after.curtainWall.nPanels);
        response.Add ("nFrames", (Int32) after.curtainWall.nFrames);
        response.Add ("height", after.curtainWall.height);
    }
    response.Add ("contourVertices", (Int32) contour.GetSize ());
    return response;
}
