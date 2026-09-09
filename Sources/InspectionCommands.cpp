#include "InspectionCommands.hpp"
#include "MigrationHelper.hpp"
#include "AddOnVersion.hpp"

#include "DGModule.hpp"
#include "DGDialog.hpp"

#include <cmath>
#include <map>
#include <vector>

namespace {

// --- gemeinsame Helfer -------------------------------------------------------

GS::UniString UserNameOf (short userId)
{
    if (userId == 0 || !ACAPI_Teamwork_HasConnection ()) {
        return GS::UniString ();
    }
    GS::UniString name;
    if (ACAPI_Teamwork_GetUsernameFromId (userId, &name) != NoError) {
        return GS::UniString ();
    }
    return name;
}

GS::UniString WindowTypeName (API_WindowTypeID type)
{
    switch (type) {
        case APIWind_FloorPlanID:          return "FloorPlan";
        case APIWind_SectionID:            return "Section";
        case APIWind_DetailID:             return "Details";
        case APIWind_3DModelID:            return "3DModel";
        case APIWind_LayoutID:             return "Layout";
        case APIWind_DrawingID:            return "Drawing";
        case APIWind_MyTextID:             return "CustomText";
        case APIWind_MyDrawID:             return "CustomDraw";
        case APIWind_MasterLayoutID:       return "MasterLayout";
        case APIWind_ElevationID:          return "Elevation";
        case APIWind_InteriorElevationID:  return "InteriorElevation";
        case APIWind_WorksheetID:          return "Worksheet";
        case APIWind_ReportID:             return "Report";
        case APIWind_DocumentFrom3DID:     return "3DDocument";
        case APIWind_External3DID:         return "External3D";
        case APIWind_Movie3DID:            return "Movie3D";
        case APIWind_MovieRenderingID:     return "MovieRendering";
        case APIWind_RenderingID:          return "Rendering";
        case APIWind_ModelCompareID:       return "ModelCompare";
        case APIWind_IESCommonDrawingID:   return "InteractiveSchedule";
        default:                           return "Unknown";
    }
}

// Der Bibliotheksteil-Index je Elementtyp — dieselbe Liste wie in Tapirs
// ElementGDLParameterCommands. -1 = Element ist nicht bibliotheksteilbasiert.
Int32 LibIndOf (const API_Element& e)
{
    switch (GetElemTypeId (e.header)) {
        case API_ObjectID:            return e.object.libInd;
        case API_LampID:              return e.lamp.libInd;
        case API_WindowID:            return e.window.openingBase.libInd;
        case API_DoorID:              return e.door.openingBase.libInd;
        case API_SkylightID:          return e.skylight.openingBase.libInd;
        case API_ZoneID:              return e.zone.libInd;
        case API_LabelID:             return e.label.u.symbol.libInd;
        case API_CurtainWallFrameID:  return e.cwFrame.libInd;
        case API_CurtainWallPanelID:  return e.cwPanel.libInd;
        case API_CurtainWallJunctionID:  return e.cwJunction.libInd;
        case API_CurtainWallAccessoryID: return e.cwAccessory.libInd;
        default:                      return -1;
    }
}

struct Dims {
    bool          valid = false;
    double        x = 0.0, y = 0.0, z = 0.0;
    GS::UniString source;
};

// Ist-Masse eines Elements. Fuer Objekte/Lampen sind xRatio/yRatio die Masse, mit
// denen der Nutzer und die GDL-Parameter A/B arbeiten — die 3D-Huelle waere bei
// gedrehten oder ueberstehenden Symbolen etwas anderes. Alle uebrigen Typen bekommen
// die Ausdehnung der 3D-Huelle; `source` sagt immer, woher die Zahlen kommen.
Dims IstDims (const API_Element& e, const API_Box3D& box, bool boxValid)
{
    Dims d;
    const API_ElemTypeID t = GetElemTypeId (e.header);
    if (t == API_ObjectID || t == API_LampID) {
        d.valid  = true;
        d.x      = e.object.xRatio;
        d.y      = e.object.yRatio;
        d.z      = boxValid ? (box.zMax - box.zMin) : 0.0;
        d.source = boxValid ? "objectRatiosAndBoundingBox" : "objectRatios";
        return d;
    }
    if (t == API_WallID) {
        d.valid  = true;
        const double dx = e.wall.endC.x - e.wall.begC.x;
        const double dy = e.wall.endC.y - e.wall.begC.y;
        d.x      = std::sqrt (dx * dx + dy * dy);   // Achslaenge
        d.y      = e.wall.thickness;
        d.z      = e.wall.height;
        d.source = "wallLengthThicknessHeight";
        return d;
    }
    if (boxValid) {
        d.valid  = true;
        d.x      = box.xMax - box.xMin;
        d.y      = box.yMax - box.yMin;
        d.z      = box.zMax - box.zMin;
        d.source = "boundingBox3D";
    }
    return d;
}

} // namespace

