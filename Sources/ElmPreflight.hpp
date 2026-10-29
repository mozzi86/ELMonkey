// ELMonkey 0.9.32 — Vorpruefung fuer schreibende Befehle (Wunschliste Michi A1/A4).
//
// Anlass: Schreibende Befehle meldeten "success", obwohl nichts passierte — ausgeblendete oder
// gesperrte Ebene, gesperrtes Element, Hotlink, fremde Teamwork-Reservierung, falsches aktives
// Fenster, Undo-Klammer nicht gestartet (THN 28./31.08., ELT-Leuchten 02.10.).
//
// Konvention: Gruende kommen als Code in eckigen Klammern am Anfang der Fehlermeldung, z. B.
// "[LAYER_HIDDEN] Ebene '98 Schnitt Marker' ist ausgeblendet". So bleibt das Antwortschema der
// Tapir-Befehle (Error = {code, message}, additionalProperties:false) unveraendert gueltig.
//
// Codes: NOT_FOUND, LAYER_HIDDEN, LAYER_LOCKED, LAYER_NOT_FOUND, ELEMENT_LOCKED, HOTLINK,
//        RESERVED_BY_OTHER, WRONG_WINDOW_TYPE, UNDO_SCOPE_FAILED, NO_EFFECT,
//        FIELD_NOT_AVAILABLE (0.10.1: Stiftfeld/Rolle gibt es fuer den Bauteiltyp nicht)
#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "MigrationHelper.hpp"   // AC27/28: ACAPI_Database_GetCurrentDatabase, ACAPI_ProjectOperation_Project
#include "UniString.hpp"

#include <map>

struct ElmLayerStateELM {
    bool          found  = false;
    bool          hidden = false;
    bool          locked = false;
    GS::UniString name;
};

inline ElmLayerStateELM GetLayerStateELM (const API_AttributeIndex& layer)
{
    ElmLayerStateELM st;
    API_Attribute attr = {};
    attr.header.typeID = API_LayerID;
    attr.header.index  = layer;
    if (ACAPI_Attribute_Get (&attr) == NoError) {
        st.found  = true;
        st.hidden = (attr.header.flags & APILay_Hidden) != 0;
        st.locked = (attr.header.flags & APILay_Locked) != 0;
        st.name   = GS::UniString (attr.header.name);
    }
    return st;
}

// Ebene per Name -> echter Attribut-Index (nicht die Listenposition aus GetAttributesByType).
inline bool ResolveLayerByNameELM (const GS::UniString& layerName, API_AttributeIndex& out)
{
    API_Attr_Head head = {};
    head.typeID = API_LayerID;
    GS::UniString name = layerName;
    head.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Search (&head) != NoError)
        return false;
    out = head.index;
    return true;
}

// "" = Ebene ok, sonst "[CODE] Text".
inline GS::UniString LayerBlockReasonELM (const API_AttributeIndex& layer)
{
    const ElmLayerStateELM st = GetLayerStateELM (layer);
    if (!st.found)
        return GS::UniString ("[LAYER_NOT_FOUND] Ebene mit Index ") + GS::ValueToUniString (layer.ToInt32_Deprecated ()) + GS::UniString (" existiert nicht");
    if (st.hidden)
        return GS::UniString ("[LAYER_HIDDEN] Ebene '") + st.name + GS::UniString ("' ist ausgeblendet - erst einblenden (ELM_SAB.SetLayerAttributes)");
    if (st.locked)
        return GS::UniString ("[LAYER_LOCKED] Ebene '") + st.name + GS::UniString ("' ist gesperrt - erst entsperren (ELM_SAB.SetLayerAttributes)");
    return GS::UniString ();
}

inline short MyTeamworkUserIdELM ()
{
    API_ProjectInfo projectInfo = {};
    if (ACAPI_ProjectOperation_Project (&projectInfo) == NoError)
        return projectInfo.userId;
    return 0;
}

// Pruefung eines bestehenden Elements vor einer Aenderung. "" = darf geaendert werden.
// layerCache spart wiederholte Attribut-Abfragen bei grossen Listen.
inline GS::UniString PreflightElementELM (const API_Guid& guid, short myUserId,
                                          std::map<Int32, GS::UniString>& layerCache)
{
    API_Elem_Head head = {};
    head.guid = guid;
    if (guid == APINULLGuid || ACAPI_Element_GetHeader (&head) != NoError)
        return GS::UniString ("[NOT_FOUND] Element ") + APIGuidToString (guid) + GS::UniString (" existiert nicht");

    const Int32 li = head.layer.ToInt32_Deprecated ();
    auto it = layerCache.find (li);
    if (it == layerCache.end ())
        it = layerCache.emplace (li, LayerBlockReasonELM (head.layer)).first;
    if (!it->second.IsEmpty ())
        return it->second;

    const bool teamwork = ACAPI_Teamwork_HasConnection ();
    // Ohne Teamwork ist lockId der Sperrschalter, in Teamwork die Nutzer-ID des Reservierenden.
    if (!teamwork && head.lockId != 0)
        return GS::UniString ("[ELEMENT_LOCKED] Element ist gesperrt (UnlockElements)");
    if (head.hotlinkGuid != APINULLGuid)
        return GS::UniString ("[HOTLINK] Element gehoert zu einem Hotlink und ist nicht aenderbar");
    if (teamwork && head.lockId != 0 && head.lockId != myUserId)
        return GS::UniString ("[RESERVED_BY_OTHER] Element ist von einem anderen Teamwork-Nutzer reserviert");
    return GS::UniString ();
}

// Modellelemente gehoeren in den Grundriss; in Schnitt/Ansicht/Layout landen Erstellungen falsch
// oder verlieren z. 3D-Fenster ist erlaubt (Grundriss-Datenbank).
inline GS::UniString ModelWindowBlockReasonELM ()
{
    API_DatabaseInfo db = {};
    if (ACAPI_Database_GetCurrentDatabase (&db) != NoError)
        return GS::UniString ();
    if (db.typeID == APIWind_FloorPlanID || db.typeID == APIWind_3DModelID)
        return GS::UniString ();
    return GS::UniString ("[WRONG_WINDOW_TYPE] Aktives Fenster ist kein Grundriss (Schnitt/Ansicht/Layout) - Grundriss nach vorne holen");
}

inline GSErrCode ReasonToErrCodeELM (const GS::UniString& reason)
{
    if (reason.BeginsWith ("[NOT_FOUND]"))         return APIERR_BADID;
    if (reason.BeginsWith ("[LAYER_HIDDEN]"))      return APIERR_HIDDENLAY;
    if (reason.BeginsWith ("[LAYER_LOCKED]"))      return APIERR_LOCKEDLAY;
    if (reason.BeginsWith ("[LAYER_NOT_FOUND]"))   return APIERR_BADINDEX;
    if (reason.BeginsWith ("[RESERVED_BY_OTHER]")) return APIERR_NOACCESSRIGHT;
    if (reason.BeginsWith ("[WRONG_WINDOW_TYPE]")) return APIERR_BADWINDOW;
    return APIERR_GENERAL;
}
