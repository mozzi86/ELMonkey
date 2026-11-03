// SetObjectParametersForce — setzt GDL-Parameter (AddPars) von Objekten/Lampen/
// Etiketten/Zonen und meldet PRO ELEMENT einen echten Fehlercode.
//
// Grund (THN SuD, 2026-09-08): Tapirs SetGDLParametersOfElements, MoveElements und
// SetDetailsOfElements bauen ihre Ergebnisliste INNERHALB des Lambdas von
// ACAPI_CallUndoableCommand auf und werfen dessen Rueckgabewert weg. Fuehrt Archicad
// das Lambda nicht aus (Undo-Scope verweigert), kommt `{"executionResults": []}`
// zurueck — kein Fehler, keine Aenderung, kein Hinweis. 1896 von 3037 KI-Objekten
// standen so still auf Bibliotheks-Defaults.
//
// Dieser Befehl macht es andersherum:
//   * Ergebnisvektor liegt AUSSERHALB des Lambdas (wie SetStoryVisibilityOfElements),
//   * der GSErrCode von ACAPI_CallUndoableCommand wird gemeldet (`undoScope`),
//   * lief das Lambda nicht, wird der Durchgang ohne Undo-Klammer wiederholt
//     (`mode: "direct"`) statt still nichts zu tun,
//   * kein APIFilt-Vorfilter, dafuer Ruecklese-Verifikation je Element.
#pragma once

#include "ELMCommandBase.hpp"

class SetObjectParametersForceCommand : public ELMCommandBase
{
public:
    SetObjectParametersForceCommand () = default;

    virtual GS::String GetName () const override
    {
        return "SetObjectParametersForce";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
