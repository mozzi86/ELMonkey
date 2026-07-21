// CreateCurtainWallFromAxes — erzeugt eine Pfosten-Riegel-Fassade (Curtain
// Wall) aus expliziten Achsmaßen: Spaltenbreiten (aus 2D-Pfostenachsen) und
// Zeilenhöhen (aus Foto/Schnitt), inkl. opaker Brüstungszeilen. Frames/Panels
// kommen aus den CW-Werkzeug-Defaults (vorher Favorit setzen!). Entstanden
// 2026-07-21 (THN Südfassade: CW-Raster-Neubau per API war sonst unmöglich).
#pragma once

#include "ELMCommandBase.hpp"

class CreateCurtainWallCommand : public ELMCommandBase
{
public:
    CreateCurtainWallCommand () = default;

    virtual GS::String GetName () const override
    {
        return "CreateCurtainWallFromAxes";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
