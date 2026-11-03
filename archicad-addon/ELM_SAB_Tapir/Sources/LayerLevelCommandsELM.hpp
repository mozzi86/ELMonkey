// ELMonkey 0.9.32 — Ebenen lesen/schalten und Deckenhoehen absolut (Wunschliste Michi A3/A4/A5).
#pragma once

#include "ELMCommandBase.hpp"

// A4: alle Ebenen mit ECHTEM Attribut-Index (nicht Listenposition), Name, ausgeblendet, gesperrt, Drahtmodell.
class GetLayersELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "GetLayers"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// A5: Ebenen ein-/ausblenden, sperren/entsperren, Drahtmodell - mit Ruecklese je Ebene.
class SetLayerAttributesELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "SetLayerAttributes"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// A3: Ober- und Unterkante von Decken absolut (aus dem 3D-Begrenzungsquader) plus Parameterwerte.
class GetSlabLevelsELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "GetSlabLevels"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