// =============================================================================
// GetElementEditState
// =============================================================================

GS::Optional<GS::UniString> GetElementEditStateCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "items": { "type": "object",
                "properties": { "elementId": { "type": "object",
                    "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                "required": ["elementId"] } }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> GetElementEditStateCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "editStates": { "type": "array", "items": { "type": "object" } },
            "editableCount": { "type": "integer" },
            "elementCount": { "type": "integer" }
        },
        "required": ["editStates"]
    })");
}

GS::ObjectState GetElementEditStateCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    const bool teamwork = ACAPI_Teamwork_HasConnection ();
    short myUserId = 0;
    {
        API_ProjectInfo projectInfo = {};
        if (ACAPI_ProjectOperation_Project (&projectInfo) == NoError) {
            myUserId = projectInfo.userId;
        }
        // API_ProjectInfo-Zeiger werden bewusst NICHT freigegeben — Tapir macht es an
        // allen anderen Stellen genauso; die Besitzverhaeltnisse sagt das DevKit nicht zu.
    }

    // Ebenen-Zustaende nur einmal je Ebene holen.
    std::map<Int32, std::pair<bool, bool>> layerCache;   // index -> (hidden, locked)
    std::map<Int32, GS::UniString>         layerNames;

    Int32 editableCount = 0;
    GS::ObjectState response;
    const auto& out = response.AddList<GS::ObjectState> ("editStates");

    for (const GS::ObjectState& item : elements) {
        const API_Guid guid = GetGuidFromElementsArrayItem (item);
        GS::ObjectState o;
        o.Add ("elementId", CreateGuidObjectState (guid));

        if (guid == APINULLGuid) {
            o.Add ("exists", false);
            o.Add ("editable", false);
            o.Add ("reason", GS::UniString ("badElementId"));
            out (o);
            continue;
        }

        API_Element element = {};
        element.header.guid = guid;
        if (ACAPI_Element_Get (&element) != NoError) {
            o.Add ("exists", false);
            o.Add ("editable", false);
            o.Add ("reason", GS::UniString ("notFound"));
            out (o);
            continue;
        }

        const API_Elem_Head& h = element.header;
        o.Add ("exists", true);
        o.Add ("elemType", GetElementTypeNonLocalizedName (GetElemTypeId (h)));
        o.Add ("floorIndex", (Int32) h.floorInd);

        const Int32 layerIndex = GetAttributeIndex (h.layer);
        o.Add ("layerIndex", layerIndex);
        if (layerCache.find (layerIndex) == layerCache.end ()) {
            API_Attribute layerAttr = {};
            layerAttr.header.typeID = API_LayerID;
            layerAttr.header.index  = h.layer;
            if (ACAPI_Attribute_Get (&layerAttr) == NoError) {
                layerCache[layerIndex] = std::make_pair ((layerAttr.header.flags & APILay_Hidden) != 0,
                                                         (layerAttr.header.flags & APILay_Locked) != 0);
                layerNames[layerIndex] = GS::UniString (layerAttr.header.name);
            } else {
                layerCache[layerIndex] = std::make_pair (false, false);
                layerNames[layerIndex] = GS::UniString ();
            }
        }
        const bool layerHidden = layerCache[layerIndex].first;
        const bool layerLocked = layerCache[layerIndex].second;
        o.Add ("layerName",   layerNames[layerIndex]);
        o.Add ("layerHidden", layerHidden);
        o.Add ("layerLocked", layerLocked);

        // In Nicht-Teamwork ist lockId der Sperr-Schalter des Elements, in Teamwork
        // die Nutzer-ID des Reservierenden (DevKit-Doku zu API_Elem_Head).
        const bool isLocked = !teamwork && h.lockId != 0;
        o.Add ("isLocked", isLocked);

        const bool inGroup = h.groupGuid != APINULLGuid;
        o.Add ("inGroup", inGroup);
        if (inGroup) {
            o.Add ("groupId", CreateGuidObjectState (h.groupGuid));
        }

        const bool inHotlink = h.hotlinkGuid != APINULLGuid;
        o.Add ("hotlink", inHotlink);
        if (inHotlink) {
            o.Add ("hotlinkId", CreateGuidObjectState (h.hotlinkGuid));
        }

        // Teamwork
        o.Add ("teamwork", teamwork);
        bool reservedByOther = false;
        if (teamwork) {
            o.Add ("ownerUserId",   (Int32) h.userId);
            o.Add ("ownerUserName", UserNameOf (h.userId));
            o.Add ("lockUserId",    (Int32) h.lockId);
            o.Add ("reservedByUser", UserNameOf (h.lockId));
            const bool reservedByMe = (h.lockId != 0 && h.lockId == myUserId);
            reservedByOther = (h.lockId != 0 && h.lockId != myUserId);
            o.Add ("isReservedByMe", reservedByMe);
            o.Add ("hasDeleteModifyRight", ACAPI_Teamwork_HasDeleteModifyRight (guid));
        }

        // Bibliotheksteil
        const Int32 libInd = LibIndOf (element);
        bool libPartMissing = false;
        if (libInd > 0) {
            API_LibPart lp = {};
            lp.index = libInd;
            const GSErrCode lpErr = ACAPI_LibraryPart_Get (&lp);
            if (lpErr != NoError) {
                libPartMissing = true;
                o.Add ("libPartError", (Int32) lpErr);
            } else {
                libPartMissing = lp.missingDef;
                o.Add ("libPartName", GS::UniString (lp.docu_UName));
            }
            o.Add ("libPartMissing", libPartMissing);
        }

        // APIFilt-Flags werden MITGELIEFERT, aber NICHT fuer das Urteil benutzt:
        // am THN meldeten IsEditable/InMyWorkspace auch fuer nachweislich
        // aenderbare Elemente false (reference/mcp-extension.md, 2026-09-08).
        o.Add ("filterFlags", GS::ObjectState (
            "isEditable",      ACAPI_Element_Filter (guid, APIFilt_IsEditable),
            "onVisibleLayer",  ACAPI_Element_Filter (guid, APIFilt_OnVisLayer),
            "inMyWorkspace",   ACAPI_Element_Filter (guid, APIFilt_InMyWorkspace),
            "hasAccessRight",  ACAPI_Element_Filter (guid, APIFilt_HasAccessRight)));

        GS::UniString reason;
        if (layerHidden)          reason = "layerHidden";
        else if (layerLocked)     reason = "layerLocked";
        else if (isLocked)        reason = "elementLocked";
        else if (inHotlink)       reason = "hotlink";
        else if (reservedByOther) reason = GS::UniString ("reservedByUser:") + UserNameOf (h.lockId);
        else if (libPartMissing)  reason = "libraryPartMissing";

        const bool editable = reason.IsEmpty ();
        o.Add ("editable", editable);
        o.Add ("reason", editable ? GS::UniString ("editable") : reason);
        if (editable && inGroup) {
            // Gruppen blockieren nicht die API, wohl aber die Bearbeitung in der UI.
            o.Add ("note", GS::UniString ("inGroup: API-Aenderung geht, UI-Bearbeitung braucht Gruppen aussetzen."));
        }
        if (editable) {
            ++editableCount;
        }
        out (o);
    }

    response.Add ("editableCount", editableCount);
    response.Add ("elementCount",  (Int32) elements.GetSize ());
    return response;
}

