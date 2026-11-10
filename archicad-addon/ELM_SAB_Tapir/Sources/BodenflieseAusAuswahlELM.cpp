#include "BodenflieseAusAuswahlELM.hpp"

#include "CommandBase.hpp"
#include "MigrationHelper.hpp"
#include "ElementCreationCommands.hpp"
#include "ElementGDLParameterCommands.hpp"
#include "GSProcessControl.hpp"
#include "DGModule.hpp"

#include <cmath>
#include <set>
#include <string>

namespace {

const char*  kLibPartName  = "SAB Bodenfliese liegend";
const Int32  kMaxCorners   = 64;

struct Outline {
    API_Guid                 source = APINULLGuid;
    short                    floorInd = 0;
    GS::Array<API_Coord>     pts;
    bool                     hadArcs = false;
};

bool SamePoint (const API_Coord& a, const API_Coord& b)
{
    return std::fabs (a.x - b.x) < 1e-6 && std::fabs (a.y - b.y) < 1e-6;
}

// Erste (aeussere) Kontur eines Polygons aus memo.coords/pends, ohne Schlusspunkt.
void FirstContour (const API_ElementMemo& memo, Int32 nCoords, GS::Array<API_Coord>& out)
{
    if (memo.coords == nullptr || nCoords < 3) return;
    Int32 last = nCoords;
    if (memo.pends != nullptr) {
        const Int32 pend1 = (*memo.pends)[1];
        if (pend1 > 1 && pend1 <= nCoords) last = pend1;
    }
    for (Int32 i = 1; i <= last; ++i)
        out.Push ((*memo.coords)[i]);
    while (out.GetSize () > 1 && SamePoint (out[0], out[out.GetSize () - 1]))
        out.Pop ();
}

bool ReadOutline (const API_Guid& guid, Outline& o, GS::UniString& why)
{
    API_Element element = {};
    element.header.guid = guid;
    if (ACAPI_Element_Get (&element) != NoError) { why = "nicht lesbar"; return false; }

    const API_ElemTypeID typeId = GetElemTypeId (element.header);
    if (typeId != API_PolyLineID && typeId != API_HatchID && typeId != API_SlabID && typeId != API_ZoneID) {
        why = "kein Umriss-Element (Polylinie, Schraffur, Decke oder Zone)";
        return false;
    }

    API_ElementMemo memo = {};
    if (ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_Polygon) != NoError) { why = "Umriss nicht lesbar"; return false; }

    o.source = guid;
    o.floorInd = element.header.floorInd;
    switch (typeId) {
        case API_PolyLineID: {
            const Int32 n = element.polyLine.poly.nCoords;
            o.hadArcs = element.polyLine.poly.nArcs > 0;
            if (memo.coords != nullptr && n >= 3) {
                if (!SamePoint ((*memo.coords)[1], (*memo.coords)[n])) {
                    why = "Polylinie ist nicht geschlossen";
                    ACAPI_DisposeElemMemoHdls (&memo);
                    return false;
                }
                for (Int32 i = 1; i <= n; ++i) o.pts.Push ((*memo.coords)[i]);
                while (o.pts.GetSize () > 1 && SamePoint (o.pts[0], o.pts[o.pts.GetSize () - 1])) o.pts.Pop ();
            }
            break;
        }
        case API_HatchID: FirstContour (memo, element.hatch.poly.nCoords, o.pts); o.hadArcs = element.hatch.poly.nArcs > 0; break;
        case API_SlabID:  FirstContour (memo, element.slab.poly.nCoords, o.pts);  o.hadArcs = element.slab.poly.nArcs > 0;  break;
        case API_ZoneID:  FirstContour (memo, element.zone.poly.nCoords, o.pts);  o.hadArcs = element.zone.poly.nArcs > 0;  break;
        default: break;
    }
    ACAPI_DisposeElemMemoHdls (&memo);

    if (o.pts.GetSize () < 3) { why = "Umriss hat weniger als 3 Ecken"; return false; }
    if ((Int32) o.pts.GetSize () > kMaxCorners) { why = GS::UniString::Printf ("Umriss hat mehr als %d Ecken", (int) kMaxCorners); return false; }
    return true;
}

// Werkzeug-Standard: wenn die Bodenfliese im Objektwerkzeug eingestellt ist,
// deren Parameter (ohne Geometrie/Mengen) uebernehmen; Hoehe und Ebene ebenso.
struct ToolDefaults {
    bool                         isOurs = false;
    double                       level = 0.0;
    Int32                        layer = 0;
    GS::Array<GS::ObjectState>   params;
};

