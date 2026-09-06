// ELM_SAB — "Auf Geschossen zeigen" von Objekten und Lampen setzen/lesen.
// Weder Tapirs SetDetailsOfElements (kennt nur floorInd, also das Ursprungsgeschoss)
// noch die offizielle JSON-API fassen API_ObjectType::visibility oder
// isAutoOnStoryVisibility an. API_LampType ist nur ein Alias auf API_ObjectType
// (DevKit APIdefs_Elements.h:5713), ein Codepfad deckt also Object und Lamp ab.
// Achtung DevKit-Doku (APIdefs_Elements.h:950-955): showRelAbove/showRelBelow sind
// fuer API_ObjectType als "not extended" dokumentiert — deshalb ist die Ruecklese
// in ApplyToOne der alleinige Erfolgsmassstab, nicht der NoError der API.
// 2026-09-05.
#pragma once
#include "ELMCommandBase.hpp"

class SetStoryVisibilityOfElementsCommand : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "SetStoryVisibilityOfElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState&, GS::ProcessControl&) const override;
};

class GetStoryVisibilityOfElementsCommand : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "GetStoryVisibilityOfElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState&, GS::ProcessControl&) const override;
};
