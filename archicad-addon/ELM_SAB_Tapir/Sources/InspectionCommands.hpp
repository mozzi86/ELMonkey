// ELM_SAB Stufe 2 „Auge" — Zustandsabfragen.
//
// GetElementEditState: pro Element ein ehrliches Urteil, WARUM ein Schreibbefehl
//   scheitern wuerde (Ebene ausgeblendet/gesperrt, Element gesperrt, Gruppe,
//   Hotlink, fremd reserviert, Bibliotheksteil fehlt) — statt nach dem
//   Fehlschlag zu raten.
// GetUIState: modaler Dialog offen? welches Fenster? Teamwork? welcher Nutzer?
// GetDeviations: Ist-Masse gegen uebergebene Sollmasse, je Element und Achse.
#pragma once

#include "ELMCommandBase.hpp"

class GetElementEditStateCommand : public ELMCommandBase
{
public:
    GetElementEditStateCommand () = default;
    virtual GS::String GetName () const override { return "GetElementEditState"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetUIStateCommand : public ELMCommandBase
{
public:
    GetUIStateCommand () = default;
    virtual GS::String GetName () const override { return "GetUIState"; }
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetDeviationsCommand : public ELMCommandBase
{
public:
    GetDeviationsCommand () = default;
    virtual GS::String GetName () const override { return "GetDeviations"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
