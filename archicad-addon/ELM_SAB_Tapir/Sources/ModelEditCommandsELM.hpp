// ELMonkey 0.9.33 — Wunschliste Michi B2/B3/B7: Aenderungen am Projektzustand mit Ruecklese.
#pragma once

#include "ELMCommandBase.hpp"

// B2: Eigenschafts-Definition aendern OHNE GUID-Wechsel (Auswahlwerte anhaengen, Verfuegbarkeit,
// Name/Beschreibung). Bisher ging das nur ueber Loeschen + Neu (neue GUID, GDL-Referenzen brechen).
class ChangePropertyDefinitionsELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "ChangePropertyDefinitions"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// B3: Aufbau eines BESTEHENDEN Bauteils setzen (Baustoff / Mehrschicht / Profil) fuer Wand, Decke,
// Dach, Schale, Stuetze, Unterzug - nur dieses Element, nicht die Aufbau-Definition.
class SetStructureOfElementsELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "SetStructureOfElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// B7: Sicherungskopie eines lokalen Projekts: erst Sichern, dann Datei kopieren. Das offene Projekt
// bleibt das Original (kein "Sichern unter", das auf die Kopie umschalten wuerde). Teamwork und
// unbenannte Projekte werden mit Begruendung abgelehnt.
class SaveProjectCopyELMCommand : public ELMCommandBase
{
public:
    virtual GS::String GetName () const override { return "SaveProjectCopy"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
