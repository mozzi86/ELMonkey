// SetAddParsOfElements — setzt GDL-Parameter (AddPars) über das Element-Memo.
// Sicherer Ersatz für SetGDLParametersOfElements bei ETIKETTEN: Tapirs Befehl
// nutzt APIAny_OpenParameters, das bei Labels crasht (GetLibPartId-Nullzeiger).
// Der Memo-Weg (APIMemoMask_AddPars) funktioniert für Object, Lamp und Label.
#pragma once

#include "ELMCommandBase.hpp"

class SetAddParsCommand : public ELMCommandBase
{
public:
    SetAddParsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "SetAddParsOfElements";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
