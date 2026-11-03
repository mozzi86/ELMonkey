// SetPenOfElements — setzt Stifte von 2D-Elementen (Schraffur, Linie, Polylinie,
// Bogen, Kreis, Spline, Text) auf definierte Stift-Indizes der aktiven Stifttabelle.
//
// Motivation (SAB, THN-Projekt): DWG-Importe von Fachplanern bringen Schraffuren/
// Linien mit wilden RGB-Werten mit. Per Ebenen-Filter + diesem Befehl lassen sie
// sich auf saubere SAB-Stifte normalisieren — was weder Tapir noch die offizielle
// JSON-API bisher können (Stand Tapir 1.5.3, Juli 2026).
#pragma once

#include "ELMCommandBase.hpp"

class SetPenOfElementsCommand : public ELMCommandBase
{
public:
    SetPenOfElementsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "SetPenOfElements";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
