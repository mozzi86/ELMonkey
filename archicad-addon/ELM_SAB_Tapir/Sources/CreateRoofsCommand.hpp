// CreateRoofs — erzeugt Ein-Flächen-Dächer (API_PlaneRoofID) aus Polygon +
// Pivot-Linie (Traufkante) + Neigung. Tapir 1.5.3 hat CreateRoofs nur als
// Stub, der IMMER „Multi-plane roof creation is not yet supported" liefert
// (PolyRoof braucht pivotPolyEdges-Setup, das dort fehlt) — Ein-Flächen-
// Dächer über ACAPI_Element_Create sind dagegen stabil. Jede Dachfläche der
// DWG-Dachaufsicht wird ein eigenes PlaneRoof.
#pragma once

#include "ELMCommandBase.hpp"

class ELMCreateRoofsCommand : public ELMCommandBase
{
public:
    ELMCreateRoofsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "CreateRoofs";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
