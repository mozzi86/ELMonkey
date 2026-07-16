// GetPenOfElements — liest die Stift-Indizes von 2D-Elementen (Hatch, Line,
// PolyLine, Arc, Circle, Spline, Text). Gegenstück zu SetPenOfElements:
// nötig für den RGB-Abgleich Import-Stifte → SAB-Stifttabelle.
#pragma once

#include "ELMCommandBase.hpp"

class GetPenOfElementsCommand : public ELMCommandBase
{
public:
    GetPenOfElementsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "GetPenOfElements";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
