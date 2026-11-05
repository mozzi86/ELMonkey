// ELM_SAB 0.10.4/0.10.5 (2026-11-02) – Bibliotheksteile im Bestand pflegen, ohne Loeschen/Neu-Einfuegen
//   GetLibraryPartInfo   – alle Treffer eines Namens: Bibliothek (eingebettet/Server/lokal), Datei, GUID,
//                          Teamwork-Sperrstatus, Benutzer-Cache; optional Skripte, Parameterliste, GSM-Sicherung
//   ReserveLibraryPart   – Teamwork: Bibliothekselement reservieren bzw. freigeben (release=true)  [experimentell]
//   UpdateLibraryPart    – Skripte ersetzen und/oder Parameter ergaenzen, mit Sicherung, Ruecklese und Ruecknahme
//                          bei Abweichung. Schreibt nur, wenn das Element bearbeitbar ist (Teamwork: reserviert).
// Anlass: BIM-all-doors (dormakaba) liegt im Projekt in der BIMcloud-Serverbibliothek „BIM_ALL_DOORS“,
// nicht in der eingebetteten Bibliothek. Der Nutzer fuegte jeden Code-Stand von Hand im GDL-Editor ein.
// Header-only, AC29.
#pragma once

#include "Extras0103ELM.hpp"

#ifdef ServerMainVers_2900
#include "ACAPI/Library/LibraryManager.hpp"
#include "ACAPI/Library/LibraryTreePath.hpp"
#include "ACAPI/Library/GSMObject.hpp"
#include "GSUnID.hpp"
#include "FileSystem.hpp"
#include "Folder.hpp"
#include "GSTime.hpp"

// AC30 hat UniString::ToLowerCase durch GetLowerCased ersetzt (gleiches Muster wie TapirPalette.cpp).
#ifdef ServerMainVers_3000
#define ELM_LOWER_CASED GetLowerCased
#else
#define ELM_LOWER_CASED ToLowerCase
#endif

