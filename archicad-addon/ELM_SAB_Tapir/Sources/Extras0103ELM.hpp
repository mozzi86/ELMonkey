// ELM_SAB 0.10.3 (2026-11-02)
//   ReplaceDrawingSource  – Zeichnung 1:1 mit neuer Quelle ersetzen (Archicad erlaubt kein Umverknuepfen
//                           einer platzierten Zeichnung, auch nicht per C++-API). Kopie mit allen
//                           Einstellungen (Lage, Massstab, Anker, Rahmen/Ausschnitt, Titel, Nummerierung),
//                           Quelle = neuer Ausschnitt; Verknuepfung pruefen; erst dann alte Zeichnung loeschen.
//   MoveNavigatorItems    – Navigator-Eintraege (Ausschnitte, Ordner, Layouts) verschieben (Eltern + Vorgaenger)
//   GetLibraryPartScripts – GDL-Skripte eines geladenen Bibliotheksteils lesen
//   SetLibraryPartScripts – einzelne GDL-Skripte eines Bibliotheksteils im Bestand ersetzen (ohne Neu-Einfuegen)
// Header-only, AC29.
#pragma once

#include "ELMCommandBase.hpp"
#include "CommandBase.hpp"

#ifdef ServerMainVers_2900
#include "ACAPI_Navigator.h"

namespace ElmExtras0103 {

inline API_Guid GuidOf (const GS::ObjectState& os, const char* field)
{
    const GS::ObjectState* sub = os.Get (field);
    return sub != nullptr ? GetGuidFromObjectState (*sub) : APINULLGuid;
}

inline GS::ObjectState G (const API_Guid& g) { return GS::ObjectState ("guid", APIGuidToString (g)); }

inline GS::ObjectState Err (GSErrCode e, const GS::UniString& m)
{
    GS::ObjectState r; r.Add ("success", false); r.Add ("code", (Int32) e); r.Add ("error", m); return r;
}

// Layout-Fenster aktivieren (Zeichnungen lassen sich nur in der aktiven Layout-DB anlegen/loeschen)
inline GSErrCode ActivateLayout (const API_Guid& layoutNav, GS::UniString& msg)
{
    API_NavigatorItem item = {};
    GSErrCode err = ACAPI_Navigator_GetNavigatorItem (&layoutNav, &item);
    if (err != NoError) { msg = "Layout navigator item not found."; return err; }
    API_DatabaseInfo db = item.db;
    err = ACAPI_Window_GetDatabaseInfo (&db);
    if (err != NoError) { msg = "Failed to resolve layout database."; return err; }
    API_WindowInfo wi = db;
    err = ACAPI_Window_ChangeWindow (&wi);
    if (err != NoError) { msg = "Failed to activate layout window."; return err; }
    err = ACAPI_Database_ChangeCurrentDatabase (&db);
    if (err != NoError) { msg = "Failed to activate layout database."; return err; }
    return NoError;
}

inline API_Guid LinkOf (const API_Guid& drawing)
{
    API_DrawingLinkInfo link = {};
    if (ACAPI_Drawing_GetDrawingLink (&drawing, &link) != NoError) return APINULLGuid;
    if (link.linkPath != nullptr) delete link.linkPath;
    if (link.viewPath != nullptr) BMKillPtr (&link.viewPath);
    return link.linkGuid;
}

struct SectDef { const char* key; GSType type; };
inline const SectDef* Sections ()
{
    static const SectDef s[] = {
        { "master", API_Sect1DScript }, { "2d", API_Sect2DScript }, { "3d", API_Sect3DScript },
        { "parameter", API_SectVLScript }, { "ui", API_SectUIScript }, { "properties", API_SectPRScript },
        { "comment", API_SectComText }, { nullptr, 0 } };
    return s;
}

inline GSErrCode FindLibPart (const GS::UniString& name, API_LibPart& lp)
{
    lp = {};
    GS::ucsncpy (lp.docu_UName, name.ToUStr ().Get (), API_UniLongNameLen - 1);
    GSErrCode err = ACAPI_LibraryPart_Search (&lp, false);
    if (lp.location != nullptr) { delete lp.location; lp.location = nullptr; }
    if (err != NoError) {
        lp = {}; lp.typeID = APILib_ObjectID;
        GS::ucsncpy (lp.docu_UName, name.ToUStr ().Get (), API_UniLongNameLen - 1);
        err = ACAPI_LibraryPart_Search (&lp, false);
        if (lp.location != nullptr) { delete lp.location; lp.location = nullptr; }
    }
    return err;
}

} // namespace ElmExtras0103


