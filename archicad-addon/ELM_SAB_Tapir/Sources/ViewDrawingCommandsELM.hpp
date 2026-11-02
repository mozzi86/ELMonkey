// ELM_SAB 0.10.2 — Ausschnitte und Zeichnungen fuer Designoptionen (Thekengestaltung, 2026-11-02)
//
// Anlass: Varianten-Layouts (5.13.19–22) sollen dieselben Zeichnungen zeigen, nur mit anderer
// Designoptionen-Kombination. Tapir kann weder die Kombination eines Ausschnitts setzen
// (GetViewSettings/SetViewSettings kennen das Feld nicht) noch die Quelle einer platzierten
// Zeichnung lesen oder aendern. Fuenf Befehle schliessen die Luecke:
//   GetViewDesignOptionCombinations  – Kombination je Ausschnitt lesen
//   SetViewDesignOptionCombination   – Kombination (oder "nur Hauptmodell") setzen, mit Ruecklese
//   DuplicateViews                   – Ausschnitt mit allen Einstellungen als neuen Ausschnitt anlegen
//   GetDrawingsOfLayout              – Zeichnungen eines Layouts mit Quelle, Lage, Massstab
//   RelinkDrawings                   – Quelle (drawingGuid) einer Zeichnung umhaengen, Lage bleibt
// Header-only (kein CMake-Eingriff), nur AC29 (DesignOptions-API in dieser Form erst ab 29).
#pragma once

#include "ELMCommandBase.hpp"
#include "CommandBase.hpp"

#ifdef ServerMainVers_2900
#include "ACAPI_Navigator.h"
#include "ACAPI/View.hpp"
#include "ACAPI/DesignOptionManager.hpp"
#include <variant>

namespace ElmViewDrawing {

inline API_Guid GuidOf (const GS::ObjectState& os, const char* field)
{
    const GS::ObjectState* sub = os.Get (field);
    if (sub == nullptr) return APINULLGuid;
    return GetGuidFromObjectState (*sub);
}

inline GS::ObjectState NavIdOS (const API_Guid& guid)
{
    return GS::ObjectState ("guid", APIGuidToString (guid));
}

// Status der Designoptionen-Kombination eines Ausschnitts als (status, name)
inline bool ReadViewCombination (ACAPI::DesignOptions::DesignOptionManager& manager, const API_Guid& viewGuid,
                                 GS::UniString& status, GS::UniString& name, GS::UniString& error)
{
    const ACAPI::Result<ACAPI::ModelViews::View> view = ACAPI::ModelViews::FindViewByGuid (viewGuid);
    if (view.IsErr ()) { error = "View not found."; return false; }
    const auto settings = manager.GetDesignOptionCombinationSettingsOf (view.Unwrap ());
    if (settings.IsErr ()) { error = "No design option settings for this view (not a model view?)."; return false; }
    const auto st = settings->GetStatus ();
    if (st.IsErr ()) { error = "Failed to read design option status."; return false; }
    name = "";
    std::visit ([&] (const auto& s) {
        using T = std::decay_t<decltype (s)>;
        if constexpr (std::is_same_v<T, ACAPI::DesignOptions::DesignOptionCombinationViewSettings::MainModelOnly>) {
            status = "MainModelOnly";
        } else if constexpr (std::is_same_v<T, ACAPI::DesignOptions::DesignOptionCombinationViewSettings::Standard>) {
            status = "Standard"; name = s.combination.GetName ();
        } else if constexpr (std::is_same_v<T, ACAPI::DesignOptions::DesignOptionCombinationViewSettings::Custom>) {
            status = "Custom"; name = s.combination.GetName ();
        } else {
            status = "Missing";
        }
    }, st.Unwrap ());
    return true;
}

// Aktive Datenbank auf die des Navigator-Eintrags (Layout) wechseln; Ausgangs-DB merken
class LayoutDatabaseScope {
public:
    explicit LayoutDatabaseScope (const API_Guid& layoutNavGuid)
    {
        hasStart = (ACAPI_Database_GetCurrentDatabase (&start) == NoError);
        API_NavigatorItem item = {};
        err = ACAPI_Navigator_GetNavigatorItem (&layoutNavGuid, &item);
        if (err != NoError) { msg = "Layout navigator item not found."; return; }
        API_DatabaseInfo db = item.db;
        err = ACAPI_Window_GetDatabaseInfo (&db);
        if (err != NoError) { msg = "Failed to resolve layout database."; return; }
        err = ACAPI_Database_ChangeCurrentDatabase (&db);
        if (err != NoError) { msg = "Failed to activate layout database."; return; }
        changed = true;
    }
    ~LayoutDatabaseScope ()
    {
        if (changed && hasStart) ACAPI_Database_ChangeCurrentDatabase (&start);
    }
    GSErrCode err = NoError;
    GS::UniString msg;
private:
    API_DatabaseInfo start = {};
    bool hasStart = false;
    bool changed = false;
};

inline GS::ObjectState DrawingToOS (const API_Element& el)
{
    GS::ObjectState os;
    os.Add ("drawingId", NavIdOS (el.header.guid));
    os.Add ("name", GS::UniString (el.drawing.name));
    os.Add ("sourceNavigatorItemId", NavIdOS (el.drawing.drawingGuid));
    GS::ObjectState pos; pos.Add ("x", el.drawing.pos.x); pos.Add ("y", el.drawing.pos.y);
    os.Add ("position", pos);
    os.Add ("ratio", el.drawing.ratio);
    os.Add ("anchorPoint", (Int32) el.drawing.anchorPoint);
    API_DrawingLinkInfo link = {};
    if (ACAPI_Drawing_GetDrawingLink (&el.header.guid, &link) == NoError) {
        os.Add ("linkGuid", NavIdOS (link.linkGuid));
        os.Add ("linkTypeId", (Int32) link.linkTypeID);
        if (link.linkPath != nullptr) delete link.linkPath;
        if (link.viewPath != nullptr) BMKillPtr (&link.viewPath);
    }
    API_NavigatorItem src = {};
    if (ACAPI_Navigator_GetNavigatorItem (&el.drawing.drawingGuid, &src) == NoError) {
        os.Add ("sourceName", GS::UniString (src.uName));
        os.Add ("sourceUiId", GS::UniString (src.uiId));
    }
    return os;
}

} // namespace ElmViewDrawing