namespace ElmLibPart0104 {

struct Match {
    Int32           libInd = 0;
    GS::UniString   fileName;
    GS::UniString   libraryName;
    GS::UniString   libraryType;   // Embedded | Server | BuiltIn | Local
    IO::Location    location;
    GS::UniString   unId;          // {main}-{rev}
    GS::UniString   mainGuid;
};

inline GS::UniString StatusText (API_LockableStatus s)
{
    switch (s) {
        case APILockableStatus_NotExist:     return "NotExist";
        case APILockableStatus_Free:         return "Free";
        case APILockableStatus_Editable:     return "Editable";
        case APILockableStatus_Locked:       return "Locked";
        case APILockableStatus_NotAvailable: return "NotAvailable";
        default:                             return "Unknown";
    }
}

inline GS::UniString Stem (const GS::UniString& fileName)
{
    GS::UniString s = fileName;
    const UIndex dot = s.FindLast ('.');
    if (dot != MaxUIndex) return GS::UniString (s.GetPrefix (dot));
    return s;
}

// Alle geladenen Bibliotheksteile mit diesem Namen (ohne Endung, Gross/Klein egal), optional auf Haupt-GUID
// und Bibliotheksnamen (Teilstring) eingegrenzt.
inline GSErrCode FindMatches (const GS::UniString& name, const GS::UniString& mainGuid, const GS::UniString& library,
                              GS::Array<Match>& out, GS::UniString& msg)
{
    auto lmRes = ACAPI::Library::GetLibraryManager ();
    if (lmRes.IsErr ()) { msg = "LibraryManager nicht verfuegbar (kein Projekt?)"; return APIERR_NOPLAN; }
    const auto lm = lmRes.Unwrap ();
    const GS::UniString wanted = Stem (name).ELM_LOWER_CASED ();
    const auto parts = lm.FindLibParts ([&] (const ACAPI::Library::LibPart& lp) {
        auto n = lp.GetName ();
        return n.IsOk () && Stem (n.Unwrap ()).ELM_LOWER_CASED () == wanted;
    });
    for (const auto& lp : parts) {
        Match m;
        auto ind = lm.GetLibIndOfLibPart (lp);
        if (ind.IsErr ()) continue;
        m.libInd = ind.Unwrap ();
        auto n = lp.GetName ();       if (n.IsOk ()) m.fileName = n.Unwrap ();
        auto loc = lp.GetLocation (); if (loc.IsOk ()) m.location = loc.Unwrap ();
        auto lib = lm.GetLibraryOfLibPart (lp);
        if (lib.IsOk ()) {
            const auto l = lib.Unwrap ();
            auto ln = l.GetName (); if (ln.IsOk ()) m.libraryName = ln.Unwrap ();
            auto emb = l.IsEmbedded (); auto srv = l.IsServer (); auto bi = l.IsBuiltIn ();
            m.libraryType = (emb.IsOk () && emb.Unwrap ()) ? "Embedded" : (srv.IsOk () && srv.Unwrap ()) ? "Server" :
                            (bi.IsOk () && bi.Unwrap ()) ? "BuiltIn" : "Local";
        }
        API_LibPart alp = {}; alp.index = m.libInd;
        if (ACAPI_LibraryPart_Get (&alp) == NoError) {
            m.unId = GS::UniString (alp.ownUnID);
            m.mainGuid = GS::UnID (alp.ownUnID).GetMainGuid ().ToUniString ();
            if (alp.location != nullptr) { delete alp.location; alp.location = nullptr; }
        }
        if (!mainGuid.IsEmpty ()) {
            GS::UniString g = mainGuid; g.ReplaceAll ("{", ""); g.ReplaceAll ("}", "");
            if (g.ELM_LOWER_CASED () != m.mainGuid.ELM_LOWER_CASED ()) continue;
        }
        if (!library.IsEmpty () && !m.libraryName.ELM_LOWER_CASED ().Contains (library.ELM_LOWER_CASED ())) continue;
        out.Push (m);
    }
    if (out.IsEmpty ()) { msg = GS::UniString ("Kein geladenes Bibliothekselement gefunden: ") + name; return APIERR_BADNAME; }
    return NoError;
}

inline GS::ObjectState MatchesList (const GS::Array<Match>& ms)
{
    GS::ObjectState os;
    const auto& add = os.AddList<GS::ObjectState> ("matches");
    for (const auto& m : ms) {
        GS::ObjectState e; e.Add ("fileName", m.fileName); e.Add ("library", m.libraryName); e.Add ("mainGuid", m.mainGuid); add (e);
    }
    return os;
}

// Genau ein Treffer, sonst Fehler mit Trefferliste (dann mainGuid oder library angeben).
inline GSErrCode ResolveUnique (const GS::ObjectState& p, Match& m, GS::ObjectState& err)
{
    GS::UniString name, guid, lib; p.Get ("name", name); p.Get ("mainGuid", guid); p.Get ("library", lib);
    if (name.IsEmpty ()) { err = ElmExtras0103::Err (APIERR_BADPARS, "name fehlt"); return APIERR_BADPARS; }
    GS::Array<Match> ms; GS::UniString msg;
    const GSErrCode e = FindMatches (name, guid, lib, ms, msg);
    if (e != NoError) { err = ElmExtras0103::Err (e, msg); return e; }
    if (ms.GetSize () > 1) {
        err = ElmExtras0103::Err (APIERR_BADNAME, "Name ist mehrdeutig - mainGuid oder library angeben");
        GS::Array<GS::ObjectState> list; MatchesList (ms).Get ("matches", list);
        const auto& add = err.AddList<GS::ObjectState> ("matches"); for (const auto& x : list) add (x);
        return APIERR_BADNAME;
    }
    m = ms[0];
    return NoError;
}

inline API_LockableStatus LockStatus (const Match& m, IO::Location* userCache = nullptr)
{
    API_LockableStatus s = APILockableStatus_NotExist;
    IO::Location uc;
    if (ACAPI_LibraryPart_GetLibPartLockStatus (&m.location, &s, &uc) != NoError) return APILockableStatus_NotExist;
    if (userCache != nullptr) *userCache = uc;
    return s;
}

inline GS::UniString Normalize (GS::UniString s)
{
    s.ReplaceAll (GS::UniString ("\r\n"), GS::UniString ("\r"));
    s.ReplaceAll (GS::UniChar ('\n'), GS::UniChar ('\r'));
    s.ReplaceAll (GS::UniString (u"﻿"), GS::UniString ());   // BOM aus Textdateien (Code_1.29-Falle)
    return s;
}

inline GS::UniString TrimEnd (GS::UniString s)
{
    while (s.GetLength () > 0 && (s[s.GetLength () - 1] == '\r' || s[s.GetLength () - 1] == ' ' || s[s.GetLength () - 1] == '\t'))
        s.DeleteLast ();
    return s;
}

inline const char* TypeName (API_AddParID t)
{
    switch (t) {
        case APIParT_Integer: return "Integer";      case APIParT_Length: return "Length";
        case APIParT_Angle: return "Angle";          case APIParT_RealNum: return "RealNum";
        case APIParT_LightSw: return "LightSwitch";  case APIParT_ColRGB: return "ColorRGB";
        case APIParT_Intens: return "Intensity";     case APIParT_LineTyp: return "LineType";
        case APIParT_Mater: return "Material";       case APIParT_FillPat: return "FillPattern";
        case APIParT_PenCol: return "PenColor";      case APIParT_CString: return "String";
        case APIParT_Boolean: return "Boolean";      case APIParT_Separator: return "Separator";
        case APIParT_Title: return "Title";          case APIParT_BuildingMaterial: return "BuildingMaterial";
        case APIParT_Profile: return "Profile";      case APIParT_Dictionary: return "Dictionary";
        default: return "Unknown";
    }
}

inline API_AddParID TypeFromName (const GS::UniString& n)
{
    for (int t = APIParT_Integer; t <= APIParT_Profile; ++t)
        if (n.ELM_LOWER_CASED () == GS::UniString (TypeName ((API_AddParID) t)).ELM_LOWER_CASED ()) return (API_AddParID) t;
    if (n.ELM_LOWER_CASED () == "text" || n.ELM_LOWER_CASED () == "abc") return APIParT_CString;
    return API_ZombieParT;
}

inline Int32 ParCount (API_AddParType** h) { return h == nullptr ? 0 : (Int32) (BMGetHandleSize ((GSConstHandle) h) / sizeof (API_AddParType)); }

inline Int32 FindPar (API_AddParType** h, const char* name)
{
    for (Int32 i = 0; i < ParCount (h); ++i) if (CHEqualCStrings ((*h)[i].name, name)) return i;
    return -1;
}

inline GS::ObjectState ParToOS (const API_AddParType& p)
{
    GS::ObjectState o;
    o.Add ("name", GS::UniString (p.name));
    o.Add ("type", GS::UniString (TypeName (p.typeID)));
    o.Add ("description", GS::UniString (p.uDescname));
    o.Add ("flags", (Int32) p.flags);
    if (p.typeMod == API_ParArray) { o.Add ("isArray", true); o.Add ("dim1", p.dim1); o.Add ("dim2", p.dim2); }
    else if (p.typeID == APIParT_CString) o.Add ("value", GS::UniString (p.value.uStr));
    else if (p.typeID != APIParT_Separator && p.typeID != APIParT_Title && p.typeID != APIParT_Dictionary) o.Add ("value", p.value.real);
    return o;
}

// Gleicher Bestand? Name, Typ, Flags, Array-Masse und Einzelwert (Arrays/Dictionaries nur strukturell).
inline bool SamePar (const API_AddParType& a, const API_AddParType& b)
{
    if (!CHEqualCStrings (a.name, b.name) || a.typeID != b.typeID || a.flags != b.flags || a.typeMod != b.typeMod) return false;
    if (a.typeMod == API_ParArray) return a.dim1 == b.dim1 && a.dim2 == b.dim2;
    if (a.typeID == APIParT_CString) return GS::UniString (a.value.uStr) == GS::UniString (b.value.uStr);
    if (a.typeID == APIParT_Separator || a.typeID == APIParT_Title || a.typeID == APIParT_Dictionary) return true;
    return fabs (a.value.real - b.value.real) < 1e-9;
}

inline GSErrCode CopyGsm (const Match& m, const GS::UniString& folder, GS::UniString& written)
{
    IO::Location src = m.location;
    IO::Location uc;
    if (LockStatus (m, &uc) == APILockableStatus_Editable) {
        bool ex = false; IO::fileSystem.Contains (uc, &ex);
        if (ex) src = uc;   // reserviert: der bearbeitbare Stand liegt im Benutzer-Cache
    }
    IO::Location dst (folder);
    if (IO::fileSystem.CreateFolderTree (dst) != NoError) return APIERR_GENERAL;
    GSTime t = TIGetTime (); GSTimeRecord d; TIGetTimeRecord (t, &d, TI_CURRENT_TIME);
    char stamp[32]; snprintf (stamp, sizeof (stamp), "%04d-%02d-%02d_%02d%02d%02d", d.year, d.month, d.day, d.hour, d.minute, d.second);
    dst.AppendToLocal (IO::Name (Stem (m.fileName) + "__" + GS::UniString (stamp) + ".gsm"));
    // 0.10.5: Archicad schreibt die Kopie selbst. Eingebettete Teile liegen in Teamwork in einer
    // Containerdatei (AutoSave-…/Emb_…/content ist kein Ordner) - eine Dateikopie scheitert dort (-7003).
    GSErrCode e = APIERR_GENERAL;
    auto lmRes = ACAPI::Library::GetLibraryManager ();
    if (lmRes.IsOk ()) {
        const auto lm = lmRes.Unwrap ();
        auto lp = lm.GetLibPartByLibInd (m.libInd);
        if (lp.IsOk ()) {
            auto gsm = lm.GetGSMObjectOfLibPart (lp.Unwrap ());
            if (gsm.IsOk ()) {
                auto obj = gsm.Unwrap ();
                if (obj.CopyTo (dst, ACAPI::Library::GSMObject::CopyMethod::KeepMainGuid).IsOk ()) e = NoError;
            }
        }
    }
    if (e != NoError) e = IO::fileSystem.Copy (src, dst);
    bool ex = false; IO::fileSystem.Contains (dst, &ex);
    if (e == NoError && ex) written = dst.ToDisplayText (); else if (e == NoError) e = APIERR_GENERAL;
    return e;
}

} // namespace ElmLibPart0104

