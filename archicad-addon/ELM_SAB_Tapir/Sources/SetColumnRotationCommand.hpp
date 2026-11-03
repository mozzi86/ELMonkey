// SetColumnRotation — setzt den Achs-Drehwinkel (axisRotationAngle) von
// Stützen direkt via ACAPI_Element_Change. Entstanden 2026-07-23, weil Tapir
// RotateElements bei Stützen "Erfolg" meldet, den Winkel aber nicht ändert
// (per Mikro-Test bewiesen: MoveElements greift, RotateElements nicht).
// Eingabe je Element: guid + angleDeg (absoluter Zielwinkel in Grad).
// Antwort je Element: success + angleDeg (zurückgelesener Ist-Winkel).
#pragma once

#include "ELMCommandBase.hpp"

class SetColumnRotationCommand : public ELMCommandBase
{
public:
    SetColumnRotationCommand () = default;

    virtual GS::String GetName () const override
    {
        return "SetColumnRotation";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
