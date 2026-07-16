// Get2DGeometryOfElements — liest die 2D-Geometrie von Line, Arc, Circle und
// PolyLine (inkl. Bogensegmenten) direkt aus dem Projekt. Schließt die
// Tapir-Lücke „GetDetailsOfElements: Not yet supported element type" für
// Linien/Bögen — Grundlage der Konturen→Wände-Pipeline ohne DXF-Export.
#pragma once

#include "ELMCommandBase.hpp"

class Get2DGeometryCommand : public ELMCommandBase
{
public:
    Get2DGeometryCommand () = default;

    virtual GS::String GetName () const override
    {
        return "Get2DGeometryOfElements";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
