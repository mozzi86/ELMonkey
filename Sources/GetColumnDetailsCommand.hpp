// GetColumnDetails — liest Stützen-Felder, die Tapir/offizielle API nicht
// liefern: axisRotationAngle (Drehwinkel!), origoPos, isSlanted/isFlipped,
// Kernmaße aus dem ersten Segment. Entstanden 2026-07-20, weil verdrehte
// Bestandsstützen per API sonst unauffindbar sind (Details ohne Winkel,
// Properties defekt, BBox bei Quadraten rotationsblind, PreviewImage
// normalisiert auf Lokalkoordinaten).
#pragma once

#include "ELMCommandBase.hpp"

class GetColumnDetailsCommand : public ELMCommandBase
{
public:
    GetColumnDetailsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "GetColumnDetails";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
