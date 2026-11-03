// ELM_SAB 0.9.23 (2026-10-13, WiSchu-Tuerenabgleich) — FindElements: Elementsuche wie
// "Suchen & aktivieren" mit and/or/not ueber beliebig tiefe Kriterienbaeume. Liest IMMER die
// Grundriss-Datenbank (alle Geschosse), nie das aktive Fenster. Nur lesen.
// Motor + guenstige Kriterien: FindElementsCommand.cpp; Wert-/Beziehungskriterien:
// FindElementsLeaves.cpp; gemeinsamer Vertrag: FindElementsInternal.hpp.
#pragma once
#include "ELMCommandBase.hpp"

class FindElementsCommand : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "FindElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState&, GS::ProcessControl&) const override;
};
