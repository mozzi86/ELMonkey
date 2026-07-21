// SetColumnDetails — setzt Stützen-Kernmaße (nominalWidth/Height im
// columnSegments-Memo), die weder Tapir noch die offizielle JSON-API
// schreiben können. Entstanden 2026-07-21 für Bohrpfahl-Anpassung an
// DWG-Kreisdurchmesser (THN). Mit Rücklese-Verifikation.
#pragma once

#include "ELMCommandBase.hpp"

class SetColumnDetailsCommand : public ELMCommandBase
{
public:
    SetColumnDetailsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "SetColumnDetails";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
