// ExportSurfaces — Archicad-Oberflaechen samt Texturbildern als Ordner exportieren (ELM_SAB 0.11.12).
//
// Ergebnis (Format `sab-oberflaechen/1`, Beschreibung in Tools/oberflaechen/FORMAT_sab-oberflaechen-1.md):
//
//   <Ziel>/<Projekt>_oberflaechen_<JJJJMMTT-hhmmss>/
//     oberflaechen.json   je Oberflaeche Name, Index, GUID, Art, Farben, Glanz, Transparenz,
//                         Textur {name, datei, breite_m, hoehe_m, drehung_grad, Flags}
//     texturen/           genau die Bilder, die eine Oberflaeche nutzt
//     LIESMICH.md
//
// Das Format ist dasselbe, das das Archicad-Add-on der Plattform schreibt — der Ordner geht
// unveraendert durch deren Einlese-Werkzeug. Liest nur, aendert nichts am Projekt, reserviert
// in Teamwork nichts.
#pragma once

#include "ELMCommandBase.hpp"

class ExportSurfacesCommand : public ELMCommandBase
{
public:
    ExportSurfacesCommand () = default;

    virtual GS::String GetName () const override
    {
        return "ExportSurfaces";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
