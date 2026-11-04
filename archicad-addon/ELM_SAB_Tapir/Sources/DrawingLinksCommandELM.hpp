// ELM_SAB 0.10.8 — GetDrawingLinks: Quelle und Status eingesetzter Zeichnungen (2026-11-04)
//
// Anlass: Fachplaner-DWGs/PDFs liegen als Zeichnung (ein Element) im Grundriss. Bisher war per API
// nicht feststellbar, aus welcher Datei eine Zeichnung stammt und ob diese Datei fehlt oder neuer ist
// als der eingelesene Stand (Zeichnungsverwaltung). Nur lesen, aendert nichts.
//   Eingabe : elements [{elementId}] optional. Ohne Angabe: alle Zeichnungen der AKTUELLEN Datenbank
//             (Grundriss oder das aktive Layout). Zeichnungen anderer Datenbanken -> Fehler je Eintrag.
//   Ausgabe : je Zeichnung elementId, name, number, linkType (Dwg/Pdf/Image/InternalView/ExternalView/...),
//             contentType, filePath, fileExists, status (UpToDate/Modified/Unknown wie in der
//             Zeichnungsverwaltung), view* bei Ausschnitt-Links, layerName, floorIndex.
// Header-only, alle Versionen (nur alte, stabile Drawing-API).
#pragma once

#include "ELMCommandBase.hpp"
#include "CommandBase.hpp"
#include <filesystem>

namespace ElmDrawingLinks {

inline const char* LinkTypeName (API_DrawingLinkTypeID t)
{
    switch (t) {
        case API_DrawingLink_InternalViewID: return "InternalView";
        case API_DrawingLink_ExternalViewID: return "ExternalView";
        case API_DrawingLink_DrawingID:      return "Drawing";
        case API_DrawingLink_DXF_DWGID:      return "Dwg";
        case API_DrawingLink_PDFID:          return "Pdf";
        case API_DrawingLink_ImageID:        return "Image";
        case API_DrawingLink_PMKID:          return "Pmk";
        case API_DrawingLink_APIID:          return "AddOn";
        case API_DrawingLink_InfoID:         return "Broken";
        case API_DrawingLink_OtherID:        return "Other";
        default:                             return "Unknown";
    }
}

inline const char* ContentTypeName (API_IDFTypeID t)
{
    switch ((Int32) t) {
        case 1:  return "Drawing";
        case 2:  return "Image";
        case 3:  return "Pdf";
        default: return "Undefined";
    }
}

inline GS::UniString LayerName (const API_AttributeIndex& layerIndex)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = API_LayerID;
    attr.header.index = layerIndex;
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError) {
        return GS::UniString ();
    }
    return name;
}

inline GS::ObjectState DrawingLinkToOS (const API_Guid& guid)
{
    API_Element el = {};
    el.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get (&el);
    if (err != NoError) {
        return CreateErrorResponseELM (err, "Zeichnung nicht gefunden (anderes Layout/andere Datenbank aktiv?)");
    }
#ifdef ServerMainVers_2600
    if (el.header.type.typeID != API_DrawingID) {
#else
    if (el.header.typeID != API_DrawingID) {
#endif
        return CreateErrorResponseELM (APIERR_BADID, "Element ist keine Zeichnung");
    }

    GS::ObjectState os;
    os.Add ("elementId", CreateGuidObjectStateELM (guid));
    os.Add ("name", GS::UniString (el.drawing.name, CC_UTF8));
    os.Add ("layerName", LayerName (el.header.layer));
    os.Add ("floorIndex", (Int32) el.header.floorInd);

    API_DrawingLinkInfo link = {};
    err = ACAPI_Drawing_GetDrawingLink (&el.header.guid, &link);
    if (err != NoError) {
        os.Add ("linkType", GS::UniString ("Unknown"));
        os.Add ("linkError", (Int32) err);
    } else {
        os.Add ("linkType", GS::UniString (LinkTypeName (link.linkTypeID)));
        os.Add ("contentType", GS::UniString (ContentTypeName (link.idfTypeID)));
        os.Add ("number", GS::UniString (link.number, CC_UTF8));
        if (link.linkTypeID == API_DrawingLink_InfoID) {
            os.Add ("originalLinkType", GS::UniString (LinkTypeName (link.infoTypeID)));
        }
        if (link.linkPath != nullptr) {
            GS::UniString path;
            if (link.linkPath->ToPath (&path) == NoError && !path.IsEmpty ()) {
                os.Add ("filePath", path);
                std::error_code ec;
                os.Add ("fileExists", std::filesystem::exists (std::filesystem::path (path.ToCStr (CC_UTF8).Get ()), ec));
            }
            delete link.linkPath;
            link.linkPath = nullptr;
        }
        if (link.linkTypeID == API_DrawingLink_InternalViewID || link.linkTypeID == API_DrawingLink_ExternalViewID) {
            os.Add ("viewLinkId", CreateGuidObjectStateELM (link.linkGuid));
            os.Add ("viewType", (Int32) link.viewType);
            os.Add ("viewDeleted", link.viewDeleted);
        }
        if (link.viewPath != nullptr) {
            os.Add ("viewPath", GS::UniString (reinterpret_cast<const char*> (link.viewPath), CC_UTF8));
            BMKillPtr (&link.viewPath);
        }
    }

    Int32 status = -1;
    API_Guid g = el.header.guid;
    if (ACAPI_Drawing_CheckDrawingStatus (&g, &status) == NoError) {
        os.Add ("status", GS::UniString (status == 1 ? "UpToDate" : status == 0 ? "Modified" : "Unknown"));
    } else {
        os.Add ("status", GS::UniString ("Unknown"));
    }
    return os;
}

} // namespace ElmDrawingLinks


class GetDrawingLinksCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "GetDrawingLinks"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmDrawingLinks;
        GS::Array<API_Guid> guids;
        GS::Array<GS::ObjectState> elements;
        if (parameters.Get ("elements", elements) && !elements.IsEmpty ()) {
            for (const GS::ObjectState& e : elements) {
                const GS::ObjectState* id = e.Get ("elementId");
                guids.Push (id != nullptr ? GetGuidFromObjectState (*id) : APINULLGuid);
            }
        } else {
            ACAPI_Element_GetElemList (API_DrawingID, &guids);
        }
        GS::ObjectState response;
        const auto& drawings = response.AddList<GS::ObjectState> ("drawings");
        for (const API_Guid& g : guids) {
            drawings (DrawingLinkToOS (g));
        }
        return response;
    }
};
