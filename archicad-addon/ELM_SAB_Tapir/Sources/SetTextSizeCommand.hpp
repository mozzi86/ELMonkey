// SetTextSizeOfElements — setzt die Schriftgröße von Text-Elementen und
// Text-Labels (Etiketten), absolut in mm oder relativ per Faktor.
// Anwendungsfall: Capmo-Ticket-Etiketten massenhaft verkleinern, ohne den
// Inhalt (klickbare URL im PDF-Plot) anzutasten.
#pragma once

#include "ELMCommandBase.hpp"

class SetTextSizeCommand : public ELMCommandBase
{
public:
    SetTextSizeCommand () = default;

    virtual GS::String GetName () const override
    {
        return "SetTextSizeOfElements";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