ToolDefaults ReadToolDefaults (Int32 ourLibInd)
{
    ToolDefaults td;
    API_Element def = {};
    def.header.type = API_ObjectID;
    API_ElementMemo memo = {};
    if (ACAPI_Element_GetDefaults (&def, &memo) != NoError) return td;

    td.layer = GetAttributeIndex (def.header.layer);
    if (def.object.libInd == ourLibInd) {
        td.isOurs = true;
        td.level = def.object.level;
        static const std::set<std::string> skip = {
            "A", "B", "ZZYZX", "AC_show2DHotspotsIn3D", "ac_bottomlevel", "ac_toplevel",
            "nP", "nPOld", "px", "py", "offX", "offY", "gridAng",
            "qArea", "qTilesWhole", "qTilesCut", "qTilesTotal", "qJointLen" };
        if (memo.params != nullptr) {
            const Int32 n = (Int32) (BMGetHandleSize ((GSHandle) memo.params) / sizeof (API_AddParType));
            for (Int32 i = 0; i < n; ++i) {
                const API_AddParType& p = (*memo.params)[i];
                if (p.typeMod == API_ParArray) continue;
                if (skip.count (p.name) > 0) continue;
                GS::ObjectState os;
                os.Add ("name", GS::UniString (p.name));
                switch (p.typeID) {
                    case APIParT_Length:
                    case APIParT_RealNum:
                        os.Add ("value", p.value.real); break;
                    case APIParT_Integer:
                    case APIParT_PenCol:
                    case APIParT_LineTyp:
                    case APIParT_Mater:
                    case APIParT_FillPat:
                    case APIParT_BuildingMaterial:
                        os.Add ("value", (Int32) std::lround (p.value.real)); break;
                    case APIParT_Boolean:
                        os.Add ("value", p.value.real != 0.0); break;
                    case APIParT_CString:
                        os.Add ("value", GS::UniString (p.value.uStr)); break;
                    default:
                        continue;
                }
                td.params.Push (os);
            }
        }
    }
    ACAPI_DisposeElemMemoHdls (&memo);
    return td;
}

bool CreateFloor (const Outline& o, const ToolDefaults& td, API_Guid& newGuid, GS::UniString& why)
{
    const API_Coord p0 = o.pts[0];
    const API_Coord p1 = o.pts[1];
    const double angDeg = std::atan2 (p1.y - p0.y, p1.x - p0.x) * 180.0 / 3.14159265358979323846;

    // 1) Objekt an der ersten Ecke anlegen (bewaehrter Weg von CreateObjects)
    GS::ObjectState createParams;
    {
        const auto& list = createParams.AddList<GS::ObjectState> ("objectsData");
        GS::ObjectState item;
        item.Add ("libraryPartName", GS::UniString (kLibPartName));
        item.Add ("coordinates", GS::ObjectState ("x", p0.x, "y", p0.y, "z", td.isOurs ? td.level : 0.0));
        item.Add ("zReference", GS::UniString ("story"));
        item.Add ("floorIndex", (Int32) o.floorInd);
        item.Add ("angle", 0.0);
        if (td.layer > 0) item.Add ("layerIndex", td.layer);
        list (item);
    }
    GS::NonInterruptibleProcessControl pc;
    const GS::ObjectState createResp = CreateObjectsCommand ().Execute (createParams, pc);
    GS::Array<GS::ObjectState> created;
    createResp.Get ("elements", created);
    if (created.IsEmpty ()) { why = "Objekt konnte nicht angelegt werden (ist „SAB Bodenfliese liegend“ geladen?)"; return false; }
    const GS::ObjectState* elementId = created[0].Get ("elementId");
    if (elementId == nullptr) { why = "Objekt konnte nicht angelegt werden"; return false; }
    newGuid = GetGuidFromObjectState (*elementId);

    // 2) Umriss, Startpunkt, Raster und Werkzeug-Einstellungen setzen
    GS::ObjectState setParams;
    {
        const auto& list = setParams.AddList<GS::ObjectState> ("elementsWithGDLParameters");
        GS::ObjectState item;
        item.Add ("elementId", CreateGuidObjectState (newGuid));
        const auto& gdl = item.AddList<GS::ObjectState> ("gdlParameters");
        for (const GS::ObjectState& p : td.params) gdl (p);
        const Int32 n = (Int32) o.pts.GetSize ();
        gdl (GS::ObjectState ("name", GS::UniString ("nP"), "value", n));
        gdl (GS::ObjectState ("name", GS::UniString ("nPOld"), "value", n));
        GS::ObjectState pxOs ("name", GS::UniString ("px"));
        GS::ObjectState pyOs ("name", GS::UniString ("py"));
        const auto& pxl = pxOs.AddList<double> ("value");
        const auto& pyl = pyOs.AddList<double> ("value");
        for (Int32 i = 0; i < kMaxCorners; ++i) {
            pxl (i < n ? o.pts[i].x - p0.x : 0.0);
            pyl (i < n ? o.pts[i].y - p0.y : 0.0);
        }
        gdl (pxOs);
        gdl (pyOs);
        gdl (GS::ObjectState ("name", GS::UniString ("offX"), "value", 0.0));
        gdl (GS::ObjectState ("name", GS::UniString ("offY"), "value", 0.0));
        gdl (GS::ObjectState ("name", GS::UniString ("gridAng"), "value", angDeg));
        list (item);
    }
    const GS::ObjectState setResp = SetGDLParametersOfElementsCommand ().Execute (setParams, pc);
    GS::Array<GS::ObjectState> results;
    setResp.Get ("executionResults", results);
    bool ok = !results.IsEmpty ();
    for (const GS::ObjectState& r : results) {
        bool s = false;
        r.Get ("success", s);
        ok = ok && s;
    }
    if (!ok) { why = "Objekt angelegt, aber Umriss/Raster konnten nicht gesetzt werden"; return false; }
    return true;
}

} // namespace

