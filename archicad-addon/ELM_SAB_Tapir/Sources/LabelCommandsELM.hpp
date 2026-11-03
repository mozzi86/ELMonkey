// ELM_SAB 0.9.19 — Etiketten-Befehle (Anlass: THN-Session 2026-10-06).
//  * CreateLabels: Tapirs CreateLabels ueberschreibt floorInd mit dem Parent-Geschoss
//    (ElementCreationCommands.cpp, "element.header.floorInd = parentElemHead.floorInd")
//    und leitet Zeigerlinien teils gespiegelt ab. Diese Variante nimmt floorInd aus der
//    Eingabe, setzt Koordinaten ohne Ableitung, kann ein Vorlage-Etikett uebernehmen und
//    liest jedes Etikett zurueck. Klassenname bewusst anders als Tapirs CreateLabelsCommand
//    (ODR/Linker); der Befehlsname bleibt "CreateLabels" im Namespace ELM_SAB.
//  * GetLabelsOfElements: ein einziger GetElemList(API_LabelID)-Durchlauf statt einer
//    Abfrage je Owner.
#pragma once
#include "ELMCommandBase.hpp"

class CreateLabelsELMCommand : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "CreateLabels"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState&, GS::ProcessControl&) const override;
};

class GetLabelsOfElementsCommand : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "GetLabelsOfElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState&, GS::ProcessControl&) const override;
};