// =============================================================================
// GetUIState
// =============================================================================

GS::Optional<GS::UniString> GetUIStateCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "modalDialogOpen": { "type": "boolean" },
            "modalDialogCount": { "type": "integer" },
            "modelessDialogCount": { "type": "integer" },
            "currentWindow": { "type": "object" },
            "teamwork": { "type": "object" },
            "project": { "type": "object" },
            "elmSabVersion": { "type": "string" }
        },
        "required": ["modalDialogOpen", "modalDialogCount"]
    })");
}

GS::ObjectState GetUIStateCommand::Execute (const GS::ObjectState&, GS::ProcessControl&) const
{
    GS::ObjectState response;

    // Modale Dialoge zaehlen. Der TITEL ist nicht abrufbar: DG::Dialog::GetTitle ist
    // im DevKit protected, es gibt keinen oeffentlichen Weg an den Text eines
    // fremden Dialogs. Deshalb nur die Anzahl — nicht erfunden, sondern die Grenze.
    Int32 modalCount = 0;
    for (DG::ModalDialog* d = DG::GetFirstModalDialog (); d != nullptr; d = d->GetNextModalDialog ()) {
        ++modalCount;
        if (modalCount > 64) {
            break;   // Schutz gegen eine kaputte Verkettung
        }
    }
    Int32 modelessCount = 0;
    for (DG::ModelessDialog* d = DG::GetFirstModelessDialog (); d != nullptr; d = d->GetNextModelessDialog ()) {
        ++modelessCount;
        if (modelessCount > 256) {
            break;
        }
    }
    response.Add ("modalDialogOpen",     modalCount > 0);
    response.Add ("modalDialogCount",    modalCount);
    response.Add ("modelessDialogCount", modelessCount);

    API_WindowInfo windowInfo = {};
    if (ACAPI_Window_GetCurrentWindow (&windowInfo) == NoError) {
        response.Add ("currentWindow", GS::ObjectState (
            "type",  WindowTypeName (windowInfo.typeID),
            "title", GS::UniString (windowInfo.title),
            "name",  GS::UniString (windowInfo.name),
            "index", (Int32) windowInfo.index));
    } else {
        response.Add ("currentWindow", GS::ObjectState ("type", GS::UniString ("Unavailable")));
    }

    API_ProjectInfo projectInfo = {};
    const GSErrCode projErr = ACAPI_ProjectOperation_Project (&projectInfo);
    if (projErr == NoError) {
        GS::ObjectState project;
        project.Add ("untitled", projectInfo.untitled);
        project.Add ("name", projectInfo.projectName != nullptr ? *projectInfo.projectName : GS::UniString ());
        project.Add ("path", projectInfo.projectPath != nullptr ? *projectInfo.projectPath : GS::UniString ());
        response.Add ("project", project);

        GS::ObjectState tw;
        tw.Add ("isTeamworkProject", projectInfo.teamwork);
        tw.Add ("hasConnection",     ACAPI_Teamwork_HasConnection ());
        tw.Add ("isOnline",          ACAPI_Teamwork_IsOnline ());
        tw.Add ("userId",            (Int32) projectInfo.userId);
        tw.Add ("userName",          UserNameOf (projectInfo.userId));
        tw.Add ("workGroupMode",     projectInfo.workGroupMode);
        response.Add ("teamwork", tw);
    } else {
        response.Add ("project", GS::ObjectState ("error", (Int32) projErr));
    }
    response.Add ("elmSabVersion", GS::UniString (ELM_SAB_VERSION));
    return response;
}

