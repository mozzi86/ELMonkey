// GetDiffDataELM — Lesebefehl für den Diff-Checker Archicad ↔ Plattform (ELM_SAB 0.11.13).
//
// Liefert je Element alles, was der Abgleich mit der Plattform braucht, OHNE IFC-Datei:
// GUID, IFC-GlobalId, IFC-Klasse, IFC-Attribute und -Eigenschaften (Vorschau des aktiven
// IFC-Übersetzers), Geschoss, Umbau-Status, Element-ID, Ebene, Aufbau, Oberflächen,
// Klassifizierung, Zone, Wirt von Öffnungen und eine Box der Geometrie aus dem 3D-Modell des
// Elements (nach Anschlüssen/Verschneidungen; alle Körper eines Elements = eine Box).
//
// Verglichen wird NICHT hier, sondern im Python-Monkey (elmonkey.diff_plattform).
// Liest nur: keine Reservierung, kein Filter-, Ebenen- oder Ansichtswechsel. Einzige
// vorübergehende Umstellung: ist nicht der Grundriss die aktuelle Datenbank, wird für das
// Lesen auf ihn umgeschaltet und im selben Aufruf zurückgestellt (gemeldet in context).
#pragma once

#include "ELMCommandBase.hpp"

class GetDiffDataCommandELM : public ELMCommandBase
{
public:
    GetDiffDataCommandELM () = default;

    virtual GS::String GetName () const override
    {
        return "GetDiffDataELM";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
