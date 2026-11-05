// CreatePolygonWalls — erzeugt Polygon-Wände (Polywand, APIWtyp_Poly) aus
// geschlossenen 2D-Konturen. Kern des Skill-Workflows „Konturen → Wände":
// 2D-Zeichnung polygonisieren (Magic-Wand-Logik) → dünne, längliche Polygone
// als Wand-Umrisse erkennen → hier als echte Wände erzeugen.
// Tapir 1.5.3 kann nur gerade Wände (beg/end) — Polygon-Wände fehlen dort.
#pragma once

#include "ELMCommandBase.hpp"

class CreatePolygonWallsCommand : public ELMCommandBase
{
public:
    CreatePolygonWallsCommand () = default;

    virtual GS::String GetName () const override
    {
        return "CreatePolygonWalls";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// ELMonkey 0.11.7 — ClonePolygonWall: liest eine bestehende Wand samt komplettem Memo und legt sie um dx/dy
// versetzt per ACAPI_Element_Create neu an. Diagnose: kann die API in dieser Umgebung ueberhaupt Polygonwaende
// erzeugen, wenn die Daten 1:1 von einer Handwand stammen?
class ClonePolygonWallCommand : public ELMCommandBase
{
public:
    ClonePolygonWallCommand () = default;

    virtual GS::String GetName () const override
    {
        return "ClonePolygonWall";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// ELMonkey 0.11.5 — GetPolygonWallInfo: Rohfelder einer Wand (type, rLinInd, Referenzlinie, Polygon),
// um von Hand gezeichnete Polygonwaende mit API-erzeugten zu vergleichen (Diagnose 05.11.2026).
class GetPolygonWallInfoCommand : public ELMCommandBase
{
public:
    GetPolygonWallInfoCommand () = default;

    virtual GS::String GetName () const override
    {
        return "GetPolygonWallInfo";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
