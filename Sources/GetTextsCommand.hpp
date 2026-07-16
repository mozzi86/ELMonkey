// GetTextsOfElements — liest Inhalt + Position von Text-Elementen.
// Schließt die Tapir-Lücke „GetDetailsOfElements: Not yet supported" für Texte.
// Anwendungsfall: DWG-Beschriftungen (_BS-Ebenen) auslesen — z. B. UK-Höhen
// von Wanddurchbrüchen — und Öffnungen/Elemente damit korrekt platzieren.
#pragma once

#include "ELMCommandBase.hpp"

class GetTextsCommand : public ELMCommandBase
{
public:
    GetTextsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "GetTextsOfElements";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