// ------------------------------------------------------------------------------------------
class GetViewDesignOptionCombinationsCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "GetViewDesignOptionCombinations"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmViewDrawing;
        GS::Array<GS::ObjectState> items;
        parameters.Get ("navigatorItemIds", items);
        auto manager = ACAPI::DesignOptions::CreateDesignOptionManager ();
        if (manager.IsErr ()) return CreateErrorResponseELM (APIERR_GENERAL, "Failed to get Design Option Manager.");
        GS::ObjectState response;
        const auto& list = response.AddList<GS::ObjectState> ("views");
        for (const auto& it : items) {
            const API_Guid g = GuidOf (it, "navigatorItemId");
            GS::ObjectState os; os.Add ("navigatorItemId", NavIdOS (g));
            GS::UniString status, name, error;
            if (ReadViewCombination (*manager, g, status, name, error)) {
                os.Add ("status", status); os.Add ("combinationName", name);
            } else {
                os.Add ("error", error);
            }
            list (os);
        }
        // verfuegbare Kombinationen mitliefern (Namen fuer SetViewDesignOptionCombination)
        const auto& combos = response.AddList<GS::UniString> ("availableCombinations");
        const auto all = manager->GetDesignOptionCombinations ();
        if (all.IsOk ()) for (const auto& c : all.Unwrap ()) combos (c.GetName ());
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class SetViewDesignOptionCombinationCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "SetViewDesignOptionCombination"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmViewDrawing;
        GS::Array<GS::ObjectState> items;
        parameters.Get ("items", items);
        auto manager = ACAPI::DesignOptions::CreateDesignOptionManager ();
        if (manager.IsErr ()) return CreateErrorResponseELM (APIERR_GENERAL, "Failed to get Design Option Manager.");
        const auto all = manager->GetDesignOptionCombinations ();
        if (all.IsErr ()) return CreateErrorResponseELM (APIERR_GENERAL, "Failed to list design option combinations.");

        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        for (const auto& it : items) {
            const API_Guid g = GuidOf (it, "navigatorItemId");
            GS::UniString wanted; it.Get ("combinationName", wanted);
            bool mainOnly = false; it.Get ("mainModelOnly", mainOnly);
            GS::ObjectState r; r.Add ("navigatorItemId", NavIdOS (g));

            const auto view = ACAPI::ModelViews::FindViewByGuid (g);
            if (view.IsErr ()) { r.Add ("success", false); r.Add ("error", GS::UniString ("View not found.")); results (r); continue; }
            auto settings = manager->GetDesignOptionCombinationSettingsOf (view.Unwrap ());
            if (settings.IsErr ()) { r.Add ("success", false); r.Add ("error", GS::UniString ("No design option settings for this view.")); results (r); continue; }

            const ACAPI::DesignOptions::DesignOptionCombination* target = nullptr;
            if (!mainOnly) {
                for (const auto& c : all.Unwrap ()) if (c.GetName () == wanted) { target = &c; break; }
                if (target == nullptr) { r.Add ("success", false); r.Add ("error", GS::UniString (GS::UniString ("Combination not found: ") + wanted)); results (r); continue; }
            }
            bool ok = false;
            ACAPI_CallUndoableCommand ("ELM_SAB SetViewDesignOptionCombination", [&] () -> GSErrCode {
                const auto res = mainOnly ? settings->SetMainModelOnly () : settings->SetStandardCombination (*target);
                ok = res.IsOk ();
                return ok ? NoError : APIERR_GENERAL;
            });
            // Ruecklese
            GS::UniString status, name, error;
            const bool read = ReadViewCombination (*manager, g, status, name, error);
            const bool verified = read && (mainOnly ? status == "MainModelOnly" : (status == "Standard" && name == wanted));
            r.Add ("success", ok && verified);
            r.Add ("status", status); r.Add ("combinationName", name);
            if (!(ok && verified)) r.Add ("error", GS::UniString (ok ? "Readback does not match." : "SetStandardCombination failed (Teamwork: view reserved?)."));
            results (r);
        }
        return response;
    }
};