// =============================================================================
// GetDeviations
// =============================================================================

GS::Optional<GS::UniString> GetDeviationsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "items": { "type": "object",
                "properties": { "elementId": { "type": "object",
                    "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                "required": ["elementId"] },
                "description": "Optional. Fehlt die Liste, werden alle Elemente aus expected geprueft." },
            "expected": { "type": "array", "items": { "type": "object",
                "properties": {
                    "guid": { "type": "string" },
                    "elementId": { "type": "object",
                        "properties": { "guid": { "type": "string" } }, "required": ["guid"] },
                    "dims":   { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3,
                        "description": "Sollmasse [x, y, z] in METERN." },
                    "dimsMm": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3,
                        "description": "Sollmasse [x, y, z] in MILLIMETERN — Alternative zu dims." } } },
                "description": "Sollmasse je Element. Elemente ohne Eintrag bekommen nur ihre Ist-Masse gemeldet." },
            "toleranceMm": { "type": "number", "minimum": 0,
                "description": "Erlaubte Abweichung je Achse in mm. Default 1.0." }
        },
        "additionalProperties": false
    })");
}

GS::Optional<GS::UniString> GetDeviationsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "deviations": { "type": "array", "items": { "type": "object" } },
            "elementCount": { "type": "integer" },
            "checkedCount": { "type": "integer" },
            "deviatingCount": { "type": "integer" },
            "toleranceMm": { "type": "number" }
        },
        "required": ["deviations"]
    })");
}

