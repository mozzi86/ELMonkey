// ELM_SAB — Basis für SAB-eigene JSON-Commands im Namespace "ELM_SAB".
// Nutzt die Helfer aus Tapirs CommandBase (CreateErrorResponse etc.).
#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ObjectState.hpp"
#include "CommandBase.hpp"

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
