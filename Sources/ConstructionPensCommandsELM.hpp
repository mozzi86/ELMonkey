// ELMonkey 0.10.1 — Bauteilstifte lesen/setzen.
//
// Anlass: THN-Ausfuehrungsplaene, rote Wandkonturen aus der Zeit, als die Brandschutzeigenschaften
// in der Wand gepflegt wurden. Brandschutz laeuft jetzt ueber die grafische Ueberschreibung; die
// Bauteile sollen schwarz-weiss werden und die Schraffur wieder aus Baustoff/Verbund kommen.
//
// Weder Tapir 1.7.2 noch die offizielle JSON-API haben dafuer einen Befehl; GetPenOfElements/
// SetPenOfElements koennen nur 2D-Elemente. Typen: Wand, Stuetze, Unterzug, Decke, Dach, Schale,
// Fassade, Morph. Feldnamen = DevKit-Namen (APIdefs_Elements.h, in 27/28/29/30 gleich).
#pragma once

#include "ELMCommandBase.hpp"

// Nur lesen: Typ, Ebene, Geschoss, Aufbau und alle Stiftfelder je Element; mit includeComposites
// zusaetzlich die Konturstifte der benutzten Mehrschicht-Aufbauten (Rot im Element oder im Verbund?).
class GetConstructionPensOfElementsELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "GetConstructionPensOfElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// Schreibt: cutLinePen/uncutLinePen/overheadLinePen nach Rollentabelle, clearCutFillOverride,
// Rohfelder ueber fields; dryRun, Vorpruefung [CODE], Teamwork-Reservierung, EIN Undo-Schritt,
// Ruecklese je Element.
class SetConstructionPensOfElementsELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "SetConstructionPensOfElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
