// SetTextsOfElements — ersetzt den Inhalt von Text-Elementen und Text-Labels.
// Anwendungsfall: Capmo-Etiketten straffen („Ticket 254 · CLOSED" statt
// vierzeiligem Block) — die URL wandert in eigenen Winztext + QR-Objekt.
#pragma once

#include "ELMCommandBase.hpp"

class SetTextsCommand : public ELMCommandBase
{
public:
    SetTextsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "SetTextsOfElements";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
