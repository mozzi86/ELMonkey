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
