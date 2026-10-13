// ELM_SAB — Basis für SAB-eigene JSON-Commands im Namespace "ELM_SAB".
// Nutzt die Helfer aus Tapirs CommandBase (CreateErrorResponse etc.).
#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ObjectState.hpp"
#include "CommandBase.hpp"
// Für den AC27-Zweig von SetMemoTextContentELM: BMhAllClear/BMKillHandle und GS::ucscpy
// kommen sonst nur transitiv über ACAPinc.h und hängen damit an der Include-Reihenfolge.
#include "BM.hpp"
#include "uchar_t.hpp"

class ELMCommandBase : public API_AddOnCommand
{
public:
    ELMCommandBase () = default;

    virtual GS::String GetNamespace () const override final
    {
        return "ELM_SAB";
    }

    virtual API_AddOnCommandExecutionPolicy GetExecutionPolicy () const override final
    {
        return API_AddOnCommandExecutionPolicy::ScheduleForExecutionOnMainThread;
    }

    virtual void OnResponseValidationFailed (const GS::ObjectState& /*response*/) const override final
    {
    }

#ifdef ServerMainVers_2600
    virtual bool IsProcessWindowVisible () const override final
    {
        return false;
    }
#endif

    virtual GS::Optional<GS::UniString> GetSchemaDefinitions () const override
    {
        return GS::NoValue;
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override
    {
        return GS::NoValue;
    }

    virtual GS::Optional<GS::UniString> GetResponseSchema () const override
    {
        return GS::NoValue;
    }
};

inline GS::ObjectState CreateGuidObjectStateELM (const API_Guid& guid)
{
    return GS::ObjectState ("guid", APIGuidToString (guid));
}

// API_ElementMemo::textContent ist in Archicad 28 von einem char**-Handle auf
// GS::UniString* umgestellt worden. Wer direkt darauf zugreift, baut gegen DevKit 27
// nicht mehr — genau daran ist der AC27-Build gescheitert. Die beiden Helfer kapseln
// den Unterschied, damit Lesen und Schreiben denselben Weg nehmen.
//
// Das Muster für den AC27-Zweig ist von Tapirs SetTextContentAndParagraphs in
// ElementCreationCommands.cpp übernommen, das in Upstreams CI für AC25–29 grün baut.

inline GS::UniString GetMemoTextContentELM (const API_ElementMemo& memo)
{
#ifdef ServerMainVers_2800
    return *memo.textContent;
#else
    return GS::UniString (reinterpret_cast<const GS::uchar_t*> (*memo.textContent));
#endif
}

inline void SetMemoTextContentELM (API_ElementMemo& memo, const GS::UniString& text)
{
#ifdef ServerMainVers_2800
    *memo.textContent = text;
#else
    // Das per ACAPI_Element_GetMemo geholte Handle zuerst freigeben — sonst leckt
    // jeder Aufruf eines Massenlaufs einen Block. BMKillHandle nullt den Zeiger,
    // ACAPI_DisposeElemMemoHdls räumt danach das neue Handle ab.
    if (memo.textContent != nullptr) {
        GSHandle oldHandle = reinterpret_cast<GSHandle> (memo.textContent);
        BMKillHandle (&oldHandle);
    }
    memo.textContent = BMhAllClear ((text.GetLength () + 1) * sizeof (GS::uchar_t));
    GS::ucscpy (reinterpret_cast<GS::uchar_t*> (*memo.textContent), text.ToUStr ());
#endif
}

// ---- ELM_SAB 0.9.23 -------------------------------------------------------------------
#include <cstdio>   // std::snprintf in WindowTypeNameELM

// Fehlerform {error:{code,message}} muss im Antwortschema erlaubt sein — sonst meldet Archicad
// dem Client nur 4009 statt der Ursache (CaptureView-Befund WiSchu 2026-10-13). ELMCommandBase
// liefert keine Schema-Definitionen ($ref auf #/Error nicht aufloesbar) -> Fehlerzweig inline.
inline GS::UniString ELMResponseSchemaWithError (const GS::UniString& successSchema)
{
    return GS::UniString ("{\"oneOf\": [") + successSchema +
        GS::UniString (", {\"type\": \"object\", \"properties\": {\"error\": {\"type\": \"object\", \"properties\": "
                       "{\"code\": {\"type\": \"integer\"}, \"message\": {\"type\": \"string\"}}, "
                       "\"additionalProperties\": false, \"required\": [\"code\", \"message\"]}}, "
                       "\"additionalProperties\": false, \"required\": [\"error\"]}]}");
}

// Gleiche Form wie Tapirs CreateErrorResponse, die Meldung geht aber als GS::UniString ins
// ObjectState (CreateErrorResponse wandelt per ToCStr in CC_Default -> Umlaute in Modell-Namen
// wie "Bruestungshoehe" kaemen verstuemmelt an).
inline GS::ObjectState CreateErrorResponseELM (GSErrCode errorCode, const GS::UniString& message)
{
    GS::ObjectState error;
    error.Add ("code", errorCode);
    error.Add ("message", message);
    return GS::ObjectState ("error", error);
}

// Lesbarer Fenstertyp-Name (Kopie der Zuordnung aus InspectionCommands.cpp, dort im anonymen
// Namespace). Unbekannte Werte als "Other:<Zahl>", damit nichts still verschwindet.
inline GS::UniString WindowTypeNameELM (API_WindowTypeID type)
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
        default:                           break;
    }
    char buffer[32] = {};
    std::snprintf (buffer, sizeof (buffer), "Other:%d", (int) type);
    return GS::UniString (buffer);
}