GS::ObjectState GetDeviationsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    double toleranceMm = 1.0;
    parameters.Get ("toleranceMm", toleranceMm);
    if (toleranceMm < 0.0) {
        toleranceMm = 0.0;
    }
    const double tolerance = toleranceMm / 1000.0;

    // Sollmasse einsammeln — guid ODER elementId.guid, dims (m) ODER dimsMm (mm).
    struct Expected { double x = 0.0, y = 0.0, z = 0.0; };
    std::map<GS::UniString, Expected> expectedByGuid;
    std::vector<GS::UniString>        expectedOrder;

    GS::Array<GS::ObjectState> expectedArray;
    parameters.Get ("expected", expectedArray);
    for (const GS::ObjectState& item : expectedArray) {
        API_Guid g = APINULLGuid;
        GS::UniString guidStr;
        if (item.Get ("guid", guidStr)) {
            g = APIGuidFromString (guidStr.ToCStr ().Get ());
        } else {
            g = GetGuidFromElementsArrayItem (item);
        }
        if (g == APINULLGuid) {
            continue;
        }

        GS::Array<double> dims;
        Expected exp;
        bool haveDims = false;
        if (item.Get ("dims", dims) && dims.GetSize () >= 3) {
            exp.x = dims[0]; exp.y = dims[1]; exp.z = dims[2];
            haveDims = true;
        } else if (item.Get ("dimsMm", dims) && dims.GetSize () >= 3) {
            exp.x = dims[0] / 1000.0; exp.y = dims[1] / 1000.0; exp.z = dims[2] / 1000.0;
            haveDims = true;
        }
        if (!haveDims) {
            continue;
        }
        const GS::UniString key = APIGuidToString (g);
        if (expectedByGuid.find (key) == expectedByGuid.end ()) {
            expectedOrder.push_back (key);
        }
        expectedByGuid[key] = exp;
    }

    // Zu pruefende Elemente: `elements` wenn gegeben, sonst alle aus `expected`.
    std::vector<API_Guid> guids;
    GS::Array<GS::ObjectState> elements;
    if (parameters.Get ("elements", elements) && !elements.IsEmpty ()) {
        for (const GS::ObjectState& item : elements) {
            guids.push_back (GetGuidFromElementsArrayItem (item));
        }
    } else {
        for (const GS::UniString& key : expectedOrder) {
            guids.push_back (APIGuidFromString (key.ToCStr ().Get ()));
        }
    }

    Int32 checkedCount = 0;
    Int32 deviatingCount = 0;
    GS::ObjectState response;
    const auto& out = response.AddList<GS::ObjectState> ("deviations");

    for (const API_Guid& guid : guids) {
        GS::ObjectState o;
        o.Add ("elementId", CreateGuidObjectState (guid));

        API_Element element = {};
        element.header.guid = guid;
        if (guid == APINULLGuid || ACAPI_Element_Get (&element) != NoError) {
            o.Add ("exists", false);
            o.Add ("error", GS::UniString ("Element nicht lesbar."));
            out (o);
            continue;
        }
        o.Add ("exists", true);
        o.Add ("elemType", GetElementTypeNonLocalizedName (GetElemTypeId (element.header)));

        API_Box3D box = {};
        const bool boxValid = (ACAPI_Element_CalcBounds (&element.header, &box) == NoError);
        if (boxValid) {
            o.Add ("boundingBox3D", GS::ObjectState (
                "xMin", box.xMin, "yMin", box.yMin, "zMin", box.zMin,
                "xMax", box.xMax, "yMax", box.yMax, "zMax", box.zMax));
        }

        const Dims ist = IstDims (element, box, boxValid);
        if (!ist.valid) {
            o.Add ("error", GS::UniString ("Ist-Masse nicht bestimmbar (keine 3D-Huelle)."));
            out (o);
            continue;
        }
        const auto& actual = o.AddList<double> ("dims");
        actual (ist.x); actual (ist.y); actual (ist.z);
        o.Add ("dimsSource", ist.source);

        const GS::UniString key = APIGuidToString (guid);
        const auto found = expectedByGuid.find (key);
        if (found == expectedByGuid.end ()) {
            o.Add ("checked", false);
            o.Add ("note", GS::UniString ("Kein Sollmass uebergeben — nur Ist-Masse."));
            out (o);
            continue;
        }

        ++checkedCount;
        const Expected& exp = found->second;
        const auto& expList = o.AddList<double> ("expectedDims");
        expList (exp.x); expList (exp.y); expList (exp.z);

        const double dx = ist.x - exp.x;
        const double dy = ist.y - exp.y;
        const double dz = ist.z - exp.z;
        const auto& devList = o.AddList<double> ("deviationMm");
        devList (dx * 1000.0); devList (dy * 1000.0); devList (dz * 1000.0);
        const double maxDev = std::fmax (std::fabs (dx), std::fmax (std::fabs (dy), std::fabs (dz)));
        o.Add ("maxDeviationMm", maxDev * 1000.0);

        const bool within = (std::fabs (dx) <= tolerance) && (std::fabs (dy) <= tolerance) &&
                            (std::fabs (dz) <= tolerance);
        o.Add ("checked", true);
        o.Add ("withinTolerance", within);
        if (!within) {
            ++deviatingCount;
            GS::UniString axes;
            if (std::fabs (dx) > tolerance) axes.Append ("x");
            if (std::fabs (dy) > tolerance) axes.Append ("y");
            if (std::fabs (dz) > tolerance) axes.Append ("z");
            o.Add ("deviatingAxes", axes);
        }
        out (o);
    }

    response.Add ("elementCount",   (Int32) guids.size ());
    response.Add ("checkedCount",   checkedCount);
    response.Add ("deviatingCount", deviatingCount);
    response.Add ("toleranceMm",    toleranceMm);
    return response;
}