GSErrCode ELM_BodenflieseAusAuswahl ()
{
    API_SelectionInfo selectionInfo = {};
    GS::Array<API_Neig> selNeigs;
    const GSErrCode selErr = ACAPI_Selection_Get (&selectionInfo, &selNeigs, false);
    BMKillHandle ((GSHandle*) &selectionInfo.marquee.coords);
    if (selErr == APIERR_NOSEL || selNeigs.IsEmpty ()) {
        DGAlert (DG_INFORMATION, GS::UniString ("Bodenfliese aus Auswahl"),
                 GS::UniString ("Bitte zuerst eine geschlossene Polylinie, Schraffur, Decke oder Zone auswählen."),
                 GS::EmptyUniString, GS::UniString ("OK"));
        return NoError;
    }

    API_LibPart libPart = {};
    GS::ucscpy (libPart.docu_UName, GS::UniString (kLibPartName).ToUStr ());
    const GSErrCode lpErr = ACAPI_LibraryPart_Search (&libPart, false, true);
    delete libPart.location;
    if (lpErr != NoError) {
        DGAlert (DG_ERROR, GS::UniString ("Bodenfliese aus Auswahl"),
                 GS::UniString ("Das Objekt „SAB Bodenfliese liegend“ ist in diesem Projekt nicht geladen."),
                 GS::UniString ("Bitte die .gsm über den Bibliotheksmanager hinzufügen."), GS::UniString ("OK"));
        return NoError;
    }
    const ToolDefaults td = ReadToolDefaults (libPart.index);

    GS::Array<API_Neig> newNeigs;
    GS::UniString skipped;
    Int32 withArcs = 0;
    for (const API_Neig& neig : selNeigs) {
        Outline o;
        GS::UniString why;
        if (!ReadOutline (neig.guid, o, why)) {
            if (why.Contains ("kein Umriss-Element")) continue;   // z. B. mit ausgewaehlte Objekte still uebergehen
            skipped += GS::UniString ("\n• ") + why;
            continue;
        }
        if (o.hadArcs) ++withArcs;
        API_Guid newGuid = APINULLGuid;
        if (CreateFloor (o, td, newGuid, why))
            newNeigs.Push (API_Neig (newGuid));
        else
            skipped += GS::UniString ("\n• ") + why;
    }

    if (!newNeigs.IsEmpty ()) {
        ACAPI_Selection_DeselectAll ();
        ACAPI_Selection_Select (newNeigs, true);
    }

    GS::UniString msg = GS::UniString::Printf ("%d Bodenfliese(n) angelegt.", (int) newNeigs.GetSize ());
    if (!td.isOurs)
        msg += GS::UniString ("\nEinstellungen: Voreinstellung des Objekts (im Objektwerkzeug ist eine andere Bibliothek eingestellt).");
    if (withArcs > 0)
        msg += GS::UniString ("\nHinweis: Bögen im Umriss wurden als gerade Kanten übernommen.");
    if (!skipped.IsEmpty ())
        msg += GS::UniString ("\nNicht übernommen:") + skipped;
    if (newNeigs.IsEmpty () || withArcs > 0 || !skipped.IsEmpty ())
        DGAlert (newNeigs.IsEmpty () ? DG_WARNING : DG_INFORMATION, GS::UniString ("Bodenfliese aus Auswahl"), msg, GS::EmptyUniString, GS::UniString ("OK"));
    return NoError;
}
