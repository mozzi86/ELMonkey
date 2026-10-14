// ELM_SAB — Bemaßung in Schnitten, Ansichten und Innenansichten.
//
// Warum es die drei Befehle braucht (2026-10-14, Fliesenspiegel Futurelab):
//  * GetSectionElementsELM: Tapirs GetElementsByType/GetAllElements ersetzen jedes
//    Schnittelement (API_SectElemID) durch sein Modell-Bauteil. Assoziative Maße in einer
//    Ansicht hängen aber am Schnittelement — ohne dessen GUID ist keine Maßkette möglich.
//  * GetDimensionDetailsELM: liest die Rohdaten einer bestehenden Maßkette (Bezugsart je
//    Messpunkt: inIndex/special/node_*, Text-Override, Einstellungen). Damit lassen sich
//    Muster-Maßketten nachbauen statt Knotennummern zu raten.
//  * CreateDimensionsLikeTemplateELM: legt Maßketten an, die ALLE Einstellungen (Stifte,
//    Schrift, Marker, Ebene, Textlage) von einer Vorlage-Maßkette übernehmen; Messpunkte
//    assoziativ oder statisch, optional mit eigenem Text (z. B. „OK FFB").
//  * SetDimensionAppearanceELM (0.9.13): ändert bestehende Maßketten an Ort und Stelle —
//    Schriftgröße aller Maßtexte und Hilfslinienwert je Messpunkt; Bezüge bleiben erhalten.
//    0.9.14: verschiebt auch statische Messpunkte und verweigert Ketten mit Bezug auf
//    Fassadenprofile (node_typ 2101) — Archicad verwirft solche Ketten beim Zurückschreiben.
//  * Get/SetInteriorElevation…ELM (0.9.14): vertikaler Bereich (Ober-/Untergrenze) von
//    Innenansichten lesen und setzen.
//
// Kein C++20 (AC27/28 bauen nach C++17).
#pragma once

#include "ELMCommandBase.hpp"

class GetSectionElementsELMCommand : public ELMCommandBase
{
public:
    GetSectionElementsELMCommand () = default;
    virtual GS::String GetName () const override { return "GetSectionElementsELM"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetDimensionDetailsELMCommand : public ELMCommandBase
{
public:
    GetDimensionDetailsELMCommand () = default;
    virtual GS::String GetName () const override { return "GetDimensionDetailsELM"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class SetDimensionAppearanceELMCommand : public ELMCommandBase
{
public:
    SetDimensionAppearanceELMCommand () = default;
    virtual GS::String GetName () const override { return "SetDimensionAppearanceELM"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetInteriorElevationsELMCommand : public ELMCommandBase
{
public:
    GetInteriorElevationsELMCommand () = default;
    virtual GS::String GetName () const override { return "GetInteriorElevationsELM"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class SetInteriorElevationVerticalRangeELMCommand : public ELMCommandBase
{
public:
    SetInteriorElevationVerticalRangeELMCommand () = default;
    virtual GS::String GetName () const override { return "SetInteriorElevationVerticalRangeELM"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class CreateDimensionsLikeTemplateELMCommand : public ELMCommandBase
{
public:
    CreateDimensionsLikeTemplateELMCommand () = default;
    virtual GS::String GetName () const override { return "CreateDimensionsLikeTemplateELM"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
