// ELM_SAB 0.9.26 — Create2DElements: legt 2D-Elemente (Line, Arc, Circle, PolyLine, Spline,
// Hatch) in der aktiven Grundriss-Datenbank an. Schreib-Gegenstueck zu Get2DGeometryOfElements:
// dessen Ausgabe kann ohne Umbau zurueckgegeben werden. Ablauf: Pruefen je Element (ohne
// anzulegen) -> Anlegen in EINEM Undo-Schritt -> Ruecklese je Element (Geschoss, Ebene,
// Koordinaten; Laenge bzw. Flaeche/Umfang). Fehler je Element, der Rest laeuft weiter.
// Anlass: farbige Fuellflaechen im Flucht- und Rettungswegeplan WiSchu (Frist 15.10.2026) -
// Tapir kann nur Polylinien anlegen. Verbindliche Spec:
// .planning/quick/261013-n2y-elm-sab-0-9-26-create2delements/261013-n2y-SPEC.md
#pragma once
#include "ELMCommandBase.hpp"

class Create2DElementsCommand : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "Create2DElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState&, GS::ProcessControl&) const override;
};