// ------------------------------------------------------------------------------------------
class ReplaceDrawingSourceCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "ReplaceDrawingSource"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmExtras0103;
        const API_Guid layout = GuidOf (parameters, "layoutNavigatorItemId");
        GS::Array<GS::ObjectState> items; parameters.Get ("items", items);
        bool deleteOld = true; parameters.Get ("deleteOld", deleteOld);
        bool update = true; parameters.Get ("updateDrawings", update);

        GS::UniString msg;
        GSErrCode err = ActivateLayout (layout, msg);
        if (err != NoError) return CreateErrorResponseELM (err, msg);

        GS::Array<API_Guid> olds;
        for (const auto& it : items) olds.Push (GuidOf (it, "drawingId"));
        GS::HashTable<API_Guid, short> conflicts;
        if (ACAPI_Teamwork_HasConnection () && !olds.IsEmpty ()) ACAPI_Teamwork_ReserveElements (olds, &conflicts, false);

        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        GS::Array<API_Guid> created;
        ACAPI_CallUndoableCommand ("ELM_SAB ReplaceDrawingSource", [&] () -> GSErrCode {
            for (const auto& it : items) {
                const API_Guid oldG = GuidOf (it, "drawingId");
                const API_Guid view = GuidOf (it, "viewNavigatorItemId");
                GS::ObjectState r; r.Add ("oldDrawingId", G (oldG));
                if (conflicts.ContainsKey (oldG)) { r = Err (APIERR_NOACCESSRIGHT, "Reservierung abgelehnt"); r.Add ("oldDrawingId", G (oldG)); results (r); continue; }
                API_Element el = {}; el.header.guid = oldG;
                GSErrCode e = ACAPI_Element_Get (&el);
                if (e != NoError || el.header.type.typeID != API_DrawingID) { r = Err (e != NoError ? e : APIERR_BADID, "Zeichnung nicht im Layout"); r.Add ("oldDrawingId", G (oldG)); results (r); continue; }
                API_ElementMemo memo = {};
                ACAPI_Element_GetMemo (oldG, &memo, APIMemoMask_Polygon | APIMemoMask_AddPars);
                API_Element neu = el;
                neu.header.guid = APINULLGuid;
                neu.drawing.drawingGuid = view;     // so legt auch Tapirs CreateDrawings die Quelle fest
                e = ACAPI_Element_Create (&neu, &memo);
                ACAPI_DisposeElemMemoHdls (&memo);
                if (e != NoError) { r = Err (e, "ACAPI_Element_Create fehlgeschlagen"); r.Add ("oldDrawingId", G (oldG)); results (r); continue; }
                const API_Guid newLink = LinkOf (neu.header.guid);
                if (newLink != view) {
                    ACAPI_Element_Delete ({ neu.header.guid });
                    r = Err (APIERR_GENERAL, "Neue Zeichnung zeigt nicht auf den Ausschnitt - zurueckgenommen, alte bleibt");
                    r.Add ("oldDrawingId", G (oldG)); results (r); continue;
                }
                API_Element bk = {}; bk.header.guid = neu.header.guid; ACAPI_Element_Get (&bk);
                const bool samePlace = fabs (bk.drawing.pos.x - el.drawing.pos.x) < 1e-6 && fabs (bk.drawing.pos.y - el.drawing.pos.y) < 1e-6 &&
                                       bk.drawing.anchorPoint == el.drawing.anchorPoint && fabs (bk.drawing.ratio - el.drawing.ratio) < 1e-9;
                bool deleted = false;
                if (deleteOld && samePlace) deleted = (ACAPI_Element_Delete ({ oldG }) == NoError);
                created.Push (neu.header.guid);
                r.Add ("success", samePlace);
                r.Add ("newDrawingId", G (neu.header.guid));
                r.Add ("linkGuid", G (newLink));
                r.Add ("samePlacement", samePlace);
                r.Add ("oldDeleted", deleted);
                GS::ObjectState p; p.Add ("x", bk.drawing.pos.x); p.Add ("y", bk.drawing.pos.y); r.Add ("position", p);
                r.Add ("anchorPoint", (Int32) bk.drawing.anchorPoint); r.Add ("ratio", bk.drawing.ratio);
                if (!samePlace) r.Add ("error", GS::UniString ("Lage/Anker/Massstab weichen ab - alte Zeichnung NICHT geloescht"));
                results (r);
            }
            return NoError;
        });
        if (update && !created.IsEmpty ()) {
            const GSErrCode u = ACAPI_CallUndoableCommand ("ELM_SAB ReplaceDrawingSource Update", [&] () -> GSErrCode { return ACAPI_Drawing_Update_Drawings (created); });
            response.Add ("updateDrawingsError", (Int32) u);
        }
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class MoveNavigatorItemsCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "MoveNavigatorItems"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmExtras0103;
        GS::Array<GS::ObjectState> items; parameters.Get ("items", items);
        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        for (const auto& it : items) {
            const API_Guid src = GuidOf (it, "navigatorItemId");
            API_Guid parent = GuidOf (it, "parentNavigatorItemId");
            const API_Guid after = GuidOf (it, "afterNavigatorItemId");
            if (parent == APINULLGuid && after != APINULLGuid) {
                API_NavigatorItem p = {};
                if (ACAPI_Navigator_GetNavigatorParentItem (&after, &p) == NoError) parent = p.guid;
            }
            const GS::Guid s = APIGuid2GSGuid (src), pg = APIGuid2GSGuid (parent), ag = APIGuid2GSGuid (after);
            const GSErrCode e = ACAPI_Navigator_SetNavigatorItemPosition (&s, &pg, after != APINULLGuid ? &ag : nullptr);
            GS::ObjectState r; r.Add ("navigatorItemId", G (src));
            API_NavigatorItem now = {};
            const bool ok = e == NoError && ACAPI_Navigator_GetNavigatorParentItem (&src, &now) == NoError && now.guid == parent;
            r.Add ("success", ok);
            if (!ok) { r.Add ("code", (Int32) e); r.Add ("error", GS::UniString ("SetNavigatorItemPosition fehlgeschlagen oder Eltern abweichend")); }
            results (r);
        }
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class GetLibraryPartScriptsCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "GetLibraryPartScripts"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmExtras0103;
        GS::UniString name; parameters.Get ("name", name);
        API_LibPart lp = {};
        const GSErrCode e = FindLibPart (name, lp);
        if (e != NoError) return CreateErrorResponseELM (e, GS::UniString ("Bibliotheksteil nicht gefunden: ") + name);
        GS::ObjectState response;
        response.Add ("name", name);
        response.Add ("libIndex", (Int32) lp.index);
        response.Add ("ownUnID", GS::UniString (lp.ownUnID));
        GS::ObjectState scripts;
        for (const SectDef* s = Sections (); s->key != nullptr; ++s) {
            API_LibPartSection sec = {}; sec.sectType = s->type;
            GS::UniString str;
            if (ACAPI_LibraryPart_GetSection (lp.index, &sec, nullptr, &str) == NoError) scripts.Add (s->key, str);
        }
        response.Add ("scripts", scripts);
        return response;
    }
};

class SetLibraryPartScriptsCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "SetLibraryPartScripts"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmExtras0103;
        GS::UniString name; parameters.Get ("name", name);
        const GS::ObjectState* scripts = parameters.Get ("scripts");
        if (scripts == nullptr) return CreateErrorResponseELM (APIERR_BADPARS, "scripts fehlt");
        API_LibPart lp = {};
        GSErrCode e = FindLibPart (name, lp);
        if (e != NoError) return CreateErrorResponseELM (e, GS::UniString ("Bibliotheksteil nicht gefunden: ") + name);
        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        for (const SectDef* s = Sections (); s->key != nullptr; ++s) {
            GS::UniString code;
            if (!scripts->Get (s->key, code)) continue;
            API_LibPartSection sec = {}; sec.sectType = s->type;
            GS::UniString old;
            ACAPI_LibraryPart_GetSection (lp.index, &sec, nullptr, &old);
            API_LibPartSection up = {}; up.sectType = s->type; up.subIdent = sec.subIdent;
            e = ACAPI_LibraryPart_UpdateSection (lp.index, &up, nullptr, &code);
            GS::UniString back;
            API_LibPartSection rb = {}; rb.sectType = s->type;
            ACAPI_LibraryPart_GetSection (lp.index, &rb, nullptr, &back);
            GS::ObjectState r; r.Add ("script", GS::UniString (s->key));
            r.Add ("success", e == NoError && back == code);
            r.Add ("oldLength", (Int32) old.GetLength ()); r.Add ("newLength", (Int32) back.GetLength ());
            if (!(e == NoError && back == code)) { r.Add ("code", (Int32) e); r.Add ("error", GS::UniString ("UpdateSection fehlgeschlagen oder Ruecklese abweichend (Bibliothek reserviert? Server-/BIMcloud-Bibliothek ist schreibgeschuetzt)")); }
            results (r);
        }
        return response;
    }
};

#endif // ServerMainVers_2900
