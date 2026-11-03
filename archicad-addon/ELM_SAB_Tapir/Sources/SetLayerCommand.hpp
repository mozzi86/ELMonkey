// ELM_SAB 0.9.19 — SetLayerOfElements: Ebene (header.layer) einer Elementliste setzen.
// Ebene per Attribut-GUID oder Name, Undo-Scope, optionale Teamwork-Reservierung
// (Standard true), Ruecklese je Element. Anlass: THN-Session 2026-10-06 (Ebene setzen
// ohne Ruecklese). Aufbau 1:1 nach SetStoryVisibilityOfElements.
#pragma once
#include "ELMCommandBase.hpp"

class SetLayerOfElementsCommand : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "SetLayerOfElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState&, GS::ProcessControl&) const override;
};