// ------------------------------------------------------------------------------------------
class GetLibraryPartInfoCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "GetLibraryPartInfo"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmLibPart0104;
        GS::UniString name, guid, lib, backup;
        parameters.Get ("name", name); parameters.Get ("mainGuid", guid); parameters.Get ("library", lib); parameters.Get ("backupFolder", backup);
        bool withScripts = false, withParams = false;
        parameters.Get ("includeScripts", withScripts); parameters.Get ("includeParameters", withParams);
        GS::Array<Match> ms; GS::UniString msg;
        const GSErrCode e = FindMatches (name, guid, lib, ms, msg);
        if (e != NoError) return CreateErrorResponseELM (e, msg);
        GS::ObjectState response;
        response.Add ("teamwork", ACAPI_Teamwork_HasConnection ());
        const auto& add = response.AddList<GS::ObjectState> ("libraryParts");
        for (const auto& m : ms) {
            GS::ObjectState o;
            o.Add ("fileName", m.fileName); o.Add ("library", m.libraryName); o.Add ("libraryType", m.libraryType);
            o.Add ("libIndex", m.libInd); o.Add ("location", m.location.ToDisplayText ());
            o.Add ("ownUnID", m.unId); o.Add ("mainGuid", m.mainGuid);
            o.Add ("isServerLibPart", ACAPI_Teamwork_IsServerLibPart (m.location));
            IO::Location uc;
            o.Add ("lockStatus", StatusText (LockStatus (m, &uc)));
            if (!uc.IsEmpty ()) o.Add ("userCacheLocation", uc.ToDisplayText ());
            if (withScripts) {
                GS::ObjectState sc;
                for (const ElmExtras0103::SectDef* s = ElmExtras0103::Sections (); s->key != nullptr; ++s) {
                    API_LibPartSection sec = {}; sec.sectType = s->type; GS::UniString str;
                    if (ACAPI_LibraryPart_GetSection (m.libInd, &sec, nullptr, &str) == NoError) sc.Add (s->key, str);
                }
                o.Add ("scripts", sc);
            }
            if (withParams) {
                double a = 0, b = 0; Int32 n = 0; API_AddParType** pars = nullptr;
                const GSErrCode pe = ACAPI_LibraryPart_GetParams (m.libInd, &a, &b, &n, &pars);
                if (pe == NoError) {
                    o.Add ("A", a); o.Add ("B", b);
                    const auto& pa = o.AddList<GS::ObjectState> ("parameters");
                    for (Int32 i = 0; i < ParCount (pars); ++i) pa (ParToOS ((*pars)[i]));
                    ACAPI_DisposeAddParHdl (&pars);
                } else o.Add ("parametersError", (Int32) pe);
            }
            if (!backup.IsEmpty ()) {
                GS::UniString written;
                const GSErrCode be = CopyGsm (m, backup, written);
                if (be == NoError) o.Add ("backupGsm", written); else o.Add ("backupError", (Int32) be);
            }
            add (o);
        }
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class ReserveLibraryPartCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "ReserveLibraryPart"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmLibPart0104;
        Match m; GS::ObjectState err;
        if (ResolveUnique (parameters, m, err) != NoError) return err;
        bool release = false, dialogs = false;
        parameters.Get ("release", release); parameters.Get ("enableDialogs", dialogs);
        GS::ObjectState response;
        response.Add ("fileName", m.fileName); response.Add ("library", m.libraryName); response.Add ("mainGuid", m.mainGuid);
        response.Add ("lockStatusBefore", StatusText (LockStatus (m)));
        if (!ACAPI_Teamwork_HasConnection ()) { response.Add ("success", true); response.Add ("note", GS::UniString ("Kein Teamwork - keine Reservierung noetig")); return response; }
        auto lmRes = ACAPI::Library::GetLibraryManager ();
        if (lmRes.IsErr ()) return CreateErrorResponseELM (APIERR_NOPLAN, "LibraryManager nicht verfuegbar");
        const auto lm = lmRes.Unwrap ();
        auto lp = lm.GetLibPartByLibInd (m.libInd);
        if (lp.IsErr ()) return CreateErrorResponseELM (APIERR_BADINDEX, "LibPart zum Index nicht gefunden");
        auto path = lm.GetLibraryTreePathOfLibPart (lp.Unwrap ());
        if (path.IsErr ()) return CreateErrorResponseELM (APIERR_GENERAL, "Bibliothekspfad nicht ermittelbar");
        auto twid = lm.GetTWID (path.Unwrap ());
        if (twid.IsErr ()) return CreateErrorResponseELM (APIERR_GENERAL, "Kein Teamwork-Bezeichner fuer dieses Element (nicht reservierbar)");
        const API_Guid id = GSGuid2APIGuid (twid.Unwrap ());
        response.Add ("twId", APIGuidToString (id));
        GS::PagedArray<short> conflicts;
        response.Add ("lockableStatusBefore", StatusText (ACAPI_Teamwork_GetLockableStatus (id, &conflicts)));
        const GSErrCode e = release ? ACAPI_Teamwork_ReleaseLockable (id, dialogs) : ACAPI_Teamwork_ReserveLockable (id, &conflicts, dialogs);
        response.Add ("code", (Int32) e);
        const API_LockableStatus after = LockStatus (m);
        response.Add ("lockStatusAfter", StatusText (after));
        const auto& cl = response.AddList<GS::ObjectState> ("conflicts");
        for (short u : conflicts) {
            GS::UniString un; ACAPI_Teamwork_GetUsernameFromId (u, &un);
            GS::ObjectState c; c.Add ("userId", (Int32) u); c.Add ("userName", un); cl (c);
        }
        const bool ok = e == NoError && (release ? after != APILockableStatus_Editable : after == APILockableStatus_Editable);
        response.Add ("success", ok);
        if (!ok) response.Add ("error", GS::UniString (release ? "Freigabe nicht bestaetigt" : "Reservierung nicht bestaetigt - im Bibliothekenmanager reservieren"));
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class UpdateLibraryPartCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "UpdateLibraryPart"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmLibPart0104;
        using ElmExtras0103::SectDef;
        Match m; GS::ObjectState err;
        if (ResolveUnique (parameters, m, err) != NoError) return err;

        const GS::ObjectState* scripts = parameters.Get ("scripts");
        GS::Array<GS::ObjectState> newPars; parameters.Get ("addParameters", newPars);
        GS::UniString backup; parameters.Get ("backupFolder", backup);
        bool dryRun = false, requireEditable = true;
        parameters.Get ("dryRun", dryRun); parameters.Get ("requireEditable", requireEditable);
        if (scripts == nullptr && newPars.IsEmpty ()) return CreateErrorResponseELM (APIERR_BADPARS, "scripts oder addParameters angeben");
        if (backup.IsEmpty () && !dryRun) return CreateErrorResponseELM (APIERR_BADPARS, "backupFolder fehlt (Sicherung des alten Stands ist Pflicht)");

        GS::ObjectState response;
        response.Add ("fileName", m.fileName); response.Add ("library", m.libraryName); response.Add ("mainGuid", m.mainGuid);
        response.Add ("libIndex", m.libInd);
        const API_LockableStatus st = LockStatus (m);
        response.Add ("lockStatus", StatusText (st));
        const bool editable = !ACAPI_Teamwork_HasConnection () || st == APILockableStatus_Editable || st == APILockableStatus_NotExist;
        if (requireEditable && !editable) {
            GS::ObjectState r = ElmExtras0103::Err (APIERR_NOTMINE, "Bibliothekselement ist nicht reserviert - erst ReserveLibraryPart oder im Bibliothekenmanager reservieren");
            r.Add ("lockStatus", StatusText (st)); return r;
        }

        // --- Plan: Skripte
        struct ScriptJob { const SectDef* def; GS::UniString code; GS::UniString old; bool changed; };
        GS::Array<ScriptJob> jobs;
        if (scripts != nullptr) {
            for (const SectDef* s = ElmExtras0103::Sections (); s->key != nullptr; ++s) {
                GS::UniString code; if (!scripts->Get (s->key, code)) continue;
                API_LibPartSection sec = {}; sec.sectType = s->type; GS::UniString old;
                ACAPI_LibraryPart_GetSection (m.libInd, &sec, nullptr, &old);
                const GS::UniString nc = Normalize (code);
                jobs.Push ({ s, nc, old, TrimEnd (Normalize (old)) != TrimEnd (nc) });
            }
        }
        // --- Plan: Parameter
        double a = 0, b = 0; Int32 n = 0; API_AddParType** pars = nullptr;
        GS::Array<API_AddParType> toAdd;
        const auto& pr = response.AddList<GS::ObjectState> ("parameterResults");
        if (!newPars.IsEmpty ()) {
            GSErrCode pe = ACAPI_LibraryPart_GetParams (m.libInd, &a, &b, &n, &pars);
            if (pe != NoError) return CreateErrorResponseELM (pe, "Parameterliste nicht lesbar");
            for (const auto& np : newPars) {
                GS::UniString pname, ptype, pdesc; np.Get ("name", pname); np.Get ("type", ptype); np.Get ("description", pdesc);
                GS::ObjectState r; r.Add ("name", pname);
                const API_AddParID t = TypeFromName (ptype);
                const GS::String cname = pname.ToCStr ().Get ();
                if (pname.IsEmpty () || cname.GetLength () >= API_NameLen || t == API_ZombieParT) {
                    r.Add ("success", false); r.Add ("error", GS::UniString ("Name leer/zu lang oder Typ unbekannt")); pr (r); continue;
                }
                if (FindPar (pars, cname.ToCStr ()) >= 0) { r.Add ("success", true); r.Add ("status", GS::UniString ("existiert bereits - unveraendert")); pr (r); continue; }
                API_AddParType p = {};
                p.typeID = t; p.typeMod = API_ParSimple;
                CHTruncate (cname.ToCStr (), p.name, API_NameLen);
                GS::ucsncpy (p.uDescname, pdesc.ToUStr ().Get (), API_UAddParDescLen - 1);
                bool child = false, bold = false, hidden = false;
                np.Get ("child", child); np.Get ("bold", bold); np.Get ("hidden", hidden);
                p.flags = (child ? API_ParFlg_Child : 0) | (bold ? API_ParFlg_BoldName : 0) | (hidden ? API_ParFlg_Hidden : 0);
                if (t == APIParT_CString) { GS::UniString v; np.Get ("value", v); GS::ucsncpy (p.value.uStr, v.ToUStr ().Get (), API_UAddParStrLen - 1); }
                else if (t == APIParT_Boolean) { bool v = false; double dv = 0; if (np.Get ("value", v)) p.value.real = v ? 1 : 0; else if (np.Get ("value", dv)) p.value.real = dv; }
                else { double v = 0; np.Get ("value", v); p.value.real = v; }
                toAdd.Push (p);
                r.Add ("status", GS::UniString ("neu")); pr (r);
            }
        }
        const auto& sr = response.AddList<GS::ObjectState> ("scriptResults");
        for (const auto& j : jobs) {
            GS::ObjectState r; r.Add ("script", GS::UniString (j.def->key)); r.Add ("changed", j.changed);
            r.Add ("oldLength", (Int32) j.old.GetLength ()); r.Add ("newLength", (Int32) j.code.GetLength ()); sr (r);
        }
        bool anyChange = !toAdd.IsEmpty ();
        for (const auto& j : jobs) anyChange |= j.changed;
        response.Add ("dryRun", dryRun);
        if (dryRun || !anyChange) {
            if (pars != nullptr) ACAPI_DisposeAddParHdl (&pars);
            response.Add ("success", true);
            if (!anyChange) response.Add ("note", GS::UniString ("Nichts zu aendern - Stand ist bereits gleich"));
            return response;
        }

        // --- Sicherung
        GS::UniString written;
        const GSErrCode be = CopyGsm (m, backup, written);
        if (be != NoError) { if (pars != nullptr) ACAPI_DisposeAddParHdl (&pars); return CreateErrorResponseELM (be, "Sicherung der GSM-Datei fehlgeschlagen - nichts geaendert"); }
        response.Add ("backupGsm", written);

        // --- Ruecknahme-Daten: roher ParamDef-Abschnitt
        GSHandle oldParamSect = nullptr;
        API_LibPartSection psec = {}; psec.sectType = API_SectParamDef;
        if (!toAdd.IsEmpty () && ACAPI_LibraryPart_GetSection (m.libInd, &psec, &oldParamSect, nullptr) != NoError) {
            ACAPI_DisposeAddParHdl (&pars);
            return CreateErrorResponseELM (APIERR_GENERAL, "ParamDef-Abschnitt nicht lesbar - nichts geaendert");
        }

        bool ok = true; GS::UniString why;
        // --- Skripte schreiben
        for (const auto& j : jobs) {
            if (!j.changed) continue;
            API_LibPartSection up = {}; up.sectType = j.def->type;
            GS::UniString code = j.code;
            const GSErrCode e = ACAPI_LibraryPart_UpdateSection (m.libInd, &up, nullptr, &code);
            GS::UniString back; API_LibPartSection rb = {}; rb.sectType = j.def->type;
            ACAPI_LibraryPart_GetSection (m.libInd, &rb, nullptr, &back);
            if (e != NoError || TrimEnd (Normalize (back)) != TrimEnd (j.code)) { ok = false; why = GS::UniString ("Skript ") + j.def->key + " nicht bestaetigt (Code " + GS::ValueToUniString ((Int32) e) + ")"; break; }
        }
        // --- Parameter schreiben
        if (ok && !toAdd.IsEmpty ()) {
            const Int32 oldN = ParCount (pars);
            GS::Array<API_AddParType> oldCopy; for (Int32 i = 0; i < oldN; ++i) oldCopy.Push ((*pars)[i]);
            pars = (API_AddParType**) BMReallocHandle ((GSHandle) pars, (oldN + (Int32) toAdd.GetSize ()) * sizeof (API_AddParType), REALLOC_CLEAR, 0);
            if (pars == nullptr) { ok = false; why = "Speicher"; }
            else {
                for (UIndex k = 0; k < toAdd.GetSize (); ++k) { (*pars)[oldN + k] = toAdd[k]; (*pars)[oldN + k].index = (short) (oldN + k); }
                API_LibPart lp = {}; lp.index = m.libInd;
                GSErrCode e = ACAPI_LibraryPart_Get (&lp);
                GSHandle newSect = nullptr;
                if (e == NoError) e = ACAPI_LibraryPart_GetSect_ParamDef (&lp, pars, &a, &b, nullptr, &newSect);
                if (lp.location != nullptr) { delete lp.location; lp.location = nullptr; }
                if (e == NoError) { API_LibPartSection up = {}; up.sectType = API_SectParamDef; e = ACAPI_LibraryPart_UpdateSection (m.libInd, &up, newSect, nullptr); }
                if (newSect != nullptr) BMKillHandle (&newSect);
                // Ruecklese: alter Bestand unveraendert, neue vorhanden
                double a2 = 0, b2 = 0; Int32 n2 = 0; API_AddParType** back = nullptr;
                if (e == NoError) e = ACAPI_LibraryPart_GetParams (m.libInd, &a2, &b2, &n2, &back);
                if (e != NoError) { ok = false; why = GS::UniString ("Parameter schreiben/lesen fehlgeschlagen (Code ") + GS::ValueToUniString ((Int32) e) + ")"; }
                else {
                    Int32 changedOld = 0;
                    for (const auto& op : oldCopy) { const Int32 i = FindPar (back, op.name); if (i < 0 || !SamePar (op, (*back)[i])) ++changedOld; }
                    Int32 missingNew = 0;
                    for (const auto& np : toAdd) { const Int32 i = FindPar (back, np.name); if (i < 0 || !SamePar (np, (*back)[i])) ++missingNew; }
                    response.Add ("parameterCountBefore", oldN); response.Add ("parameterCountAfter", ParCount (back));
                    if (changedOld > 0 || missingNew > 0) { ok = false; why = GS::UniString ("Parameter-Ruecklese weicht ab: alt veraendert ") + GS::ValueToUniString (changedOld) + ", neu fehlend " + GS::ValueToUniString (missingNew); }
                    ACAPI_DisposeAddParHdl (&back);
                }
            }
        }
        if (pars != nullptr) ACAPI_DisposeAddParHdl (&pars);

        // --- Ruecknahme bei Abweichung
        if (!ok) {
            for (const auto& j : jobs) {
                if (!j.changed) continue;
                API_LibPartSection up = {}; up.sectType = j.def->type; GS::UniString old = j.old;
                ACAPI_LibraryPart_UpdateSection (m.libInd, &up, nullptr, &old);
            }
            if (oldParamSect != nullptr) { API_LibPartSection up = {}; up.sectType = API_SectParamDef; ACAPI_LibraryPart_UpdateSection (m.libInd, &up, oldParamSect, nullptr); }
            response.Add ("rolledBack", true);
            response.Add ("error", why);
        }
        if (oldParamSect != nullptr) BMKillHandle (&oldParamSect);
        response.Add ("success", ok);
        return response;
    }
};

#endif // ServerMainVers_2900
