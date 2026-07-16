// CreatePolygonWalls — erzeugt Polygon-Wände (Polywand, APIWtyp_Poly) aus
// geschlossenen 2D-Konturen. Kern des Skill-Workflows „Konturen → Wände":
// 2D-Zeichnung polygonisieren (Magic-Wand-Logik) → dünne, längliche Polygone
// als Wand-Umrisse erkennen → hier als echte Wände erzeugen.
// Tapir 1.5.3 kann nur gerade Wände (beg/end) — Polygon-Wände fehlen dort.
#pragma once

#include "ELMCommandBase.hpp"

class CreatePolygonWallsCommand : public ELMCommandBase
{
public:
    CreatePolygonWallsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "CreatePolygonWalls";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