// ------------------------------------------------------------------------------------------
// Ausschnitt duplizieren: Navigator-Eintrag + Ausschnitteinstellungen lesen, als neuen Ausschnitt
// unter demselben Ordner anlegen (neuer Name/ID), Designoptionen-Kombination mitnehmen.
class DuplicateViewsCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "DuplicateViews"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmViewDrawing;
        GS::Array<GS::ObjectState> items;
        parameters.Get ("items", items);
        auto manager = ACAPI::DesignOptions::CreateDesignOptionManager ();

        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        for (const auto& it : items) {
            const API_Guid src = GuidOf (it, "navigatorItemId");
            GS::UniString newName; it.Get ("newName", newName);
            GS::UniString newId;   const bool hasId = it.Get ("newId", newId);
            GS::ObjectState r; r.Add ("sourceNavigatorItemId", NavIdOS (src));

            API_NavigatorItem item = {};
            GSErrCode err = ACAPI_Navigator_GetNavigatorItem (&src, &item);
            if (err != NoError) { r.Add ("success", false); r.Add ("error", GS::UniString ("Source navigator item not found.")); results (r); continue; }
            API_NavigatorView view = {};
            err = ACAPI_Navigator_GetNavigatorView (&item, &view);
            if (err != NoError) { r.Add ("success", false); r.Add ("error", GS::UniString ("Failed to read view settings.")); results (r); continue; }
            API_NavigatorItem parent = {};
            const bool hasParent = (ACAPI_Navigator_GetNavigatorParentItem (&src, &parent) == NoError);

            API_NavigatorItem neu = item;
            neu.guid = APINULLGuid;
            if (!newName.IsEmpty ()) {
                GS::ucsncpy (neu.uName, newName.ToUStr ().Get (), API_UniLongNameLen - 1);
                neu.customName = true;
            }
            if (hasId) {
                CHTruncate (newId.ToCStr ().Get (), neu.uiId, sizeof (neu.uiId));
                neu.customUiId = true;
            }
            const GS::Guid parentGs = APIGuid2GSGuid (parent.guid);
            const GS::Guid prevGs = APIGuid2GSGuid (src);
            err = ACAPI_Navigator_NewNavigatorView (&neu, &view, hasParent ? &parentGs : nullptr, &prevGs);
            if (err != NoError) { r.Add ("success", false); r.Add ("error", GS::UniString ("NewNavigatorView failed.")); r.Add ("code", err); results (r); continue; }
            r.Add ("navigatorItemId", NavIdOS (neu.guid));
            r.Add ("name", GS::UniString (neu.uName));
            // Designoptionen-Kombination uebernehmen
            if (manager.IsOk ()) {
                GS::UniString status, name, error;
                if (ReadViewCombination (*manager, src, status, name, error) && status == "Standard") {
                    const auto all = manager->GetDesignOptionCombinations ();
                    const auto nv = ACAPI::ModelViews::FindViewByGuid (neu.guid);
                    if (all.IsOk () && nv.IsOk ()) {
                        auto st = manager->GetDesignOptionCombinationSettingsOf (nv.Unwrap ());
                        for (const auto& c : all.Unwrap ())
                            if (c.GetName () == name && st.IsOk ())
                                ACAPI_CallUndoableCommand ("ELM_SAB DuplicateViews DO", [&] () -> GSErrCode { return st->SetStandardCombination (c).IsOk () ? NoError : APIERR_GENERAL; });
                    }
                }
            }
            r.Add ("success", true);
            results (r);
        }
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class GetDrawingsOfLayoutCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "GetDrawingsOfLayout"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmViewDrawing;
        GS::Array<GS::ObjectState> layouts;
        parameters.Get ("layoutNavigatorItemIds", layouts);
        GS::ObjectState response;
        const auto& list = response.AddList<GS::ObjectState> ("layouts");
        for (const auto& lo : layouts) {
            const API_Guid lg = GuidOf (lo, "navigatorItemId");
            GS::ObjectState out; out.Add ("layoutNavigatorItemId", NavIdOS (lg));
            LayoutDatabaseScope scope (lg);
            if (scope.err != NoError) { out.Add ("error", scope.msg); list (out); continue; }
            GS::Array<API_Guid> guids;
            ACAPI_Element_GetElemList (API_DrawingID, &guids);
            const auto& dl = out.AddList<GS::ObjectState> ("drawings");
            for (const API_Guid& g : guids) {
                API_Element el = {}; el.header.guid = g;
                if (ACAPI_Element_Get (&el) == NoError) dl (DrawingToOS (el));
            }
            list (out);
        }
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class RelinkDrawingsCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "RelinkDrawings"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmViewDrawing;
        const API_Guid lg = GuidOf (parameters, "layoutNavigatorItemId");
        GS::Array<GS::ObjectState> items;
        parameters.Get ("items", items);
        bool update = true; parameters.Get ("updateDrawings", update);

        LayoutDatabaseScope scope (lg);
        if (scope.err != NoError) return CreateErrorResponseELM (scope.err, scope.msg);

        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        GS::Array<API_Guid> changed;
        ACAPI_CallUndoableCommand ("ELM_SAB RelinkDrawings", [&] () -> GSErrCode {
            for (const auto& it : items) {
                const API_Guid dg = GuidOf (it, "drawingId");
                const API_Guid vg = GuidOf (it, "viewNavigatorItemId");
                GS::ObjectState r; r.Add ("drawingId", NavIdOS (dg));
                API_Element el = {}; el.header.guid = dg;
                GSErrCode err = ACAPI_Element_Get (&el);
                if (err != NoError) { r.Add ("success", false); r.Add ("error", GS::UniString ("Drawing not found in this layout.")); results (r); continue; }
                API_Element mask = {};
                ACAPI_ELEMENT_MASK_CLEAR (mask);
                el.drawing.drawingGuid = vg;
                ACAPI_ELEMENT_MASK_SET (mask, API_DrawingType, drawingGuid);
                err = ACAPI_Element_Change (&el, &mask, nullptr, 0, true);
                if (err != NoError) { r.Add ("success", false); r.Add ("code", err); r.Add ("error", GS::UniString ("ACAPI_Element_Change failed (Teamwork: layout reserved?).")); results (r); continue; }
                API_Element back = {}; back.header.guid = dg;
                const bool ok = (ACAPI_Element_Get (&back) == NoError) && back.drawing.drawingGuid == vg;
                r.Add ("success", ok);
                if (ok) { r.Add ("drawing", DrawingToOS (back)); changed.Push (dg); }
                else r.Add ("error", GS::UniString ("Readback: drawingGuid unchanged."));
                results (r);
            }
            return NoError;
        });
        if (update && !changed.IsEmpty ()) {
            const GSErrCode uerr = ACAPI_CallUndoableCommand ("ELM_SAB RelinkDrawings Update", [&] () -> GSErrCode {
                return ACAPI_Drawing_Update_Drawings (changed);
            });
            response.Add ("updateDrawingsError", (Int32) uerr);
        }
        return response;
    }
};

#endif // ServerMainVers_2900
