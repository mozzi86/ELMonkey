// CreateWallOpenings — ersetzt die KI-Durchbruch-SYMBOLE (Bibliotheksobjekte
// „Deckendurchbruch Symbol" o.ä. auf A_21_AUSSPAR_WAND) durch ECHTE Wand-Öffnungen
// (Archicad-Öffnungs-Werkzeug, API_OpeningID) in der Wirtswand.
//
// Warum ein eigener Befehl und nicht Tapirs CreateOpenings:
//   * Tapirs CreateOpenings kennt nur basePoint/ownerElementId/width/height und legt
//     (AC29-Zweig) immer eine POLYGONALE Öffnung — keine Rundöffnung, kein Anker,
//     keine Grenze („durch die Wand"), keine Grundriss-Darstellung.
//   * Es reserviert die Wirtswand nicht (Teamwork) und meldet Konflikte nicht.
//   * Seine Ergebnisliste entsteht INNERHALB des Undo-Lambdas — läuft das Lambda
//     nicht, kommt eine leere Liste ohne Fehler zurück (siehe
//     SetObjectParametersForceCommand, THN SuD 2026-09-08). Hier liegt die
//     Ergebnisliste AUSSERHALB, und `undoScope` meldet den GSErrCode.
//   * Es stempelt nichts: KI-Stempel, Element-ID, Klassifikation und die vom
//     Quellsymbol geerbten Properties müssten sonst in drei Folgebefehlen nachlaufen.
//
// Höhenbezug: `bottomElevation`/`topElevation` sind relativ zur OKFF des Geschosses,
// auf dem die WIRTSWAND liegt (wall.header.floorInd → API_StoryInfo::level).
//
// Archicad-Version: der Öffnungs-Zweig braucht die moderne ACAPI::Element::Opening-
// API und damit AC29 (`@since Archicad 29` in OpeningDefault.hpp). AC27/28 haben
// API_OpeningType noch in der API_Element-Union, aber die dortigen Felder für Anker,
// Grenze und Grundrissdarstellung sind in den hier vorliegenden Headern (DevKit 29)
// nicht nachlesbar — deshalb meldet der Befehl unter AC27/28 einen klaren Fehler
// statt geratene Feldnamen zu schreiben. Siehe reference/mcp-extension.md.
#pragma once

#include "ELMCommandBase.hpp"

class CreateWallOpeningsCommand : public ELMCommandBase
{
public:
    CreateWallOpeningsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "CreateWallOpenings";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
